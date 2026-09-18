/*
 * This software is dual-licensed under GPLv3 and a commercial
 * license. See the LICENSE file distributed with this software for
 * full license information.
 */

#include "oshw.h"
#include "osal.h"
#include <stdlib.h>
#include <string.h>
#include "esp_eth.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

enum {
    ECT_RED_NONE = 0,
    ECT_RED_DOUBLE
};

/* Logical source MAC address used by SOEM for network path identification,
   primary or redundant. NOT real hardware MAC address. */
const uint16 priMAC[3] = EC_PRIMARY_MAC_ARRAY;
const uint16 secMAC[3] = EC_SECONDARY_MAC_ARRAY;

static esp_err_t ecx_esp_eth_rx(esp_eth_handle_t hdl, uint8_t *buffer, uint32_t length, void *priv);
static int ecx_inframe(ecx_portt *port, uint8 idx, int stacknumber);

/* Clear the status of all Rx buffer slots to EMPTY. */
static void ecx_clear_rxbufstat(int *rxbufstat)
{
    for (int i = 0; i < EC_MAXBUF; i++) {
        rxbufstat[i] = EC_BUF_EMPTY;
    }
}

/* Initialize the invariant Ethernet header fields of a TX buffer slot. */
void ec_setupheader(void *p)
{
    ec_etherheadert *bp = p;

    /* Use Ethernet broadcast address as destination MAC to avoid unintended filtering. */
    bp->da0 = oshw_htons(0xffff);
    bp->da1 = oshw_htons(0xffff);
    bp->da2 = oshw_htons(0xffff);

    /* Use SOEM's predefined primary source MAC pattern. */
    bp->sa0 = oshw_htons(priMAC[0]);
    bp->sa1 = oshw_htons(priMAC[1]);
    bp->sa2 = oshw_htons(priMAC[2]);

    /* EtherCAT over EtherNet uses EtherType 0x88A4. */
    bp->etype = oshw_htons(ETH_P_ECAT);
}

/* Set the status of a Rx buffer slot. If redundancy enabled, update as well. */
void ecx_setbufstat(ecx_portt *port, uint8 idx, int bufstat)
{
    port->rxbufstat[idx] = bufstat;
    if (port->redstate != ECT_RED_NONE) {
        port->redport->rxbufstat[idx] = bufstat;
    }
}

/* Allocate an Rx buffer index with mutex protection.
   Search for an empty slot starting from the next index after the last allocated one, not always [0].
   @return idx, shared by txbuf[], rxbuf[], rxbufstat[], and ECAT datagram's Index field to match the response.
   Preserve the upstream SOEM behavior: if no empty slot found after a full search, reuse the next index. */
uint8 ecx_getindex(ecx_portt *port)
{
    osal_mutex_lock(port->getindex_mutex);

    uint8 idx = port->lastidx + 1;
    if (idx >= EC_MAXBUF) {
        idx = 0;
    }

    uint8 cnt = 0; /* Number of slots examined this round. */
    while ((port->rxbufstat[idx] != EC_BUF_EMPTY) && (cnt < EC_MAXBUF)) {
        idx++;
        cnt++;
        if (idx >= EC_MAXBUF) {
            idx = 0;
        }
    }

    port->rxbufstat[idx] = EC_BUF_ALLOC;
    if (port->redstate != ECT_RED_NONE) {
        port->redport->rxbufstat[idx] = EC_BUF_ALLOC;
    }

    port->lastidx = idx;

    osal_mutex_unlock(port->getindex_mutex);
    return idx;
}

/* Initialize a SOEM network port backed by an ESP-IDF Ethernet driver.
   @param ifname, Logical network interface name, must be "esp_eth".
   The Ethernet interface handle is provided through the port structure.
   @param secondary, Non-zero to enable redundant mode, not supported yet.
   @return >0 if OK, 0 if failed. */
int ecx_setupnic(ecx_portt *port, const char *ifname, int secondary)
{
    if ((port == NULL) ||
            (ifname == NULL) ||
            (strcmp(ifname, "esp_eth") != 0) ||
            (port->eth_handle == NULL) ||
            secondary) {
        return 0;
    }

    port->getindex_mutex = NULL;
    port->tx_mutex = NULL;
    port->rx_mutex = NULL;
    port->rx_sem = NULL;
    port->sockhandle = -1;
    port->lastidx = 0;
    port->redstate = ECT_RED_NONE;
    port->redport = NULL;
    /* Point the generic stack view at storage owned by the primary port. */
    port->stack.sock = &port->sockhandle;
    port->stack.txbuf = &port->txbuf;
    port->stack.txbuflength = &port->txbuflength;
    port->stack.tempbuf = &port->tempinbuf;
    port->stack.rxbuf = &port->rxbuf;
    port->stack.rxbufstat = &port->rxbufstat;
    port->stack.rxsa = &port->rxsa;
    port->stack.rxcnt = 0;

    /* Clear all the Rx buffer slots to EMPTY. */
    ecx_clear_rxbufstat(&(port->rxbufstat[0]));

    /* Prefill Ethernet header fields in every TX buffer slot to save runtime overhead. */
    for (int i = 0; i < EC_MAXBUF; i++) {
        ec_setupheader(&(port->txbuf[i]));
    }
    ec_setupheader(&(port->txbuf2));

    port->getindex_mutex = osal_mutex_create();
    port->tx_mutex = osal_mutex_create();
    port->rx_mutex = osal_mutex_create();
    if ((port->getindex_mutex == NULL) ||
            (port->tx_mutex == NULL) ||
            (port->rx_mutex == NULL)) {
        goto fail;
    }

    /* Notify waiting SOEM tasks when receive activity occurs. */
    port->rx_sem = xSemaphoreCreateBinary();
    if (port->rx_sem == NULL) {
        goto fail;
    }

    /* Register the SOEM RX callback as the Ethernet driver's input path,
       invoked when new Ethernet frame received, port must be passed as cb's private arg. */
    if (esp_eth_update_input_path((esp_eth_handle_t)port->eth_handle,
                                  ecx_esp_eth_rx, port) != ESP_OK) {
        goto fail;
    }

    return 1;

fail:
    if (port->rx_sem != NULL) {
        vSemaphoreDelete((SemaphoreHandle_t)port->rx_sem);
        port->rx_sem = NULL;
    }
    osal_mutex_destroy(port->getindex_mutex);
    osal_mutex_destroy(port->tx_mutex);
    osal_mutex_destroy(port->rx_mutex);
    port->getindex_mutex = NULL;
    port->tx_mutex = NULL;
    port->rx_mutex = NULL;
    port->eth_handle = NULL;

    return 0;
}

/* Detach the Ethernet receive path and release port resources. */
int ecx_closenic(ecx_portt *port)
{
    if (port == NULL) {
        return 0;
    }

    if (port->eth_handle != NULL) {
        (void)esp_eth_update_input_path(
            (esp_eth_handle_t)port->eth_handle, NULL, NULL);
        port->eth_handle = NULL; /* The Ethernet driver remains owned by the application. */
    }

    if (port->rx_sem != NULL) {
        vSemaphoreDelete((SemaphoreHandle_t)port->rx_sem);
        port->rx_sem = NULL;
    }

    osal_mutex_destroy(port->getindex_mutex);
    osal_mutex_destroy(port->tx_mutex);
    osal_mutex_destroy(port->rx_mutex);
    port->getindex_mutex = NULL;
    port->tx_mutex = NULL;
    port->rx_mutex = NULL;

    return 0;
}

/* Transmit a prepared Ethernet frame through ESP-IDF Ethernet driver.
   Eth header prefilled. ECAT header and datagrams assembled by SOEM core.
   Call esp_eth_transmit() to ask esp_eth driver send the whole eth frame. ESP_OK doesn't mean bitstream has already on the wire.
   @return lp, Number of bytes sent. */
int ecx_outframe(ecx_portt *port, uint8 idx, int stacknumber)
{
    (void)stacknumber; /* Only the primary path is supported. */

    /* Mark the slot as awaiting the returned EtherCAT frame with the same datagram index. */
    port->rxbufstat[idx] = EC_BUF_TX;

    int lp = port->txbuflength[idx];
    esp_eth_handle_t eth = (esp_eth_handle_t)port->eth_handle;
    if ((eth == NULL) ||
            (esp_eth_transmit(eth, port->txbuf[idx], (size_t)lp) != ESP_OK)) {
        port->rxbufstat[idx] = EC_BUF_EMPTY; /* In case the slot occupied still. */
        return -1;
    }
    return lp;
}

/* Prepare the path(only primary yet) marker and transmit the frame. */
int ecx_outframe_red(ecx_portt *port, uint8 idx)
{
    ec_etherheadert *ehp = (ec_etherheadert *) & (port->txbuf[idx]);
    ehp->sa1 = oshw_htons(priMAC[1]); /* Mark the frame as primary-path traffic. */

    return ecx_outframe(port, idx, 0);
}

/* Callback function registered with esp_eth driver, called by eth rx task.
   @param buffer, Ethernet driver's temporary Rx buffer, ownership transferred to callback, free it before return.
   @param priv, Private argument passed to the callback, must be the ecx_portt pointer.
   Store a received EtherCAT frame in its indexed SOEM RX buffer.
   */
static esp_err_t ecx_esp_eth_rx(esp_eth_handle_t hdl, uint8_t *buffer, uint32_t length, void *priv)
{
    ecx_portt *port = (ecx_portt *)priv;
    ec_etherheadert *ehp;
    ec_comt *ecp;
    uint8 idxf;
    uint16 copy_len;
    (void)hdl;

    /* Drop frames too short to contain Ethernet and EtherCAT headers. */
    if ((buffer == NULL) || (port == NULL) || (length < (ETH_HEADERSIZE + EC_HEADERSIZE))) {
        free(buffer);
        return ESP_OK;
    }

    /* Ignore non-EtherCAT Ethernet frames. */
    ehp = (ec_etherheadert *)buffer;
    if (ehp->etype != oshw_htons(ETH_P_ECAT)) {
        free(buffer);
        return ESP_OK;
    }

    /* Skip eth header. Use the first ECAT datagram index to select the matching SOEM RX slot. */
    ecp = (ec_comt *)(buffer + ETH_HEADERSIZE);
    idxf = ecp->index;

    osal_mutex_lock(port->rx_mutex);

    if ((idxf < EC_MAXBUF) && (port->rxbufstat[idxf] == EC_BUF_TX)) {
        /* Limit the copy to the actual received frame length. */
        copy_len = (uint16)(port->txbuflength[idxf] - ETH_HEADERSIZE);
        if (copy_len > (length - ETH_HEADERSIZE)) {
            copy_len = (uint16)(length - ETH_HEADERSIZE);
        }

        /* Only copy the payload field (ECAT header and datagrams). */
        memcpy(port->rxbuf[idxf], buffer + ETH_HEADERSIZE, copy_len);

        port->rxsa[idxf] = oshw_ntohs(ehp->sa1);

        /* Change the RX buffer slot status to RCVD, waiting for SOEM core to read. */
        port->rxbufstat[idxf] = EC_BUF_RCVD;

        /* Wake one waiter so it can recheck its requested RX slot. */
        if (port->rx_sem != NULL) {
            (void)xSemaphoreGive((SemaphoreHandle_t)port->rx_sem);
        }
    }
    osal_mutex_unlock(port->rx_mutex);

    free(buffer);
    return ESP_OK;
}

/* Check an indexed RX slot and return its final datagram's working counter.
   Called by ecx_waitinframe_red().
   @return wkc, Final ECAT datagram's wkc field, -1 means no frame received, 0 means received but no slave executed. */
static int ecx_inframe(ecx_portt *port, uint8 idx, int stacknumber)
{
    uint16 l;
    int rval = EC_NOFRAME; /* Equal to -1 on purpose. */
    uint8 *rxbuf;

    (void)stacknumber;

    osal_mutex_lock(port->rx_mutex);
    /* Check the intended Rx buffer slot status. */
    if ((idx < EC_MAXBUF) && (port->rxbufstat[idx] == EC_BUF_RCVD)) {
        rxbuf = port->rxbuf[idx];

        /* ECAT header's low 11bits indicate total length of all datagrams.
           Use it as array offset to find the last datagram's working counter(in little endian). */
        l = rxbuf[0] + ((uint16)(rxbuf[1] & 0x0f) << 8);
        rval = rxbuf[l] + ((uint16)rxbuf[l + 1] << 8);

        /* Change the RX buffer slot status to COMPLETE, waiting for SOEM core to read. */
        port->rxbufstat[idx] = EC_BUF_COMPLETE;
    }
    osal_mutex_unlock(port->rx_mutex);
    return rval;
}

/* Calculate time remaining before a deadline and convert it into FreeRTOS ticks.
   @param timer, Absolute deadline timepoint.
   The actual timing precision is limited by FreeRTOS tick period, not suitable for high-precision cyclic timing. */
static TickType_t ecx_remaining_ticks(osal_timert *timer)
{
    ec_timet now;
    int64_t rem_us;
    TickType_t ticks;

    /* Firstly, calculate the remaining time in us. */
    osal_get_monotonic_time(&now);
    rem_us = ((int64_t)timer->stop_time.tv_sec - (int64_t)now.tv_sec) * 1000000LL +
             ((int64_t)timer->stop_time.tv_nsec - (int64_t)now.tv_nsec) / 1000LL;
    if (rem_us <= 0) {
        return 0;
    }

    /* Secondly, round the remaining microseconds to milliseconds, and convert to FreeRTOS ticks.
       Ensure at least one tick of blocking time to avoid a zero-timeout busy loop.
       So, the real waiting precision is limited by the OS tick granularity. */
    ticks = pdMS_TO_TICKS((uint32_t)((rem_us + 999) / 1000));
    if (ticks == 0) {
        ticks = 1;
    }
    return ticks;
}

/* Wait for the returned ECAT frame with specified index.
   @param timer, Absolute deadline timepoint used to limit the total waiting time in this receive attempt.
   Rx semaphore is shared by all Rx buffer slot, need recheck the index after each wakeup.
   Also need recalculate the remaining waiting time.
   @return wkc, Final ECAT datagram's wkc field, -1 means no frame received, 0 means received but no slave executed. */
static int ecx_waitinframe_red(ecx_portt *port, uint8 idx, osal_timert *timer)
{
    int wkc = EC_NOFRAME;

    do {
        wkc = ecx_inframe(port, idx, 0);
        if (wkc > EC_NOFRAME) {
            break;
        }
        if ((port->rx_sem != NULL) && !osal_timer_is_expired(timer)) {
            (void)xSemaphoreTake((SemaphoreHandle_t)port->rx_sem,
                                 ecx_remaining_ticks(timer));
        }
    } while ((wkc <= EC_NOFRAME) && !osal_timer_is_expired(timer));

    return wkc;
}

/* Public API, function wrapper for ecx_waitinframe_red().
   @param timeout, Maximum waiting time from now on in microseconds, convert to absolute deadline timepoint. */
int ecx_waitinframe(ecx_portt *port, uint8 idx, int timeout)
{
    osal_timert timer;

    osal_timer_start(&timer, (uint32)timeout);
    return ecx_waitinframe_red(port, idx, &timer);
}

/* Send a prepared Ethernet frame and retry until a response arrives or the transaction deadline expires.
   @param timeout, Timeout in microseconds.
   timer1 limits the total budget of the whole send/receive transaction.
   timer2 limits each individual wait after transmission(need recalculate), less than EC_TIMEOUTRET.
   @return wkc, Final ECAT datagram's wkc field, -1 means no frame received, 0 means received but no slave executed. */
int ecx_srconfirm(ecx_portt *port, uint8 idx, int timeout)
{
    int wkc = EC_NOFRAME;
    osal_timert timer1, timer2;

    osal_timer_start(&timer1, (uint32)timeout);

    do {
        ecx_outframe_red(port, idx);

        if (timeout < EC_TIMEOUTRET) {
            osal_timer_start(&timer2, (uint32)timeout);
        } else {
            osal_timer_start(&timer2, EC_TIMEOUTRET);
        }

        wkc = ecx_waitinframe_red(port, idx, &timer2);
    } while ((wkc <= EC_NOFRAME) && !osal_timer_is_expired(&timer1));

    return wkc;
}
