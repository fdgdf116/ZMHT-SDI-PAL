#ifndef __RGA_ENGINE_H__
#define __RGA_ENGINE_H__

#ifdef __cplusplus
extern "C" {
#endif

unsigned int rga_chn_image_init(u_int8_t chn);
int rga_pthread_chn_destory(unsigned char chn);
void data_stream_chn_data_destory(unsigned char chn);
void* rga_deal(void* arg);

#ifdef __cplusplus
}
#endif

#endif
