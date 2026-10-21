/**
 * @file bootloader.h
 * @brief Fail-Safe Dual-Bank OTA Bootloader with Anti-Brick Watchdog Rollback
 * @author Amit Bakale (Avionics & Embedded Software Engineer)
 *
 * Designed for Safety-Critical ARM Cortex-M microcontrollers (STM32, NXP S32K, TI TMS570)
 * Compliant with ISO 26262 ASIL-B/D & DO-178C software integrity guidelines.
 */

#ifndef BOOTLOADER_H
#define BOOTLOADER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Magic Numbers */
#define OTA_IMAGE_MAGIC       0x4F544131U  /* "OTA1" in ASCII */
#define BOOT_META_MAGIC       0x424F4F54U  /* "BOOT" in ASCII */

/* Flash Memory Physical Layout */
#define FLASH_START_ADDR      0x08000000U
#define BOOTLOADER_ADDR       0x08000000U
#define BOOTLOADER_MAX_SIZE   (32 * 1024)       /* 32 KB */

#define SLOT_A_START_ADDR     0x08008000U       /* Active Bank A */
#define SLOT_B_START_ADDR     0x08040000U       /* Inactive / Staging Bank B */
#define APP_SLOT_MAX_SIZE     (224 * 1024)      /* 224 KB per slot */

#define METADATA_FLASH_ADDR   0x08078000U       /* Non-volatile boot status sector */

#define MAX_TRIAL_BOOT_COUNT  3                 /* Watchdog reset threshold before rollback */

/* Slot Identification */
typedef enum {
    SLOT_A = 0,
    SLOT_B = 1,
    SLOT_INVALID = 0xFF
} slot_id_t;

/* Bootloader FSM States */
typedef enum {
    BOOT_STATE_CONFIRMED = 0xAA, /* Firmware active and verified stable */
    BOOT_STATE_TRIAL     = 0x55, /* Newly flashed image undergoing trial boots */
    BOOT_STATE_ROLLBACK  = 0xEE, /* Trial failed; rolling back to alternate bank */
    BOOT_STATE_CORRUPT   = 0xFF  /* CRC32 failure or invalid magic */
} boot_state_t;

/* OTA Firmware Binary Header (Prepended to Application Binaries) */
typedef struct __attribute__((packed)) {
    uint32_t magic;           /**< Must match OTA_IMAGE_MAGIC (0x4F544131) */
    uint8_t  ver_major;       /**< Semantic Version Major */
    uint8_t  ver_minor;       /**< Semantic Version Minor */
    uint8_t  ver_patch;       /**< Semantic Version Patch */
    uint8_t  reserved;        /**< Alignment padding */
    uint32_t image_size;      /**< Application binary payload size in bytes */
    uint32_t image_crc32;     /**< CRC-32 of application binary payload */
    uint32_t entry_point;     /**< ARM Cortex-M Reset Vector / VTOR Address */
    uint32_t header_crc32;    /**< Integrity checksum of this header struct */
} ota_header_t;

/* Non-Volatile Boot Control Block */
typedef struct __attribute__((packed)) {
    uint32_t magic;              /**< Must match BOOT_META_MAGIC (0x424F4F54) */
    uint8_t  active_slot;        /**< Currently selected boot slot (SLOT_A or SLOT_B) */
    uint8_t  boot_state;         /**< Current lifecycle state (boot_state_t) */
    uint8_t  trial_boot_counter; /**< Incrementing boot counter */
    uint8_t  max_trial_boots;    /**< Maximum allowed trial boot resets */
    uint32_t crc32;              /**< CRC-32 of metadata structure */
} boot_metadata_t;

/**
 * @brief Initialize bootloader flash emulation and load non-volatile metadata
 */
void bootloader_init(void);

/**
 * @brief Flash a signed OTA firmware update package into target standby bank
 * @param target_slot Bank to receive image (SLOT_A or SLOT_B)
 * @param package_bytes Raw serialized OTA package containing header and payload
 * @param package_size Total length of package in bytes
 * @return True if flashed and verified against header CRC-32
 */
bool bootloader_flash_ota_package(slot_id_t target_slot, const uint8_t* package_bytes, size_t package_size);

/**
 * @brief Verify integrity of application bank by calculating hardware CRC-32
 */
bool bootloader_verify_slot(slot_id_t slot);

/**
 * @brief Executes the Bootloader Reset Sequence & VTOR jump logic
 * @return Returns false if boot fails and rollback triggered
 */
bool bootloader_execute_boot_decision(void);

/**
 * @brief Called by the freshly booted application to confirm successful startup
 * Clears TRIAL state to CONFIRMED and resets watchdog counter.
 */
void bootloader_confirm_application_stable(void);

/**
 * @brief Simulates an unhandled crash / watchdog reset during application startup
 */
void bootloader_simulate_watchdog_crash(void);

/**
 * @brief Prints memory map and bank status to console
 */
void bootloader_print_status(void);

#ifdef __cplusplus
}
#endif

#endif /* BOOTLOADER_H */
