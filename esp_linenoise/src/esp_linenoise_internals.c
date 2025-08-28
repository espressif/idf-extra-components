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
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_vfs_eventfd.h"
#include "esp_linenoise.h"
#include "esp_linenoise_private.h"

static const uint64_t s_abort_signal = 1;

ssize_t esp_linenoise_default_read_bytes(void *user_ctx, int fd, void *buf, size_t count)
{
    esp_linenoise_state_t *state = (esp_linenoise_state_t *)user_ctx;

    if ((fcntl(fd, F_GETFL, 0) & O_NONBLOCK) != 0) {
        /* non blocking read, call read directly */
        return read(fd, buf, count);
    }

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);
    if (state->abort_read_fd != -1) {
        FD_SET(state->abort_read_fd, &read_fds);
    }
    int maxFd = MAX(fd, state->abort_read_fd);
    /* call select to wait for either a read ready */
    int nread = select(maxFd + 1, &read_fds, NULL, NULL, NULL);
    if (nread < 0) {
        return -1;
    }

    if (FD_ISSET(state->abort_read_fd, &read_fds)) {
        /* read termination request happened, return */
        int temp_buf[sizeof(s_abort_signal)];
        nread = read(state->abort_read_fd, temp_buf, sizeof(s_abort_signal));
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

esp_err_t esp_linenoise_set_event_fd(esp_linenoise_state_t *state)
{
    /* Tell linenoise what file descriptor to add to the read file descriptor set,
     * that will be used to signal a read termination */
    esp_vfs_eventfd_config_t config = ESP_VFS_EVENTD_CONFIG_DEFAULT();
    esp_err_t ret = esp_vfs_eventfd_register(&config);
    if (ret != ESP_ERR_INVALID_ARG) {
        state->abort_read_fd = eventfd(0, 0);
    } else {
        /* issue with arg, this should not happen */
        return ESP_FAIL;
    }

    state->mux = xSemaphoreCreateMutex();
    if (state->mux == NULL) {
        return ESP_ERR_NO_MEM;
    }
    xSemaphoreGive(state->mux);

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
    int nwrite = write(state->abort_read_fd, &s_abort_signal, sizeof(s_abort_signal));
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
