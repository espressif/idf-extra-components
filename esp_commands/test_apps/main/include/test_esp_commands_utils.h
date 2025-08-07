/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define NB_OF_REGISTERED_CMD 8

#define GET_NAME(NAME, SUFFIX) NAME##SUFFIX
#define GET_STR(STR) #STR

#define CREATE_CMD_FUNC(NAME) \
    static int GET_NAME(NAME, _func)(void *ctx, int argc, char **argv) { \
        printf(GET_STR(NAME) GET_STR(_func)); \
        printf("\n"); \
        return 0; \
    }

#define CREATE_FUNC(NAME, SUFFIX) \
    static const char *GET_NAME(NAME, SUFFIX)(void *context) { \
        return #NAME #SUFFIX; \
    }

/* static command functions*/
CREATE_CMD_FUNC(cmd_a)
CREATE_CMD_FUNC(cmd_b)
CREATE_CMD_FUNC(cmd_c)
CREATE_CMD_FUNC(cmd_d)
CREATE_CMD_FUNC(cmd_e)
CREATE_CMD_FUNC(cmd_f)
CREATE_CMD_FUNC(cmd_g)
CREATE_CMD_FUNC(cmd_h)

/* static hint functions*/
CREATE_FUNC(cmd_a, _hint)
CREATE_FUNC(cmd_b, _hint)
CREATE_FUNC(cmd_c, _hint)
CREATE_FUNC(cmd_d, _hint)
CREATE_FUNC(cmd_e, _hint)
CREATE_FUNC(cmd_f, _hint)
CREATE_FUNC(cmd_g, _hint)
CREATE_FUNC(cmd_h, _hint)

/* static glossary functions*/
CREATE_FUNC(cmd_a, _glossary)
CREATE_FUNC(cmd_b, _glossary)
CREATE_FUNC(cmd_c, _glossary)
CREATE_FUNC(cmd_d, _glossary)
CREATE_FUNC(cmd_e, _glossary)
CREATE_FUNC(cmd_f, _glossary)
CREATE_FUNC(cmd_g, _glossary)
CREATE_FUNC(cmd_h, _glossary)

/* command registration */
ESP_COMMAND_REGISTER(cmd_a, group_1, GET_STR(cmd_a_help), cmd_a_func, NULL, cmd_a_hint, cmd_a_glossary);
ESP_COMMAND_REGISTER(cmd_b, group_1, GET_STR(cmd_b_help), cmd_b_func, NULL, cmd_b_hint, cmd_b_glossary);
ESP_COMMAND_REGISTER(cmd_c, group_2, GET_STR(cmd_c_help), cmd_c_func, NULL, cmd_c_hint, cmd_c_glossary);
ESP_COMMAND_REGISTER(cmd_d, group_2, GET_STR(cmd_d_help), cmd_d_func, NULL, cmd_d_hint, cmd_d_glossary);
ESP_COMMAND_REGISTER(cmd_e, group_3, GET_STR(cmd_e_help), cmd_e_func, NULL, cmd_e_hint, cmd_e_glossary);
ESP_COMMAND_REGISTER(cmd_f, group_3, GET_STR(cmd_f_help), cmd_f_func, NULL, cmd_f_hint, cmd_f_glossary);
ESP_COMMAND_REGISTER(cmd_g, group_4, GET_STR(cmd_g_help), cmd_g_func, NULL, cmd_g_hint, cmd_g_glossary);
ESP_COMMAND_REGISTER(cmd_h, group_4, GET_STR(cmd_h_help), cmd_h_func, NULL, cmd_h_hint, cmd_h_glossary);

#ifdef __cplusplus
}
#endif
