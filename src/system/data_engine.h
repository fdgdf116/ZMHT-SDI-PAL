/*
 * File      : data_engine.h
 * This file is vide data engine interface header  
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-1-4     fengyong    first version
 * 2021-6-22    fengyong    modify for CSI Camera and IR Camera 
 */

#ifndef _DATA_MANAGER_H
#define _DATA_MANAGER_H

#include <stdio.h>
#include <stdlib.h>
#include "ringbuffer.h"
#include "rga_interface.h"
#include "vivado_common.h"
#include "rk_type.h"
#include "mpi_decode.h"

#ifdef __cplusplus

extern "C" {
#endif

typedef struct {
    uint32_t data_offset; /* 在一帧图像中的行偏移 */
    uint32_t frame_size;
    uint8_t rb_offset;
    uint8_t resize_rb_offset;
    uint8_t rga_rb_offset;
    RGA_IMAGE_S yuv420_data[RINGBUFFER_FRAME_MAX+1];
    RGA_IMAGE_S rgb888_data;
    RGA_IMAGE_S rotate_data;
    // RGA_IMAGE_S yuv422_data[RINGBUFFER_FRAME_MAX]; 
}chn_data_info_t;
typedef struct {
    unsigned char mpp_chn;
    MppCodingType decode_type;
    u_int32_t fps;           /* 图像的帧率 */
	unsigned char position;         /*字符叠加位置*/
    u_int32_t width;
    u_int32_t height;
    int decode_state;
    MpiDecLoopData dec_data;
}mpp_info_t ;
typedef struct {
    u_int16_t rga_flag;
    u_int16_t out_color;
    u_int16_t pixel_byte;
    u_int16_t src_width;
    u_int16_t src_height;
    u_int16_t rotate_width;
    u_int16_t rotate_height;
    u_int16_t resize_width;
    u_int16_t resize_height;
    u_int16_t image_w0;             /*叠加图像的宽*/
    u_int16_t image_y0;             /*叠加图像的高*/
    u_int16_t osd_width;
    u_int16_t osd_height;
    u_int16_t out_width;
    u_int16_t out_height;
    u_int32_t rotate_type;
    u_int32_t resize;
    u_int16_t crop_flag; /*裁剪标志*/
}rga_param_t ;
typedef struct {
    u_int16_t send_flag;
    u_int16_t crop_flag;
}pcie_info_t ;
typedef struct {
    pthread_t pht_dec;               /* 解码线程句柄 */
    pthread_t pht_rga;
    pthread_t pht_osd;               /* 线程句柄 */
    pthread_t pht_up;
    pthread_t pht_crop;

    mpp_info_t mpp_info;
    rga_param_t rga_info;
    pcie_info_t pcie_info;
    osd_image_info_t osd_info;
}image_chn_info_t;
typedef struct {
    unsigned char chn;
    unsigned int frame_num;
    unsigned int rb_offset;
    unsigned char data_frame_max;
    unsigned char* rga_vir_addr;
	unsigned int rga_addr_offset;
	unsigned int rga_addr_offset_max;

    ring_buffer_t *rga_data_rb;
    ring_buffer_t *resize_data_rb;
    ring_buffer_t *pcie_data_rb;
    chn_data_info_t chn_data;
}fpga_chn_image_info_t;

typedef struct {
    unsigned char image_over_flag;
	pthread_mutex_t osd_lock;
	pthread_mutex_t rga_lock;

	image_chn_info_t* image_chn_info[DATA_CHANNEL_NUM_MAX];
	fpga_chn_image_info_t *chn_image[DATA_CHANNEL_NUM_MAX];
}data_stream_t;

data_stream_t *data_stream_get(void);
int rga_test_main();
int RingBufferCreate(int width,int height);

ring_buffer_t *GetInPutRingBufferPt(int chn);
ring_buffer_t *GetResizeRingBufferPt(int chn);
ring_buffer_t *GetOutPutRingBufferPt(int chn);

void MpiDecDestroy();
int data_stream_init(void);
void data_stream_destory(void);
void *thread_writePL(void *arg);
int writePL_chn_init(unsigned char chn);
int writePL_chn_destory(unsigned char chn);

#ifdef __cplusplus
}
#endif

#endif /* DATA_MANAGER_H */
