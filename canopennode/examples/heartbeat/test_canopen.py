#!/usr/bin/env python3
# pip install canopen
import canopen
import queue
import time

network = canopen.Network()
network.connect(channel="can0", bustype="socketcan", bitrate=200000)

node = canopen.RemoteNode(1, None)
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

network.disconnect()
