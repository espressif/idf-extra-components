/*
 * SPDX-FileCopyrightText: 2018-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "unity.h"
#include "expat.h"

typedef struct {
    int depth;
    char output[512];
    int output_off;
} user_data_t;

static void insert_space(user_data_t *user_data)
{
    const char align_str[] = "    ";

    TEST_ASSERT(sizeof(user_data->output) > user_data->output_off);
    user_data->output[user_data->output_off++] = '\n';

    for (int i = 0; i < user_data->depth; i++) {
        for (int j = 0; j < strlen(align_str); ++j) {
            TEST_ASSERT(sizeof(user_data->output) > user_data->output_off);
            user_data->output[user_data->output_off++] = align_str[j];
        }
    }
}

static void XMLCALL start_element(void *userData, const XML_Char *name, const XML_Char **atts)
{
    user_data_t *user_data = (user_data_t *) userData;

    insert_space(user_data);

    const int ret = snprintf(user_data->output + user_data->output_off,
                             sizeof(user_data->output) - user_data->output_off,
                             "<%s>", name);
    TEST_ASSERT_EQUAL(strlen(name) + 2, ret); // 2 are the tag characters: "<>"
    user_data->output_off += ret;
    ++user_data->depth;
}

static void XMLCALL end_element(void *userData, const XML_Char *name)
{
    user_data_t *user_data = (user_data_t *) userData;

    --user_data->depth;
    insert_space(user_data);

    int ret = snprintf(user_data->output + user_data->output_off, sizeof(user_data->output) - user_data->output_off,
                       "</%s>", name);
    TEST_ASSERT_EQUAL(strlen(name) + 3, ret); // 3 are the tag characters: "</>"
    user_data->output_off += ret;
}

static void data_handler(void *userData, const XML_Char *s, int len)
{
    user_data_t *user_data = (user_data_t *) userData;

    insert_space(user_data);

    // s is not zero-terminated
    char tmp_str[len + 1];
    snprintf(tmp_str, sizeof(tmp_str), "%.*s", len, s);

    int ret = snprintf(user_data->output + user_data->output_off, sizeof(user_data->output) - user_data->output_off,
                       "%s", tmp_str);
    TEST_ASSERT_EQUAL(strlen(tmp_str), ret);
    user_data->output_off += ret;
}

TEST_CASE("Expat parses XML", "[expat]")
{
    const char test_in[] = "<html><title>Page title</title><body><h>header</h><ol><li>A</li>"\
                           "<li>B</li><li>C</li></ol></body></html>";
    const char test_expected[] =    "\n"\
                                    "<html>\n"\
                                    "    <title>\n"\
                                    "        Page title\n"\
                                    "    </title>\n"\
                                    "    <body>\n"\
                                    "        <h>\n"\
                                    "            header\n"\
                                    "        </h>\n"\
                                    "        <ol>\n"\
                                    "            <li>\n"\
                                    "                A\n"\
                                    "            </li>\n"\
                                    "            <li>\n"\
                                    "                B\n"\
                                    "            </li>\n"\
                                    "            <li>\n"\
                                    "                C\n"\
                                    "            </li>\n"\
                                    "        </ol>\n"\
                                    "    </body>\n"\
                                    "</html>";
    user_data_t user_data = {
        .depth = 0,
        .output = { '\0' },
        .output_off = 0
    };

    XML_Parser parser = XML_ParserCreate(NULL);
    TEST_ASSERT_NOT_NULL(parser);
    XML_SetUserData(parser, &user_data);
    XML_SetElementHandler(parser, start_element, end_element);
    XML_SetCharacterDataHandler(parser, data_handler);

    TEST_ASSERT_NOT_EQUAL(XML_STATUS_ERROR, XML_Parse(parser, test_in, strlen(test_in), 1));
    XML_ParserFree(parser);

    TEST_ASSERT_EQUAL(0, user_data.depth); // all closing tags have been found

    TEST_ASSERT_EQUAL(strlen(test_expected), strlen(user_data.output));
    TEST_ASSERT_EQUAL_STRING(test_expected, user_data.output);
}

/* The library and this test are compiled from one expat.h: if the component
 * failed to pass XML_DTD / XML_GE on to its consumers, the prototypes below
 * would be missing and the build would fail here rather than at runtime. */
TEST_CASE("Expat feature macros reach the consumer", "[expat]")
{
    const XML_Feature *features = XML_GetFeatureList();
    bool dtd_reported = false;
    bool ge_reported = false;

    for (const XML_Feature *f = features; f->feature != XML_FEATURE_END; f++) {
        if (f->feature == XML_FEATURE_DTD) {
            dtd_reported = true;
        }
        if (f->feature == XML_FEATURE_GE) {
            ge_reported = true;
        }
    }

#if defined(XML_DTD)
    TEST_ASSERT_TRUE(dtd_reported);
#else
    TEST_ASSERT_FALSE(dtd_reported);
#endif

#if defined(XML_GE) && XML_GE == 1
    TEST_ASSERT_TRUE(ge_reported);
    /* These four are only declared when the header sees XML_GE == 1. */
    XML_Parser parser = XML_ParserCreate(NULL);
    TEST_ASSERT_NOT_NULL(parser);
    TEST_ASSERT_TRUE(XML_SetBillionLaughsAttackProtectionMaximumAmplification(parser, 20.0f));
    TEST_ASSERT_TRUE(XML_SetBillionLaughsAttackProtectionActivationThreshold(parser, 1024));
    TEST_ASSERT_TRUE(XML_SetAllocTrackerMaximumAmplification(parser, 20.0f));
    TEST_ASSERT_TRUE(XML_SetAllocTrackerActivationThreshold(parser, 1024));
    XML_ParserFree(parser);
#else
    TEST_ASSERT_FALSE(ge_reported);
#endif
}

TEST_CASE("Expat rejects malformed XML", "[expat]")
{
    const char test_in[] = "<html><body></html>";
    XML_Parser parser = XML_ParserCreate(NULL);
    TEST_ASSERT_NOT_NULL(parser);

    TEST_ASSERT_EQUAL(XML_STATUS_ERROR, XML_Parse(parser, test_in, strlen(test_in), 1));
    TEST_ASSERT_EQUAL(XML_ERROR_TAG_MISMATCH, XML_GetErrorCode(parser));
    TEST_ASSERT_NOT_NULL(XML_ErrorString(XML_GetErrorCode(parser)));
    XML_ParserFree(parser);
}
