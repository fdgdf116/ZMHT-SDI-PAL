#include <stdint.h>
#include <stdint.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <poll.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <memory.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <arpa/inet.h>
#include <inttypes.h>

#include "stream_fifo.h"
#include "log.h"

static uint32_t stream_fifo_rd32(void* addr)
{
    uint32_t regVal = *(volatile uint32_t *) addr;
    return (regVal);
}

static void stream_fifo_wr32(void* addr, uint32_t value)
{
    *(volatile uint32_t *) addr = value;
    return;
}


void stream_fifo_reset(stream_fifo_reg_t *stream_fifo_reg)
{
	stream_fifo_reg->cfg->TDFR = 0xa5;
	stream_fifo_reg->cfg->RDFR = 0xa5;
	stream_fifo_reg->cfg->SRR = 0xa5;
}

unsigned int stream_fifo_read_data_len(stream_fifo_reg_t *stream_fifo_reg)
{

	unsigned int stream_fifo_inlen = 0;
#if 1
 	volatile uint32_t read_start = 0;
    read_start = stream_fifo_reg->cfg->RDFO;
    if(read_start > 0){
        stream_fifo_inlen = stream_fifo_reg->cfg->RLR;
        stream_fifo_inlen &= 0x007FFFFF;
        stream_fifo_inlen = stream_fifo_inlen / 4;
    }
#else

	stream_fifo_inlen = stream_fifo_reg->RLR;
	stream_fifo_inlen &= 0x007FFFFF;
	stream_fifo_inlen = stream_fifo_inlen / 4;

#endif
	return stream_fifo_inlen;
}

unsigned int stream_fifo_read_data(stream_fifo_reg_t *stream_fifo_reg, unsigned char *buffer, int length)
{
	int i = 0;
	unsigned int data;
	for(i = 0; i < length; i++) { //0x100
#if OLD_AXI_FIFO
		data = stream_fifo_rd32(stream_fifo_reg->recv_data);
#else
		data = stream_fifo_reg->cfg->RDFD;
#endif
		buffer[i]= (data & 0xff);
	}
    return i;
}

int stream_fifo_write_data(stream_fifo_reg_t *stream_fifo_reg, unsigned char *buffer, int length)
{
	int i = 0;
	unsigned int tmp = 0;

	if(stream_fifo_reg->cfg->TDFV){
		for(i = 0; i < length; i++) {
			tmp = buffer[i];
#if OLD_AXI_FIFO
			stream_fifo_wr32(stream_fifo_reg->send_data, tmp);
#else
			stream_fifo_reg->cfg->TDFD = tmp;
#endif
			printf("%x ",tmp);
		}
		printf("\n");
        stream_fifo_reg->cfg->TLR=length*4;
	}
	
	return i;
}
