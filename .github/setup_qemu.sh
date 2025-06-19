#!/bin/sh
set -e

# QEMU version and date string for easy maintenance
QEMU_VERSION="9.2.2"
QEMU_DATE="20250228"
QEMU_RELEASE="esp-develop-${QEMU_VERSION}-${QEMU_DATE}"

# 1. Detect host architecture
ARCH=$(uname -m)
case "$ARCH" in
    x86_64)
        QEMU_ARCH="x86_64"
        QEMU_RISCV32_FILE="qemu-riscv32-softmmu-esp_develop_${QEMU_VERSION}_${QEMU_DATE}-x86_64-linux-gnu.tar.xz"
        QEMU_XTENSA_FILE="qemu-xtensa-softmmu-esp_develop_${QEMU_VERSION}_${QEMU_DATE}-x86_64-linux-gnu.tar.xz"
        ;;
    aarch64 | arm64)
        QEMU_ARCH="aarch64"
        QEMU_RISCV32_FILE="qemu-riscv32-softmmu-esp_develop_${QEMU_VERSION}_${QEMU_DATE}-aarch64-linux-gnu.tar.xz"
        QEMU_XTENSA_FILE="qemu-xtensa-softmmu-esp_develop_${QEMU_VERSION}_${QEMU_DATE}-aarch64-linux-gnu.tar.xz"
        ;;
    *)
        echo "Unsupported architecture: $ARCH"
        exit 1
        ;;
esac

# Install some dependencies
apt-get update
apt-get install -y libgcrypt20 libglib2.0-0 libpixman-1-0 libsdl2-2.0-0 libslirp0

QEMU_DIR="qemu"

# 2. Download the correct binary
QEMU_RISCV32_URL="https://github.com/espressif/qemu/releases/download/${QEMU_RELEASE}/$QEMU_RISCV32_FILE"
curl -LO "$QEMU_RISCV32_URL"
# 3. Extract the compressed file
tar -xJf "$QEMU_RISCV32_FILE"
rm "$QEMU_RISCV32_FILE"

if [ -f "$QEMU_DIR/bin/qemu-system-riscv32" ]; then
    echo "QEMU RISCV32 installation successful."
else
    echo "QEMU RISCV32 installation failed."
    exit 1
fi
# Rename qemu directory to avoid conflicts
mv "$QEMU_DIR" "${QEMU_DIR}_riscv32"

QEMU_XTENSA_URL="https://github.com/espressif/qemu/releases/download/${QEMU_RELEASE}/$QEMU_XTENSA_FILE"
curl -LO "$QEMU_XTENSA_URL"
# 3. Extract the compressed file
tar -xJf "$QEMU_XTENSA_FILE"
rm "$QEMU_XTENSA_FILE"

if [ -f "$QEMU_DIR/bin/qemu-system-xtensa" ]; then
    echo "QEMU XTENSA installation successful."
else
    echo "QEMU XTENSA installation failed."
    exit 1
fi

# Rename qemu directory to avoid conflicts
mv "$QEMU_DIR" "${QEMU_DIR}_xtensa"

QEMU_DIR=$(pwd)/qemu
# 4. Add both QEMU directories to PATH
export PATH="$PATH:${QEMU_DIR}_riscv32/bin:${QEMU_DIR}_xtensa/bin"
# 5. Verify QEMU installation
if command -v qemu-system-xtensa &> /dev/null; then
    echo "QEMU XTENSA is installed and available in PATH."
else
    echo "QEMU XTENSA is not installed or not available in PATH."
    exit 1
fi
if command -v qemu-system-riscv32 &> /dev/null; then
    echo "QEMU RISCV32 is installed and available in PATH."
else
    echo "QEMU RISCV32 is not installed or not available in PATH."
    exit 1
fi
