# Impact Time Meter - Master Unit Firmware

Firmware for the **Impact Time Meter Master Unit** targeting the Nordic Semiconductor **nRF52840 DK (PCA10056)** using Nordic nRF5 SDK 17.1.0 and S140 SoftDevice.

## Features

- **SoftDevice Timeslot API**: Proprietary 2.4 GHz RF protocol multiplexed alongside BLE GAP/GATT.
- **Custom BLE GATT Service**: Exposes telemetry notifications, ping, and configuration characteristics.
- **Sensor Sync Protocol**: Broadcasts synchronous epoch beacon packets to worker sensors.

## Hardware & Toolchain Requirements

- **Target Board**: Nordic nRF52840 DK (PCA10056)
- **SDK**: Nordic nRF5 SDK v17.1.0
- **SoftDevice**: S140 v7.2.0
- **Compiler**: `arm-none-eabi-gcc` (GNU Tools for Arm Embedded Processors)
- **Build Tool**: GNU Make
- **Flashing/Debugging**: SEGGER J-Link / `nrfjprog`

## Building and Flashing

### Build

```sh
make
```

### Flash Application & SoftDevice

Flash using SEGGER J-Link or `nrfjprog`:

```sh
# Program S140 SoftDevice
nrfjprog -f nrf52 --program <path-to-sdk>/components/softdevice/s140/hex/s140_nrf52_7.2.0_softdevice.hex --sectorerase

# Program Firmware Application
nrfjprog -f nrf52 --program build/nrf52840_xxaa.hex --sectorerase
nrfjprog -f nrf52 --reset
```

## Repository Structure

- `main.c`: Application entry point, BLE stack initialization, GAP, advertising, and event loop.
- `timeslot.c` / `timeslot.h`: SoftDevice Timeslot API multiplexer for 2.4 GHz RF synchronization and telemetry reception.
- `ble_impact_service.c` / `ble_impact_service.h`: Custom BLE GATT Service implementation.
- `Makefile`: Project build file referencing the Nordic SDK Makefile.
