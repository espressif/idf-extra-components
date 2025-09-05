/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ISOTPC_CONFIG_H
#define ISOTPC_CONFIG_H

// This file overrides the default isotp_config.h from isotp-c submodule
// Values are taken from ESP-IDF Kconfig system

#include "sdkconfig.h"

/* Max number of messages the receiver can receive at one time, this value
 * is affected by can driver queue length
 */
#define ISO_TP_DEFAULT_BLOCK_SIZE CONFIG_ISO_TP_DEFAULT_BLOCK_SIZE

/* The STmin parameter value specifies the minimum time gap allowed between
 * the transmission of consecutive frame network protocol data units
 */
#define ISO_TP_DEFAULT_ST_MIN_US CONFIG_ISO_TP_DEFAULT_ST_MIN_US

/* This parameter indicate how many FC N_PDU WTs can be transmitted by the
 * receiver in a row.
 */
#define ISO_TP_MAX_WFT_NUMBER CONFIG_ISO_TP_MAX_WFT_NUMBER

/* The default timeout to use when waiting for a response during a multi-frame send or receive. */
#define ISO_TP_DEFAULT_RESPONSE_TIMEOUT_US CONFIG_ISO_TP_DEFAULT_RESPONSE_TIMEOUT_US

/* Determines if by default, padding is added to ISO-TP message frames */
#ifdef CONFIG_ISO_TP_FRAME_PADDING
#define ISO_TP_FRAME_PADDING
#endif

/* The value to use when padding frames if enabled by ISO_TP_FRAME_PADDING */
#define ISO_TP_FRAME_PADDING_VALUE CONFIG_ISO_TP_FRAME_PADDING_VALUE

/* Always enable the additional user_data argument in isotp_user_send_can function */
#define ISO_TP_USER_SEND_CAN_ARG

/* Always enable the transmission complete callback */
#define ISO_TP_TRANSMIT_COMPLETE_CALLBACK

/* Always enable the receive complete callback */
#define ISO_TP_RECEIVE_COMPLETE_CALLBACK

#endif // ISOTPC_CONFIG_H
