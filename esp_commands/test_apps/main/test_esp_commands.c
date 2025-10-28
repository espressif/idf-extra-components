/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include "unity.h"
#include "esp_heap_caps.h"
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
    const esp_commands_config_t config = {
        .heap_caps_used = MALLOC_CAP_DEFAULT,
        .hint_bold = false,
        .hint_color = 39,
        .max_cmdline_args = 32,
        .max_cmdline_length = 256
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_commands_update_config(&config));
}

TEST_CASE("help command - called without command set", "[esp_commands]")
{
    test_setup();

    /* call esp_commands_execute to run help command with verbosity 0 */
    int cmd_ret = -1;
    TEST_ASSERT_EQUAL(ESP_OK, esp_commands_execute(NULL, NULL, "help -v 0", &cmd_ret));
    TEST_ASSERT_EQUAL(0, cmd_ret);

    /* call esp_commands_execute to run help command with verbosity 1 */
    cmd_ret = -1;
    TEST_ASSERT_EQUAL(ESP_OK, esp_commands_execute(NULL, NULL, "help -v 1", &cmd_ret));
    TEST_ASSERT_EQUAL(0, cmd_ret);

    /* call esp_commands_execute to run help command on a registered command */
    cmd_ret = -1;
    TEST_ASSERT_EQUAL(ESP_OK, esp_commands_execute(NULL, NULL, "help cmd_a -v 0", &cmd_ret));
    TEST_ASSERT_EQUAL(0, cmd_ret);
    TEST_ASSERT_EQUAL(ESP_OK, esp_commands_execute(NULL, NULL, "help cmd_a -v 1", &cmd_ret));
    TEST_ASSERT_EQUAL(0, cmd_ret);

    /* call esp_commands_execute to run help command on an unregistered command */
    cmd_ret = -1;
    TEST_ASSERT_EQUAL(ESP_OK, esp_commands_execute(NULL, NULL, "help cmd_w", &cmd_ret));
    TEST_ASSERT_EQUAL(1, cmd_ret);

    /* call esp_commands_execute to run help command on a registered command with wrong
     * verbosity syntax */
    cmd_ret = -1;
    TEST_ASSERT_EQUAL(ESP_OK, esp_commands_execute(NULL, NULL, "help cmd_a -v=1", &cmd_ret));
    TEST_ASSERT_EQUAL(1, cmd_ret);

    /* call esp_commands_execute to run help command with too many command names */
    cmd_ret = -1;
    TEST_ASSERT_EQUAL(ESP_OK, esp_commands_execute(NULL, NULL, "help cmd_a cmd_b -v 1", &cmd_ret));
    TEST_ASSERT_EQUAL(1, cmd_ret);
}

TEST_CASE("test command set error handling", "[esp_commands]")
{
    test_setup();

    /* create a command set with NULL passed as list of command id */
    TEST_ASSERT_NULL(esp_commands_create_cmd_set(NULL, 2, FIELD_ACCESSOR(group)));

    /* create a command set with 0 as size of list of command id */
    const char *group_set_a[] = {"b", "group_4"};
    TEST_ASSERT_NULL(esp_commands_create_cmd_set(group_set_a, 0, FIELD_ACCESSOR(group)));

    /* concatenate 2 NULL sets */
    TEST_ASSERT_NULL(esp_commands_concat_cmd_set(NULL, NULL));

    /* redefinition of esp_command_set_t so we can access the fields
     *  and test their values */
    typedef struct cmd_set {
        esp_command_t **cmd_ptr_set;
        size_t cmd_set_size;
    } cmd_set_t;

    /* pass wrong command name in array, expect a non null command set handle with 0 items in it*/
    const char *group_set_b[] = {"group2", "group4"};
    esp_command_set_handle_t group_set_handle_b = esp_commands_create_cmd_set(group_set_b, 2, FIELD_ACCESSOR(group));
    cmd_set_t *cmd_set = (cmd_set_t *)group_set_handle_b;
    TEST_ASSERT_NOT_NULL(group_set_handle_b);
    TEST_ASSERT_NULL(cmd_set->cmd_ptr_set);
    TEST_ASSERT_EQUAL(0, cmd_set->cmd_set_size);

    esp_commands_destroy_cmd_set(&group_set_handle_b);
}

typedef struct cmd_test_sequence {
    const char *cmd_list[NB_OF_REGISTERED_CMD];
    int expected_ret_val[NB_OF_REGISTERED_CMD];
} cmd_test_sequence_t;

static void run_cmd_test(esp_command_set_handle_t handle, const char **cmd_list, const int *expected_ret_val, size_t nb_cmds)
{
    for (size_t i = 0; i < nb_cmds; i++) {
        int cmd_ret = -1;
        esp_err_t expected = expected_ret_val[i] == 0 ? ESP_OK : ESP_ERR_NOT_FOUND;
        TEST_ASSERT_EQUAL(expected, esp_commands_execute(handle, NULL, cmd_list[i], &cmd_ret));
        TEST_ASSERT_EQUAL(expected_ret_val[i], cmd_ret);
    }
}

TEST_CASE("test static command set", "[esp_commands]")
{
    test_setup();

    const char *cmd_list[] = {"cmd_a", "cmd_b", "cmd_c", "cmd_d", "cmd_e", "cmd_f", "cmd_g", "cmd_h"};
    const size_t nb_cmds = sizeof(cmd_list) / sizeof(cmd_list[0]);
    int expected_ret_val[nb_cmds];

    /* create sets by group */
    const char *group_set_a[] = {"group_1", "group_3"};
    esp_command_set_handle_t handle_set_a = ESP_COMMANDS_CREATE_CMD_SET(group_set_a, FIELD_ACCESSOR(group));
    TEST_ASSERT_NOT_NULL(handle_set_a);

    const char *group_set_b[] = {"group_2", "group_4"};
    esp_command_set_handle_t handle_set_b = ESP_COMMANDS_CREATE_CMD_SET(group_set_b, FIELD_ACCESSOR(group));
    TEST_ASSERT_NOT_NULL(handle_set_b);

    /* test set_a by group */
    int tmp_ret[] = {0, 0, -1, -1, 0, 0, -1, -1};
    memcpy(expected_ret_val, tmp_ret, sizeof(tmp_ret));
    run_cmd_test(handle_set_a, cmd_list, expected_ret_val, nb_cmds);

    /* test set_b by group */
    int tmp_ret_b[] = {-1, -1, 0, 0, -1, -1, 0, 0};
    memcpy(expected_ret_val, tmp_ret_b, sizeof(tmp_ret_b));
    run_cmd_test(handle_set_b, cmd_list, expected_ret_val, nb_cmds);

    /* test help command with set of static commands */
    int cmd_ret;
    TEST_ASSERT_EQUAL(ESP_OK, esp_commands_execute(handle_set_a, NULL, "help", &cmd_ret));
    TEST_ASSERT_EQUAL(0, cmd_ret);
    TEST_ASSERT_EQUAL(ESP_OK, esp_commands_execute(handle_set_b, NULL, "help", &cmd_ret));
    TEST_ASSERT_EQUAL(0, cmd_ret);

    /* destroy sets */
    esp_commands_destroy_cmd_set(&handle_set_a);
    esp_commands_destroy_cmd_set(&handle_set_b);

    /* create sets by name */
    const char *cmd_name_set_a[] = {"cmd_a", "cmd_b", "cmd_c"};
    handle_set_a = esp_commands_create_cmd_set(cmd_name_set_a, 3, FIELD_ACCESSOR(name));
    TEST_ASSERT_NOT_NULL(handle_set_a);

    const char *cmd_name_set_b[] = {"cmd_f", "cmd_g", "cmd_h"};
    handle_set_b = esp_commands_create_cmd_set(cmd_name_set_b, 3, FIELD_ACCESSOR(name));
    TEST_ASSERT_NOT_NULL(handle_set_b);

    int tmp_ret2[] = {0, 0, 0, -1, -1, -1, -1, -1};
    memcpy(expected_ret_val, tmp_ret2, sizeof(tmp_ret2));
    run_cmd_test(handle_set_a, cmd_list, expected_ret_val, nb_cmds);

    int tmp_ret3[] = {-1, -1, -1, -1, -1, 0, 0, 0};
    memcpy(expected_ret_val, tmp_ret3, sizeof(tmp_ret3));
    run_cmd_test(handle_set_b, cmd_list, expected_ret_val, nb_cmds);

    /* concatenate sets */
    esp_command_set_handle_t handle_set_c = esp_commands_concat_cmd_set(handle_set_a, handle_set_b);
    TEST_ASSERT_NOT_NULL(handle_set_c);

    int tmp_ret4[] = {0, 0, 0, -1, -1, 0, 0, 0};
    memcpy(expected_ret_val, tmp_ret4, sizeof(tmp_ret4));
    run_cmd_test(handle_set_c, cmd_list, expected_ret_val, nb_cmds);

    esp_commands_destroy_cmd_set(&handle_set_c);
}

static int dummy_cmd_func(void *context, esp_commands_exec_arg_t *cmd_args, int argc, char **argv)
{
    (void)cmd_args;
    (void)context;
    printf("dynamic command called\n");
    return 0; // always return success
}

TEST_CASE("test dynamic command set", "[esp_commands]")
{
    test_setup();

    const char *cmd_list[] = {"cmd_1", "cmd_2", "cmd_3", "cmd_4", "cmd_5", "cmd_6", "cmd_7", "cmd_8"};
    const size_t nb_cmds = sizeof(cmd_list) / sizeof(cmd_list[0]);
    int expected_ret_val[nb_cmds];

    /* dynamically register commands */
    for (size_t i = 0; i < nb_cmds; i++) {
        esp_command_t cmd = {
            .name = cmd_list[i],
            .group = (i % 2 == 0) ? "group_a" : "group_b",
            .help = "dummy help",
            .func = dummy_cmd_func,  // implement a simple dummy function returning i%2
            .func_ctx = NULL,
            .hint_cb = NULL,
            .glossary_cb = NULL
        };
        TEST_ASSERT_EQUAL(ESP_OK, esp_commands_register_cmd(&cmd));
    }

    /* test execution by group_a */
    const char *group_set[] = {"group_a"};
    esp_command_set_handle_t handle_set_1 = ESP_COMMANDS_CREATE_CMD_SET(group_set, FIELD_ACCESSOR(group));
    TEST_ASSERT_NOT_NULL(handle_set_1);

    int tmp_ret[] = {0, -1, 0, -1, 0, -1, 0, -1};
    memcpy(expected_ret_val, tmp_ret, sizeof(tmp_ret));
    run_cmd_test(handle_set_1, cmd_list, expected_ret_val, nb_cmds);

    /* test execution by command name */
    const char *cmd_name_set[] = {"cmd_1", "cmd_2", "cmd_3"};
    esp_command_set_handle_t handle_set_2 = esp_commands_create_cmd_set(cmd_name_set, 3, FIELD_ACCESSOR(name));
    TEST_ASSERT_NOT_NULL(handle_set_2);

    int tmp_ret2[] = {0, 0, 0, -1, -1, -1, -1, -1};
    memcpy(expected_ret_val, tmp_ret2, sizeof(tmp_ret2));
    run_cmd_test(handle_set_2, cmd_list, expected_ret_val, nb_cmds);

    /* test help command with set of dynamic commands */
    int cmd_ret;
    TEST_ASSERT_EQUAL(ESP_OK, esp_commands_execute(handle_set_1, NULL, "help", &cmd_ret));
    TEST_ASSERT_EQUAL(0, cmd_ret);
    TEST_ASSERT_EQUAL(ESP_OK, esp_commands_execute(handle_set_2, NULL, "help", &cmd_ret));
    TEST_ASSERT_EQUAL(0, cmd_ret);


    /* unregister dynamically registered commands */
    for (size_t i = 0; i < nb_cmds; i++) {
        TEST_ASSERT_EQUAL(ESP_OK, esp_commands_unregister_cmd(cmd_list[i]));
    }

    esp_commands_destroy_cmd_set(&handle_set_1);
    esp_commands_destroy_cmd_set(&handle_set_2);
}

TEST_CASE("test static and dynamic command sets", "[esp_commands]")
{
    test_setup();

    // --- dynamic commands ---
    const char *dyn_cmd_list[] = {"cmd_1", "cmd_2", "cmd_3", "cmd_4", "cmd_5", "cmd_6", "cmd_7", "cmd_8"};
    const size_t nb_dyn_cmds = sizeof(dyn_cmd_list) / sizeof(dyn_cmd_list[0]);

    for (size_t i = 0; i < nb_dyn_cmds; i++) {
        esp_command_t cmd = {
            .name = dyn_cmd_list[i],
            .group = (i % 2 == 0) ? "group_a" : "group_b",
            .help = "dummy help",
            .func = dummy_cmd_func,
            .func_ctx = NULL,
            .hint_cb = NULL,
            .glossary_cb = NULL
        };
        TEST_ASSERT_EQUAL(ESP_OK, esp_commands_register_cmd(&cmd));
    }

    // --- create static command sets (already registered statically) ---
    const char *static_groups[] = {"group_1", "group_3"};
    esp_command_set_handle_t handle_static_set = ESP_COMMANDS_CREATE_CMD_SET(static_groups, FIELD_ACCESSOR(group));
    TEST_ASSERT_NOT_NULL(handle_static_set);

    // --- create dynamic command sets ---
    const char *dyn_groups[] = {"group_a"};
    esp_command_set_handle_t handle_dynamic_set = ESP_COMMANDS_CREATE_CMD_SET(dyn_groups, FIELD_ACCESSOR(group));
    TEST_ASSERT_NOT_NULL(handle_dynamic_set);

    // --- combine static and dynamic sets ---
    esp_command_set_handle_t handle_combined_set = esp_commands_concat_cmd_set(handle_static_set, handle_dynamic_set);
    TEST_ASSERT_NOT_NULL(handle_combined_set);

    // --- run tests for combined set ---
    const char *all_cmds[] = {"cmd_a", "cmd_b", "cmd_c", "cmd_d", "cmd_e", "cmd_f", "cmd_g", "cmd_h",
                              "cmd_1", "cmd_2", "cmd_3", "cmd_4", "cmd_5", "cmd_6", "cmd_7", "cmd_8"
                             };
    int expected_ret[] = {0, 0, -1, -1, 0, 0, -1, -1,
                          0, -1, 0, -1, 0, -1, 0, -1
                         };

    run_cmd_test(handle_combined_set, all_cmds, expected_ret, 16);

    /* test help command with set of dynamic commands */
    int cmd_ret;
    TEST_ASSERT_EQUAL(ESP_OK, esp_commands_execute(handle_combined_set, NULL, "help", &cmd_ret));
    TEST_ASSERT_EQUAL(0, cmd_ret);

    // --- cleanup ---
    esp_commands_destroy_cmd_set(&handle_combined_set);

    for (size_t i = 0; i < nb_dyn_cmds; i++) {
        TEST_ASSERT_EQUAL(ESP_OK, esp_commands_unregister_cmd(dyn_cmd_list[i]));
    }
}

static size_t completion_nb_of_calls = 0;

static void test_completion_cb(void *cb_ctx, const char *completed_cmd_name)
{
    completion_nb_of_calls++;
}

TEST_CASE("test completion callback", "[esp_commands]")
{
    test_setup();

    /* create sets by group */
    const char *set_a[] = {"group_1", "group_3"};
    esp_command_set_handle_t handle_set_a = ESP_COMMANDS_CREATE_CMD_SET(set_a, FIELD_ACCESSOR(group));
    TEST_ASSERT_NOT_NULL(handle_set_a);

    /* register a command dynamically and add it to the set */
    esp_command_t cmd = {
        .name = "dyn_cmd",
        .group = "dyn_cmd_group",
        .help = "dummy help",
        .func = dummy_cmd_func,
        .func_ctx = NULL,
        .hint_cb = NULL,
        .glossary_cb = NULL
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_commands_register_cmd(&cmd));

    const char *set_b[] = {"dyn_cmd"};
    esp_command_set_handle_t handle_set_b = ESP_COMMANDS_CREATE_CMD_SET(set_b, FIELD_ACCESSOR(name));
    TEST_ASSERT_NOT_NULL(handle_set_b);
    esp_command_set_handle_t handle_concat_set = esp_commands_concat_cmd_set(handle_set_a, handle_set_b);
    TEST_ASSERT_NOT_NULL(handle_concat_set);

    esp_commands_get_completion(NULL, "a", NULL, test_completion_cb);
    TEST_ASSERT_EQUAL(0, completion_nb_of_calls);

    esp_commands_get_completion(handle_concat_set, "cmd_", NULL, test_completion_cb);
    TEST_ASSERT_EQUAL(4, completion_nb_of_calls);

    /* reset the cb counter */
    completion_nb_of_calls = 0;

    esp_commands_get_completion(NULL, "cmd_", NULL, test_completion_cb);
    TEST_ASSERT_EQUAL(8, completion_nb_of_calls);

    /* reset the cb counter */
    completion_nb_of_calls = 0;

    esp_commands_get_completion(NULL, "dyn", NULL, test_completion_cb);
    TEST_ASSERT_EQUAL(1, completion_nb_of_calls);

    /* reset the cb counter */
    completion_nb_of_calls = 0;

    esp_commands_get_completion(handle_concat_set, "dyn", NULL, test_completion_cb);
    TEST_ASSERT_EQUAL(1, completion_nb_of_calls);

    /* reset the cb counter */
    completion_nb_of_calls = 0;

    esp_commands_destroy_cmd_set(&handle_concat_set);
    TEST_ASSERT_NULL(handle_concat_set);

    esp_commands_unregister_cmd("dyn_cmd");
}

typedef struct hint_cb_ctx {
    const char *message;
} hint_cb_ctx_t;

static const char *test_hint_cb(void *context)
{
    hint_cb_ctx_t *ctx = (hint_cb_ctx_t *)context;
    return ctx->message;
}

static const char *test_glossary_cb(void *context)
{
    hint_cb_ctx_t *ctx = (hint_cb_ctx_t *)context;
    return ctx->message;
}

TEST_CASE("test hint and glossary callbacks", "[esp_commands]")
{
    test_setup();

    hint_cb_ctx_t ctx_a = { .message = "msg_a" };
    hint_cb_ctx_t ctx_b = { .message = "msg_b" };

    esp_command_t cmd_a = {
        .name = "dyn_cmd_a",
        .group = "dyn_cmd_group",
        .help = "dummy help",
        .func = dummy_cmd_func,
        .func_ctx = &ctx_a,
        .hint_cb = test_hint_cb,
        .glossary_cb = test_glossary_cb
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_commands_register_cmd(&cmd_a));

    esp_command_t cmd_b = {
        .name = "dyn_cmd_b",
        .group = "dyn_cmd_group",
        .help = "dummy help",
        .func = dummy_cmd_func,
        .func_ctx = &ctx_b,
        .hint_cb = test_hint_cb,
        .glossary_cb = test_glossary_cb
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_commands_register_cmd(&cmd_b));

    bool bold = true;
    int color = 0;
    const char *dyn_cmd_a_msg_hint = esp_commands_get_hint(NULL, "dyn_cmd_a", &color, &bold);
    TEST_ASSERT_EQUAL(0, strcmp(dyn_cmd_a_msg_hint, ctx_a.message));
    TEST_ASSERT_EQUAL(false, bold); /* bold set a false by default in the component config */
    TEST_ASSERT_EQUAL(39, color); /* color set to 39 by default in the component config */

    const char *dyn_cmd_b_msg_hint = esp_commands_get_hint(NULL, "dyn_cmd_b", &color, &bold);
    TEST_ASSERT_EQUAL(0, strcmp(dyn_cmd_b_msg_hint, ctx_b.message));

    const char *dyn_cmd_a_msg_glossary = esp_commands_get_glossary(NULL, "dyn_cmd_a");
    TEST_ASSERT_EQUAL(0, strcmp(dyn_cmd_a_msg_glossary, ctx_a.message));

    const char *dyn_cmd_b_msg_glossary = esp_commands_get_glossary(NULL, "dyn_cmd_b");
    TEST_ASSERT_EQUAL(0, strcmp(dyn_cmd_b_msg_glossary, ctx_b.message));

    /* create a set with only dyn_cmd_a and check that the hint cb is called for
     * dyn_cmd_a but not for dyn_cmd_b */
    const char *set[] = {"dyn_cmd_a"};
    esp_command_set_handle_t handle_set = ESP_COMMANDS_CREATE_CMD_SET(set, FIELD_ACCESSOR(name));
    TEST_ASSERT_NOT_NULL(handle_set);

    const char *dyn_cmd_a_msg_hint_bis = esp_commands_get_hint(handle_set, "dyn_cmd_a", &color, &bold);
    TEST_ASSERT_EQUAL(0, strcmp(dyn_cmd_a_msg_hint_bis, ctx_a.message));

    const char *dyn_cmd_b_msg_hint_bis = esp_commands_get_hint(handle_set, "dyn_cmd_b", &color, &bold);
    TEST_ASSERT_NULL(dyn_cmd_b_msg_hint_bis);

    const char *dyn_cmd_a_msg_glossary_bis = esp_commands_get_glossary(handle_set, "dyn_cmd_a");
    TEST_ASSERT_EQUAL(0, strcmp(dyn_cmd_a_msg_glossary_bis, ctx_a.message));

    const char *dyn_cmd_b_msg_glossary_bis = esp_commands_get_glossary(handle_set, "dyn_cmd_b");
    TEST_ASSERT_NULL(dyn_cmd_b_msg_glossary_bis);

    TEST_ASSERT_EQUAL(ESP_OK, esp_commands_unregister_cmd("dyn_cmd_a"));
    TEST_ASSERT_EQUAL(ESP_OK, esp_commands_unregister_cmd("dyn_cmd_b"));

    esp_commands_destroy_cmd_set(&handle_set);
}
