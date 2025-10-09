SECTIONS
{
    .esp_commands :
    {
        PROVIDE(_esp_commands_start = .);
        KEEP(*(SORT(.esp_commands*))) /* Concatenate all .esp_commands */
        PROVIDE(_esp_commands_end = .);
    }
}
INSERT AFTER .rodata;
