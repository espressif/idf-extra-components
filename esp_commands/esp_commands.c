/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdbool.h>
#include "esp_commands.h"
#include "esp_commands_helpers.h"
#include "esp_err.h"

/* Default foreground color */
#define ANSI_COLOR_DEFAULT 39

/* Pointers to the first and last command in the dedicated section.
 * See linker.lf for detailed information about the section */
extern esp_command_t _esp_commands_start;
extern esp_command_t _esp_commands_end;

/**
 * @brief Array of pointers to command defining
 * a set of command. Created while calling
 * esp_commands_create_set.
 */
typedef struct esp_command_set {
    esp_command_t **cmd_ptr_set;
    size_t cmd_set_size;
} esp_command_set_t;

/** run-time configuration options */
static esp_commands_config_t s_config = {
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
#define FOR_EACH_COMMAND(cmd_set, cmd)                                              \
    for (size_t _i = 0;                                                             \
         ((cmd_set) == NULL                                                         \
              ? (((cmd) = &_esp_commands_start + _i),                               \
                 (&_esp_commands_start + _i) < &_esp_commands_end)                  \
              : (((cmd) = (cmd_set)->cmd_ptr_set[_i]),                              \
                 _i < (cmd_set)->cmd_set_size));                                    \
         ++_i)

/**
 * @brief returns the number of commands registered
 * in the .esp_commands section
 */
#define ESP_COMMANDS_COUNT (size_t)(&_esp_commands_end - &_esp_commands_start)

/**
 * @brief find a command by its name in the list of registered
 * commands or in a command set if the parameter cmd_set is set.
 *
 * @note If cmd_set is set to NULL by the caller, then the function
 * will try to find a command by name from the list of registered
 * commands in the .esp_commands section
 *
 * @param cmd_set command set to find a command from
 * @param name name of the command to find
 * @return esp_command_t* pointer to the command matching the command
 * name, NULL if the command is not found
 */
static esp_command_t *find_command_by_name(esp_command_set_t *cmd_set, const char *name)
{
    /* no need to check that cmd_set is NULL, if it is, then FOR_EACH_COMMAND will go through
     * the commands registered in the .esp_commands section */
    if (!name) {
        return NULL;
    }
    esp_command_t *cmd = NULL;
    FOR_EACH_COMMAND(cmd_set, cmd) {
        if (strcmp(cmd->name, name) == 0) {
            return cmd;
        }
    }
    return NULL;
}

esp_err_t esp_commands_update_config(const esp_commands_config_t *config)
{
    if (!config ||
        (config->max_cmdline_args == 0) ||
        (config->max_cmdline_length == 0)) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&s_config, config, sizeof(s_config));

    return ESP_OK;
}

esp_err_t esp_commands_execute(esp_command_set_handle_t cmd_set, const char *cmdline, int *cmd_ret)
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

    /* help should always be executed, if cmd_set is set or not */
    const esp_command_t *cmd = NULL;
    bool is_cmd_help = false;
    if (strcmp("help", argv[0]) == 0) {
        /* find the help command in the list in .esp_commands section */
        cmd = find_command_by_name((esp_command_set_t *)NULL, "help");
        is_cmd_help = true;
    } else {
        cmd = find_command_by_name(cmd_set, argv[0]);
    }

    if (cmd == NULL) {
        free(argv);
        free(tmp_line_buf);
        return ESP_ERR_NOT_FOUND;
    }
    if (cmd->func) {
        if (is_cmd_help) {
            // executing help command, pass the cmd_set as context
            *cmd_ret = (*cmd->func)(cmd_set, argc, argv);
        } else {
            *cmd_ret = (*cmd->func)(cmd->func_ctx, argc, argv);
        }
    }
    free(argv);
    free(tmp_line_buf);
    return ESP_OK;
}

esp_command_set_handle_t esp_commands_create_cmd_set(const char **cmd_set, const size_t cmd_set_size, esp_commands_get_field_t get_field)
{
    if (!cmd_set || cmd_set_size == 0) {
        return NULL;
    }

    esp_command_set_t *cmd_ptr_set = malloc(sizeof(esp_command_set_t));
    if (!cmd_ptr_set) {
        return NULL;
    }

    esp_command_t *cmd_ptrs_temp[ESP_COMMANDS_COUNT];
    size_t cmd_ptr_count = 0;

    /* populate the temporary cmd pointer set */
    for (size_t i = 0; i < cmd_set_size; i++) {
        esp_command_t *it = NULL;
        FOR_EACH_COMMAND((esp_command_set_t *)NULL, it) {
            if (strcmp(get_field(it), cmd_set[i]) == 0) {
                // it's a match, add the pointer to command to the cmd ptr set
                cmd_ptrs_temp[cmd_ptr_count] = it;
                cmd_ptr_count++;
                continue;
            }
        }
    }

    /* if no command was found, return a set with 0 items in it */
    if (cmd_ptr_count == 0) {
        cmd_ptr_set->cmd_ptr_set = NULL;
        cmd_ptr_set->cmd_set_size = 0;
    } else {
        const size_t alloc_cmd_ptrs_size = sizeof(esp_command_t *) * cmd_ptr_count;
        cmd_ptr_set->cmd_ptr_set = malloc(alloc_cmd_ptrs_size);
        if (!cmd_ptr_set->cmd_ptr_set) {
            free(cmd_ptr_set);
            cmd_ptr_set = NULL;
        } else {
            /* copy the temp set of pointer in to the final destination */
            memcpy(cmd_ptr_set->cmd_ptr_set, cmd_ptrs_temp, alloc_cmd_ptrs_size);
            cmd_ptr_set->cmd_set_size = cmd_ptr_count;
        }
    }

    return (esp_command_set_handle_t)cmd_ptr_set;
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
    const size_t new_set_size = cmd_set_a->cmd_set_size + cmd_set_b->cmd_set_size;
    esp_command_set_t *concat_cmd_set = malloc(sizeof(esp_command_set_t));
    if (!concat_cmd_set) {
        return NULL;
    }
    concat_cmd_set->cmd_ptr_set = calloc(new_set_size, sizeof(esp_command_t *));
    if (!concat_cmd_set->cmd_ptr_set) {
        free(concat_cmd_set);
        return NULL;
    }

    /* update the new cmd set size*/
    concat_cmd_set->cmd_set_size = new_set_size;

    /* fill the list of command pointers */
    memcpy(concat_cmd_set->cmd_ptr_set,
           cmd_set_a->cmd_ptr_set,
           sizeof(esp_command_t *) * cmd_set_a->cmd_set_size);
    memcpy(concat_cmd_set->cmd_ptr_set + cmd_set_a->cmd_set_size,
           cmd_set_b->cmd_ptr_set,
           sizeof(esp_command_t *) * cmd_set_b->cmd_set_size);

    esp_commands_destroy_cmd_set(&cmd_set_a);
    esp_commands_destroy_cmd_set(&cmd_set_b);

    return (esp_command_set_handle_t)concat_cmd_set;
}

void esp_commands_destroy_cmd_set(esp_command_set_handle_t *cmd_set)
{
    if (!cmd_set || !*cmd_set) {
        return;
    }

    if ((*cmd_set)->cmd_ptr_set) {
        free((*cmd_set)->cmd_ptr_set);
    }

    free(*cmd_set);
    *cmd_set = NULL;
}

void esp_commands_get_completion(esp_command_set_handle_t cmd_set, const char *buf, esp_command_get_completion_t completion_cb)
{
    size_t len = strlen(buf);
    if (len == 0) {
        return;
    }
    esp_command_t *it;
    FOR_EACH_COMMAND(cmd_set, it) {
        /* Check if command starts with buf */
        if (strncmp(buf, it->name, len) == 0) {
            completion_cb(it->name);
        }
    }
}

const char *esp_commands_get_hint(esp_command_set_handle_t cmd_set, const char *buf, int *color, bool *bold)
{
    size_t len = strlen(buf);
    esp_command_t *it;
    FOR_EACH_COMMAND(cmd_set, it) {
        if (strlen(it->name) == len &&
                strncmp(buf, it->name, len) == 0) {
            if (it->hint_cb == NULL) {
                return NULL;
            }
            *color = s_config.hint_color;
            *bold = s_config.hint_bold;
            return it->hint_cb();
        }
    }
    return NULL;
}

/* -------------------------------------------------------------- */
/* help command related code */
/* -------------------------------------------------------------- */

static void print_arg_help(esp_command_t *it)
{
    /* First line: command name and hint
     * Pad all the hints to the same column
     */
    printf("%-s",  it->name);
    if (it->hint_cb) {
        printf(" %s\n", it->hint_cb());
    } else {
        printf("\n");
    }

    /* Second line: print help */
    /* TODO: replace the simple print with a function that
     * replaces arg_print_formatted */
    if (it->help) {
        printf("  %s\n", it->help);
    } else {
        printf("  -\n");
    }

    /* Third line: print the glossary*/
    if (it->glossary_cb) {
        printf("%s\n", it->glossary_cb());
    } else {
        printf("  -\n");
    }

    printf("\n");
}

static void print_arg_command(esp_command_t *it)
{
    printf("%-s",  it->name);
    if (it->hint_cb) {
        printf(" %s\n", it->hint_cb());
    }
}

typedef enum {
    HELP_VERBOSE_LEVEL_0       = 0,
    HELP_VERBOSE_LEVEL_1       = 1,
    HELP_VERBOSE_LEVEL_MAX_NUM = 2
} help_verbose_level_e;

typedef void (*const fn_print_arg_t)(esp_command_t *);

static fn_print_arg_t print_verbose_level_arr[HELP_VERBOSE_LEVEL_MAX_NUM] = {
    print_arg_command,
    print_arg_help,
};

static int help_command(void *context, int argc, char **argv)
{
    char *command_name = NULL;
    help_verbose_level_e verbose_level = HELP_VERBOSE_LEVEL_1;

    /* argc can never be superior to 4 given than the format is:
     * help cmd_name -v 0 */
    if (argc <= 0 || argc > 4) {
        /* unknown issue, return error */
        printf("help: invalid number of arguments %d\n", argc);
        return 1;
    }

    esp_command_set_t *cmd_set = (esp_command_set_t *)context;

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
                    printf("help: arguments not provided in the right format\n");
                    return 1;
                } else if (strcmp(argv[i + 1], "0") == 0) {
                    verbose_level = 0;
                } else if (strcmp(argv[i + 1], "1") == 0) {
                    verbose_level = 1;
                } else {
                    /* wrong command format, return error */
                    printf("help: invalid verbose level %s\n", argv[i + 1]);
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
    bool command_found = false;
    esp_command_t *it = NULL;
    FOR_EACH_COMMAND(cmd_set, it) {
        if (!it) {
            /* this happens if a command name passed in a set was not found,
             * the pointer to command is set to NULL */
            continue;
        }

        if (!command_name) {
            /* command_name is empty, print all commands */
            print_verbose_level_arr[verbose_level](it);
        } else if (command_name &&
                   (strcmp(command_name, it->name) == 0)) {
            /* we found the command name, print the help and return */
            print_verbose_level_arr[verbose_level](it);
            command_found = true;
            break;
        }
    }

    if (command_name && !command_found) {
        printf("help: invalid command name %s\n", command_name);
        return 1;
    }

    return 0;
}

static const char *get_help_hint(void)
{
    return "[<string>] [-v <0|1>]";
}

static const char *get_help_glossary(void)
{
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
