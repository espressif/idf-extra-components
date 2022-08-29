/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#pragma once

#include "libuvc/libuvc.h"

#ifdef __cplusplus
extern "C" {
#endif

inline static char *uvc_error_string(uvc_error_t error)
{
    switch (error) {
    case UVC_SUCCESS: return "UVC_SUCCESS";
    case UVC_ERROR_IO: return "UVC_ERROR_IO";
    case UVC_ERROR_INVALID_PARAM: return "UVC_ERROR_INVALID_PARAM";
    case UVC_ERROR_ACCESS: return "UVC_ERROR_ACCESS";
    case UVC_ERROR_NO_DEVICE: return "UVC_ERROR_NO_DEVICE";
    case UVC_ERROR_NOT_FOUND: return "UVC_ERROR_NOT_FOUND";
    case UVC_ERROR_BUSY: return "UVC_ERROR_BUSY";
    case UVC_ERROR_TIMEOUT: return "UVC_ERROR_TIMEOUT";
    case UVC_ERROR_OVERFLOW: return "UVC_ERROR_OVERFLOW";
    case UVC_ERROR_PIPE: return "UVC_ERROR_PIPE";
    case UVC_ERROR_INTERRUPTED: return "UVC_ERROR_INTERRUPTED";
    case UVC_ERROR_NO_MEM: return "UVC_ERROR_NO_MEM";
    case UVC_ERROR_NOT_SUPPORTED: return "UVC_ERROR_NOT_SUPPORTED";
    case UVC_ERROR_INVALID_DEVICE: return "UVC_ERROR_INVALID_DEVICE";
    case UVC_ERROR_INVALID_MODE: return "UVC_ERROR_INVALID_MODE";
    case UVC_ERROR_CALLBACK_EXISTS: return "UVC_ERROR_CALLBACK_EXISTS";
    default: return "Unknown error";
    }
}

#ifdef __cplusplus
}
#endif
