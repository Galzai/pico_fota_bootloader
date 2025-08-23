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

#ifdef __cplusplus
}
#endif

#endif /* PICO_FOTA_BOOTLOADER_HANDLERS */
