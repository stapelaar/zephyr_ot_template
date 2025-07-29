#!/bin/bash
set -e

BOARD=xiao_ble
APP_DIR=$(dirname "$(readlink -f "$0")")
BUILD_DIR="$APP_DIR/build"
OVERLAY_CONFIG=overlay-OT-0x2410-mtd.conf

echo "-----------------------------------"
echo "Building project for board $BOARD..."
echo "-----------------------------------"

# Verwijder bestaande build directory (pristine build)
if [ -d "$BUILD_DIR" ]; then
    echo "Cleaning previous build..."
    rm -rf "$BUILD_DIR"
fi

# Start de build
west build -b $BOARD --pristine=always -- -DOVERLAY_CONFIG=$OVERLAY_CONFIG

if [ $? -ne 0 ]; then
    echo
    echo "❌ Build failed! Tijd voor koffie of stevige muziek..."
    exit 1
fi

echo
echo "✅ Build succeeded! Lekker bezig Hans!"
