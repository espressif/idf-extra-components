#!/usr/bin/env python3
# pip install canopen
import canopen
from pathlib import Path
import queue
import time

EDS_PATH = Path(__file__).resolve().parent / "main" / "OD.eds"

network = canopen.Network()
network.connect(channel="can0", bustype="socketcan", bitrate=200000)

node = canopen.RemoteNode(1, str(EDS_PATH))
network.add_node(node)

# Register listener upfront so no heartbeat is ever missed
_hb_queue = queue.Queue()
_NMT_STATE = {0x00: "INITIALISING", 0x04: "STOPPED", 0x05: "OPERATIONAL", 0x7F: "PRE-OPERATIONAL"}

def _on_hb(can_id, data, timestamp):
    _hb_queue.put(_NMT_STATE.get(data[0], f"UNKNOWN(0x{data[0]:02X})"))

network.subscribe(0x700 + node.id, _on_hb)

def check_hb(expected_state, timeout=5):
    deadline = time.monotonic() + timeout
    state = None
    while time.monotonic() < deadline:
        try:
            state = _hb_queue.get(timeout=deadline - time.monotonic())
        except queue.Empty:
            break
        if state == expected_state:
            print(f"  state={state!r}  OK")
            return
    print(f"  FAIL (expected {expected_state!r}, last state={state!r})")

def wait_tpdo1(timeout=2.0):
    node.tpdo[1].wait_for_reception(timeout)
    value = node.tpdo[1]["TestCNT"].raw
    print(f"  TPDO1 TestCNT={value}")
    return value

# ---------------------- Testing Process Begin ----------------------
print("------Testing NMT switch-------")
print("wait PRE-OPERATIONAL")
check_hb("PRE-OPERATIONAL")

print("send START")
node.nmt.send_command(0x01)
check_hb("OPERATIONAL")

print("send PRE-OPERATIONAL")
node.nmt.send_command(0x80)
check_hb("PRE-OPERATIONAL")

print("send STOP")
node.nmt.send_command(0x02)
check_hb("STOPPED")

print("send RESET COMMUNICATION")
node.nmt.send_command(0x82)
check_hb("INITIALISING")   # boot-up frame (0x00)
check_hb("PRE-OPERATIONAL")

print("\n------Testing SDO-------")
# SDO read 0x1008 sub0 (Manufacturer Device Name, VISIBLE_STRING)
print("SDO read 0x1008")
raw = node.sdo.upload(0x1008, 0)
print(f"  device name = {raw.decode('ascii', errors='replace')!r}")

# SDO write 0x1008 sub0
new_name = b"new-dev"
print(f"SDO write 0x1008 <- {new_name!r}")
node.sdo.download(0x1008, 0, new_name)

# Verify SDO
raw = node.sdo.upload(0x1008, 0)
print(f"  device name = {raw.decode('ascii', errors='replace')!r}  (expected {new_name!r})")

print("\n------Testing TPDO/RPDO-------")
print("read PDO configuration")
node.tpdo.read()
node.rpdo.read()
node.tpdo.subscribe()

print("send START")
node.nmt.send_command(0x01)

# PDO is working in OPERATIONAL state
print("wait TPDO1")
tpdo_cnt = wait_tpdo1()
for i in range(5):
    new_cnt = tpdo_cnt + 1
    print(f"send RPDO1 testCNT <- {new_cnt}")
    node.rpdo[1]["TestCNT"].raw = new_cnt
    node.rpdo[1].transmit()
    tpdo_cnt = wait_tpdo1()

print("\n------Test done, stop the node-------")
print("send STOP")
node.nmt.send_command(0x02)
check_hb("STOPPED")

network.disconnect()
