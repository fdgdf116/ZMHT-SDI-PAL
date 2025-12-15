#ifndef FIFO_ENGINE_H_
#define FIFO_ENGINE_H_


#ifdef __cplusplus
extern "C" {
#endif

#define RECV_PAK_OK    0x55
#define RECV_PAK_ERROR 0x5a
#define COMD_PROC_SUCCESS   0xaa
#define COMD_PROC_FAILED    0xa5

int axififo_data_init(void);
void axififo_data_destory(void);

static void axififo_send(unsigned char mode, unsigned int data);
int axi_fifo_write_buf(unsigned char *buffer, int length);
int axi_fifo_write_file_buf(FILE *tree_fp,int mode);
unsigned char check_sum(unsigned char* buf, int len);
int DecodeStop(int chn);
int EncodeStop(int chn);
int run_test();

void axififo_frame_data_analyse(unsigned char* frame_buffer, unsigned int frame_size);
#ifdef __cplusplus
}
#endif
#endif /* GPU_GPU_INTERFACE_H_ */
