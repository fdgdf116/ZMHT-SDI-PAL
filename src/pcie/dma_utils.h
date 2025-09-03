/*
 * File      : dma_utils.h
 * This file is pcie dma utils header file  
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-09-22     fengyong    first version
 */

#ifndef _DMA_UTILS_H
#define _DMA_UTILS_H

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

ssize_t read_to_buffer(int fd, char *buffer, uint64_t size,uint64_t base,int ch_index);
ssize_t write_from_buffer(int fd, uint8_t *buffer, uint64_t size, uint64_t base, int ch_index);

#ifdef __cplusplus
}
#endif

#endif // _DMA_UTILS_H
