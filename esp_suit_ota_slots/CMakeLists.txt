# The adapter that lets esp_suit_ota use the partition pairs of esp_ota_slots.
# It is its own component so that neither of those two depends on the other.
idf_component_register(SRCS "src/esp_suit_ota_slots.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_suit_ota esp_ota_slots esp_partition)
