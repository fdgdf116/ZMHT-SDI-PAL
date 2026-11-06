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
    pthread_t pht_picture;
    u_int32_t chn;
    u_int32_t weidth;
    u_int32_t height;
    u_int32_t fps;
    u_int32_t send_flag;
    unsigned char picture_created;
    u_int8_t* pitture_buf;
    FILE* pitture_fp;
    char picture_name[256];
} picture_info_t;

typedef struct {
    uint32_t data_offset; /* 在一帧图像中的行偏移 */
    uint32_t frame_size;
    uint8_t rb_offset;
    uint8_t resize_rb_offset;
    uint8_t rga_rb_offset;
    RGA_IMAGE_S yuv420_data[RINGBUFFER_FRAME_MAX+1];
    RGA_IMAGE_S rgb888_data;
    RGA_IMAGE_S rotate_data;
    // RGA_IMAGE_S yuv422_data[RINGBUFFER_FRAME_MAX+1]; 
}chn_data_info_t;
typedef struct {
    pthread_t pht_dec;               /* 解码线程句柄 */
    unsigned char mpp_chn;
    MppCodingType decode_type;
    u_int32_t fps;           /* 图像的帧率 */
	unsigned char position;         /*字符叠加位置*/
    u_int32_t width;
    u_int32_t height;
    int decode_state;
    unsigned char decode_thread_created;
    MpiDecLoopData dec_data;
}mpp_decode_info_t ;
#pragma pack(1)
typedef struct {
	unsigned char chn;              /*图像rga处理通道*/
	unsigned char mpp_chn;          /*图像进行h264/h265压缩所在的通道*/
	unsigned char frame_type;       /*输入图像的类型  rgb24 bggr rggb grbg gbrg gray8 gray10 gray12 gray14 gray16*/
	unsigned char fps;              /* 图像的帧率 */
	unsigned int width;             /* 本通道图像的宽 */
	unsigned int height;            /* 本通道图像的高 */
    unsigned int image_w0;             /*叠加图像的宽*/
    unsigned int image_y0;             /*叠加图像的高*/
	unsigned char mpp_type;
	unsigned int mpp_bps;
	unsigned char position;         /*字符叠加位置*/
	unsigned char out_type;
	unsigned char extraction;		/*抽帧比*/
	unsigned char rotate;           /*旋转类型0/1/2/4*/
	unsigned char resize;           /*是否缩放*/
	unsigned int resize_width;
	unsigned int resize_height;
	unsigned char crop;
	unsigned int crop_width;
	unsigned int crop_height;
    unsigned char odd_even_transform;
}mpp_encode_info_t;
#pragma pack()
typedef struct {
    u_int16_t rga_chn;
    u_int16_t rga_flag;
    float pixel_byte;
    u_int16_t out_color;
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
    u_int16_t odd_even_transform; /*奇数行偶数行变换标志*/
}rga_param_t ;
typedef struct {
    pthread_t pht_up;
    u_int16_t send_flag;
    unsigned char send_created;
    u_int16_t out_width;
    u_int16_t out_height;
}pcie_info_t ;
typedef struct {
    pthread_t pht_rga;
    pthread_t pht_osd;               /* 线程句柄 */
    pthread_t pht_crop;

    rga_param_t rga_info;
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
    mpp_decode_info_t* mpp_decode_info[MPP_DECODE_NTHREAD_MAX];
    mpp_encode_info_t* mpp_encode_info[MPP_ENCODE_NTHREAD_MAX];
    pcie_info_t* pcie_info[MPP_DECODE_NTHREAD_MAX];
    picture_info_t picture_info[MPP_DECODE_NTHREAD_MAX];
}data_stream_t;

data_stream_t *data_stream_get(void);
int RingBufferCreate(int width,int height);

ring_buffer_t *GetInPutRingBufferPt(int chn);
ring_buffer_t *GetResizeRingBufferPt(int chn);
ring_buffer_t *GetOutPutRingBufferPt(int chn);

int picture_init(u_int32_t chn);
void MpiDecDestroy();
int data_stream_init(void);
void data_stream_destory(void);
void data_stream_chn_ringbuffer_is_empty(unsigned char chn);
void *thread_writePL(void *arg);
int writePL_chn_init(unsigned char chn);
int writePL_chn_destory(unsigned char chn);

#ifdef __cplusplus
}
#endif

#endif /* DATA_MANAGER_H */
