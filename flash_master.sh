#!/usr/bin/env bash
set -e
echo "Flashing nRF52840 DK (Master Unit)"
nrfjprog -f NRF52 --recover
nrfjprog -f NRF52 --program s140_nrf52_7.2.0_softdevice.hex --chiperase
nrfjprog -f NRF52 --program firmware/master/_build/nrf52840_master.hex --sectorerase -r
echo "[SUCCESS] Master running!"
