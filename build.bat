@echo off
echo ==============================================================
echo Building Fail-Safe Dual-Bank OTA Bootloader Testbench
echo Author: Amit Bakale (Avionics / Embedded Software Engineer)
echo ==============================================================
gcc -O2 -Wall -Iinclude src\crc32.c src\bootloader.c src\main.c -o ota_bootloader_sim.exe
if %ERRORLEVEL% EQU 0 (
    echo [SUCCESS] Build completed successfully: ota_bootloader_sim.exe
    echo Run ota_bootloader_sim.exe to verify ping-pong flashing and anti-brick watchdog rollback!
) else (
    echo [FAILED] Compilation failed with error %ERRORLEVEL%
)
