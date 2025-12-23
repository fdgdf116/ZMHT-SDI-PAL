/*
 * File      : pcie_impl.h
 * This file is pcie impl header file  
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-09-21     fengyong    first version
 */

#ifndef _PCIE_IMPL_H
#define _PCIE_IMPL_H

#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PCIE_C2H_0 = 0,
    PCIE_C2H_1 = 1,
    PCIE_C2H_MAX
} pcie_c2h_t;

typedef enum {
    PCIE_H2C_0 = 0,
    PCIE_H2C_1 = 1,
    PCIE_H2C_MAX
} pcie_h2c_t;

int pcie_impl_init(void);
void pcie_impl_destory(void);

uint8_t *align_read_buf_get(void);
uint8_t *align_over_buf_get(void);
//uint8_t *align_write_buf_get(void);
 uint8_t *align_write_mpp_buf_get(unsigned char index);
uint8_t *align_write_buf_get(unsigned char index);

uint8_t *align_chn_reset_buf_get(void);

ssize_t dma_memory_mapped_read(uint8_t *buf, uint64_t dma_addr, uint64_t size, pcie_c2h_t ch);
ssize_t dma_memory_mapped_write(uint8_t *buf, uint64_t dma_addr, uint64_t size, pcie_h2c_t ch);

#ifdef __cplusplus
}
#endif

#endif // _PCIE_IMPL_H
