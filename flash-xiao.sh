#!/bin/bash

# Locatie van de .uf2
UF2_FILE="build/zephyr/zephyr.uf2"

# Controleer of bestand bestaat
if [ ! -f "$UF2_FILE" ]; then
    echo "❌ .uf2 bestand niet gevonden op $UF2_FILE"
    exit 1
fi

# Zoek naar het mount point van de XIAO (UF2 volume)
XIAO_MOUNT=$(find /Volumes -type d -maxdepth 1 -name "XIAO-BLE*" | head -n 1)

if [ -z "$XIAO_MOUNT" ]; then
    echo "❌ XIAO volume niet gevonden. Zet het board in bootloader-modus (dubbelklik reset)."
    exit 1
fi

echo "✅ XIAO volume gevonden op $XIAO_MOUNT"
echo "📤 Kopiëren van $UF2_FILE naar $XIAO_MOUNT..."

cp "$UF2_FILE" "$XIAO_MOUNT"

if [ $? -eq 0 ]; then
    echo "✅ Flash voltooid!"
else
    echo "❌ Kopiëren mislukt!"
    exit 1
fi
