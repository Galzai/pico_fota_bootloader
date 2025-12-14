#ifndef PICO_FOTA_BOOTLOADER_HANDLERS
#define PICO_FOTA_BOOTLOADER_HANDLERS

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * Bootloader status codes for on_boot_completed handler.
 */
typedef enum {
    BOOT_STATUS_OK = 0,        // Normal boot, nothing swapped
    BOOT_STATUS_SWAP = 1,      // Firmware swapped
    BOOT_STATUS_ROLLBACK = 2   // Rollback performed
} boot_status_t;

/**
 * Optional handler called when the bootloader starts.
 * Define this function in your application to run custom code at bootloader start.
 */
__attribute__((weak)) void on_bootloader_started(void);

/**
 * Optional handler called when the bootloader completes.
 * Define this function in your application to run custom code at bootloader end.
 * @param status boot_status_t value indicating the result
 */
__attribute__((weak)) void on_boot_completed(boot_status_t status);

/**
 * Optional handler called during flash swap operation.
 * Define this function in your application to track progress.
 * @param current_sector Current sector being swapped (0 to total_sectors-1)
 * @param total_sectors Total number of sectors to swap
 */
__attribute__((weak)) void on_flash_operation_progress(uint32_t current_sector, uint32_t total_sectors);

#ifdef __cplusplus
}
#endif

#endif /* PICO_FOTA_BOOTLOADER_HANDLERS */
