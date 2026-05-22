# SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

set(SRTP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/libsrtp")

# Vendored port-side source — replaces the upstream crypto/kernel/crypto_kernel.c
# (which we exclude from SRCS below). The only delta vs upstream is opting out
# of the AES-ICM-192 cipher registration when GCM is enabled, to save binary
# size — AES-CM-128 + AES-GCM cover all WebRTC SRTP suites in use.
#
# Bundled into port/ rather than applied as a `git apply` patch at configure
# time, so the wrapper works cleanly when installed via the IDF Component
# Registry (the registry tarball has no .git dir for `git apply` to operate
# on). Re-port from upstream when bumping the libsrtp submodule.
set(SRTP_PORT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/port")

idf_component_register(
    SRCS "${SRTP_ROOT}/srtp/srtp.c"
        "${SRTP_ROOT}/crypto/cipher/aes_gcm_mbedtls.c"
        "${SRTP_ROOT}/crypto/cipher/aes_icm_mbedtls.c"
        "${SRTP_ROOT}/crypto/cipher/aes_icm.c"
        "${SRTP_ROOT}/crypto/cipher/aes.c"
        "${SRTP_ROOT}/crypto/cipher/cipher.c"
        "${SRTP_ROOT}/crypto/cipher/cipher_test_cases.c"
        "${SRTP_ROOT}/crypto/cipher/null_cipher.c"
        "${SRTP_ROOT}/crypto/hash/auth.c"
        "${SRTP_ROOT}/crypto/hash/hmac.c"
        "${SRTP_ROOT}/crypto/hash/hmac_mbedtls.c"
        "${SRTP_ROOT}/crypto/hash/auth_test_cases.c"
        "${SRTP_ROOT}/crypto/hash/null_auth.c"
        "${SRTP_ROOT}/crypto/hash/sha1.c"
        "${SRTP_PORT_DIR}/crypto_kernel.c"     # replaces ${SRTP_ROOT}/crypto/kernel/crypto_kernel.c
        "${SRTP_ROOT}/crypto/kernel/alloc.c"
        "${SRTP_ROOT}/crypto/kernel/err.c"
        "${SRTP_ROOT}/crypto/kernel/key.c"
        "${SRTP_ROOT}/crypto/math/datatypes.c"
        "${SRTP_ROOT}/crypto/replay/rdb.c"
        "${SRTP_ROOT}/crypto/replay/rdbx.c"

    PRIV_INCLUDE_DIRS
        "${SRTP_ROOT}/crypto/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/port"
    INCLUDE_DIRS
        "${SRTP_ROOT}/include"                  # upstream API headers (srtp.h, ekt.h, ...)
        "${CMAKE_CURRENT_SOURCE_DIR}/include"   # srtp2/ namespace shim so consumers can still <srtp2/srtp.h>

    REQUIRES "mbedtls"
)

# Reduce paranoia while building libsrtp2. Apple clang on the IDF Linux
# target does not recognise -Wno-conflicting-types, so probe before adding.
include(CheckCCompilerFlag)
check_c_compiler_flag("-Wno-conflicting-types" SRTP2_HAS_NO_CONFLICTING_TYPES)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-error=format
    -Wno-error=unused-function
    -Wno-error=unused-variable
    -Wno-error=unused-but-set-variable
    -Wno-error=sign-compare
    -Wno-error=type-limits
    -Wno-error=pointer-sign
    -Wno-error=implicit-function-declaration
    -Wno-error=strict-prototypes
    -Wno-error=old-style-definition
    -Wno-error=incompatible-pointer-types
)
if(SRTP2_HAS_NO_CONFLICTING_TYPES)
    target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-conflicting-types)
endif()

target_compile_definitions(${COMPONENT_LIB} PUBLIC
    -DHAVE_CONFIG_H
)
