/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include "esp_heap_caps.h"
#include "esp_commands.h"
#include "esp_commands_internal.h"
#include "esp_dynamic_commands.h"
#include "esp_err.h"

/* Default foreground color */
#define ANSI_COLOR_DEFAULT 39

/* Pointers to the first and last command in the dedicated section.
 * See linker.lf for detailed information about the section */
extern esp_command_t _esp_commands_start;
extern esp_command_t _esp_commands_end;

typedef struct esp_command_sets {
    esp_command_set_t static_set;
    esp_command_set_t dynamic_set;
} esp_command_sets_t;

/** run-time configuration options */
static esp_commands_config_t s_config = {
    .write_func = write,
    .heap_caps_used = MALLOC_CAP_DEFAULT,
    .hint_bold = false,
    .hint_color = ANSI_COLOR_DEFAULT,
    .max_cmdline_args = 32,
    .max_cmdline_length = 256
};

/**
 * @brief go through all commands registered in the
 * memory section starting at _esp_commands_start
 * and ending at _esp_commands_end OR go through all
 * the commands listed in cmd_set if not NULL
 */
#define FOR_EACH_STATIC_COMMAND(cmd_set, cmd)                       \
    for (size_t _i = 0;                                             \
         ((cmd_set) == NULL                                         \
              ? (((cmd) = &_esp_commands_start + _i),               \
                 (&_esp_commands_start + _i) < &_esp_commands_end)  \
              : (((cmd) = (cmd_set)->cmd_ptr_set[_i]),              \
                 _i < (cmd_set)->cmd_set_size));                    \
         ++_i)

/**
 * @brief returns the number of commands registered
 * in the .esp_commands section
 */
#define ESP_COMMANDS_COUNT (size_t)(&_esp_commands_end - &_esp_commands_start)

/**
 * @brief check the location of the pointer to esp_command_t
 *
 * @param cmd the pointer to the command to check
 * @return true if the command was registered statically
 *         false if the command was registered dynamically
 */
static inline __attribute__((always_inline)) bool command_is_static(esp_command_t *cmd)
{
    if (cmd >= &_esp_commands_start && cmd <= &_esp_commands_end) {
        return true;
    }
    return false;
}

typedef bool (*walker_t)(void *walker_ctx, esp_command_t *cmd);
static inline __attribute__((always_inline))
void go_through_commands(esp_command_sets_t *cmd_sets, void *cmd_walker_ctx, walker_t cmd_walker)
{
    if (!cmd_walker) {
        return;
    }

    esp_command_t *cmd = NULL;
    bool continue_walk = false;

    /* cmd_sets is composed of 2 sets (static and dynamic).
     * - If cmd_sets is NULL, go through all the statically AND dynamically registered commands.
     * - If cmd_sets is not NULL and either the static or the dynamic set is empty, then the macros
     * FOR_EACH_XX_COMMAND will not go through the whole list of static (resp. dynamic) commands but
     * through the empty set, so no command will be walked.
     */

    esp_command_set_t *static_set = cmd_sets ? &cmd_sets->static_set : NULL;
    /* it is possible that the set is empty, in which case set static_set to NULL
     * to prevent the for loop to try to access a list of commands pointer set to NULL */
    if (static_set && !static_set->cmd_ptr_set) {
        static_set = NULL;
    }
    FOR_EACH_STATIC_COMMAND(static_set, cmd) {
        continue_walk = cmd_walker(cmd_walker_ctx, cmd);
        if (!continue_walk) {
            return;
        }
    }

    esp_command_set_t *dynamic_set = cmd_sets ? &cmd_sets->dynamic_set : NULL;
    /* it is possible that the set is empty, in which case set dynamic_set to NULL
     * to prevent the for loop to try to access a list of commands pointer set to NULL */
    if (dynamic_set && !dynamic_set->cmd_ptr_set) {
        dynamic_set = NULL;
    }
    esp_dynamic_commands_lock();
    FOR_EACH_DYNAMIC_COMMAND(dynamic_set, cmd) {
        continue_walk = cmd_walker(cmd_walker_ctx, cmd);
        if (!continue_walk) {
            esp_dynamic_commands_unlock();
            return;
        }
    }
    esp_dynamic_commands_unlock();
}

typedef struct find_cmd_ctx {
    const char *name; /*!< the name to check commands against */
    esp_command_t *cmd; /*!< the command matching the name */
} find_cmd_ctx_t;

static inline __attribute__((always_inline))
bool compare_command_name(void *ctx, esp_command_t *cmd)
{
    /* called by esp_commands_find_command through go_through_commands,
     * ctx cannot be NULL */
    find_cmd_ctx_t *cmd_ctx = (find_cmd_ctx_t *)ctx;

    /* called by go_through_commands, thus cmd cannot be NULL */
    if (strcmp(cmd->name, cmd_ctx->name) == 0) {
        /* command found, store it in the ctx so esp_commands_find_command
         * can process it. Notify go_through_commands to stop the walk by
         * returning false */
        cmd_ctx->cmd = cmd;
        return false;
    }

    /* command not matching with the name from the ctx, continue the walk */
    return true;
}

void *esp_commands_malloc(const size_t malloc_size)
{
    return heap_caps_malloc(malloc_size, s_config.heap_caps_used);
}

esp_err_t esp_commands_update_config(const esp_commands_config_t *config)
{
    if (!config ||
            (config->max_cmdline_args == 0) ||
            (config->max_cmdline_length == 0)) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&s_config, config, sizeof(s_config));

    /* if no write function was passed in parameter,
     * default it to the posix write */
    if (s_config.write_func == NULL) {
        s_config.write_func = write;
    }

    /* if the heap_caps_used field is set to 0, set
     * it to MALLOC_CAP_DEFAULT */
    if (s_config.heap_caps_used == 0) {
        s_config.heap_caps_used = MALLOC_CAP_DEFAULT;
    }

    return ESP_OK;
}

esp_err_t esp_commands_register_cmd(esp_command_t *cmd)
{
    if (cmd == NULL ||
            (cmd->name == NULL || strchr(cmd->name, ' ') != NULL) ||
            (cmd->func == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }

    /* try to find the command in the static and dynamic lists.
     * if the dynamic list is empty, the mutex locking will fail
     * in esp_commands_find_command and the function will return after
     * checking the static list only. */
    esp_command_t *list_item_cmd = esp_commands_find_command((esp_command_sets_t *)NULL, cmd->name);
    esp_err_t ret_val = ESP_FAIL;
    if (!list_item_cmd) {
        /* command with given name not found, it is a new command, we can allocate
         * the list item and the command itself */
        ret_val = esp_dynamic_commands_add(cmd);
    } else if (command_is_static(list_item_cmd)) {
        /* a command with matching name is found in the list of commands
         * that were registered at runtime, in which case it cannot be
         * replaced with the new command */
        ret_val = ESP_FAIL;
    } else {
        /* an item with matching name was found in the list of dynamically
         * registered commands. Replace the command on spot with the new esp_command_t. */
        ret_val = esp_dynamic_commands_replace(cmd);
    }

    return ret_val;
}

esp_err_t esp_commands_unregister_cmd(const char *cmd_name)
{
    /* only items dynamically registered can be unregistered.
     * try to remove the item with the given name from the list
     * of dynamically registered commands */
    esp_command_t *cmd = esp_commands_find_command((esp_command_sets_t *)NULL, cmd_name);
    if (!cmd) {
        return ESP_ERR_NOT_FOUND;
    } else if (command_is_static(cmd)) {
        return ESP_ERR_INVALID_ARG;
    } else {
        return esp_dynamic_commands_remove(cmd);
    }
}

esp_err_t esp_commands_execute(esp_command_set_handle_t cmd_set, const int cmd_fd, const char *cmdline, int *cmd_ret)
{
    char **argv = (char **) calloc(s_config.max_cmdline_args, sizeof(char *));
    if (argv == NULL) {
        return ESP_ERR_NO_MEM;
    }
    char *tmp_line_buf = (char *) calloc(1, s_config.max_cmdline_length);
    if (!tmp_line_buf) {
        free(argv);
        return ESP_ERR_NO_MEM;
    }

    strlcpy(tmp_line_buf, cmdline, s_config.max_cmdline_length);

    size_t argc = esp_commands_split_argv(tmp_line_buf, argv, s_config.max_cmdline_args);
    if (argc == 0) {
        free(argv);
        free(tmp_line_buf);
        return ESP_ERR_INVALID_ARG;
    }

    /* help should always be executed, if cmd_sets is set or not */
    const esp_command_t *cmd = NULL;
    bool is_cmd_help = false;
    if (strcmp("help", argv[0]) == 0) {
        /* find the help command in the list in .esp_commands section */
        cmd = esp_commands_find_command((esp_command_sets_t *)NULL, "help");
        is_cmd_help = true;
    } else {
        cmd = esp_commands_find_command(cmd_set, argv[0]);
    }

    if (cmd == NULL) {
        free(argv);
        free(tmp_line_buf);
        return ESP_ERR_NOT_FOUND;
    }

    const int fd_out = cmd_fd == -1 ? STDOUT_FILENO : cmd_fd;
    if (cmd->func) {
        if (is_cmd_help) {
            // executing help command, pass the cmd_set as context
            *cmd_ret = (*cmd->func)(cmd_set, fd_out, argc, argv);
        } else {
            *cmd_ret = (*cmd->func)(cmd->func_ctx, fd_out, argc, argv);
        }
    }
    free(argv);
    free(tmp_line_buf);
    return ESP_OK;
}

esp_command_t *esp_commands_find_command(esp_command_set_handle_t cmd_set, const char *name)
{
    /* no need to check that cmd_set is NULL, if it is, then FOR_EACH_XX_COMMAND
     * will go through all registered commands */
    if (!name) {
        return NULL;
    }

    find_cmd_ctx_t ctx = { .cmd = NULL, .name = name };
    go_through_commands(cmd_set, &ctx, compare_command_name);

    /* if command was found during the walk, cmd field will be populated with
     * the command matching the name given in parameter, otherwise it will still
     * be NULL (value set as default value above) */
    return ctx.cmd;
}
typedef struct create_cmd_set_ctx {
    esp_commands_get_field_t get_field;
    const char *cmd_set_name;
    esp_command_t **static_cmd_ptrs;
    size_t static_cmd_count;
    esp_command_t **dynamic_cmd_ptrs;
    size_t dynamic_cmd_count;
} create_cmd_set_ctx_t;

static inline __attribute__((always_inline))
bool fill_temp_set_info(void *caller_ctx, esp_command_t *cmd)
{
    /* called by esp_commands_find_command through go_through_commands,
     * ctx cannot be NULL */
    create_cmd_set_ctx_t *ctx = (create_cmd_set_ctx_t *)caller_ctx;

    /* called by go_through_commands, thus cmd cannot be NULL */
    if (strcmp(ctx->get_field(cmd), ctx->cmd_set_name) == 0) {
        // it's a match, add the pointer to command to the cmd ptr set
        if (command_is_static(cmd)) {
            ctx->static_cmd_ptrs[ctx->static_cmd_count] = cmd;
            ctx->static_cmd_count++;
        } else {
            ctx->dynamic_cmd_ptrs[ctx->dynamic_cmd_count] = cmd;
            ctx->dynamic_cmd_count++;
        }
    }

    /* command not matching with the name from the ctx, continue the walk */
    return true;
}

static inline __attribute__((always_inline))
esp_err_t update_cmd_set_with_temp_info(esp_command_set_t *cmd_set, size_t cmd_count, esp_command_t **cmd_ptrs)
{
    if (cmd_count == 0) {
        cmd_set->cmd_ptr_set = NULL;
        cmd_set->cmd_set_size = 0;
    } else {
        const size_t alloc_cmd_ptrs_size = sizeof(esp_command_t *) * cmd_count;
        cmd_set->cmd_ptr_set = heap_caps_malloc(alloc_cmd_ptrs_size, s_config.heap_caps_used);
        if (!cmd_set->cmd_ptr_set) {
            return ESP_ERR_NO_MEM;
        } else {
            /* copy the temp set of pointer in to the final destination */
            memcpy(cmd_set->cmd_ptr_set, cmd_ptrs, alloc_cmd_ptrs_size);
            cmd_set->cmd_set_size = cmd_count;
        }
    }
    return ESP_OK;
}

esp_command_set_handle_t esp_commands_create_cmd_set(const char **cmd_set, const size_t cmd_set_size, esp_commands_get_field_t get_field)
{
    if (!cmd_set || cmd_set_size == 0) {
        return NULL;
    }

    esp_command_sets_t *cmd_ptr_sets = heap_caps_malloc(sizeof(esp_command_sets_t), s_config.heap_caps_used);
    if (!cmd_ptr_sets) {
        return NULL;
    }


    esp_command_t *static_cmd_ptrs_temp[ESP_COMMANDS_COUNT];
    esp_command_t *dynamic_cmd_ptrs_temp[esp_dynamic_commands_get_number_of_cmd()];
    create_cmd_set_ctx_t ctx = {
        .cmd_set_name = NULL,
        .get_field = get_field,
        .static_cmd_ptrs = static_cmd_ptrs_temp,
        .static_cmd_count = 0,
        .dynamic_cmd_ptrs = dynamic_cmd_ptrs_temp,
        .dynamic_cmd_count = 0
    };

    /* populate the temporary cmd pointer sets */
    for (size_t i = 0; i < cmd_set_size; i++) {
        ctx.cmd_set_name = cmd_set[i];
        go_through_commands(NULL, &ctx, fill_temp_set_info);
    }

    /* if no static command was found, return a static set with 0 items in it */
    esp_err_t ret_val = update_cmd_set_with_temp_info(&cmd_ptr_sets->static_set,
                        ctx.static_cmd_count,
                        ctx.static_cmd_ptrs);
    if (ret_val == ESP_ERR_NO_MEM) {
        free(cmd_ptr_sets);
        return NULL;
    }

    /* if no dynamic command was found, return a dynamic set with 0 items in it */
    ret_val = update_cmd_set_with_temp_info(&cmd_ptr_sets->dynamic_set,
                                            ctx.dynamic_cmd_count,
                                            ctx.dynamic_cmd_ptrs);
    if (ret_val == ESP_ERR_NO_MEM) {
        free(cmd_ptr_sets->static_set.cmd_ptr_set);
        free(cmd_ptr_sets);
        return NULL;
    }

    return (esp_command_set_handle_t)cmd_ptr_sets;
}

esp_command_set_handle_t esp_commands_concat_cmd_set(esp_command_set_handle_t cmd_set_a, esp_command_set_handle_t cmd_set_b)
{
    if (!cmd_set_a && !cmd_set_b) {
        return NULL;
    } else if (cmd_set_a && !cmd_set_b) {
        return cmd_set_a;
    } else if (!cmd_set_a && cmd_set_b) {
        return cmd_set_b;
    }

    /* Reaching this point, both cmd_set_a and cmd_set_b are set.
     * Create a new cmd_set that can host the items from both sets,
     * assign the items to the new set and free the input sets */
    esp_command_sets_t *concat_cmd_sets = heap_caps_malloc(sizeof(esp_command_sets_t), s_config.heap_caps_used);
    if (!concat_cmd_sets) {
        return NULL;
    }
    const size_t new_static_set_size = cmd_set_a->static_set.cmd_set_size + cmd_set_b->static_set.cmd_set_size;
    concat_cmd_sets->static_set.cmd_ptr_set = calloc(new_static_set_size, sizeof(esp_command_t *));
    if (!concat_cmd_sets->static_set.cmd_ptr_set) {
        free(concat_cmd_sets);
        return NULL;
    }

    const size_t new_dynamic_set_size = cmd_set_a->dynamic_set.cmd_set_size + cmd_set_b->dynamic_set.cmd_set_size;
    concat_cmd_sets->dynamic_set.cmd_ptr_set = calloc(new_dynamic_set_size, sizeof(esp_command_t *));
    if (!concat_cmd_sets->static_set.cmd_ptr_set) {
        free(concat_cmd_sets->static_set.cmd_ptr_set);
        free(concat_cmd_sets);
        return NULL;
    }

    /* update the new cmd set sizes */
    concat_cmd_sets->static_set.cmd_set_size = new_static_set_size;
    concat_cmd_sets->dynamic_set.cmd_set_size = new_dynamic_set_size;

    /* fill the list of command pointers */
    memcpy(concat_cmd_sets->static_set.cmd_ptr_set,
           cmd_set_a->static_set.cmd_ptr_set,
           sizeof(esp_command_t *) * cmd_set_a->static_set.cmd_set_size);
    memcpy(concat_cmd_sets->static_set.cmd_ptr_set + cmd_set_a->static_set.cmd_set_size,
           cmd_set_b->static_set.cmd_ptr_set,
           sizeof(esp_command_t *) * cmd_set_b->static_set.cmd_set_size);

    memcpy(concat_cmd_sets->dynamic_set.cmd_ptr_set,
           cmd_set_a->dynamic_set.cmd_ptr_set,
           sizeof(esp_command_t *) * cmd_set_a->dynamic_set.cmd_set_size);
    memcpy(concat_cmd_sets->dynamic_set.cmd_ptr_set + cmd_set_a->dynamic_set.cmd_set_size,
           cmd_set_b->dynamic_set.cmd_ptr_set,
           sizeof(esp_command_t *) * cmd_set_b->dynamic_set.cmd_set_size);

    esp_commands_destroy_cmd_set(&cmd_set_a);
    esp_commands_destroy_cmd_set(&cmd_set_b);

    return (esp_command_set_handle_t)concat_cmd_sets;
}

void esp_commands_destroy_cmd_set(esp_command_set_handle_t *cmd_set)
{
    if (!cmd_set || !*cmd_set) {
        return;
    }

    if ((*cmd_set)->static_set.cmd_ptr_set) {
        free((*cmd_set)->static_set.cmd_ptr_set);
    }

    if ((*cmd_set)->dynamic_set.cmd_ptr_set) {
        free((*cmd_set)->dynamic_set.cmd_ptr_set);
    }

    free(*cmd_set);
    *cmd_set = NULL;
}

typedef struct call_completion_cb_ctx {
    const char *buf;
    const size_t buf_len;
    void *cb_ctx;
    esp_command_get_completion_t completion_cb;
} call_completion_cb_ctx_t;

static bool call_completion_cb(void *caller_ctx, esp_command_t *cmd)
{
    call_completion_cb_ctx_t *ctx = (call_completion_cb_ctx_t *)caller_ctx;

    /* Check if command starts with buf */
    if (strncmp(ctx->buf, cmd->name, ctx->buf_len) == 0) {
        ctx->completion_cb(ctx->cb_ctx, cmd->name);
    }
    return true;
}

void esp_commands_get_completion(esp_command_set_handle_t cmd_set, const char *buf, void *cb_ctx, esp_command_get_completion_t completion_cb)
{
    size_t len = strlen(buf);
    if (len == 0) {
        return;
    }

    call_completion_cb_ctx_t ctx = {
        .buf = buf,
        .buf_len = len,
        .cb_ctx = cb_ctx,
        .completion_cb = completion_cb
    };
    go_through_commands(cmd_set, &ctx, call_completion_cb);
}

const char *esp_commands_get_hint(esp_command_set_handle_t cmd_set, const char *buf, int *color, bool *bold)
{
    *color = s_config.hint_color;
    *bold = s_config.hint_bold;

    esp_command_t *cmd = esp_commands_find_command(cmd_set, buf);
    if (cmd && cmd->hint_cb != NULL) {
        return cmd->hint_cb(cmd->func_ctx);
    }

    return NULL;
}

const char *esp_commands_get_glossary(esp_command_set_handle_t cmd_set, const char *buf)
{
    esp_command_t *cmd = esp_commands_find_command(cmd_set, buf);
    if (cmd && cmd->glossary_cb != NULL) {
        return cmd->glossary_cb(cmd->func_ctx);
    }

    return NULL;
}

/* -------------------------------------------------------------- */
/* help command related code */
/* -------------------------------------------------------------- */

#define FDPRINTF(fd, fmt, ...) do {                                                \
    char _buf[s_config.max_cmdline_length];                                        \
    int _len = snprintf(_buf, sizeof(_buf), fmt, ##__VA_ARGS__);                   \
    if (_len > 0) {                                                                \
        ssize_t _ignored __attribute__((unused));                                  \
        _ignored = write(fd, _buf,                                                 \
            _len < (int)sizeof(_buf) ? _len : (int)sizeof(_buf) - 1);              \
    }                                                                              \
} while (0)

static void print_arg_help(const int fd_out, esp_command_t *it)
{
    /* First line: command name and hint
     * Pad all the hints to the same column
     */
    FDPRINTF(fd_out, "%-s",  it->name);

    const char *hint = NULL;
    if (it->hint_cb) {
        hint = it->hint_cb(it->func_ctx);
    }

    if (hint) {
        FDPRINTF(fd_out, "%s\n", it->hint_cb(it->func_ctx));
    } else {
        FDPRINTF(fd_out, "\n");
    }

    /* Second line: print help */
    /* TODO: replace the simple print with a function that
     * replaces arg_print_formatted */
    if (it->help) {
        FDPRINTF(fd_out, "  %s\n", it->help);
    } else {
        FDPRINTF(fd_out, "  -\n");
    }

    /* Third line: print the glossary*/
    const char *glossary = NULL;
    if (it->glossary_cb) {
        glossary = it->glossary_cb(it->func_ctx);
    }

    if (glossary) {
        FDPRINTF(fd_out, " %s\n", it->glossary_cb(it->func_ctx));
    } else {
        FDPRINTF(fd_out, "  -\n");
    }

    FDPRINTF(fd_out, "\n");
}

static void print_arg_command(const int fd_out, esp_command_t *it)
{
    FDPRINTF(fd_out, "%-s", it->name);
    if (it->hint_cb) {
        const char *hint = it->hint_cb(it->func_ctx);
        if (hint) {
            FDPRINTF(fd_out, " %s", it->hint_cb(it->func_ctx));
        }
    }

    FDPRINTF(fd_out, "\n");
}

typedef enum {
    HELP_VERBOSE_LEVEL_0       = 0,
    HELP_VERBOSE_LEVEL_1       = 1,
    HELP_VERBOSE_LEVEL_MAX_NUM = 2
} help_verbose_level_e;

typedef void (*const fn_print_arg_t)(const int fd_out, esp_command_t *);

static fn_print_arg_t print_verbose_level_arr[HELP_VERBOSE_LEVEL_MAX_NUM] = {
    print_arg_command,
    print_arg_help,
};

typedef struct call_cmd_ctx {
    const int fd_out;
    help_verbose_level_e verbose_level;
    const char *command_name;
    bool command_found;
} call_cmd_ctx_t;

static inline __attribute__((always_inline))
bool call_command_funcs(void *caller_ctx, esp_command_t *cmd)
{
    call_cmd_ctx_t *ctx = (call_cmd_ctx_t *)caller_ctx;

    if (!ctx->command_name) {
        /* ctx->command_name is empty, print all commands */
        print_verbose_level_arr[ctx->verbose_level](ctx->fd_out, cmd);
    } else if (ctx->command_name &&
               (strcmp(ctx->command_name, cmd->name) == 0)) {
        /* we found the command name, print the help and return */
        print_verbose_level_arr[ctx->verbose_level](ctx->fd_out, cmd);
        ctx->command_found = true;
        return false;
    }

    return true;
}

static int help_command(void *context, const int fd_out, int argc, char **argv)
{
    char *command_name = NULL;
    help_verbose_level_e verbose_level = HELP_VERBOSE_LEVEL_1;

    /* argc can never be superior to 4 given than the format is:
     * help cmd_name -v 0 */
    if (argc <= 0 || argc > 4) {
        /* unknown issue, return error */
        FDPRINTF(fd_out, "help: invalid number of arguments %d\n", argc);
        return 1;
    }

    esp_command_sets_t *cmd_sets = (esp_command_sets_t *)context;

    if (argc > 1) {
        /* more than 1 arg, figure out if only verbose level argument
         * was passed and if a specific command was passed.
         * start from the second argument since the first one is "help" */
        for (int i = 1; i < argc; i++) {
            if ((strcmp(argv[i], "-v") == 0) ||
                    (strcmp(argv[i], "--verbose") == 0)) {
                /* check if the following argument is either 0, or 1 */
                if (i + 1 >= argc) {
                    /* format error, return with error */
                    FDPRINTF(fd_out, "help: arguments not provided in the right format\n");
                    return 1;
                } else if (strcmp(argv[i + 1], "0") == 0) {
                    verbose_level = 0;
                } else if (strcmp(argv[i + 1], "1") == 0) {
                    verbose_level = 1;
                } else {
                    /* wrong command format, return error */
                    FDPRINTF(fd_out, "help: invalid verbose level %s\n", argv[i + 1]);
                    return 1;
                }

                /* we found the -v / --verbose, bump i to skip the value of
                 * the verbose argument since it was just parsed */
                i++;
            } else {
                /* the argument is not -v or --verbose, it is then the command name
                 * of which we should print the hint, store it for latter */
                command_name = argv[i];
            }
        }
    }

    /* at this point we should have figured out all the arguments of the help
     * command. if command_name is NULL, then print all commands. if command_name
     * is not NULL, find the command and only print the help for this command. if the
     * command is not found, return with error */
    call_cmd_ctx_t ctx = {
        .fd_out = fd_out,
        .verbose_level = verbose_level,
        .command_name = command_name,
        .command_found = false
    };
    go_through_commands(cmd_sets, &ctx, call_command_funcs);

    if (command_name && !ctx.command_found) {
        FDPRINTF(fd_out, "help: invalid command name %s\n", command_name);
        return 1;
    }

    return 0;
}

static const char *get_help_hint(void *context)
{
    (void)context;
    return "[<string>] [-v <0|1>]";
}

static const char *get_help_glossary(void *context)
{
    (void)context;
    return "  <string>             Name of command\n"
           "  -v, --verbose <0|1>  If specified, list console commands with given verbose level";;
}

static const char help_str[] = "Print the summary of all registered commands if no arguments "
                               "are given, otherwise print summary of given command.";

ESP_COMMAND_REGISTER(help, /* name of the heap command */
                     help, /* group of the help command */
                     help_str, /* help string of the help command */
                     help_command, /* func */
                     NULL, /* the context is null here, it will provided by the exec function */
                     get_help_hint, /* hint callback */
                     get_help_glossary); /* glossary callback */
