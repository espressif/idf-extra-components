# ISO-TP Protocol Details

## Overview

ISO 15765-2, commonly known as ISO-TP (ISO Transport Protocol), is a transport protocol that runs over Controller Area Network (CAN). It extends the 8-byte CAN frame limitation by providing segmentation and reassembly services for larger payloads.

## Frame Structure

### N_PCI (Network Protocol Control Information) Layout

The frame header structure of the ISO-TP protocol follows ISO 15765-2 specification:

| N_PDU Name | Applicability | N_PCI Byte 1 (upper nibble) | N_PCI Byte 1 (lower nibble) | Byte 2 | Byte 3 | Byte 4 | Byte 5 | Byte 6 |
|------------|---------------|------------------------------|------------------------------|---------|---------|---------|---------|---------|
| Single Frame (SF) | Classic CAN (`CAN_DL ≤ 8`) | `0000` | `SF_DL` (0..7) | — | — | — | — | — |
| Single Frame (SF) | CAN FD (`CAN_DL > 8`) | `0000` | `0000` | `SF_DL` (8..4095) | — | — | — | — |
| First Frame (FF) | `FF_DL ≤ 4095` | `0001` | `FF_DL[11:8]` | `FF_DL[7:0]` | — | — | — | — |
| First Frame (FF, extended) | `FF_DL > 4095` (CAN FD only) | `0001` | `0000` | `0000` | `FF_DL[31:24]` | `FF_DL[23:16]` | `FF_DL[15:8]` | `FF_DL[7:0]` |
| Consecutive Frame (CF) | Segmented payload | `0010` | `SN` (0..F, wraps) | — | — | — | — | — |
| Flow Control (FC) | Receiver → Sender | `0011` | `FS` | `BS` | `STmin` | `N/A` | `N/A` | `N/A` |

**Field Descriptions:**

- `SF_DL`: Single Frame Data Length. In Classic CAN, it's a 4-bit value (in Byte-1 lower nibble). In CAN FD with length > 7, the lower nibble is `0000` and the actual length is in Byte 2.
- `FF_DL`: Total message length. Typically a 12-bit value (Byte-1 lower nibble + Byte 2). For lengths > 4095, a 32-bit length field is used (Bytes 3-6).
- `SN`: Consecutive Frame sequence number, 4-bit value cycling from 0 to 15.
- `FS`: Flow Status, 4-bit value: 0=CTS (Continue To Send), 1=WT (Wait), 2=OVFLW (Overflow/Abort).
- `BS`: Block Size, 8-bit value indicating the number of CFs allowed before next FC; 0 = unlimited (rate-limited only by STmin).
- `STmin`: Minimum separation time, 8-bit value: 0x00-0x7F → 0-127 ms; 0xF1-0xF9 → 100-900 μs. Other values are reserved.

### Basic Frame Types

ISO-TP defines four frame types, identified by the Protocol Control Information (PCI) in the first byte:

### Single Frame (SF)

Used for messages that fit in a single CAN frame (≤ 7 bytes):

```
Byte 0: [0000|DLLL] - PCI_TYPE=0, Data Length (0-7)
Byte 1-7: Data payload
```

**Example:**
```
CAN ID: 0x7E0, Data: [03 22 F1 90 00 00 00 00]
       PCI──┘ │  └─ UDS Read DID 0xF190
       SF_DL=3 bytes
```

### First Frame (FF)

Starts a multi-frame transmission (> 7 bytes):

```
Byte 0: [0001|LLLH] - PCI_TYPE=1, Length high nibble
Byte 1: [LLLLLLLL]  - Length low byte (total: 12-bit length)
Byte 2-7: First 6 bytes of data
```

**Example:**
```
CAN ID: 0x7E8, Data: [10 0A 62 F1 90 AA BB CC]
       FF──┘ │  │  └─ First 6 data bytes
       Length=10 bytes total
```

### Consecutive Frame (CF)

Continues multi-frame transmission:

```
Byte 0: [0010|SSSS] - PCI_TYPE=2, Sequence Number (0-15, wrapping)
Byte 1-7: Continuation data (up to 7 bytes)
```

**Example:**
```
CAN ID: 0x7E8, Data: [21 DD EE FF 00 CC CC CC]
       CF──┘ │  └─ Remaining 4 data bytes + padding
       SN=1
```

### Flow Control (FC)

Receiver controls transmission flow:

```
Byte 0: [0011|FSSS] - PCI_TYPE=3, Flow Status
Byte 1: [BBBBBBBB]  - Block Size (frames allowed before next FC)
Byte 2: [STSTSTST]  - Separation Time minimum
Byte 3-7: Reserved (typically 0x00 or 0xCC)
```

**Flow Status Values:**
- `0x0` (CTS): Continue To Send
- `0x1` (WT): Wait (temporary pause)
- `0x2` (OVFLW): Overflow, abort transmission

**Example:**
```
CAN ID: 0x7E0, Data: [30 08 14 00 00 00 00 00]
       FC──┘ │  │  └─ Reserved bytes
       CTS   │  STmin=0x14 (20ms)
       BS=8 frames
```

## Message Flow Examples

### Single Frame Communication

```
Tester → ECU:  [03 22 F1 90 00 00 00 00]  # UDS Read DID request
ECU → Tester:  [04 62 F1 90 42 00 00 00]  # Response with data 0x42
```

### Multi-Frame Communication

```
1. Tester → ECU:  [03 22 F1 B0 00 00 00 00]     # Request large data
2. ECU → Tester:  [10 0C 62 F1 B0 01 02 03]     # FF: 12 bytes total
3. Tester → ECU:  [30 08 14 00 00 00 00 00]     # FC: Continue, BS=8, STmin=20ms
4. ECU → Tester:  [21 04 05 06 07 08 09 CC]     # CF SN=1: remaining 6 bytes + padding

Reassembled payload: [62 F1 B0 01 02 03 04 05 06 07 08 09] (12 bytes)
```

## Timing Parameters

### Separation Time (STmin)

Controls minimum gap between consecutive frames:

| STmin Value | Meaning |
|-------------|---------|
| `0x00-0x7F` | 0-127 milliseconds |
| `0xF1-0xF9` | 100-900 microseconds (0xF1=100µs, 0xF2=200µs, etc.) |
| Others | Reserved |

### Block Size (BS)

Number of consecutive frames sender may transmit before expecting flow control:

- `BS = 0`: No limit (use STmin for pacing)
- `BS = 1-255`: Send this many CFs, then wait for FC

### Timeouts

Standard timeout values (implementation dependent):

- **N_As (sender)**: Time to wait for FC after FF transmission (~1000ms)
- **N_Bs (sender)**: Time between FC and next CF (~1000ms)
- **N_Ar (receiver)**: Time to send FC after receiving FF (~1000ms)
- **N_Br (receiver)**: Time to send FC after receiving BS consecutive frames (~1000ms)
- **N_Cr (receiver)**: Time to receive next CF (~1000ms)

## Error Handling

### Protocol Errors

1. **Unexpected Frame**: Wrong PCI type for current state
2. **Sequence Error**: CF with wrong sequence number
3. **Timeout**: No response within timeout period
4. **Overflow**: Message too large for receiver buffer

### Error Recovery

- **Timeout**: Abort transmission, notify application
- **Wrong SN**: Abort reception, send FC with OVFLW
- **Buffer overflow**: Send FC with OVFLW immediately

## Implementation Considerations

### Performance Optimization

1. **Frame Packing**: Use full 8 bytes when possible
2. **STmin Tuning**: Balance throughput vs. receiver processing time
3. **Block Size**: Larger blocks = higher throughput, more receiver memory needed

### Memory Management

- **Segmentation Buffers**: Size for largest expected message
- **Frame Queues**: Handle burst reception without loss
- **Flow Control**: Prevent receiver overflow

### Multi-Instance Support

Each ISO-TP instance requires:
- Unique CAN ID pair (TX/RX)
- Separate state machine
- Independent buffers and timers

This allows multiple simultaneous ISO-TP sessions on the same CAN bus, commonly used for:
- Diagnostic services (multiple ECUs)
- Data logging (multiple sensors)
- Firmware updates (parallel programming)

## Worked Example (Classic CAN, DLC=8)

Below is a detailed ISO-TP frame transmission example to help understand how the protocol works in practice:

```text
# Step 1: Single Frame  (Tester → ECU)
CAN ID: 0x7E0, Data: [03 22 F1 B3 00 00 00 00]

# Step 2: First Frame   (ECU → Tester)  
CAN ID: 0x7E8, Data: [10 08 62 F1 B3 15 15 16]

# Step 3: Flow Control  (Tester → ECU)
CAN ID: 0x7E0, Data: [30 08 14 00 00 00 00 00]

# Step 4: Consecutive Frame #1 (ECU → Tester)
CAN ID: 0x7E8, Data: [21 51 36 36 CC CC CC CC]
```

### Detailed Analysis

#### Step 1 — Single Frame Request

```
03 22 F1 B3 00 00 00 00
│  │  │  │  └─ Padding bytes (0x00)
│  │  └─────── DID 0xF1B3 (Data Identifier)
│  └────────── UDS Service 0x22 (Read Data By Identifier)
└───────────── PCI: SF with SF_DL=3 bytes
```

- `03`: Upper nibble `0` (SF), lower nibble `3` (SF_DL=3 bytes of payload)
- Payload: `22 F1 B3` (UDS service `0x22` with DID `F1B3`)
- Remaining bytes are padding (`00`) to reach DLC=8

#### Step 2 — First Frame Response (Beginning of Segmented Message)

```
10 08 62 F1 B3 15 15 16
│  │  │  │  │  │  │  │
│  │  │  │  │  └──┴──┴── First 6 data bytes
│  │  │  └─────────────── DID echo (0xF1B3)  
│  │  └────────────────── UDS positive response (0x62 = 0x22 + 0x40)
│  └───────────────────── FF_DL low byte = 8 bytes total
└──────────────────────── PCI: FF with FF_DL high nibble = 0
```

- `10`: Upper nibble `1` (FF), lower nibble `0` (high 4 bits of length)
- `08`: Combined with lower nibble gives **FF_DL = 0x008 = 8 bytes total**
- FF carries first 6 data bytes: `62 F1 B3 15 15 16`
- Since total is 8, remaining 2 bytes will come in next CF

#### Step 3 — Flow Control from Tester

```
30 08 14 00 00 00 00 00
│  │  │  └─ Reserved bytes (padding)
│  │  └───── STmin = 0x14 = 20ms minimum gap between CFs
│  └──────── BS = 8 (ECU may send up to 8 CFs before next FC)
└─────────── PCI: FC with FS=0 (CTS - Continue To Send)
```

- `30`: Upper nibble `3` (FC), lower nibble `0` (FS=0, CTS)
- `08`: BS=8 (ECU may send up to 8 CFs before expecting another FC)
- `14`: STmin = 0x14 = 20ms (minimum gap between CFs)
- Rest are padding (`00`)

#### Step 4 — Consecutive Frame #1 from ECU

```
21 51 36 36 CC CC CC CC
│  │  │  │  └─ Padding bytes (0xCC)
│  └──┴──┴───── Remaining 2 data bytes to complete 8-byte message
└────────────── PCI: CF with SN=1 (first CF after FF)
```

- `21`: Upper nibble `2` (CF), lower nibble `1` (SN=1, first CF after FF)
- Payload: `51 36 36` (the remaining 2 bytes + padding)
- `CC` bytes are padding (common ISO-TP filler)

**Reassembled Application Payload:**
```
From FF: [62 F1 B3 15 15 16] (6 bytes)
From CF: [51 36]              (2 bytes)
Total:   [62 F1 B3 15 15 16 51 36] (8 bytes, matches FF_DL)
```

### Flow Control Benefits

This example demonstrates several key ISO-TP benefits:

- **Automatic Segmentation**: 8-byte response automatically split across FF + CF
- **Flow Control**: Receiver (tester) controls transmission rate with FC frame
- **Sequence Validation**: CF sequence number ensures proper ordering
- **Timing Control**: STmin=20ms ensures ECU has adequate processing time
- **Buffer Management**: BS=8 prevents receiver buffer overflow

### Network Timing Analysis

```
Time 0ms:    Tester sends SF request
Time 5ms:    ECU processes request, sends FF response
Time 10ms:   Tester processes FF, sends FC (CTS, BS=8, STmin=20ms)
Time 35ms:   ECU waits STmin (20ms) + processing, sends CF
Time 40ms:   Communication complete, total time = 40ms
```

> **Notes:**
>
> - In Classic CAN, padding is typically `0x00` or `0xCC` depending on implementation
> - With **BS=8** and **STmin=20ms**, the ECU can send up to 8 CFs, each spaced by at least 20ms
> - Real-world timing depends on CAN bus speed, processing delays, and network load

## Configuration Guidelines

Based on the protocol analysis, here are practical configuration recommendations:

### Tuning Guidelines

| Use Case | Block Size | STmin | Timeout | Description |
|----------|------------|--------|---------|-------------|
| **High throughput** | 16-32 | 0 | 50ms | Maximum speed networks |
| **Reliable networks** | 8 | 5ms | 200ms | Standard automotive applications |
| **Resource constrained** | 4 | 10ms | 500ms | Memory-limited devices |

#### Block Size (BS) Considerations

- **Higher values (16-32)**: Better throughput, requires more receiver memory
- **Lower values (4-8)**: More flow control overhead, better for memory-constrained devices  
- **BS=0**: Unlimited - receiver won't send additional FC frames (use with caution)

#### Separation Time (STmin) Optimization

- **0x00**: No delay between frames (maximum speed)
- **0x01-0x7F**: Delay in milliseconds (1-127ms)
- **0xF1-0xF9**: Delay in microseconds (100-900μs) for high-speed applications
- **Other values**: Reserved, should not be used

### Network-Specific Recommendations

#### High-Speed CAN (500 kbit/s - 1 Mbit/s)
- Use shorter timeouts (50-100ms)
- Minimize separation time (0-2ms)
- Higher block sizes (16-32 frames)

#### Low-Speed CAN (125-250 kbit/s)
- Use longer timeouts (200-500ms)
- Allow more separation time (5-10ms)
- Moderate block sizes (8-16 frames)

#### Noisy/Unreliable Networks
- Increase timeout significantly (500-1000ms)
- Use smaller block sizes (4-8 frames)
- Implement application-level retransmission