/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include "unity.h"
#include "test_utils.h"

// list of commands that the linenoise tests need
// to intercept and potentially answer for linenoise to
// behave as expected
const command_t commands[] = {
    {"\x1b[5n", "\x1b[0n"}, // probe for escape sequence support
    {"\x1b[H\x1b[2J", "screen cleared"}, // clear screen request, the response will be analyzed in the concerned test
    {"\x1b[6n", "\x1b[10;50R"}, // request for rows, cols
    {"\x1b[999C", NULL}, // move the cursor right, no response needed
};

const size_t commands_count = sizeof(commands) / sizeof(command_t);

void test_send_characters(int socket_fd, const char *msg)
{
    // wait to simulate that the user is doing the input
    // and prevent linenoise to detect the incoming character(s)
    // as pasted
    wait_ms(50);

    const size_t msg_len = strlen(msg);
    const int nwrite = write(socket_fd, msg, msg_len);
    TEST_ASSERT_EQUAL(msg_len, nwrite);
}
