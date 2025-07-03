/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdio.h>
#include "unity.h"
#include "linenoise/linenoise.h"

static linenoise_handle_t get_linenoise_default_instance_param(void)
{
    linenoise_instance_param_t param = linenoise_get_instance_param_default();
    linenoise_handle_t h = linenoise_create_instance(&param);
    TEST_ASSERT_NOT_NULL(h);
    return h;
}

TEST_CASE("set and get multi-line mode", "[linenoise]")
{
    linenoise_handle_t h = get_linenoise_default_instance_param();

    linenoise_set_multi_line(h, true);
    TEST_ASSERT_TRUE(linenoise_is_multi_line(h));

    linenoise_set_multi_line(h, false);
    TEST_ASSERT_FALSE(linenoise_is_multi_line(h));

    linenoise_delete_instance(h);
}

TEST_CASE("set and get dumb mode", "[linenoise]")
{
    linenoise_handle_t h = get_linenoise_default_instance_param();

    linenoise_set_dumb_mode(h, true);
    TEST_ASSERT_TRUE(linenoise_is_dumb_mode(h));

    linenoise_set_dumb_mode(h, false);
    TEST_ASSERT_FALSE(linenoise_is_dumb_mode(h));

    linenoise_delete_instance(h);
}

TEST_CASE("set and get empty line flag", "[linenoise]")
{
    linenoise_handle_t h = get_linenoise_default_instance_param();

    linenoise_set_empty_line(h, true);
    TEST_ASSERT_TRUE(linenoise_is_empty_line(h));

    linenoise_set_empty_line(h, false);
    TEST_ASSERT_FALSE(linenoise_is_empty_line(h));

    linenoise_delete_instance(h);
}

TEST_CASE("default max line length and max history length", "[linenoise]")
{
    linenoise_instance_param_t param = linenoise_get_instance_param_default();
    TEST_ASSERT_GREATER_THAN(0, param.max_cmd_line_length);
    TEST_ASSERT_GREATER_THAN(0, param.history_max_length);
}

TEST_CASE("set and get max command line length", "[linenoise]")
{
    linenoise_handle_t h = get_linenoise_default_instance_param();

    linenoise_set_max_cmd_line_length(h, 1024);
    TEST_ASSERT_EQUAL(1024, linenoise_get_max_cmd_line_length(h));

    linenoise_delete_instance(h);
}

TEST_CASE("add and free history", "[linenoise]")
{
    linenoise_handle_t h = get_linenoise_default_instance_param();

    TEST_ASSERT_EQUAL(true, linenoise_history_add(h, "entry1"));
    TEST_ASSERT_EQUAL(true, linenoise_history_set_max_len(h, 5));
    TEST_ASSERT_EQUAL(true, linenoise_history_add(h, "entry2"));
    linenoise_history_free(h);

    linenoise_delete_instance(h);
}

TEST_CASE("save and load history to file", "[linenoise]")
{
    linenoise_handle_t h = get_linenoise_default_instance_param();

    const char *filename = "/tmp/test_linenoise_history.txt";

    linenoise_history_add(h, "one");
    linenoise_history_add(h, "two");

    TEST_ASSERT_EQUAL(0, linenoise_history_save(h, filename));
    linenoise_history_free(h);

    TEST_ASSERT_EQUAL(0, linenoise_history_load(h, filename));
    linenoise_delete_instance(h);
}
