#include "unity.h"
#include "unity_test_runner.h"
#include "json_generator.h"
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>
#include <inttypes.h>

void setUp(void) {}

void tearDown(void) {}

static void json_generator_setup(json_gen_str_t *p_jstr, char *buf, int buf_size)
{
    json_gen_str_start(p_jstr, buf, buf_size, NULL, NULL);
    json_gen_start_object(p_jstr);
}

static void json_generator_teardown(json_gen_str_t *p_jstr)
{
    json_gen_end_object(p_jstr);
    json_gen_str_end(p_jstr);
}

TEST_CASE("int64_t test", "[json_generator]")
{
#if CONFIG_LIBC_NEWLIB_NANO_FORMAT
    printf("Using modulo 10 parsing to format the 64-bit integer. Set CONFIG_LIBC_NEWLIB_NANO_FORMAT=n to use snprintf.\n");
#else /* !CONFIG_LIBC_NEWLIB_NANO_FORMAT */
    printf("Using snprintf to format the 64-bit integer. Set CONFIG_LIBC_NEWLIB_NANO_FORMAT=y to use modulo 10 parsing.\n");
#endif /* CONFIG_LIBC_NEWLIB_NANO_FORMAT */

    char buf[1024];
    json_gen_str_t jstr;
    json_generator_setup(&jstr, buf, sizeof(buf));

    /* Try to add a int64_t value, positive and negative */
    const int64_t pos_val = rand();
    const int64_t neg_val = -rand();
    json_gen_obj_set_int64(&jstr, "pos", pos_val);
    json_gen_obj_set_int64(&jstr, "neg", neg_val);

    /* Try boundary values */
    json_gen_obj_set_int64(&jstr, "zero", 0);
    json_gen_obj_set_int64(&jstr, "max", INT64_MAX);
    json_gen_obj_set_int64(&jstr, "min", INT64_MIN);

    json_generator_teardown(&jstr);
    char expected_str[1024];
    snprintf(expected_str, sizeof(expected_str), "{\"pos\":%" PRId64 ",\"neg\":%" PRId64 ",\"zero\":0,\"max\":%" PRId64 ",\"min\":%" PRId64 "}", pos_val, neg_val, INT64_MAX, INT64_MIN);
    printf("Expected string: %s\n", expected_str);
    printf("Actual string: %s\n", buf);
    TEST_ASSERT_EQUAL_STRING(expected_str, buf);
}

void app_main(void)
{
    /* Random seed for the test */
    srand(time(NULL));

    printf("Running json_generator tests\n");
    unity_run_menu();
}