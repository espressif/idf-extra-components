#!/bin/bash

# Wokwi simulation script for raylib examples
# This script works both in CI and for local development

# Detect firmware location - prefer build directory for local dev
if [ -f "build/flasher_args.json" ]; then
    FIRMWARE="build/flasher_args.json"
elif [ -f "firmware.elf" ]; then
    # CI scenario: firmware.elf copied from downloaded artifacts
    FIRMWARE="firmware.elf"
elif [ -f "build/*.bin" ]; then
    # Fallback to .bin if no flasher_args.json
    FIRMWARE=$(ls build/*.bin | grep -v -E '(bootloader|partition-table)' | head -1)
else
    echo "Error: No firmware found (need build/flasher_args.json or firmware.elf)"
    exit 1
fi

# Build wokwi-cli command
WOKWI_ARGS=(
    --timeout 15000
    --timeout-exit-code 0
    --fail-text "Backtrace: "
    --expect-text "Starting demo loop..."
)

# Custom board - screenshot disabled (not registered on Wokwi server)


# Add firmware
WOKWI_ARGS+=(--elf "$FIRMWARE")

# Add serial log
WOKWI_ARGS+=(--serial-log-file 'wokwi-log.txt')

# Run wokwi-cli
echo "Running Wokwi simulation with firmware: $FIRMWARE"
wokwi-cli "${WOKWI_ARGS[@]}" "."
