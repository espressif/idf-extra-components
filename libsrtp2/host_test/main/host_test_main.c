/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Host (IDF Linux target) smoke + crypto-roundtrip test for libsrtp2.
 *
 * - srtp_init() / srtp_shutdown()
 * - srtp_create() with AES-GCM-128 policy
 * - srtp_protect() + srtp_unprotect() roundtrip; assert plaintext is recovered
 *
 * Deeper coverage lives in libsrtp's own test/srtp_driver.c; we exercise
 * the protect/unprotect happy path here as a sanity-check that the
 * ESP-IDF Linux-target build produces a working library.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "srtp2/srtp.h"

static int rc = 0;

#define CHECK(_e)                                                   \
    do {                                                            \
        srtp_err_status_t _s = (_e);                                \
        if (_s != srtp_err_status_ok) {                             \
            fprintf(stderr, "FAIL: %s -> %d\n", #_e, (int)_s);      \
            rc = 1;                                                 \
        }                                                           \
    } while (0)

static int run_smoke(void)
{
    printf("libsrtp2 host_test: libsrtp version %s\n", srtp_get_version_string());

    CHECK(srtp_init());

    /* Build a sender + receiver context against the same AES-GCM-128 policy. */
    static const uint8_t key[SRTP_AES_GCM_128_KEY_LEN_WSALT] = { 0 };

    srtp_policy_t policy_tx = { 0 };
    srtp_crypto_policy_set_aes_gcm_128_8_auth(&policy_tx.rtp);
    srtp_crypto_policy_set_aes_gcm_128_8_auth(&policy_tx.rtcp);
    policy_tx.ssrc.type = ssrc_any_outbound;
    policy_tx.key = (uint8_t *)key;
    policy_tx.window_size = 128;

    srtp_policy_t policy_rx = policy_tx;
    policy_rx.ssrc.type = ssrc_any_inbound;

    srtp_t sender = NULL;
    srtp_t receiver = NULL;
    CHECK(srtp_create(&sender, &policy_tx));
    CHECK(srtp_create(&receiver, &policy_rx));

    /* Minimal RTP-shaped buffer:
     *   12B header (V=2, PT=0, seq=1, ts=0, ssrc=0xCAFEBABE)
     *   payload "hello srtp"
     */
    uint8_t pkt[12 + 10 + SRTP_MAX_TRAILER_LEN] = {
        0x80, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x00,
        0xCA, 0xFE, 0xBA, 0xBE,
        'h', 'e', 'l', 'l', 'o', ' ', 's', 'r', 't', 'p',
    };
    int   protected_len = 12 + 10;

    CHECK(srtp_protect(sender, pkt, &protected_len));
    if (protected_len <= 12 + 10) {
        fprintf(stderr, "FAIL: protect did not expand the packet (got len=%d)\n", protected_len);
        rc = 1;
    }

    int unprotected_len = protected_len;
    CHECK(srtp_unprotect(receiver, pkt, &unprotected_len));
    if (unprotected_len != 12 + 10) {
        fprintf(stderr, "FAIL: unprotect returned wrong len (got %d, want %d)\n", unprotected_len, 12 + 10);
        rc = 1;
    }
    if (memcmp(pkt + 12, "hello srtp", 10) != 0) {
        fprintf(stderr, "FAIL: recovered payload mismatch\n");
        rc = 1;
    }

    srtp_dealloc(sender);
    srtp_dealloc(receiver);
    CHECK(srtp_shutdown());

    if (rc == 0) {
        printf("libsrtp2 host_test: PASS\n");
    } else {
        printf("libsrtp2 host_test: FAIL\n");
    }
    return rc;
}

void app_main(void)
{
    exit(run_smoke());
}
