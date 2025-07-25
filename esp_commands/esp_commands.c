#include "esp_commands.h"
#include "esp_commands_helpers.h"
#include "esp_heap_caps.h"
#include "esp_err.h"

#define ANSI_COLOR_DEFAULT      39      /** Default foreground color */

/* Pointers to the first and last command in the dedicated section.
 * See linker.lf for detailed information about the section */
extern esp_command_t _esp_commands_start;
extern esp_command_t _esp_commands_end;

/**
 * @brief go through all commands registered in the
 * memory section starting at _esp_commands_start
 * and ending at _esp_commands_end
 */
#define FOR_EACH_COMMAND_IN_SECTION(cmd) \
    for ((cmd) = &_esp_commands_start; \
        (cmd) < &_esp_commands_end; \
        (esp_command_t*)(cmd)++)

#define FOR_EACH_COMMAND_IN_SET(cmd, cmd_set) \
    for ((cmd) = cmd_set->cmd_ptr_set[0]; \
        (cmd) < cmd_set->cmd_ptr_set[cmd_set->cmd_prt_set_size - 1]; \
        (esp_command_t*)(cmd)++)

/**
 * @brief Array of pointers to command defining
 * a set of command. Created while calling
 * esp_commands_create_set.
 */
typedef struct esp_command_set {
    esp_command_t **cmd_ptr_set;
    size_t cmd_set_size;
} esp_command_set_t;

/**
 * @brief find a command by its name in the list of registered
 * commands
 *
 * @param name name of the command to find
 * @return esp_command_t* pointer to the command matching the command
 * name, NULL if the command is not found
 */
static esp_command_t *find_command_by_name(const char *name)
{
    esp_command_t *cmd = NULL;
    FOR_EACH_COMMAND_IN_SECTION(cmd) {
        if (strcmp(cmd->command, name) == 0) {
            return cmd;
        }
    }
    return NULL;
}

/**
 * @brief find a command by its name in the list of registered
 * commands
 *
 * @param name name of the command to find
 * @return esp_command_t* pointer to the command matching the command
 * name, NULL if the command is not found
 */
static esp_command_t *find_command_by_name_in_set(esp_command_set_t *cmd_set, const char *name)
{
    if (!cmd_set || !name) {
        return NULL;
    }

    esp_command_t *cmd = NULL;
    FOR_EACH_COMMAND_IN_SET(cmd, cmd_set) {
        if (!cmd) {
            /* this happens if a command name passed in a set was not found,
             * the pointer to command is set to NULL */
            continue;
        }
        if (strcmp(cmd->command, name) == 0) {
            return cmd;
        }
    }
    return NULL;
}

/** run-time configuration options */
static esp_console_config_t s_config = {
    .heap_alloc_caps = MALLOC_CAP_DEFAULT
};

/** temporary buffer used for command line parsing */
static char *s_tmp_line_buf;

esp_err_t esp_console_init(const esp_console_config_t *config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_tmp_line_buf) {
        return ESP_ERR_INVALID_STATE;
    }
    memcpy(&s_config, config, sizeof(s_config));
    if (s_config.hint_color == 0) {
        s_config.hint_color = ANSI_COLOR_DEFAULT;
    }
    if (s_config.heap_alloc_caps == 0) {
        s_config.heap_alloc_caps = MALLOC_CAP_DEFAULT;
    }
    s_tmp_line_buf = heap_caps_calloc(1, config->max_cmdline_length, s_config.heap_alloc_caps);
    if (s_tmp_line_buf == NULL) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t esp_console_deinit(void)
{
    if (!s_tmp_line_buf) {
        return ESP_ERR_INVALID_STATE;
    }
    free(s_tmp_line_buf);
    s_tmp_line_buf = NULL;

    return ESP_OK;
}

esp_err_t esp_commands_execute(esp_command_set_handle_t cmd_set, const char *cmdline, int *cmd_ret)
{
    if (s_tmp_line_buf == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    char **argv = (char **) heap_caps_calloc(s_config.max_cmdline_args, sizeof(char *), s_config.heap_alloc_caps);
    if (argv == NULL) {
        return ESP_ERR_NO_MEM;
    }
    strlcpy(s_tmp_line_buf, cmdline, s_config.max_cmdline_length);

    size_t argc = esp_console_split_argv(s_tmp_line_buf, argv,
                                         s_config.max_cmdline_args);
    if (argc == 0) {
        free(argv);
        return ESP_ERR_INVALID_ARG;
    }
    const esp_command_t *cmd = NULL;
    if (cmd_set) {
        cmd = find_command_by_name_in_set(cmd_set, argv[0]);
    } else {
        cmd = find_command_by_name(argv[0]);
    }
    if (cmd == NULL) {
        free(argv);
        return ESP_ERR_NOT_FOUND;
    }
    if (cmd->func) {
        *cmd_ret = (*cmd->func)(argc, argv);
    }
    if (cmd->func_w_context) {
        if (strcmp(cmd->command, "help") == 0) {
            // this one is tricky, we have to pass a custom context
            *cmd_ret = (*cmd->func_w_context)(cmd_set, argc, argv);
        } else {
            *cmd_ret = (*cmd->func_w_context)(cmd->context, argc, argv);
        }
    }
    free(argv);
    return ESP_OK;
}

esp_command_set_handle_t esp_commands_create_cmd_set(const char **cmd_name_set, const size_t cmd_name_set_size, esp_commands_get_field_t get_field)
{
    esp_command_set_t *cmd_set = heap_caps_malloc(sizeof(esp_command_set_t), s_config.heap_alloc_caps);
    if (!cmd_set) {
        return NULL;
    }

    esp_command_t **cmd_ptrs = heap_caps_calloc(cmd_name_set_size, sizeof(esp_command_t *), s_config.heap_alloc_caps);
    if (!cmd_ptrs) {
        heap_caps_free(cmd_set);
        return NULL;
    }

    /* populate the cmd pointer set */
    esp_command_t *it = NULL;
    for (size_t i = 0; i < cmd_name_set_size; i++) {
        bool command_found = false;
        FOR_EACH_COMMAND(it) {
            if (strcmp(field_getter(it), cmd_name_set[i]) == 0) {
                // it's a match, add the pointer to command to the cmd ptr set
                cmd_ptrs[i] = it;
                command_found = true;
                continue;
            }
        }
        if (!command_found) {
            cmd_ptrs[i] = NULL;
        }
    }

    cmd_set->cmd_ptr_set = cmd_ptrs;
    cmd_set->cmd_set_size = cmd_name_set_size;

    return (esp_command_set_t)cmd_set;
}

void esp_commands_destroy_cmd_set(esp_command_set_handle_t cmd_set)
{
    if (!cmd_set) {
        return;
    }

    if (cmd_set->cmd_ptr_set) {
        heap_caps_free(cmd_set->cmd_ptr_set);
    }

    heap_caps_free(cmd_set);
}

void esp_commands_get_completion(const char *buf, esp_command_get_completion_t completion_cb)
{
    size_t len = strlen(buf);
    if (len == 0) {
        return;
    }
    esp_command_t *it;
    FOR_EACH_COMMAND_IN_SECTION(it) {
        /* Check if command starts with buf */
        if (strncmp(buf, it->command, len) == 0) {
            completion_cb(it->command);
        }
    }
}

const char *esp_commands_get_hint(const char *buf, int *color, int *bold)
{
    size_t len = strlen(buf);
    esp_command_t *it;
    FOR_EACH_COMMAND_IN_SECTION(it) {
        if (strlen(it->command) == len &&
                strncmp(buf, it->command, len) == 0) {
            if (it->get_hint_cb == NULL) {
                return NULL;
            }
            *color = s_config.hint_color;
            *bold = s_config.hint_bold;
            return it->get_hint_cb();
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
    printf("%-s",  it->command);
    if (it->get_hint_cb) {
        printf(" %s\n", it->get_hint_cb());
    } else {
        printf("\n");
    }

    /* Second line: print help */
    /* TODO: replace the simple print with a function that
     * replaces arg_print_formatted */
    if (it->get_help_cb) {
        printf("  %s\n", it->get_help_cb());
    } else {
        printf("  -\n");
    }

    /* Third line: print the glossary*/
    if (it->get_glossary_cb) {
        printf("%s\n", it->get_glossary_cb());
    } else {
        printf("  -\n");
    }

    printf("\n");
}

static void print_arg_command(esp_command_t *it)
{
    printf("%-s",  it->command);
    if (it->get_hint_cb) {
        printf(" %s\n", it->get_hint_cb());
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
    for (size_t i = 0; i < cmd_set->cmd_set_size; i++) {

        if (!cmd_set->cmd_ptr_set[i]) {
            /* this happens if a command name passed in a set was not found,
             * the pointer to command is set to NULL */
            continue;
        }

        if (!command_name) {
            /* command_name is empty, print all commands */
            print_verbose_level_arr[verbose_level](cmd_set->cmd_ptr_set[i]);
        } else if (command_name &&
                   (strcmp(command_name, cmd_set->cmd_ptr_set[i]->command) == 0)) {
            /* we found the command name, print the help and return */
            print_verbose_level_arr[verbose_level](cmd_set->cmd_ptr_set[i]);
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

ESP_COMMAND_REGISTER(help,
                     NULL, /* the help should be a part of all set, it does not need a group name */
                     "Print the summary of all registered commands if no arguments "
                     "are given, otherwise print summary of given command.",
                     &help_command,
                     NULL, /* the context is null here, it will provided by the exec function */
                     get_help_hint,
                     get_help_glossary);
