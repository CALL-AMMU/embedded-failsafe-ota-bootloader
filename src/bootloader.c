/**
 * @file bootloader.c
 * @brief Fail-Safe Dual-Bank OTA Bootloader Core Implementation
 * @author Amit Bakale (Avionics & Embedded Software Engineer)
 */

#include "bootloader.h"
#include "crc32.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define TOTAL_FLASH_SIZE (512 * 1024)

/* Simulated On-Chip NOR Flash Memory Array */
static uint8_t s_flash[TOTAL_FLASH_SIZE];

static inline uint32_t flash_offset(uint32_t addr) {
    return addr - FLASH_START_ADDR;
}

static boot_metadata_t* get_metadata_ptr(void) {
    return (boot_metadata_t*)&s_flash[flash_offset(METADATA_FLASH_ADDR)];
}

static void update_metadata_crc(boot_metadata_t* meta) {
    meta->crc32 = crc32_calculate((const uint8_t*)meta, sizeof(boot_metadata_t) - sizeof(uint32_t));
}

static bool verify_metadata_crc(const boot_metadata_t* meta) {
    if (meta->magic != BOOT_META_MAGIC) return false;
    uint32_t calc = crc32_calculate((const uint8_t*)meta, sizeof(boot_metadata_t) - sizeof(uint32_t));
    return (calc == meta->crc32);
}

static uint32_t get_slot_addr(slot_id_t slot) {
    return (slot == SLOT_A) ? SLOT_A_START_ADDR : SLOT_B_START_ADDR;
}

static slot_id_t get_alternate_slot(slot_id_t slot) {
    return (slot == SLOT_A) ? SLOT_B : SLOT_A;
}

const char* slot_name(slot_id_t slot) {
    if (slot == SLOT_A) return "SLOT_A (Primary Bank)";
    if (slot == SLOT_B) return "SLOT_B (Secondary Bank)";
    return "UNKNOWN_SLOT";
}

const char* state_name(boot_state_t st) {
    switch (st) {
        case BOOT_STATE_CONFIRMED: return "CONFIRMED (Operational)";
        case BOOT_STATE_TRIAL:     return "TRIAL (Watchdog Monitored)";
        case BOOT_STATE_ROLLBACK:  return "ROLLBACK (Automatic Safe Fallback)";
        case BOOT_STATE_CORRUPT:   return "CORRUPTED";
        default:                   return "UNKNOWN";
    }
}

void bootloader_init(void) {
    memset(s_flash, 0xFF, sizeof(s_flash));

    /* Initialize Default Metadata */
    boot_metadata_t* meta = get_metadata_ptr();
    meta->magic = BOOT_META_MAGIC;
    meta->active_slot = SLOT_A;
    meta->boot_state = BOOT_STATE_CONFIRMED;
    meta->trial_boot_counter = 0;
    meta->max_trial_boots = MAX_TRIAL_BOOT_COUNT;
    update_metadata_crc(meta);

    /* Install Golden Baseline Firmware v1.0.0 in Slot A */
    const char golden_payload[] = "Cortex-M Baseline Flight Control Application v1.0.0 [ACTIVE]";
    size_t payload_len = strlen(golden_payload);

    ota_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = OTA_IMAGE_MAGIC;
    hdr.ver_major = 1;
    hdr.ver_minor = 0;
    hdr.ver_patch = 0;
    hdr.image_size = (uint32_t)payload_len;
    hdr.image_crc32 = crc32_calculate((const uint8_t*)golden_payload, payload_len);
    hdr.entry_point = SLOT_A_START_ADDR + sizeof(ota_header_t);
    hdr.header_crc32 = crc32_calculate((const uint8_t*)&hdr, sizeof(ota_header_t) - sizeof(uint32_t));

    uint32_t offset = flash_offset(SLOT_A_START_ADDR);
    memcpy(&s_flash[offset], &hdr, sizeof(hdr));
    memcpy(&s_flash[offset + sizeof(hdr)], golden_payload, payload_len);
}

bool bootloader_verify_slot(slot_id_t slot) {
    uint32_t offset = flash_offset(get_slot_addr(slot));
    const ota_header_t* hdr = (const ota_header_t*)&s_flash[offset];

    /* 1. Validate Header Magic */
    if (hdr->magic != OTA_IMAGE_MAGIC) {
        return false;
    }

    /* 2. Validate Header CRC-32 */
    uint32_t calc_hdr_crc = crc32_calculate((const uint8_t*)hdr, sizeof(ota_header_t) - sizeof(uint32_t));
    if (calc_hdr_crc != hdr->header_crc32) {
        return false;
    }

    /* 3. Validate Application Payload Bounds */
    if (hdr->image_size == 0 || hdr->image_size > (APP_SLOT_MAX_SIZE - sizeof(ota_header_t))) {
        return false;
    }

    /* 4. Validate Full Payload Hardware CRC-32 */
    const uint8_t* payload = &s_flash[offset + sizeof(ota_header_t)];
    uint32_t calc_payload_crc = crc32_calculate(payload, hdr->image_size);

    return (calc_payload_crc == hdr->image_crc32);
}

bool bootloader_flash_ota_package(slot_id_t target_slot, const uint8_t* package_bytes, size_t package_size) {
    if (package_size < sizeof(ota_header_t)) {
        printf("[BOOTLOADER] Error: Package smaller than header size!\n");
        return false;
    }

    const ota_header_t* in_hdr = (const ota_header_t*)package_bytes;
    if (in_hdr->magic != OTA_IMAGE_MAGIC) {
        printf("[BOOTLOADER] Error: Invalid OTA Image Magic (0x%08X)!\n", in_hdr->magic);
        return false;
    }

    /* Verify payload CRC before burning to flash */
    const uint8_t* payload = package_bytes + sizeof(ota_header_t);
    uint32_t calc_crc = crc32_calculate(payload, in_hdr->image_size);
    if (calc_crc != in_hdr->image_crc32) {
        printf("[BOOTLOADER] Error: Payload CRC mismatch! In: 0x%08X, Computed: 0x%08X\n",
               in_hdr->image_crc32, calc_crc);
        return false;
    }

    /* Simulate Flash Sector Erase and Write */
    uint32_t offset = flash_offset(get_slot_addr(target_slot));
    memset(&s_flash[offset], 0xFF, APP_SLOT_MAX_SIZE);
    memcpy(&s_flash[offset], package_bytes, package_size);

    /* Update Non-Volatile Boot Metadata to Target Slot in TRIAL mode */
    boot_metadata_t* meta = get_metadata_ptr();
    meta->active_slot = target_slot;
    meta->boot_state = BOOT_STATE_TRIAL;
    meta->trial_boot_counter = 0;
    update_metadata_crc(meta);

    printf("[BOOTLOADER] Flash Successful: Staged v%u.%u.%u into %s (%u bytes). Boot state set to TRIAL.\n",
           in_hdr->ver_major, in_hdr->ver_minor, in_hdr->ver_patch,
           slot_name(target_slot), in_hdr->image_size);

    return true;
}

bool bootloader_execute_boot_decision(void) {
    boot_metadata_t* meta = get_metadata_ptr();

    if (!verify_metadata_crc(meta)) {
        printf("[BOOTLOADER CRITICAL] Metadata CRC corrupted! Restoring safe defaults.\n");
        meta->magic = BOOT_META_MAGIC;
        meta->active_slot = SLOT_A;
        meta->boot_state = BOOT_STATE_CONFIRMED;
        meta->trial_boot_counter = 0;
        update_metadata_crc(meta);
    }

    printf("\n>>> [BOOTLOADER RESET HANDLER] Evaluating Boot Decision <<<\n");
    printf("    Target: %s | State: %s | Trial Count: %u/%u\n",
           slot_name((slot_id_t)meta->active_slot),
           state_name((boot_state_t)meta->boot_state),
           meta->trial_boot_counter, meta->max_trial_boots);

    /* Check if currently in Trial Boot mode */
    if (meta->boot_state == BOOT_STATE_TRIAL) {
        if (meta->trial_boot_counter >= meta->max_trial_boots) {
            /* Watchdog timeout threshold exceeded -> AUTOMATIC ROLLBACK! */
            slot_id_t fallback_slot = get_alternate_slot((slot_id_t)meta->active_slot);
            printf("[ANTI-BRICK WATCHDOG] Exceeded max trial boots (%u). Triggering FAIL-SAFE ROLLBACK to %s!\n",
                   meta->max_trial_boots, slot_name(fallback_slot));
            
            meta->active_slot = fallback_slot;
            meta->boot_state = BOOT_STATE_ROLLBACK;
            meta->trial_boot_counter = 0;
            update_metadata_crc(meta);
        } else {
            meta->trial_boot_counter++;
            update_metadata_crc(meta);
            printf("[TRIAL BOOT] Attempt %u of %u armed with watchdog supervisor...\n",
                   meta->trial_boot_counter, meta->max_trial_boots);
        }
    }

    /* Verify selected slot integrity with CRC-32 */
    slot_id_t selected_slot = (slot_id_t)meta->active_slot;
    if (!bootloader_verify_slot(selected_slot)) {
        printf("[INTEGRITY FAILURE] Slot %s failed CRC32 audit!\n", slot_name(selected_slot));
        slot_id_t fallback = get_alternate_slot(selected_slot);
        printf("[FAIL-SAFE ACTION] Diverting execution to alternate bank %s...\n", slot_name(fallback));
        
        if (!bootloader_verify_slot(fallback)) {
            printf("[PANIC] Both firmware banks corrupted! Halting in recovery bootloader loop.\n");
            return false;
        }

        meta->active_slot = fallback;
        meta->boot_state = BOOT_STATE_CONFIRMED;
        meta->trial_boot_counter = 0;
        update_metadata_crc(meta);
        selected_slot = fallback;
    }

    /* Simulate Vector Table Relocation and Jump */
    uint32_t slot_addr = get_slot_addr(selected_slot);
    const ota_header_t* hdr = (const ota_header_t*)&s_flash[flash_offset(slot_addr)];
    
    printf("[ARM CORTEX-M VTOR] Relocating Vector Table -> SCB->VTOR = 0x%08X\n", hdr->entry_point);
    printf("[CPU CONTEXT] Setting MSP to 0x20020000 and jumping to Reset Handler at 0x%08X...\n", hdr->entry_point);
    
    const char* app_str = (const char*)&s_flash[flash_offset(slot_addr) + sizeof(ota_header_t)];
    printf("[APPLICATION RUNNING] Output: \"%.*s\"\n\n", (int)hdr->image_size, app_str);

    return true;
}

void bootloader_confirm_application_stable(void) {
    boot_metadata_t* meta = get_metadata_ptr();
    if (meta->boot_state == BOOT_STATE_TRIAL) {
        meta->boot_state = BOOT_STATE_CONFIRMED;
        meta->trial_boot_counter = 0;
        update_metadata_crc(meta);
        printf("[SELF-TEST PASS] Application called ota_confirm_boot()! State permanently promoted to CONFIRMED.\n");
    }
}

void bootloader_simulate_watchdog_crash(void) {
    printf("[WATCHDOG TIMEOUT / CRASH] Hardware Watchdog (IWDG) expired! Resetting CPU...\n");
}

void bootloader_print_status(void) {
    boot_metadata_t* meta = get_metadata_ptr();
    printf("==============================================================\n");
    printf("   DUAL-BANK OTA BOOTLOADER MEMORY & TELEMETRY STATUS\n");
    printf("==============================================================\n");
    printf("  Active Boot Slot : %s\n", slot_name((slot_id_t)meta->active_slot));
    printf("  Lifecycle State  : %s\n", state_name((boot_state_t)meta->boot_state));
    printf("  Trial Boots      : %u / %u\n", meta->trial_boot_counter, meta->max_trial_boots);
    
    printf("  Bank A Integrity : %s (v1.0.0 Golden)\n", 
           bootloader_verify_slot(SLOT_A) ? "OK [CRC32 MATCH]" : "CORRUPT");
    printf("  Bank B Integrity : %s\n", 
           bootloader_verify_slot(SLOT_B) ? "OK [CRC32 MATCH]" : "EMPTY / UNPROVISIONED");
    printf("==============================================================\n");
}
