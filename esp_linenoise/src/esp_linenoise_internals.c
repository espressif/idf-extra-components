/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/param.h>
#include "sys/queue.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_vfs_eventfd.h"
#include "esp_linenoise.h"
#include "esp_linenoise_private.h"

typedef struct eventfd_pair {
    int eventfd;
    int in_fd;
    SLIST_ENTRY(eventfd_pair) next_pair;
} eventfd_pair_t;

static const uint64_t s_abort_signal = 1;
static SLIST_HEAD(eventfd_pair_ll, eventfd_pair) s_eventfd_pairs = SLIST_HEAD_INITIALIZER(eventfd_pair);

static int esp_linenoise_get_eventfd_from_fd(const int fd)
{
    /* find the eventfd to use to abort for the given fd */
    eventfd_pair_t *eventfd_pair = NULL;
    SLIST_FOREACH(eventfd_pair, &s_eventfd_pairs, next_pair) {
        if (eventfd_pair->in_fd == fd) {
            return eventfd_pair->eventfd;
        }
    }

    return -1;
}

ssize_t esp_linenoise_default_read_bytes(int fd, void *buf, size_t count)
{
    if ((fcntl(fd, F_GETFL, 0) & O_NONBLOCK) != 0) {
        /* non blocking read, call read directly */
        return read(fd, buf, count);
    }

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);

    /* find the eventfd to use to abort for the given fd */
    int max_fd = -1;
    int abort_read_fd = esp_linenoise_get_eventfd_from_fd(fd);
    if (abort_read_fd != -1) {
        FD_SET(abort_read_fd, &read_fds);
        max_fd = MAX(fd, abort_read_fd);
    } else {
        max_fd = fd;
    }

    /* call select to wait for data to read or an abort signal */
    int nread = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
    if (nread < 0) {
        return -1;
    }

    if (FD_ISSET(abort_read_fd, &read_fds)) {
        /* read termination request happened, return */
        int temp_buf[sizeof(s_abort_signal)];
        nread = read(abort_read_fd, temp_buf, sizeof(s_abort_signal));
        if ((nread == sizeof(s_abort_signal)) && (temp_buf[0] == s_abort_signal)) {
            /* populate the buffer with a new line character, forcing esp_linenoise_raw
            * or esp_linenoise_dumb to return */
            nread = 1;
            memcpy(buf, "\n", 1);
        }
    } else if (FD_ISSET(fd, &read_fds)) {
        /* a read ready triggered the select to return. call the
        * read function with the number of bytes max_bytes */
        nread = read(fd, buf, count);
    }
    return nread;
}

esp_err_t esp_linenoise_set_event_fd(esp_linenoise_instance_t *instance)
{
    esp_linenoise_config_t *config = &instance->config;
    esp_linenoise_state_t *state = &instance->state;

    /* Tell linenoise what file descriptor to add to the read file descriptor set,
     * that will be used to signal a read termination */
    esp_vfs_eventfd_config_t eventfd_config = {
        .max_fds = CONFIG_ESP_LINENOISE_MAX_INSTANCE_NB
    };
    esp_err_t ret = esp_vfs_eventfd_register(&eventfd_config);
    int new_eventfd = -1;
    if (ret != ESP_ERR_INVALID_ARG) {
        new_eventfd = eventfd(0, 0);
    } else {
        /* issue with arg, this should not happen */
        return ESP_FAIL;
    }

    /* make sure the FD returned is not -1, which would indicate that eventfd
     * has reached the maximum number of FDs it can create */
    if (new_eventfd == -1) {
        return ESP_FAIL;
    }

    state->mux = xSemaphoreCreateMutex();
    if (state->mux == NULL) {
        close(new_eventfd);
        return ESP_ERR_NO_MEM;
    }

    /* one eventfd will be created for a given instance. In order to be able
     * to use the proper eventfd in the read, couple the created eventfd to
     * the in_fd from the config of the given instance. */
    eventfd_pair_t *new_pair = malloc(sizeof(eventfd_pair_t));
    if (new_pair == NULL) {
        close(new_eventfd);
        vSemaphoreDelete(state->mux);
        return ESP_ERR_NO_MEM;
    }
    new_pair->eventfd = new_eventfd;
    new_pair->in_fd = config->in_fd;
    SLIST_INSERT_HEAD(&s_eventfd_pairs, new_pair, next_pair);

    xSemaphoreGive(state->mux);
    return ESP_OK;
}

esp_err_t esp_linenoise_remove_event_fd(esp_linenoise_instance_t *instance)
{
    esp_linenoise_config_t *config = &instance->config;
    esp_linenoise_state_t *state = &instance->state;

    /* find the in_fd in the list of eventfd / in_fd pairs.
     * if found, remove the item from the list, close the eventfd */
    eventfd_pair_t *cur = NULL;
    eventfd_pair_t *prev = NULL;
    SLIST_FOREACH(cur, &s_eventfd_pairs, next_pair) {
        if (cur->in_fd == config->in_fd) {
            /* close the eventfd */
            close(cur->eventfd);

            if (prev == NULL) {
                /* remove head */
                SLIST_REMOVE_HEAD(&s_eventfd_pairs, next_pair);
            } else {
                /* remove item in the middle of the list */
                prev->next_pair.sle_next = cur->next_pair.sle_next;
            }

            /* return from the loop */
            break;
        }

        prev = cur;
    }

    if (cur == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    /* free the item that was removed */
    free(cur);

    /* free the mutex */
    vSemaphoreDelete(state->mux);

    /* if the list is empty, it means the last instance of esp_linenoise
     * running is being deleted. Unregister eventfd to free the heap memory
     * allocated when calling esp_vfs_eventfd_register */
    if (SLIST_EMPTY(&s_eventfd_pairs)) {
        return esp_vfs_eventfd_unregister();
    }

    return ESP_OK;
}

esp_err_t esp_linenoise_abort(esp_linenoise_handle_t handle)
{
    esp_linenoise_config_t *config = &handle->config;
    esp_linenoise_state_t *state = &handle->state;

    if (config->read_bytes_cb != esp_linenoise_default_read_bytes) {
        /* we are not using the default read bytes function provided\
         * by esp_linenoise, therefore it is not esp_linenoise responsibility
         * to trigger the return from esp_linenoise_get_line */
        return ESP_ERR_INVALID_STATE;
    }

    /* send the signal to force esp_linenoise_default_read_bytes
     * to return */
    int abort_read_fd = esp_linenoise_get_eventfd_from_fd(config->in_fd);
    if (abort_read_fd == -1) {
        return ESP_FAIL;
    }

    int nwrite = write(abort_read_fd, &s_abort_signal, sizeof(s_abort_signal));
    if (nwrite != sizeof(s_abort_signal)) {
        return ESP_FAIL;
    }

    /* wait for esp_linenoise_get_line to signal it returned */
    (void)xSemaphoreTake(state->mux, portMAX_DELAY);

    /* mutex acquired successfully, esp_linenoise_get_line returned
     * we can release the mutex directly so it can be taken again
     * when esp_linenoise_get_line is called again */
    xSemaphoreGive(state->mux);

    return ESP_OK;
}
