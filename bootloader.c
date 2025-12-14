/*
 * Copyright (c) 2024 Jakub Zimnol
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <string.h>

#ifdef RP2350
#include <RP2350.h>
#else
#include <RP2040.h>
#endif

#include "pico/flash.h"
#include <hardware/flash.h>
#include <hardware/resets.h>
#include <hardware/sync.h>
#include <pico/stdlib.h>

#include <pico_fota_bootloader.h>
#include "pico_fota_bootloader_handlers.h"
#include <stdlib.h>

#include "linker_common/linker_definitions.h"

#ifdef PFB_WITH_BOOTLOADER_LOGS
#    define BOOTLOADER_LOG(...)                \
        do {                                   \
            puts("[BOOTLOADER] " __VA_ARGS__); \
            sleep_ms(5);                       \
        } while (0)
#else // PFB_WITH_BOOTLOADER_LOGS
#    define BOOTLOADER_LOG(...) ((void) 0)
#endif // PFB_WITH_BOOTLOADER_LOGS

void _pfb_mark_pico_has_new_firmware(void);
void _pfb_mark_pico_has_no_new_firmware(void);
void _pfb_mark_is_after_rollback(void);
void _pfb_mark_is_not_after_rollback(void);
bool _pfb_should_rollback(void);
void _pfb_mark_should_rollback(void);
bool _pfb_has_firmware_to_swap(void);
static void swap_images_unsafe(void* param_data);

typedef struct {
    uint8_t* swap_buff_from_download_slot;
    uint8_t* swap_buff_from_application_slot;
    uint32_t iteration_index;
} swap_params_t;

static void swap_images(void) {
    uint8_t swap_buff_from_download_slot[FLASH_SECTOR_SIZE];
    uint8_t swap_buff_from_application_slot[FLASH_SECTOR_SIZE];
    const uint32_t SWAP_ITERATIONS =
            PFB_ADDR_AS_U32(__FLASH_SWAP_SPACE_LENGTH) / FLASH_SECTOR_SIZE;

    swap_params_t params;
    params.swap_buff_from_download_slot = swap_buff_from_download_slot;
    params.swap_buff_from_application_slot = swap_buff_from_application_slot;

    for (uint32_t i = 0; i < SWAP_ITERATIONS; i++) {
        on_flash_operation_progress(i, SWAP_ITERATIONS);
        params.iteration_index = i;
        flash_safe_execute(swap_images_unsafe, &params, UINT32_MAX);
    }
}

static inline void swap_images_unsafe(void* param_data) {
    swap_params_t* params = (swap_params_t*)param_data;
    uint8_t* swap_buff_from_download_slot = params->swap_buff_from_download_slot;
    uint8_t* swap_buff_from_application_slot = params->swap_buff_from_application_slot;
    uint32_t i = params->iteration_index;

    memcpy(swap_buff_from_download_slot,
           (void *) (PFB_ADDR_AS_U32(__FLASH_DOWNLOAD_SLOT_START)
                     + i * FLASH_SECTOR_SIZE),
           FLASH_SECTOR_SIZE);
    memcpy(swap_buff_from_application_slot,
           (void *) (PFB_ADDR_AS_U32(__FLASH_APP_START)
                     + i * FLASH_SECTOR_SIZE),
           FLASH_SECTOR_SIZE);
    flash_range_erase(PFB_ADDR_WITH_XIP_OFFSET_AS_U32(__FLASH_APP_START)
                              + i * FLASH_SECTOR_SIZE,
                      FLASH_SECTOR_SIZE);
    flash_range_erase(PFB_ADDR_WITH_XIP_OFFSET_AS_U32(
                              __FLASH_DOWNLOAD_SLOT_START)
                              + i * FLASH_SECTOR_SIZE,
                      FLASH_SECTOR_SIZE);
    flash_range_program(PFB_ADDR_WITH_XIP_OFFSET_AS_U32(__FLASH_APP_START)
                                + i * FLASH_SECTOR_SIZE,
                        swap_buff_from_download_slot,
                        FLASH_SECTOR_SIZE);
    flash_range_program(PFB_ADDR_WITH_XIP_OFFSET_AS_U32(
                                __FLASH_DOWNLOAD_SLOT_START)
                                + i * FLASH_SECTOR_SIZE,
                        swap_buff_from_application_slot,
                        FLASH_SECTOR_SIZE);
}

static void _disable_interrupts(void) {
    SysTick->CTRL &= ~1;

    NVIC->ICER[0] = 0xFFFFFFFF;
    NVIC->ICPR[0] = 0xFFFFFFFF;
}

static void reset_peripherals(void) {
    reset_block(~(RESETS_RESET_IO_QSPI_BITS | RESETS_RESET_PADS_QSPI_BITS
                  | RESETS_RESET_SYSCFG_BITS | RESETS_RESET_PLL_SYS_BITS));
}

static void jump_to_vtor(uint32_t vtor) {
    // Derived from the Leaf Labs Cortex-M3 bootloader.
    // Copyright (c) 2010 LeafLabs LLC.
    // Modified 2021 Brian Starkey <stark3y@gmail.com>
    // Originally under The MIT License

    uint32_t reset_vector = *(volatile uint32_t *) (vtor + 0x04);
    SCB->VTOR = (volatile uint32_t)(vtor);

    asm volatile("msr msp, %0" ::"g"(*(volatile uint32_t *) vtor));
    asm volatile("bx %0" ::"r"(reset_vector));
}

static void print_welcome_message(void) {
#ifdef PFB_WITH_BOOTLOADER_LOGS
    uint32_t space = PFB_ADDR_AS_U32(__FLASH_SWAP_SPACE_LENGTH) / 1024;
    puts("");
    puts("***********************************************************");
    puts("*                                                         *");
    puts("*           Raspberry Pi Pico (2)W FOTA Bootloader        *");
    puts("*             Copyright (c) 2024 Jakub Zimnol             *");
    puts("*                                                         *");
    puts("***********************************************************");
    puts("");
    printf("Maximum code length: %luK\r\n", space);
#endif // PFB_WITH_BOOTLOADER_LOGS
}

__attribute__((weak)) void on_bootloader_started(void) {}
__attribute__((weak)) void on_boot_completed(boot_status_t status) { (void)status; }
__attribute__((weak)) void on_flash_operation_progress(uint32_t current_sector, uint32_t total_sectors) {
    (void)current_sector;
    (void)total_sectors;
}

int main(void) {
    stdio_init_all();
    print_welcome_message();
    on_bootloader_started();

    boot_status_t status = BOOT_STATUS_OK;
    if (_pfb_should_rollback()) {
        BOOTLOADER_LOG("Rolling back to the previous firmware");
        swap_images();
        pfb_firmware_commit();
        _pfb_mark_pico_has_no_new_firmware();
        _pfb_mark_is_after_rollback();
        status = BOOT_STATUS_ROLLBACK;
    } else if (_pfb_has_firmware_to_swap()) {
        BOOTLOADER_LOG("Swapping images");
        swap_images();
        _pfb_mark_pico_has_new_firmware();
        _pfb_mark_is_not_after_rollback();
        _pfb_mark_should_rollback();
        status = BOOT_STATUS_SWAP;
    } else {
        BOOTLOADER_LOG("Nothing to swap");
        pfb_firmware_commit();
        _pfb_mark_pico_has_no_new_firmware();
        status = BOOT_STATUS_OK;
    }


    pfb_mark_download_slot_as_invalid();
    on_boot_completed(status);
    BOOTLOADER_LOG("End of execution, executing the application...\n");

    _disable_interrupts();
    reset_peripherals();
    jump_to_vtor(__flash_info_app_vtor);

    return 0;
}
