@echo off
echo ========================================================
echo Flashing nRF52840 DK (Master Unit)
echo ========================================================
where nrfjprog >nul 2>nul
if %errorlevel% neq 0 (
    echo [ERROR] nrfjprog not found in PATH!
    echo Install Nordic Command Line Tools: https://www.nordicsemi.com/Products/Development-tools/nrf-command-line-tools
    pause
    exit /b 1
)
echo [1/3] Recovering chip...
nrfjprog -f NRF52 --recover
echo [2/3] Flashing SoftDevice S140...
nrfjprog -f NRF52 --program s140_nrf52_7.2.0_softdevice.hex --chiperase
echo [3/3] Flashing Application & Resetting...
nrfjprog -f NRF52 --program firmware/master/_build/nrf52840_master.hex --sectorerase -r
echo [SUCCESS] Master is running and advertising as 'ImpactMaster'!
pause
