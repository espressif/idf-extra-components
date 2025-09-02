////////////////////////////////////////////////////////////////////////
//                  ___ ___  ___ _____ ___      ___                   //
//                 |_ _/ __|/ _ \_   _| _ \___ / __|                  //
//                  | |\__ \ (_) || | |  _/___| (__                   //
//                 |___|___/\___/ |_| |_|      \___|                  //
//                                                                    //
//                      ___ ___  _  _ ___ ___ ___                     //
//                     / __/ _ \| \| | __|_ _/ __|                    //
//                    | (_| (_) | .` | _| | | (_ |                    //
//                     \___\___/|_|\_|_| |___\___|                    //
//                                                                    //
////////////////////////////////////////////////////////////////////////

#ifndef ISOTPC_CONFIG_H
#define ISOTPC_CONFIG_H

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

/* Private: The default timeout to use when waiting for a response during a
 * multi-frame send or receive.
 */
#define ISO_TP_DEFAULT_RESPONSE_TIMEOUT_US CONFIG_ISO_TP_DEFAULT_RESPONSE_TIMEOUT_US

/* Private: Determines if by default, padding is added to ISO-TP message frames.
 */
#ifdef CONFIG_ISO_TP_FRAME_PADDING
#define ISO_TP_FRAME_PADDING
#endif

/* Private: Value to use when padding frames if enabled by ISO_TP_FRAME_PADDING
 */
#define ISO_TP_FRAME_PADDING_VALUE CONFIG_ISO_TP_FRAME_PADDING_VALUE

/* Private: Determines if by default, an additional argument is present in the
 * definition of isotp_user_send_can.
 */
#ifdef CONFIG_ISO_TP_USER_SEND_CAN_ARG
#define ISO_TP_USER_SEND_CAN_ARG
#endif

/* Enable support for transmission complete callback */
#ifdef CONFIG_ISO_TP_TRANSMIT_COMPLETE_CALLBACK
#define ISO_TP_TRANSMIT_COMPLETE_CALLBACK
#endif

/* Enable support for receive complete callback */
#ifdef CONFIG_ISO_TP_RECEIVE_COMPLETE_CALLBACK
#define ISO_TP_RECEIVE_COMPLETE_CALLBACK
#endif

#endif // ISOTPC_CONFIG_H
