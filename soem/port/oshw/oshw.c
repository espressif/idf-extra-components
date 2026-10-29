/*
 * This software is dual-licensed under GPLv3 and a commercial
 * license. See the LICENSE file distributed with this software for
 * full license information.
 */

#include "oshw.h"
#include <stdlib.h>
#include <string.h>

/* Unsigned short byte order changes from host memory(little-endian) to network analysis(big-endian),
   mainly used for eth frame's MAC and EtherType fields. But for payload field(EtherCAT frame),
   it's still little-endian. */
uint16 oshw_htons(uint16 host)
{
    return (uint16)((host << 8) | (host >> 8));
}
uint16 oshw_ntohs(uint16 network)
{
    return oshw_htons(network); /* same as oshw_htons */
}

/* Expose the ESP-IDF Ethernet interface as a virtual SOEM adapter(imitate linux version).
   Fix 'esp_eth' as nic name and description.
   Only support single nic yet. */
ec_adaptert *oshw_find_adapters(void)
{
    ec_adaptert *adapter = (ec_adaptert *)malloc(sizeof(ec_adaptert)); /* nic info list node */
    if (adapter == NULL) {
        return NULL;
    }

    adapter->next = NULL;
    strncpy(adapter->name, "esp_eth", EC_MAXLEN_ADAPTERNAME - 1);
    adapter->name[EC_MAXLEN_ADAPTERNAME - 1] = '\0'; /* manually ending with '\0' */
    strncpy(adapter->desc, "esp_eth", EC_MAXLEN_ADAPTERNAME - 1);
    adapter->desc[EC_MAXLEN_ADAPTERNAME - 1] = '\0';
    return adapter;
}
void oshw_free_adapters(ec_adaptert *adapter)
{
    while (adapter) {
        ec_adaptert *next = adapter->next;
        free(adapter);
        adapter = next;
    }
}
