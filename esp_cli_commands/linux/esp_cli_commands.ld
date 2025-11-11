SECTIONS
{
    .esp_cli_commands :
    {
        PROVIDE(_esp_cli_commands_start = .);
        KEEP(*(SORT(.esp_cli_commands*))) /* Concatenate all .esp_cli_commands */
        PROVIDE(_esp_cli_commands_end = .);
    }
}
INSERT AFTER .rodata;
