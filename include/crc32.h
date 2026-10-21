/**
 * @file crc32.h
 * @brief High-Speed Hardware/Software CRC-32 (IEEE 802.3 Ethernet Polynomial 0x04C11DB7)
 * @author Amit Bakale (Avionics & Embedded Software Engineer)
 */

#ifndef CRC32_H
#define CRC32_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Computes standard IEEE 802.3 CRC-32 over memory buffer
 * @param data Pointer to input byte array
 * @param length Size of buffer in bytes
 * @return 32-bit CRC checksum
 */
uint32_t crc32_calculate(const uint8_t* data, size_t length);

/**
 * @brief Incremental CRC-32 update function
 */
uint32_t crc32_update(uint32_t crc, const uint8_t* data, size_t length);

#ifdef __cplusplus
}
#endif

#endif /* CRC32_H */
