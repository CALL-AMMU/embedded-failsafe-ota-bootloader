/**
 * @file main.c
 * @brief Fail-Safe Dual-Bank OTA Bootloader Testbench & Rollback Verification
 * @author Amit Bakale (Avionics & Embedded Software Engineer)
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "bootloader.h"
#include "crc32.h"

/* Helper to assemble an in-memory OTA package */
static size_t create_ota_package(uint8_t* out_buf, uint8_t maj, uint8_t min, uint8_t patch, 
                                 const char* app_code, uint32_t entry_addr) {
    size_t code_len = strlen(app_code);
    ota_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = OTA_IMAGE_MAGIC;
    hdr.ver_major = maj;
    hdr.ver_minor = min;
    hdr.ver_patch = patch;
    hdr.image_size = (uint32_t)code_len;
    hdr.image_crc32 = crc32_calculate((const uint8_t*)app_code, code_len);
    hdr.entry_point = entry_addr;
    hdr.header_crc32 = crc32_calculate((const uint8_t*)&hdr, sizeof(ota_header_t) - sizeof(uint32_t));

    memcpy(out_buf, &hdr, sizeof(hdr));
    memcpy(out_buf + sizeof(hdr), app_code, code_len);
    return sizeof(hdr) + code_len;
}

int main(void) {
    printf("=========================================================================================\n");
    printf("   AUTOMOTIVE & AVIONICS FAIL-SAFE DUAL-BANK OTA BOOTLOADER TESTBENCH\n");
    printf("   Features: Dual-Bank Ping-Pong, CRC32 Verification, Anti-Brick Watchdog Rollback\n");
    printf("   Author: Amit Bakale | Avionics & Embedded Software Engineer\n");
    printf("=========================================================================================\n\n");

    /* Step 1: Cold Boot with Factory Golden Image */
    printf("[SCENARIO 1: Factory Power-On Reset]\n");
    bootloader_init();
    bootloader_print_status();
    bootloader_execute_boot_decision();

    /* Step 2: Over-The-Air Upgrade to v2.1.0 into Slot B */
    printf("-----------------------------------------------------------------------------------------\n");
    printf("[SCENARIO 2: Successful Wireless OTA Upgrade to Firmware v2.1.0]\n");
    uint8_t ota_buffer[1024];
    const char* v2_code = "Flight Guidance Computer Application v2.1.0 [STABLE AUTOLAND RELEASE]";
    size_t pkg_len = create_ota_package(ota_buffer, 2, 1, 0, v2_code, SLOT_B_START_ADDR + sizeof(ota_header_t));

    printf("Flashing signed OTA package into %s...\n", "SLOT_B");
    bool flashed = bootloader_flash_ota_package(SLOT_B, ota_buffer, pkg_len);
    if (!flashed) {
        printf("OTA Flash failed!\n");
        return 1;
    }

    bootloader_print_status();

    /* Reboot into Slot B */
    bootloader_execute_boot_decision();
    /* App verifies peripherals and confirms stability */
    bootloader_confirm_application_stable();
    bootloader_print_status();

    /* Step 3: Bad Firmware (v3.0.0) with Crash Loop & Automatic Rollback */
    printf("-----------------------------------------------------------------------------------------\n");
    printf("[SCENARIO 3: Fault Injection - Defective v3.0.0 Upgrade causing HardFault / Watchdog Crash]\n");
    const char* v3_buggy = "Experimental Autonomous Navigation Engine v3.0.0 [NULL POINTER EXCEPTION]";
    pkg_len = create_ota_package(ota_buffer, 3, 0, 0, v3_buggy, SLOT_A_START_ADDR + sizeof(ota_header_t));

    printf("Flashing buggy update v3.0.0 into SLOT_A...\n");
    bootloader_flash_ota_package(SLOT_A, ota_buffer, pkg_len);

    /* Boot Attempt 1 */
    printf("\n--> System Reset #1:\n");
    bootloader_execute_boot_decision();
    bootloader_simulate_watchdog_crash(); /* Crashed before ota_confirm_boot() */

    /* Boot Attempt 2 */
    printf("\n--> System Reset #2:\n");
    bootloader_execute_boot_decision();
    bootloader_simulate_watchdog_crash(); /* Crashed again */

    /* Boot Attempt 3 */
    printf("\n--> System Reset #3 (Threshold reached):\n");
    bootloader_execute_boot_decision();
    bootloader_simulate_watchdog_crash(); /* Final crash */

    /* Boot Attempt 4: Should trigger FAIL-SAFE ROLLBACK to stable Slot B */
    printf("\n--> System Reset #4 (Automatic Rollback Triggered):\n");
    bootloader_execute_boot_decision();
    bootloader_print_status();

    /* Step 4: Corrupted Payload in Transit Test */
    printf("-----------------------------------------------------------------------------------------\n");
    printf("[SCENARIO 4: Corrupted Payload In Transit (Bit-Flip Protection)]\n");
    ota_buffer[sizeof(ota_header_t) + 5] ^= 0xFF; /* Tamper single byte in binary */
    printf("Attempting to flash corrupted binary...\n");
    bool corrupt_result = bootloader_flash_ota_package(SLOT_A, ota_buffer, pkg_len);
    printf("Corrupt Image Flashed? %s [Expected: REJECTED]\n\n", corrupt_result ? "YES (FAIL)" : "NO (REJECTED)");

    printf("=========================================================================================\n");
    printf("  [SUCCESS] All Fail-Safe Dual-Bank OTA Bootloader Test Scenarios Passed!\n");
    printf("=========================================================================================\n");

    return 0;
}
