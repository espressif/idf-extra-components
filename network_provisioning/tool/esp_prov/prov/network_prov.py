# SPDX-FileCopyrightText: 2018-2024 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
#

# APIs for interpreting and creating protobuf packets for Wi-Fi provisioning

import proto
from utils import hex_str_to_bytes, str_to_bytes


def print_verbose(security_ctx, data):
    if (security_ctx.verbose):
        print(f'\x1b[32;20m++++ {data} ++++\x1b[0m')


def config_get_status_request(network_type, security_ctx):
    # Form protobuf request packet for GetStatus command
    cfg1 = proto.network_config_pb2.NetworkConfigPayload()
    cfg1.msg = proto.network_config_pb2.TypeCmdGetStatus
    cmd_get_status = proto.network_config_pb2.CmdGetStatus()
    cfg1.cmd_get_status.MergeFrom(cmd_get_status)
    if network_type == 'wifi':
        cfg1.cmd_get_status.net_type = 0
    elif network_type == 'thread':
        cfg1.cmd_get_status.net_type = 1
    else:
        raise RuntimeError
    encrypted_cfg = security_ctx.encrypt_data(cfg1.SerializeToString())
    print_verbose(security_ctx, f'Client -> Device (Encrypted CmdGetStatus): 0x{encrypted_cfg.hex()}')
    return encrypted_cfg.decode('latin-1')


def config_get_status_response(security_ctx, response_data):
    # Interpret protobuf response packet from GetStatus command
    decrypted_message = security_ctx.decrypt_data(str_to_bytes(response_data))
    cmd_resp1 = proto.network_config_pb2.NetworkConfigPayload()
    cmd_resp1.ParseFromString(decrypted_message)
    print_verbose(security_ctx, f'CmdGetStatus type: {str(cmd_resp1.msg)}')
    print_verbose(security_ctx, f'CmdGetStatus status: {str(cmd_resp1.resp_get_status.status)}')

    if cmd_resp1.resp_get_status.net_type == 0:
        if cmd_resp1.resp_get_status.wifi_sta_state == 0:
            print('==== WiFi state: Connected ====')
            return 'connected'
        elif cmd_resp1.resp_get_status.wifi_sta_state == 1:
            print('++++ WiFi state: Connecting... ++++')
            return 'connecting'
        elif cmd_resp1.resp_get_status.wifi_sta_state == 2:
            print('---- WiFi state: Disconnected ----')
            return 'disconnected'
        elif cmd_resp1.resp_get_status.wifi_sta_state == 3:
            print('---- WiFi state: Connection Failed ----')
            if cmd_resp1.resp_get_status.wifi_fail_reason == 0:
                print('---- Failure reason: Incorrect Password ----')
            elif cmd_resp1.resp_get_status.wifi_fail_reason == 1:
                print('---- Failure reason: Incorrect SSID ----')
            return 'failed'
    elif cmd_resp1.resp_get_status.net_type == 1:
        if cmd_resp1.resp_get_status.thread_state == 0:
            print('==== Thread state: Attached ====')
            return 'attached'
        elif cmd_resp1.resp_get_status.thread_state == 1:
            print('==== Thread state: Attaching ====')
            return 'attaching'
        elif cmd_resp1.resp_get_status.thread_state == 2:
            print('==== Thread state: Detached ====')
            return 'detached'
        elif cmd_resp1.resp_get_status.thread_state == 3:
            print('==== Thread state: Attaching Failed ====')
            if cmd_resp1.resp_get_status.thread_fail_reason == 0:
                print('---- Failure reason: Invalid Dataset ----')
            elif cmd_resp1.resp_get_status.thread_fail_reason == 1:
                print('---- Failure reason: Network Not Found ----')
            return 'failed'

    return 'unknown'


def config_set_config_request(network_type, security_ctx, ssid_or_dataset_tlvs, passphrase=''):
    # Form protobuf request packet for SetConfig command
    cmd = proto.network_config_pb2.NetworkConfigPayload()
    cmd.msg = proto.network_config_pb2.TypeCmdSetConfig
    if network_type == 'wifi':
        cmd.cmd_set_config.net_type = 0
        cmd.cmd_set_config.wifi_config.ssid = str_to_bytes(ssid_or_dataset_tlvs)
        cmd.cmd_set_config.wifi_config.passphrase = str_to_bytes(passphrase)
    elif network_type == 'thread':
        cmd.cmd_set_config.net_type = 1
        cmd.cmd_set_config.thread_config.dataset = hex_str_to_bytes(ssid_or_dataset_tlvs)
    else:
        raise RuntimeError
    enc_cmd = security_ctx.encrypt_data(cmd.SerializeToString())
    print_verbose(security_ctx, f'Client -> Device (SetConfig cmd): 0x{enc_cmd.hex()}')
    return enc_cmd.decode('latin-1')


def config_set_config_response(security_ctx, response_data):
    # Interpret protobuf response packet from SetConfig command
    decrypt = security_ctx.decrypt_data(str_to_bytes(response_data))
    cmd_resp4 = proto.network_config_pb2.NetworkConfigPayload()
    cmd_resp4.ParseFromString(decrypt)
    print_verbose(security_ctx, f'SetConfig status: 0x{str(cmd_resp4.resp_set_config.status)}')
    return cmd_resp4.resp_set_config.status


def config_apply_config_request(network_type, security_ctx):
    # Form protobuf request packet for ApplyConfig command
    cmd = proto.network_config_pb2.NetworkConfigPayload()
    cmd.msg = proto.network_config_pb2.TypeCmdApplyConfig
    if network_type == 'wifi':
        cmd.cmd_apply_config.net_type = 0
    elif network_type == 'thread':
        cmd.cmd_apply_config.net_type = 1
    else:
        raise RuntimeError
    enc_cmd = security_ctx.encrypt_data(cmd.SerializeToString())
    print_verbose(security_ctx, f'Client -> Device (ApplyConfig cmd): 0x{enc_cmd.hex()}')
    return enc_cmd.decode('latin-1')


def config_apply_config_response(security_ctx, response_data):
    # Interpret protobuf response packet from ApplyConfig command
    decrypt = security_ctx.decrypt_data(str_to_bytes(response_data))
    cmd_resp5 = proto.network_config_pb2.NetworkConfigPayload()
    cmd_resp5.ParseFromString(decrypt)
    print_verbose(security_ctx, f'ApplyConfig status: 0x{str(cmd_resp5.resp_apply_config.status)}')
    return cmd_resp5.resp_apply_config.status
