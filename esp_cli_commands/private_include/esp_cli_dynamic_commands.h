/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <string.h>
#include "sys/queue.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_cli_commands.h"

/**
 * @brief Structure representing a fixed set of commands.
 *
 * This is typically used for static or predefined command lists.
 */
typedef struct esp_cli_command_set {
    esp_cli_command_t **cmd_ptr_set; /*!< Array of pointers to commands. */
    size_t cmd_set_size; /*!< Number of commands in the set. */
} esp_cli_command_set_t;

/**
 * @brief Internal structure for a dynamically registered command.
 *
 * Each dynamic command is stored as an `esp_cli_command_t` plus
 * linked list metadata for insertion/removal.
 */
typedef struct esp_cli_command_internal {
    esp_cli_command_t cmd; /*!< Command instance. */
    SLIST_ENTRY(esp_cli_command_internal) next_item; /*!< Linked list entry metadata. */
} esp_cli_command_internal_t;

/**
 * @brief Linked list head type for dynamic command storage.
 */
typedef SLIST_HEAD(esp_cli_command_internal_ll, esp_cli_command_internal) esp_cli_command_internal_ll_t;

/**
 * @brief Iterate over a set of commands, either from a static set or dynamic list.
 *
 * This macro supports iterating over:
 * - A provided `esp_cli_command_set_t` (static set), OR
 * - The global dynamic command list if `cmd_set` is `NULL`.
 *
 * @param cmd_set Pointer to a command set (`esp_cli_command_set_t`) or `NULL` for dynamic commands.
 * @param item_cmd Iterator variable of type `esp_cli_command_t *` that will point to each command.
 *
 * @note Internally, the macro uses `_node` and `_i` as hidden variables.
 */
#define FOR_EACH_DYNAMIC_COMMAND(cmd_set, item_cmd)                             \
    __attribute__((unused)) esp_cli_command_internal_t *_node =                     \
            ((cmd_set) == NULL ? SLIST_FIRST(esp_cli_dynamic_commands_get_list())  \
                                : NULL);                                        \
    __attribute__((unused)) size_t _i = 0;                                      \
    for (;                                                                      \
         ((cmd_set) == NULL                                                     \
              ? ((_node != NULL) && ((item_cmd) = &_node->cmd))                 \
              : (_i < (cmd_set)->cmd_set_size &&                                \
                 ((item_cmd) = (cmd_set)->cmd_ptr_set[_i])));                   \
         ((cmd_set) == NULL                                                     \
              ? (_node = SLIST_NEXT(_node, next_item))                          \
              : (void)++_i))

/**
 * @brief Acquire the dynamic commands lock.
 *
 * This function must be called before modifying or iterating over
 * the dynamic command list to ensure thread safety.
 */
void esp_cli_dynamic_commands_lock(void);

/**
 * @brief Release the dynamic commands lock.
 *
 * Call this after operations on the dynamic command list are complete.
 */
void esp_cli_dynamic_commands_unlock(void);

/**
 * @brief Get the internal linked list of dynamic commands.
 *
 * @return Pointer to the dynamic command linked list head.
 *
 * @warning The returned list is internal; do not modify it directly.
 *          Use provided API functions to modify dynamic commands.
 */
const esp_cli_command_internal_ll_t *esp_cli_dynamic_commands_get_list(void);

/**
 * @brief Add a new command to the dynamic command list.
 *
 * @param cmd Pointer to the command to add.
 * @return
 *      - `ESP_OK` on success.
 *      - Appropriate error code on failure.
 *
 * @note The function acquires the lock internally.
 */
esp_err_t esp_cli_dynamic_commands_add(esp_cli_command_t *cmd);

/**
 * @brief Replace an existing command in the dynamic command list.
 *
 * If a command with the same name exists, it will be replaced.
 *
 * @param item_cmd Pointer to the new command data.
 * @return
 *      - `ESP_OK` on success.
 *      - Appropriate error code on failure.
 */
esp_err_t esp_cli_dynamic_commands_replace(esp_cli_command_t *item_cmd);

/**
 * @brief Remove a command from the dynamic command list.
 *
 * @param item_cmd Pointer to the command to remove.
 * @return
 *      - `ESP_OK` on success.
 *      - Appropriate error code on failure.
 */
esp_err_t esp_cli_dynamic_commands_remove(esp_cli_command_t *item_cmd);

/**
 * @brief Get the number of registered dynamic commands.
 *
 * @return The total number of dynamic commands currently registered.
 */
size_t esp_cli_dynamic_commands_get_number_of_cmd(void);

#ifdef __cplusplus
}
#endif
