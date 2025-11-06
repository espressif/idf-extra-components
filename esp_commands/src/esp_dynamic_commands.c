/*
 * SPDX-FileCopyrightText: 2016-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_commands_internal.h"
#include "esp_dynamic_commands.h"
#include "esp_commands.h"

#define CONTAINER_OF(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

static esp_command_internal_ll_t s_dynamic_cmd_list = SLIST_HEAD_INITIALIZER(esp_command_internal);
static size_t s_number_of_registered_commands = 0;
static SemaphoreHandle_t s_esp_commands_dyn_mutex = NULL;
static StaticSemaphore_t s_esp_commands_dyn_mutex_buf;

void esp_dynamic_commands_lock(void)
{
    /* check if the mutex needs to be initialized and initialized it only
     * is requested in by the state of the create parameter */
    if (s_esp_commands_dyn_mutex == NULL) {
        s_esp_commands_dyn_mutex = xSemaphoreCreateMutexStatic(&s_esp_commands_dyn_mutex_buf);
        assert(s_esp_commands_dyn_mutex != NULL);
    }

    xSemaphoreTake(s_esp_commands_dyn_mutex, portMAX_DELAY);
}

void esp_dynamic_commands_unlock(void)
{
    if (s_esp_commands_dyn_mutex == NULL) {
        return;
    }
    xSemaphoreGive(s_esp_commands_dyn_mutex);
}

const esp_command_internal_ll_t *esp_dynamic_commands_get_list(void)
{
    return &s_dynamic_cmd_list;
}

esp_err_t esp_dynamic_commands_add(esp_command_t *cmd)
{
    if (!cmd) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_command_internal_t *list_item = esp_commands_malloc(sizeof(esp_command_internal_t));
    if (!list_item) {
        return ESP_ERR_NO_MEM;
    }

    memcpy(&list_item->cmd, cmd, sizeof(esp_command_t));

    esp_command_internal_t *last = NULL;
    esp_command_internal_t *it =  NULL;

    /* this could be called on an empty list, make sure the
     * mutex is initialized */
    esp_dynamic_commands_lock();

    SLIST_FOREACH(it, &s_dynamic_cmd_list, next_item) {
        if (strcmp(it->cmd.name, list_item->cmd.name) > 0) {
            break;
        }
        last = it;
    }

    if (last == NULL) {
        SLIST_INSERT_HEAD(&s_dynamic_cmd_list, list_item, next_item);
    } else {
        SLIST_INSERT_AFTER(last, list_item, next_item);
    }

    s_number_of_registered_commands++;

    esp_dynamic_commands_unlock();

    return ESP_OK;
}

esp_err_t esp_dynamic_commands_replace(esp_command_t *item_cmd)
{
    esp_dynamic_commands_lock();

    esp_command_internal_t *list_item = CONTAINER_OF(item_cmd, esp_command_internal_t, cmd);
    memcpy(&list_item->cmd, item_cmd, sizeof(esp_command_t));

    esp_dynamic_commands_unlock();

    return ESP_OK;
}

esp_err_t esp_dynamic_commands_remove(esp_command_t *item_cmd)
{
    esp_dynamic_commands_lock();

    esp_command_internal_t *list_item = CONTAINER_OF(item_cmd, esp_command_internal_t, cmd);
    SLIST_REMOVE(&s_dynamic_cmd_list, list_item, esp_command_internal, next_item);

    s_number_of_registered_commands--;

    esp_dynamic_commands_unlock();

    free(list_item);

    return ESP_OK;
}

size_t esp_dynamic_commands_get_number_of_cmd(void)
{
    esp_dynamic_commands_lock();
    size_t nb_of_registered_cmd = s_number_of_registered_commands;
    esp_dynamic_commands_unlock();
    return nb_of_registered_cmd;
}
