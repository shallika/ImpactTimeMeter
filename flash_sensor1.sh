#!/usr/bin/env bash
set -e
echo "Flashing Sensor 1 (Seeed XIAO nRF52840 Sense)"
MOUNT_DIR=""
if [ -d "/Volumes/XIAO-SENSE" ]; then
    MOUNT_DIR="/Volumes/XIAO-SENSE"
elif [ -d "/media/$USER/XIAO-SENSE" ]; then
    MOUNT_DIR="/media/$USER/XIAO-SENSE"
fi
cp firmware/sensor/sensor1.uf2 "$MOUNT_DIR/"
echo "[SUCCESS] Sensor 1 flashed!"
