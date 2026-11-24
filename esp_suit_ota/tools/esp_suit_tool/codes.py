# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""SUIT and COSE code points, draft-ietf-suit-manifest-38 Appendix A."""

ENVELOPE_TAG = 107
COSE_SIGN1_TAG = 18
PEN_TAG = 112                       # cbor-pen: RFC 9090 relative OID under 1.3.6.1.4.1

SHA256 = -16
ES256 = -7
ESP256 = -9
SIGNATURE_ALGS = {'ESP256': ESP256, 'ES256': ES256}

# envelope
ENV_AUTH, ENV_MANIFEST, ENV_PAYLOAD_FETCH, ENV_INSTALL, ENV_TEXT = 2, 3, 16, 20, 23
SEVERABLE = (ENV_PAYLOAD_FETCH, ENV_INSTALL, ENV_TEXT)

# manifest
MAN_VERSION, MAN_SEQUENCE, MAN_COMMON, MAN_REFERENCE_URI = 1, 2, 3, 4
MAN_VALIDATE, MAN_LOAD, MAN_INVOKE = 7, 8, 9
MAN_PAYLOAD_FETCH, MAN_INSTALL, MAN_TEXT = 16, 20, 23
COMMON_COMPONENTS, COMMON_SHARED = 2, 4

# commands
COND_VENDOR, COND_CLASS, COND_IMAGE_MATCH = 1, 2, 3
COND_SLOT, COND_CHECK_CONTENT, COND_ABORT, COND_DEVICE = 5, 6, 14, 24
SET_INDEX, TRY_EACH, WRITE, OVERRIDE, FETCH, COPY, INVOKE, SWAP, RUN_SEQUENCE = 12, 15, 18, 20, 21, 22, 23, 31, 32

# parameters
P_VENDOR, P_CLASS, P_DIGEST, P_SLOT, P_STRICT_ORDER, P_SOFT_FAILURE, P_SIZE = 1, 2, 3, 5, 12, 13, 14
P_CONTENT, P_URI, P_SOURCE, P_INVOKE_ARGS, P_DEVICE, P_FETCH_ARGS = 18, 21, 22, 23, 24, 25

# text
TEXT_MANIFEST_DESCRIPTION, TEXT_UPDATE_DESCRIPTION = 1, 2
TEXT_KEYS = {'manifest-description': 1, 'update-description': 2, 'manifest-json-source': 3, 'manifest-yaml-source': 4}

# reporting policy bits: record on success, record on failure, system info on success, on failure
REP_ALL = 15
REP_FAIL = 2

ENV_NAMES = {2: 'authentication-wrapper', 3: 'manifest', 16: 'payload-fetch', 20: 'install', 23: 'text'}
MAN_NAMES = {1: 'manifest-version', 2: 'manifest-sequence-number', 3: 'common', 4: 'reference-uri',
             7: 'validate', 8: 'load', 9: 'invoke', 16: 'payload-fetch', 20: 'install', 23: 'text'}
COMMON_NAMES = {2: 'components', 4: 'shared-sequence'}
CMD_NAMES = {1: 'condition-vendor-identifier', 2: 'condition-class-identifier', 3: 'condition-image-match',
             5: 'condition-component-slot', 6: 'condition-check-content', 12: 'directive-set-component-index',
             14: 'condition-abort', 15: 'directive-try-each', 18: 'directive-write',
             20: 'directive-override-parameters', 21: 'directive-fetch', 22: 'directive-copy',
             23: 'directive-invoke', 24: 'condition-device-identifier', 31: 'directive-swap',
             32: 'directive-run-sequence'}
PARAM_NAMES = {1: 'vendor-id', 2: 'class-id', 3: 'image-digest', 5: 'component-slot', 12: 'strict-order',
               13: 'soft-failure', 14: 'image-size', 18: 'content', 19: 'encryption-info', 21: 'uri',
               22: 'source-component', 23: 'invoke-args', 24: 'device-id', 25: 'fetch-arguments'}
TEXT_NAMES = {1: 'manifest-description', 2: 'update-description', 3: 'manifest-json-source',
              4: 'manifest-yaml-source'}
ALG_NAMES = {-16: 'sha-256', -7: 'ES256', -9: 'ESP256', 1: 'A128GCM', 3: 'A256GCM', -65534: 'A128CTR',
             -65532: 'A256CTR', -3: 'A128KW', -5: 'A256KW', -29: 'ECDH-ES + A128KW', -31: 'ECDH-ES + A256KW'}

# payload encryption, draft-ietf-suit-firmware-encryption
P_ENCRYPTION_INFO = 19
COSE_ENCRYPT_TAG = 96

# COSE header labels
HDR_ALG, HDR_KID, HDR_IV = 1, 4, 5
HDR_SALT = -20                         # RFC 9053 table 11: the HKDF salt
HDR_EPHEMERAL_KEY = -1

# content encryption algorithms
A128GCM, A256GCM = 1, 3
A128CTR, A256CTR = -65534, -65532      # RFC 9459, the values the draft's examples use

# key wrap and key agreement
A128KW, A256KW = -3, -5
ECDH_ES_A128KW, ECDH_ES_A256KW = -29, -31

CONTENT_ALGS = {'A128GCM': A128GCM, 'A256GCM': A256GCM, 'A128CTR': A128CTR, 'A256CTR': A256CTR}
KW_ALGS = {'A128KW': A128KW, 'A256KW': A256KW}
ESDH_FOR_KW = {A128KW: ECDH_ES_A128KW, A256KW: ECDH_ES_A256KW}
KEY_BYTES = {A128GCM: 16, A256GCM: 32, A128CTR: 16, A256CTR: 32, A128KW: 16, A256KW: 32}
IV_BYTES = {A128GCM: 12, A256GCM: 12, A128CTR: 16, A256CTR: 16}
AEAD_ALGS = (A128GCM, A256GCM)
GCM_TAG_BYTES = 16

# COSE_Key members of an EC2 ephemeral key
KEY_KTY, KEY_CRV, KEY_X, KEY_Y = 1, -1, -2, -3
KTY_EC2, CRV_P256 = 2, 1

# draft section 5.2.4: the constant that goes in SuppPubInfo.other
KDF_OTHER = b'SUIT Payload Encryption'

# a command whose argument is a reporting policy, as opposed to a directive with its own argument
CONDITIONS = (COND_VENDOR, COND_CLASS, COND_IMAGE_MATCH, COND_SLOT, COND_CHECK_CONTENT, COND_ABORT, COND_DEVICE)
