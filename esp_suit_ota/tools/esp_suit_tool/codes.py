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
               13: 'soft-failure', 14: 'image-size', 18: 'content', 21: 'uri', 22: 'source-component',
               23: 'invoke-args', 24: 'device-id', 25: 'fetch-arguments'}
TEXT_NAMES = {1: 'manifest-description', 2: 'update-description', 3: 'manifest-json-source',
              4: 'manifest-yaml-source'}
ALG_NAMES = {-16: 'sha-256', -7: 'ES256', -9: 'ESP256'}

# a command whose argument is a reporting policy, as opposed to a directive with its own argument
CONDITIONS = (COND_VENDOR, COND_CLASS, COND_IMAGE_MATCH, COND_SLOT, COND_CHECK_CONTENT, COND_ABORT, COND_DEVICE)
