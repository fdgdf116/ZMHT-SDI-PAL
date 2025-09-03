/*
 * This file is part of the Xilinx DMA IP Core driver tools for Linux
 *
 * Copyright (c) 2016-present,  Xilinx, Inc.
 * All rights reserved.
 *
 * This source code is licensed under BSD-style license (found in the
 * LICENSE file in the root directory of this source tree)
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <byteswap.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/mman.h>
#include "log.h"

#include "stream_fifo.h"

/* ltoh: little endian to host */
/* htol: host to little endian */
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define ltohl(x)       (x)
#define ltohs(x)       (x)
#define htoll(x)       (x)
#define htols(x)       (x)
#elif __BYTE_ORDER == __BIG_ENDIAN
#define ltohl(x)     __bswap_32(x)
#define ltohs(x)     __bswap_16(x)
#define htoll(x)     __bswap_32(x)
#define htols(x)     __bswap_16(x)
#endif
/*
 * This file is part of the Xilinx DMA IP Core driver tools for Linux
 *
 * Copyright (c) 2016-present,  Xilinx, Inc.
 * All rights reserved.
 *
 * This source code is licensed under BSD-style license (found in the
 * LICENSE file in the root directory of this source tree)
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <byteswap.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/mman.h>
// #include "log.h"

/* ltoh: little endian to host */
/* htol: host to little endian */
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define ltohl(x)       (x)
#define ltohs(x)       (x)
#define htoll(x)       (x)
#define htols(x)       (x)
#elif __BYTE_ORDER == __BIG_ENDIAN
#define ltohl(x)     __bswap_32(x)
#define ltohs(x)     __bswap_16(x)
#define htoll(x)     __bswap_32(x)
#define htols(x)     __bswap_16(x)
#endif

#define PCIE_CONTROL_DEV_0   "/dev/xdma0_user"        //bar0
#define PCIE_CONTROL_DEV_1   "/dev/xdma0_control" //bar1
#define PCIE_CONTROL_DEV_2   "/dev/xdma0_bypass"  //bar2
// #include "pcie_impl.h"
#include "pcie_reg_rw.h"

static int gs_regrw_fd[3] = {0};
static int bar_device_num = 2;

#define PCIE_GET_TIME_OFFSE_H (0x0C)
#define PCIE_GET_TIME_OFFSE_L (0x10)

static void* timestamp_addr;
unsigned char bar_device[20];

void SetBarDevice(int num){
	bar_device_num = num;
}
unsigned char *GetBarDevice(){
	
	switch (bar_device_num)
	{
	case 0:
		sprintf(bar_device,PCIE_CONTROL_DEV_0);
		break;
	case 1:
		sprintf(bar_device,PCIE_CONTROL_DEV_1);
		break;
	case 2:
		sprintf(bar_device,PCIE_CONTROL_DEV_2);
		break;
	default:
		break;
	}
	return bar_device;
}

static uint32_t pcie_reg_rd32(void* addr)
{
    uint32_t regVal = *(volatile uint32_t *) addr;
    return (regVal);
}


static void pcie_reg_wr32(void* addr, uint32_t value)
{
    *(volatile uint32_t *) addr = value;
    return;
}
static uint8_t pcie_reg_rd8(void* addr)
{
    uint8_t regVal = *(volatile uint8_t *) addr;
    return (regVal);
}


static void pcie_reg_wr8(void* addr, uint8_t value)
{
    *(volatile uint8_t *) addr = value;
    return;
}
#define PCIE_BAR_WRITE_REG(BaseAddress, RegOffset, Data) \
	pcie_reg_wr32((BaseAddress) + (RegOffset), (uint32_t)(Data))

#define PCIE_BAR_WRITE_UNIT8(BaseAddress, RegOffset, Data) \
	pcie_reg_wr8((BaseAddress) + (RegOffset), (uint8_t)(Data))

#define PCIE_BAR_READ_REG_UNIT8(BaseAddress, RegOffset) \
	pcie_reg_rd8((BaseAddress) + (RegOffset))

#define PCIE_BAR_READ_REG(BaseAddress, RegOffset) \
	pcie_reg_rd32((BaseAddress) + (RegOffset))

void pcie_write_buf(void* addr, unsigned int offset, unsigned char * buf,unsigned int len){
	int i = 0;
	int buf_len = len;
	for(i = 0; i < buf_len; i++){
		PCIE_BAR_WRITE_UNIT8(addr,offset + i,buf[i]);
	}
}

void pcie_read_buf(void* addr, unsigned int offset, unsigned char * buf,unsigned int len){
	int i = 0;
	int buf_len = len;
	for(i = 0; i < buf_len; i++){
		buf[i] = PCIE_BAR_READ_REG_UNIT8(addr,offset + i);
	}
}
int pcie_bar_open(int bar_x)
{
	SetBarDevice(bar_x);
	const char *device = GetBarDevice();
	printf("device name :%s\n",device);
	if ((gs_regrw_fd[bar_x] = open(device, O_RDWR | O_SYNC)) == -1) {
		printf("character device %s opened failed: %s.\n",
			device, strerror(errno));
		return -errno;
	}
	return 0;
}

void* pcie_bar_mmap(unsigned int phy_address, unsigned int size,int bar_num)
{
	void* user_addr;
	if(size <= 0){
		printf("phy:0x%x mmap size 0x%x error \n", phy_address, size);
		return NULL;
	}
	user_addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, gs_regrw_fd[bar_num],
		       	phy_address);
	if (user_addr == (void *)-1) {
		printf("Memory 0x%lx mapped failed: %s.\n",
			phy_address, strerror(errno));
		//goto close;
		return NULL;
	}

	printf("Memory 0x%lx mapped len:0x%x at address %p.\n", phy_address, size, user_addr);

	return user_addr;
}

unsigned int pcie_bar_read_reg(void* addr, unsigned int offset)
{
	return PCIE_BAR_READ_REG(addr, offset);
}

void pcie_bar_write_reg(void* addr, unsigned int offset, unsigned int value)
{
	PCIE_BAR_WRITE_REG(addr,offset,value);
	return ;
}

void pcie_bar_munmap(void* user_addr, unsigned int size)
{
	if(user_addr != NULL){
		if (munmap(user_addr,size) == -1) {
			printf("Memory 0x%lx mapped failed: %s.\n",
				0, strerror(errno));
		}
	}
	return ;
}

void pcie_bar_close(void)
{
	close(gs_regrw_fd[0]);
	close(gs_regrw_fd[2]);
	return ;
}

void set_pcie_timestamp_addr(void* addr)
{
	timestamp_addr = addr;
}

void get_pcie_timestamp(pcie_timestamp_t* timestamp)
{
	if(!timestamp_addr){
		timestamp->time_h = 0;
		timestamp->time_l = 0;
		return ;
	}

	timestamp->time_h = pcie_bar_read_reg(timestamp_addr, PCIE_GET_TIME_OFFSE_H);
	timestamp->time_l = pcie_bar_read_reg(timestamp_addr, PCIE_GET_TIME_OFFSE_L);
}

// static uint64_t pcie_diff_timestamp(pcie_timestamp_t *end_tm, pcie_timestamp_t *start_tm)
// {
//     uint64_t time, time_end, time_start;

//     time_start = ((uint64_t)start_tm->time_h << 32) | start_tm->time_l;

//     time_end = ((uint64_t)end_tm->time_h << 32) | end_tm->time_l;

// 	time = (time_end - time_start) * 7;

// 	return time;
// }

unsigned long pcie_diff_timestamp(pcie_timestamp_t *end_tm, pcie_timestamp_t *start_tm)
{

	unsigned long time, time_end, time_start;

	time_start = ((unsigned long)start_tm->time_h << 32) | start_tm->time_l;
	time_end = ((unsigned long)end_tm->time_h << 32) | end_tm->time_l;
	time = (unsigned long)((time_end - time_start) * 6.6);

	return time;
}

int test_main(int argc,char **argv){
	int base_address = 0x44d00000;
	int reg_address = 0xA1000000;
	int bram_address[12];
	if(argc != 2){
		printf("usage: [%s] [the number of bram]]\n",argv[0]);
		return -1;
	}
	int num = atoi(argv[1]);
	
	for(int i = 0; i<12; i++){
		bram_address[i] = base_address + i * 0x10000;
	}
	pcie_bar_open(2);
	char * bram_pt = pcie_bar_mmap(bram_address[num],40*1024,2);
	// char * bar_pt = pcie_bar_mmap(reg_address,0x10000);
	// pcie_bar_write_reg(bar_pt ,8,0x11010101);
	// int ret = pcie_bar_read_reg(bar_pt,8);
	FILE *fp = fopen("testfile","r+");
	FILE *fp1 = fopen("output","w+");
	if(fp == NULL){
		printf("open file error!\n");
		return -1;
	}
	int len = fread(bram_pt,1,40*1024,fp);
	if(len != 40*1024){
		printf("read file error!\n");
		goto error;
	}
	fwrite(bram_pt,1,40*1024,fp1);
	// printf("pcie read bar2 reg value : %x\n",ret);
	pcie_bar_close();
	pcie_bar_munmap(bram_pt,40*1024);

	fclose(fp);
	fclose(fp1);
	
	return 0;
	// pcie_bar_munmap(bar_pt,0x10000);
	error:
	pcie_bar_close();
	pcie_bar_munmap(bram_pt,40*1024);
	fclose(fp);
	fclose(fp1);
	system("md5sum testfile;md5sum output");
	// pcie_bar_munmap(bar_pt,0x10000);
}