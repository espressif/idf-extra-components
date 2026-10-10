/*
 * This software is dual-licensed under GPLv3 and a commercial
 * license. See the LICENSE file distributed with this software for
 * full license information.
 */

#ifndef _nicdrvh_
#define _nicdrvh_

#ifdef __cplusplus
extern "C" {
#endif

/* Pointer-based view of a SOEM port's TX/RX buffers and state.
   Provides a common access interface so the same frame-processing code
   can operate on either the primary or redundant port storage. */
typedef struct {
    int *sock;

    ec_bufT(*txbuf)[EC_MAXBUF];
    int (*txbuflength)[EC_MAXBUF];
    ec_bufT *tempbuf;
    ec_bufT(*rxbuf)[EC_MAXBUF];
    int (*rxbufstat)[EC_MAXBUF];
    int (*rxsa)[EC_MAXBUF];

    /* Number of received frames on this port path. */
    uint64 rxcnt;
} ec_stackT;

/* Runtime context for the optional SOEM redundant network port. */
typedef struct {
    ec_stackT stack;
    int sockhandle;
    ec_bufT rxbuf[EC_MAXBUF];
    int rxbufstat[EC_MAXBUF];
    int rxsa[EC_MAXBUF];
    ec_bufT tempinbuf;
} ecx_redportt;

/* Runtime context for a SOEM network port.
   Hold the Tx/Rx buffer pool, buffer state, redundancy info, ESP-IDF Ethernet driver handle
   used by SOEM frame transmission and reception paths. */
typedef struct {
    /* Pointer view of the ECAT port Tx/Rx buffers and associated state,
       the storage itself is owned by ecx_portt. */
    ec_stackT stack;
    /* Not used by ESP-IDF Ethernet. Legacy socket handle retained for SOEM structure compatibility. */
    int sockhandle;

    /* Rx buffer pool with EC_MAXBUF slots, each slot has EC_BUFSIZE bytes and
       stores one received EtherCAT frame(ECAT Header + ECAT Datagrams). */
    ec_bufT rxbuf[EC_MAXBUF];
    /* Status flag for each Rx buffer slot */
    int rxbufstat[EC_MAXBUF];
    /* Source MAC address(middle 16bits) associated with each rx frame, stored for redundancy handling. */
    int rxsa[EC_MAXBUF];
    /* One temporary Rx buffer slot used by SOEM redundancy logic. */
    ec_bufT tempinbuf;
    /* Temporary Rx buffer slot status */
    int tempinbufs;

    /* TX buffer pool, same type and size as rxbuf[].
       But each slot stores one complete Ethernet frame(CRC excluded) ready for transmission. */
    ec_bufT txbuf[EC_MAXBUF];
    /* Number of bytes to transmit for each Tx buffer slot. */
    int txbuflength[EC_MAXBUF];
    /* One temporary Tx buffer slot used by SOEM redundancy path. */
    ec_bufT txbuf2;
    /* Number of bytes to transmit for the temporary Tx buffer slot. */
    int txbuflength2;

    /* Index of the most recently allocated buffer slot.
       ecx_getindex() uses it as the starting point when searching for the next free slot. */
    uint8 lastidx;

    /* Current redundancy state */
    int redstate;
    /* Pointer to redundancy port and buffers */
    ecx_redportt *redport;

    /* Mutex protecting Rx/Tx buffer slot allocation and lastidx. */
    osal_mutext getindex_mutex;
    /* Mutex protecting write access to shared transmit-side operations. */
    osal_mutext tx_mutex;
    /* Mutex protecting read access to Rx buffers and their associated state. */
    osal_mutext rx_mutex;

    /* Borrow ESP-IDF Ethernet driver handle. */
    void *eth_handle;

    /* FreeRTOS semaphore used to notify waiting SOEM code of Rx activity. */
    void *rx_sem;
} ecx_portt;

extern const uint16 priMAC[3];
extern const uint16 secMAC[3];

void ec_setupheader(void *p);
int ecx_setupnic(ecx_portt *port, const char *ifname, int secondary);
int ecx_closenic(ecx_portt *port);
void ecx_setbufstat(ecx_portt *port, uint8 idx, int bufstat);
uint8 ecx_getindex(ecx_portt *port);
int ecx_outframe(ecx_portt *port, uint8 idx, int sock);
int ecx_outframe_red(ecx_portt *port, uint8 idx);
int ecx_waitinframe(ecx_portt *port, uint8 idx, int timeout);
int ecx_srconfirm(ecx_portt *port, uint8 idx, int timeout);

#ifdef __cplusplus
}
#endif

#endif
