@echo off
echo ========================================================
echo Flashing Sensor 1 (Seeed XIAO nRF52840 Sense)
echo ========================================================
echo Double-click RESET button on Xiao Sense so 'XIAO-SENSE' mounts.
set TARGET_DRIVE=
for %%d in (D E F G H I J K L M N O P Q R S T U V W X Y Z) do (
    if exist %%d:\INFO_UF2.TXT (
        set TARGET_DRIVE=%%d:
    )
)
if "%TARGET_DRIVE%"=="" (
    echo [ERROR] Xiao Sense UF2 drive not detected!
    pause
    exit /b 1
)
echo Found Xiao Sense at %TARGET_DRIVE%\
copy /Y firmware\sensor\sensor1.uf2 %TARGET_DRIVE%\ >nul
echo [SUCCESS] Sensor 1 flashed!
pause
