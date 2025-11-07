/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdio.h>
#include "unity.h"
#include "esp_linenoise.h"
#include "esp_err.h"

static esp_linenoise_handle_t get_linenoise_instance_default_config(void)
{
    esp_linenoise_config_t config;
    esp_linenoise_get_instance_config_default(&config);
    esp_linenoise_handle_t h;
    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_create_instance(&config, &h));
    TEST_ASSERT_NOT_NULL(h);
    return h;
}

TEST_CASE("set and get multi-line mode", "[esp_linenoise]")
{
    esp_linenoise_handle_t h = get_linenoise_instance_default_config();

    bool is_multi_line = false;

    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_set_multi_line(h, true));
    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_is_multi_line(h, &is_multi_line));
    TEST_ASSERT_TRUE(is_multi_line);

    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_set_multi_line(h, false));
    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_is_multi_line(h, &is_multi_line));
    TEST_ASSERT_FALSE(is_multi_line);

    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_delete_instance(h));
}

TEST_CASE("set and get dumb mode", "[esp_linenoise]")
{
    esp_linenoise_handle_t h = get_linenoise_instance_default_config();

    bool is_dumb_mode = false;

    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_set_dumb_mode(h, true));
    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_is_dumb_mode(h, &is_dumb_mode));
    TEST_ASSERT_TRUE(is_dumb_mode);

    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_set_dumb_mode(h, false));
    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_is_dumb_mode(h, &is_dumb_mode));
    TEST_ASSERT_FALSE(is_dumb_mode);


    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_delete_instance(h));
}

TEST_CASE("set and get empty line flag", "[esp_linenoise]")
{
    esp_linenoise_handle_t h = get_linenoise_instance_default_config();

    bool is_empty_line = false;

    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_set_empty_line(h, true));
    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_is_empty_line(h, &is_empty_line));
    TEST_ASSERT_TRUE(is_empty_line);

    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_set_empty_line(h, false));
    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_is_empty_line(h, &is_empty_line));
    TEST_ASSERT_FALSE(is_empty_line);

    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_delete_instance(h));
}

TEST_CASE("default max line length and max history length", "[esp_linenoise]")
{
    esp_linenoise_config_t config;
    esp_linenoise_get_instance_config_default(&config);
    TEST_ASSERT_GREATER_THAN(0, config.max_cmd_line_length);
    TEST_ASSERT_GREATER_THAN(0, config.history_max_length);
}

TEST_CASE("set and get max command line length", "[esp_linenoise]")
{
    esp_linenoise_handle_t h = get_linenoise_instance_default_config();

    size_t max_cmd_line_len = 0;

    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_set_max_cmd_line_length(h, 1024));
    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_get_max_cmd_line_length(h, &max_cmd_line_len));
    TEST_ASSERT_EQUAL(1024, max_cmd_line_len);

    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_delete_instance(h));
}

TEST_CASE("add and free history", "[esp_linenoise]")
{
    esp_linenoise_handle_t h = get_linenoise_instance_default_config();

    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_history_add(h, "entry1"));
    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_history_set_max_len(h, 5));
    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_history_add(h, "entry2"));
    esp_linenoise_history_free(h);

    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_delete_instance(h));
}

TEST_CASE("save and load history to file", "[esp_linenoise]")
{
    esp_linenoise_handle_t h = get_linenoise_instance_default_config();

    const char *filename = "/tmp/test_esp_linenoise_history.txt";

    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_history_add(h, "one"));
    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_history_add(h, "two"));

    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_history_save(h, filename));
    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_history_free(h));

    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_history_load(h, filename));

    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_delete_instance(h));
}

TEST_CASE("get out_fd and in_fd", "[esp_linenoise]")
{
    const int test_out_fd = 5;
    const int test_in_fd = 6;
    esp_linenoise_config_t config;
    esp_linenoise_get_instance_config_default(&config);
    config.out_fd = test_out_fd;
    config.in_fd = test_in_fd;

    esp_linenoise_handle_t h;
    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_create_instance(&config, &h));
    TEST_ASSERT_NOT_NULL(h);

    int in_fd = -1;
    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_get_in_fd(h, &in_fd));
    TEST_ASSERT_EQUAL(test_in_fd, in_fd);
    int out_fd = -1;
    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_get_out_fd(h, &out_fd));
    TEST_ASSERT_EQUAL(test_out_fd, out_fd);

    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_delete_instance(h));
}

static ssize_t test_read(int fd, void *buf, size_t count)
{
    (void)fd;
    (void)buf;
    (void)count;
    return -1;
}

static ssize_t test_write(int fd, const void *buf, size_t count)
{
    (void)fd;
    (void)buf;
    (void)count;
    return -1;
}


TEST_CASE("get read_func and write_func", "[esp_linenoise]")
{
    esp_linenoise_config_t config;
    esp_linenoise_get_instance_config_default(&config);
    config.read_bytes_cb = test_read;
    config.write_bytes_cb = test_write;

    esp_linenoise_handle_t h;
    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_create_instance(&config, &h));
    TEST_ASSERT_NOT_NULL(h);

    esp_linenoise_read_bytes_t read_ret;
    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_get_read(h, &read_ret));
    TEST_ASSERT_EQUAL(test_read, read_ret);
    esp_linenoise_write_bytes_t write_ret;
    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_get_write(h, &write_ret));
    TEST_ASSERT_EQUAL(test_write, write_ret);

    TEST_ASSERT_EQUAL(ESP_OK, esp_linenoise_delete_instance(h));
}
