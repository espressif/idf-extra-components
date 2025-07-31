/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include "unity.h"
#include "esp_commands.h"
#include "test_esp_commands_utils.h"

/*
 * IMPORTANT:
 * - 8 commands are created in test_esp_commands_utils.h (cmd_a - cmd_h)
 * - the commands are divided in 4 groups (group_1 - group_4)
 * - each group contains 2 commands.
 *      - group_1 contains cmd_a and cmd_b,
 *      [...]
 *      - group_4 contains cmd_g and cmd_h
 */

static void test_setup(void)
{
    const esp_commands_config_t config = ESP_COMMANDS_CONFIG_DEFAULT();
    esp_commands_init(&config);
}

static void test_teardown(void)
{
    esp_commands_deinit();
}

TEST_CASE("help command - called without command set", "[esp_commands]")
{
    test_setup();

    /* call esp_commands_execute to run help command with verbosity 0 */
    int cmd_ret = -1;
    TEST_ASSERT_EQUAL(ESP_OK, esp_commands_execute(NULL, "help -v 0", &cmd_ret));
    TEST_ASSERT_EQUAL(0, cmd_ret);

    /* call esp_commands_execute to run help command with verbosity 1 */
    cmd_ret = -1;
    TEST_ASSERT_EQUAL(ESP_OK, esp_commands_execute(NULL, "help -v 1", &cmd_ret));
    TEST_ASSERT_EQUAL(0, cmd_ret);

    /* call esp_commands_execute to run help command on a registered command */
    cmd_ret = -1;
    TEST_ASSERT_EQUAL(ESP_OK, esp_commands_execute(NULL, "help cmd_a -v 0", &cmd_ret));
    TEST_ASSERT_EQUAL(0, cmd_ret);
    TEST_ASSERT_EQUAL(ESP_OK, esp_commands_execute(NULL, "help cmd_a -v 1", &cmd_ret));
    TEST_ASSERT_EQUAL(0, cmd_ret);

    /* call esp_commands_execute to run help command on an unregistered command */
    cmd_ret = -1;
    TEST_ASSERT_EQUAL(ESP_OK, esp_commands_execute(NULL, "help cmd_w", &cmd_ret));
    TEST_ASSERT_EQUAL(1, cmd_ret);

    /* call esp_commands_execute to run help command on a registered command with wrong
     * verbosity syntax */
    cmd_ret = -1;
    TEST_ASSERT_EQUAL(ESP_OK, esp_commands_execute(NULL, "help cmd_a -v=1", &cmd_ret));
    TEST_ASSERT_EQUAL(1, cmd_ret);

    /* call esp_commands_execute to run help command with too many command names */
    cmd_ret = -1;
    TEST_ASSERT_EQUAL(ESP_OK, esp_commands_execute(NULL, "help cmd_a cmd_b -v 1", &cmd_ret));
    TEST_ASSERT_EQUAL(1, cmd_ret);

    test_teardown();
}

typedef struct cmd_test_sequence {
    const char *cmd_list[NB_OF_REGISTERED_CMD];
    int expected_ret_val[NB_OF_REGISTERED_CMD];
} cmd_test_sequence_t;

TEST_CASE("help command - called with command set", "[esp_commands]")
{
    test_setup();

    int cmd_ret = -1;

    /* create a command set with group 1 and 3 */
    const char *group_set_a[] = {"group_1", "group_3"};
    esp_command_set_handle_t handle_set_a = esp_commands_create_cmd_set(group_set_a, 2, FIELD_ACCESSOR(group));
    printf("TEST_ASSERT_NOT_NULL(handle_set_a)\n");
    TEST_ASSERT_NOT_NULL(handle_set_a);

    /* create a command set with group 2 and 4 */
    const char *group_set_b[] = {"group_2", "group_4"};
    esp_command_set_handle_t handle_set_b = esp_commands_create_cmd_set(group_set_b, 2, FIELD_ACCESSOR(group));
    printf("TEST_ASSERT_NOT_NULL(handle_set_b)\n");
    TEST_ASSERT_NOT_NULL(handle_set_b);

    /* call execute command with the set_a and all command names */
    cmd_test_sequence_t test_cmd = {
        .cmd_list = {"cmd_a", "cmd_b", "cmd_c", "cmd_d", "cmd_e", "cmd_f", "cmd_g", "cmd_h"},
        .expected_ret_val = {0, 0, 1, 1, 0, 0, 1, 1}
    };
    for (size_t i = 0; i < NB_OF_REGISTERED_CMD; i++) {
        esp_err_t expected_ret_val = ESP_OK;
        if (test_cmd.expected_ret_val[i] == 1) {
            expected_ret_val = ESP_ERR_NOT_FOUND;
        }
        TEST_ASSERT_EQUAL(expected_ret_val, esp_commands_execute(handle_set_a, test_cmd.cmd_list[i], &cmd_ret));
        TEST_ASSERT_EQUAL(test_cmd.expected_ret_val[i], cmd_ret);
    }

    /* call execute command with the set_b and all command names */
    memcpy(test_cmd.expected_ret_val, (int[]) {
        1, 1, 0, 0, 1, 1, 0, 0
    }, sizeof(int) * NB_OF_REGISTERED_CMD);
    for (size_t i = 0; i < NB_OF_REGISTERED_CMD; i++) {
        esp_err_t expected_ret_val = ESP_OK;
        if (test_cmd.expected_ret_val[i] == 1) {
            expected_ret_val = ESP_ERR_NOT_FOUND;
        }
        TEST_ASSERT_EQUAL(expected_ret_val, esp_commands_execute(handle_set_b, test_cmd.cmd_list[i], &cmd_ret));
        TEST_ASSERT_EQUAL(test_cmd.expected_ret_val[i], cmd_ret);
    }

    /* call esp_commands_execute to run help command with both sets */
    TEST_ASSERT_EQUAL(ESP_OK, esp_commands_execute(handle_set_a, "help", &cmd_ret));
    TEST_ASSERT_EQUAL(0, cmd_ret);

    TEST_ASSERT_EQUAL(ESP_OK, esp_commands_execute(handle_set_b, "help", &cmd_ret));
    TEST_ASSERT_EQUAL(0, cmd_ret);

    /* destroy the sets */
    esp_commands_destroy_cmd_set(handle_set_a);
    TEST_ASSERT_EQUAL(NULL, handle_set_a);
    esp_commands_destroy_cmd_set(handle_set_b);
    TEST_ASSERT_EQUAL(NULL, handle_set_b);

    /* create a command set with cmd_a, cmd_b and cmd_c */
    const char *cmd_name_set_a[] = {"cmd_a", "cmd_b", "cmd_c"};
    handle_set_a = esp_commands_create_cmd_set(cmd_name_set_a, 3, FIELD_ACCESSOR(name));
    printf("TEST_ASSERT_NOT_NULL(handle_set_a)\n");
    TEST_ASSERT_NOT_NULL(handle_set_a);

    /* create a command set with cmd_a, cmd_b and cmd_c */
    const char *cmd_name_set_b[] = {"cmd_f", "cmd_g", "cmd_h"};
    handle_set_b = esp_commands_create_cmd_set(cmd_name_set_b, 3, FIELD_ACCESSOR(name));
    printf("TEST_ASSERT_NOT_NULL(handle_set_b)\n");
    TEST_ASSERT_NOT_NULL(handle_set_b);

    /* create a command set that concatenates the previous 2 sets */
    esp_command_set_handle_t handle_set_c = esp_commands_concat_cmd_set(handle_set_a, handle_set_b);
    printf("TEST_ASSERT_NOT_NULL(handle_set_c)\n");
    TEST_ASSERT_NOT_NULL(handle_set_c);

    /* call execute command with the set_a and all command names */
    memcpy(test_cmd.expected_ret_val, (int[]) {
        0, 0, 0, 1, 1, 1, 1, 1
    }, sizeof(int) * NB_OF_REGISTERED_CMD);
    for (size_t i = 0; i < NB_OF_REGISTERED_CMD; i++) {
        esp_err_t expected_ret_val = ESP_OK;
        if (test_cmd.expected_ret_val[i] == 1) {
            expected_ret_val = ESP_ERR_NOT_FOUND;
        }
        TEST_ASSERT_EQUAL(expected_ret_val, esp_commands_execute(handle_set_a, test_cmd.cmd_list[i], &cmd_ret));
        TEST_ASSERT_EQUAL(test_cmd.expected_ret_val[i], cmd_ret);
    }

    /* call execute command with the set_b and all command names */
    memcpy(test_cmd.expected_ret_val, (int[]) {
        1, 1, 1, 1, 1, 0, 0, 0
    }, sizeof(int) * NB_OF_REGISTERED_CMD);
    for (size_t i = 0; i < NB_OF_REGISTERED_CMD; i++) {
        esp_err_t expected_ret_val = ESP_OK;
        if (test_cmd.expected_ret_val[i] == 1) {
            expected_ret_val = ESP_ERR_NOT_FOUND;
        }
        TEST_ASSERT_EQUAL(expected_ret_val, esp_commands_execute(handle_set_b, test_cmd.cmd_list[i], &cmd_ret));
        TEST_ASSERT_EQUAL(test_cmd.expected_ret_val[i], cmd_ret);
    }

    /* call execute command with the set_c and all command names */
    memcpy(test_cmd.expected_ret_val, (int[]) {
        0, 0, 0, 1, 1, 0, 0, 0
    }, sizeof(int) * NB_OF_REGISTERED_CMD);
    for (size_t i = 0; i < NB_OF_REGISTERED_CMD; i++) {
        esp_err_t expected_ret_val = ESP_OK;
        if (test_cmd.expected_ret_val[i] == 1) {
            expected_ret_val = ESP_ERR_NOT_FOUND;
        }
        TEST_ASSERT_EQUAL(expected_ret_val, esp_commands_execute(handle_set_c, test_cmd.cmd_list[i], &cmd_ret));
        TEST_ASSERT_EQUAL(test_cmd.expected_ret_val[i], cmd_ret);
    }
}
