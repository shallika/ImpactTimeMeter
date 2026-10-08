# Impact Time Meter

An end-to-end system for precision impact timing featuring an Android application, a Nordic nRF52840 DK Master Unit, and Seeed XIAO nRF52840 Sense Sensor Units.

## Repository Overview

```
.
├── android/            # Android Studio Kotlin application (BLE interface & UI)
├── docs/               # System architecture and setup documentation
└── firmware/
    ├── master/         # Master Unit Firmware for Nordic nRF52840 DK (SoftDevice Timeslot API + BLE)
    └── sensor/
        └── sensor_xiao_sense/ # Sensor Unit Firmware for Seeed XIAO nRF52840 Sense
```

## System Components

### 1. Android Application (`android/`)
- Mobile application built with Kotlin and Android Studio.
- Connects via Bluetooth Low Energy (BLE) to the Master Unit.
- Handles telemetry visualization, device pinging, and sensor configuration.

### 2. Master Unit Firmware (`firmware/master/`)
- Target: Nordic Semiconductor **nRF52840 DK (PCA10056)**.
- Built with Nordic nRF5 SDK v17.1.0 and S140 SoftDevice v7.2.0.
- Implements SoftDevice Timeslot API for proprietary sub-millisecond 2.4 GHz RF beacon synchronization alongside active BLE connections.

### 3. Sensor Unit Firmware (`firmware/sensor/sensor_xiao_sense/`)
- Target: **Seeed XIAO nRF52840 Sense**.
- Listens for RF synchronization beacons from the Master Unit and sends impact telemetry packets.

## Quick Start

### Building Firmware (Master Unit)
```sh
cd firmware/master
make
```

### Building Android App
Open the `android/` directory in Android Studio or build with Gradle:
```sh
cd android
./gradlew assembleDebug
```

## License
Confidential / Proprietary.
