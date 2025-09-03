#ifndef _STREAM_FIFO_H_
#define _STREAM_FIFO_H_

typedef struct {
	volatile unsigned int ISR; 
	volatile unsigned int IER;
	volatile unsigned int TDFR;
	volatile unsigned int TDFV;
	volatile unsigned int TDFD;
	volatile unsigned int TLR;
	volatile unsigned int RDFR;
	volatile unsigned int RDFO;
	volatile unsigned int RDFD;
	volatile unsigned int RLR;
	volatile unsigned int SRR;
} config_reg_t;

typedef struct {
	config_reg_t* cfg;
	void *recv_data;
	void *send_data;
}stream_fifo_reg_t;

#define OLD_AXI_FIFO 1

void stream_fifo_reset(stream_fifo_reg_t *stream_fifo_reg);
unsigned int stream_fifo_read_data_len(stream_fifo_reg_t *stream_fifo_reg);
unsigned int stream_fifo_read_data(stream_fifo_reg_t *stream_fifo_reg, unsigned char *buffer, int length);
int stream_fifo_write_data(stream_fifo_reg_t *stream_fifo_reg, unsigned char *buffer, int length);

#endif