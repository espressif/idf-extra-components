#include <stdio.h>
#include <string.h>
#include "esp_cli_commands.h"
#include "argtable3/argtable3.h"
#include "command_utils.h"

static struct {
    struct arg_str *operator;
    struct arg_int *operand_a;
    struct arg_int *operand_b;
    struct arg_end *end;
} math_op_args;

static void math_op_args_init(void)
{
    math_op_args.operator  = arg_str1("o", "operator", "<op>", "operation to perform (add, sub, mul, div)");
    math_op_args.operand_a = arg_int1("a", "operand-a", "<a>", "left side operand");
    math_op_args.operand_b = arg_int1("b", "operand-b", "<b>", "right side operand");
    math_op_args.end       = arg_end(3);
}

// Handler function signature must match: int (*)(void *, esp_cli_commands_exec_arg_t *, int, char **)
static int math_op_cmd_handler(void *ctx, esp_cli_commands_exec_arg_t *cmd_arg, int argc, char **argv)
{
    (void)ctx;
    if (argc != 4) {
        int color = 0;
        bool bold = false;
        const char *hint = esp_cli_commands_get_hint(NULL, "math_op", &color, &bold);
        WRITE_FN(cmd_arg->write_func, cmd_arg->out_fd, "Usage: math_op %s\n", hint ? hint : "<add|sub|mul|div> <a> <b>");
        return -1;
    }
    const char *op = argv[1];
    int a = atoi(argv[2]);
    int b = atoi(argv[3]);
    int result = 0;
    WRITE_FN(cmd_arg->write_func, cmd_arg->out_fd, "Performing operation: %s %d %d\n", op, a, b);
    if (strcmp(op, "add") == 0) {
        result = a + b;
    } else if (strcmp(op, "sub") == 0) {
        result = a - b;
    } else if (strcmp(op, "mul") == 0) {
        result = a * b;
    } else if (strcmp(op, "div") == 0) {
        if (b == 0) {
            WRITE_FN(cmd_arg->write_func, cmd_arg->out_fd, "Error: Division by zero\n");
            return -2;
        }
        result = a / b;
    } else {
        WRITE_FN(cmd_arg->write_func, cmd_arg->out_fd, "Unknown operation: %s\n", op);
        return -3;
    }
    WRITE_FN(cmd_arg->write_func, cmd_arg->out_fd, "Result: %d\n", result);
    return 0;
}

// Hint callback signature: const char *(*)(void *)
static const char *math_op_cmd_hint_cb(void *ctx)
{
    (void)ctx;

    arg_dstr_t ds = arg_dstr_create();
    arg_print_syntax_ds(ds, (void *)&math_op_args, NULL);
    const char *hint_str = strdup(arg_dstr_cstr(ds));
    arg_dstr_destroy(ds);

    return hint_str;
}

// Glossary callback signature: const char *(*)(void *)
static const char *math_op_cmd_glossary_cb(void *ctx)
{
    (void)ctx;

    arg_dstr_t ds = arg_dstr_create();
    arg_print_glossary_ds(ds, (void *)&math_op_args, NULL);
    const char *glossary_str = strdup(arg_dstr_cstr(ds));
    arg_dstr_destroy(ds);

    return glossary_str;
}

// Static registration of the math_op command with all fields
ESP_CLI_COMMAND_REGISTER(
    math_op,                // Command name
    example,                // Command group
    "Performs math operation on two integers", // Help string
    math_op_cmd_handler,    // Handler function
    NULL,                   // Context pointer
    math_op_cmd_hint_cb,    // Hint callback
    math_op_cmd_glossary_cb // Glossary callback
);

void app_main(void)
{
    printf("esp_cli_commands integration_with_argtable example started.\n");

    esp_cli_commands_exec_arg_t cmd_args = {
        .out_fd = STDOUT_FILENO,
        .write_func = write,
        .dynamic_ctx = NULL
    };

    math_op_args_init();

    // Get hint for the 'math_op' command
    int color = 0;
    bool bold = false;
    const char *hint = esp_cli_commands_get_hint(NULL, "math_op", &color, &bold);
    printf("Hint for 'math_op': %s (color: %d, bold: %d)\n", hint ? hint : "none", color, bold);

    // Get glossary for the 'math_op' command
    const char *glossary = esp_cli_commands_get_glossary(NULL, "math_op");
    printf("Glossary for 'math_op': %s\n", glossary ? glossary : "none");

    int ret = -1;
    esp_err_t err = esp_cli_commands_execute("math_op add 3 5", &ret, NULL, &cmd_args);
    if (err == ESP_OK) {
        printf("'math_op' command executed successfully, return value: %d\n", ret);
    } else {
        printf("Failed to execute 'math_op' command, error: %d\n", err);
    }

    printf("end of example\n");
}
