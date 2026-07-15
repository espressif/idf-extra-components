# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.16)

# Pick up the blemesh_manager component sitting two directories up.
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../..")

# The controller scan-duplicate knobs use different Kconfig symbol names per
# chip family, so each scheme lives in its own defaults file and we append the
# one matching the target. Setting a symbol that doesn't exist for a target
# emits an "unknown kconfig symbol" warning, hence the per-scheme split:
#   BTDM_*    (legacy dual-mode controller) -> esp32
#   BT_CTRL_*                               -> esp32c3, esp32s3
#   BT_LE_*   (NimBLE-era controller)       -> esp32c5, esp32c6, esp32c61, esp32h2
# Non-BLE / no-dup-symbol targets (esp32s2, etc.) get no extra file — harmless.
# Add a new chip by dropping its name into the matching list below.
#
# Must be resolved before project.cmake. The target may arrive as a -DIDF_TARGET
# cmake var (idf.py set-target), a cached cmake var (later builds), or an env
# var — check all three. Defaults to esp32, matching IDF's own fallback.
if(DEFINED IDF_TARGET)
    set(_resolved_target "${IDF_TARGET}")
elseif(DEFINED ENV{IDF_TARGET})
    set(_resolved_target "$ENV{IDF_TARGET}")
else()
    set(_resolved_target "esp32")
endif()

set(_btdm_targets   esp32)
set(_btctrl_targets esp32c3 esp32s3)
set(_btle_targets   esp32c5 esp32c6 esp32c61 esp32h2)

set(SDKCONFIG_DEFAULTS "sdkconfig.defaults")
if(_resolved_target IN_LIST _btdm_targets)
    list(APPEND SDKCONFIG_DEFAULTS "sdkconfig.defaults.btdm")
elseif(_resolved_target IN_LIST _btctrl_targets)
    list(APPEND SDKCONFIG_DEFAULTS "sdkconfig.defaults.btctrl")
elseif(_resolved_target IN_LIST _btle_targets)
    list(APPEND SDKCONFIG_DEFAULTS "sdkconfig.defaults.btle")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(blemesh_manager_example)
