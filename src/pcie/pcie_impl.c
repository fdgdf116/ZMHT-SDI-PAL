/*
 * File      : dma_from_device.c
 * This file is pcie read data from device with DMA  
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-09-21     fengyong    first version
 */

#include <fcntl.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <pthread.h>

#include "cdev_sgdma.h"
#include "dma_utils.h"
#include "pcie_impl.h"
#include "vivado_common.h"

struct pcie_impl_info {
	int fd_r[PCIE_C2H_MAX];	
	int fd_w[PCIE_H2C_MAX];	
};

static const uint32_t s_pcie_buf_size_max = (80*1024*1024);
 static const uint32_t s_pcie_mpp_w_buf_size_max = (1024*1024);
static const uint32_t s_pcie_w_buf_size_max = (5120*5120*3*(RINGBUFFER_FRAME_MAX+1));

static const char *xdma_c2h_dev = "/dev/xdma0_c2h_0";
static const char *xdma_h2c_dev = "/dev/xdma0_h2c_0";
static struct pcie_impl_info *g_pcie_info = NULL;

static uint8_t *align_buf_r = NULL;
static uint8_t *align_buf_w[DATA_CHANNEL_NUM_MAX] = {NULL};
 static uint8_t *align_buf_mpp_w[MPP_ENCODE_NTHREAD_MAX] = {NULL};
static pthread_mutex_t send_mutex;


static uint8_t *align_chn_reset_buf = NULL;


extern 
int pcie_impl_init(void) 
{
	int ret = 0;
	int fd_r = 0;
	int fd_w = 0;
	int index;
	struct pcie_impl_info *info = NULL;

	pthread_mutex_init(&send_mutex,NULL);

	info = (struct pcie_impl_info *)malloc(sizeof(struct pcie_impl_info));
	if (!info) {
		fprintf(stderr, "malloc pcie_impl_info failed.\n");
		ret = -1;
		goto out;
	}
	
	fd_r = open(xdma_c2h_dev, O_RDWR);
	if (fd_r < 0) {
		fprintf(stderr, "unable to open device %s, %d.\n",xdma_c2h_dev, fd_r);
		perror("open device");
		ret = -2;
		goto out;
	}

	fd_w = open(xdma_h2c_dev, O_RDWR);
	if (fd_w < 0) {
		fprintf(stderr, "unable to open device %s, %d.\n",xdma_h2c_dev, fd_w);
		perror("open device");
		ret = -2;
		goto out;
	}

	posix_memalign((void **)&align_buf_r, 4096 /*alignment */ , s_pcie_buf_size_max + 4096);
	if (!align_buf_r) {
		fprintf(stderr, "OOM %lu.\n", s_pcie_buf_size_max + 4096);
		ret = -1;
		goto out;
	}

	for(index = 0; index < DATA_CHANNEL_NUM_MAX; index++){
		posix_memalign((void **)&align_buf_w[index], 4096 /*alignment */ , s_pcie_w_buf_size_max + 4096);
		if (!align_buf_w) {
			fprintf(stderr, "OOM %lu.\n", s_pcie_w_buf_size_max + 4096);
			ret = -1;
			goto out;
		}
	}

	 for(index = 0; index < MPP_ENCODE_NTHREAD_MAX; index++){
		 posix_memalign((void **)&align_buf_mpp_w[index], 4096 /*alignment */ , s_pcie_mpp_w_buf_size_max + 4096);
		 if (!align_buf_w) {
			 fprintf(stderr, "OOM %lu.\n", s_pcie_mpp_w_buf_size_max + 4096);
			 ret = -1;
			 goto out;
		 }
	 }

	posix_memalign((void **)&align_chn_reset_buf, 4096 /*alignment */ , s_pcie_w_buf_size_max + 4096);
	if (!align_chn_reset_buf) {
		fprintf(stderr, "OOM %lu.\n", s_pcie_w_buf_size_max + 4096);
		ret = -1;
		goto out;
	}


	info->fd_r[0] = fd_r;
	info->fd_w[0] = fd_w;

	g_pcie_info = info;

	printf("%s OK\n",__func__);

	return 0;

out:
	if (info) {
		free(info);
	}

	if (fd_r) {
		close(fd_r);
	}

	if (fd_w) {
		close(fd_w);
	}

	if (align_buf_r) {
		free(align_buf_r);
	}

	for(index = 0; index < DATA_CHANNEL_NUM_MAX; index++){
		if (align_buf_w[index]) {
			free(align_buf_w[index]);
		}
	}

	if(align_chn_reset_buf){
		free(align_chn_reset_buf);
	}


	return ret;
}

uint8_t *align_chn_reset_buf_get(void)
{
	return align_chn_reset_buf;
}


void pcie_impl_destory(void)
{
	struct pcie_impl_info *info = g_pcie_info;

	if(align_chn_reset_buf){
		free(align_chn_reset_buf);
		align_chn_reset_buf = NULL;
	}

	if (align_buf_r) {
		free(align_buf_r);
		align_buf_r = NULL;
	}

	for(int index = 0; index < DATA_CHANNEL_NUM_MAX; index++){
		if (align_buf_w[index]) {
			free(align_buf_w[index]);
			align_buf_w[index] = NULL;
		}
	}

	for(int index = 0; index < MPP_ENCODE_NTHREAD_MAX; index++){
		if (align_buf_mpp_w[index]) {
			free(align_buf_mpp_w[index]);
			align_buf_mpp_w[index] = NULL;
		}
	}

	if (info) {
	
		close(info->fd_r[0]);
		close(info->fd_w[0]);

		free(info);
	}

	printf("%s OK\n",__func__);
}

uint8_t *align_read_buf_get(void)
{
	return align_buf_r;
}

 uint8_t *align_write_buf_get(unsigned char index)
 {
	 return align_buf_w[index];
 }
 
 uint8_t *align_write_mpp_buf_get(unsigned char index)
 {
	 return align_buf_mpp_w[index];
 }

ssize_t dma_memory_mapped_read(uint8_t *buf, uint64_t dma_addr, uint64_t size, pcie_c2h_t ch)
{
    ssize_t rc = 0;
    struct pcie_impl_info *info = g_pcie_info;
	size_t bytes_done = 0;
	uint64_t addr = dma_addr;

    rc = read_to_buffer(info->fd_r[ch], buf, size, addr,ch);
	if (rc < 0) {
		goto out;
	}
		
	bytes_done = rc;

out:

	return rc;	
}

ssize_t dma_memory_mapped_write(uint8_t *buf, uint64_t dma_addr, uint64_t size, pcie_h2c_t ch)
{
    ssize_t rc = 0;
    struct pcie_impl_info *info = g_pcie_info;
	size_t bytes_done = 0;
	uint64_t addr = dma_addr;

   //ssize_t write_from_buffer(int fd, uint8_t *buffer, uint64_t size, uint64_t base, int ch_index);
	pthread_mutex_lock(&send_mutex);
    rc = write_from_buffer(info->fd_w[ch], buf, size, addr,ch);
	pthread_mutex_unlock(&send_mutex);
	if (rc < 0) {
		goto out;
	}
	usleep(10);
		
	bytes_done = rc;

out:

	return rc;	
}


