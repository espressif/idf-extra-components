/*
 * esp_linenoise history usage example
 * Demonstrates enabling, using, and persisting input history.
 */
#include "common_io.h"
#include "esp_linenoise.h"
#include <stdio.h>
#include <stdlib.h>

#define HISTORY_PATH "/tmp/linenoise_history.txt"
#define HISTORY_LEN 10

void app_main(void)
{
    common_init_io();
    esp_linenoise_t *lno = esp_linenoise_new();
    if (!lno) {
        printf("Failed to create linenoise instance\n");
        return;
    }
    esp_linenoise_set_prompt(lno, "history> ");
    esp_linenoise_set_history_max_len(lno, HISTORY_LEN);
    esp_linenoise_history_load(lno, HISTORY_PATH);

    while (1) {
        char *line = esp_linenoise_edit(lno, esp_linenoise_get_default_in_fd(), esp_linenoise_get_default_out_fd());
        if (!line) {
            break;
        }
        if (line[0] != '\0') {
            printf("You entered: %s\n", line);
            esp_linenoise_history_add(lno, line);
            esp_linenoise_history_save(lno, HISTORY_PATH);
        }
        free(line);
    }
    esp_linenoise_delete(lno);
    esp_linenoise_deinit_io();
}
