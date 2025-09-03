/*
 * Copyright (C) 2020 Rockchip Electronics Co., Ltd.
 * Authors:
 *  PutinLee <putin.lee@rock-chips.com>
 *  Cerf Yu <cerf.yu@rock-chips.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ARGS_H
#define ARGS_H

#include "rga.h"
#include "im2d_type.h"
typedef enum _mode_code {
    MODE_QUERYSTRING = 0,
    MODE_COPY,
    MODE_RESIZE,
    MODE_CROP,
    MODE_ROTATE,
    MODE_FLIP,
    MODE_TRANSLATE,
    MODE_BLEND,
    MODE_CVTCOLOR,
    MODE_FILL,
    MODE_WHILE,
    MODE_NONE,
    MODE_MAX
} MODE_CODE;

typedef struct
{
    unsigned char* vir_addr;//虚拟地址 （必须有）
    unsigned long int phy_addr;//物理地址 （可能没有）
    unsigned int Width;//宽
    unsigned int Height;//高
    unsigned int Stride_W;//宽
    unsigned int Stride_H;//高
    rga_buffer_handle_t src_handle; //不用的时候释放此句柄
    rga_buffer_t handle_buff;//在实际操作函数中使用这个缓存
    RgaSURF_FORMAT enType;// 图像类型
}RGA_IMAGE_S;

#ifdef __cplusplus
extern "C" {
#endif


int rga_comm_create_image(RGA_IMAGE_S* rga_stDst,RgaSURF_FORMAT enType,unsigned int w,unsigned int h);
void rga_release_frame_image(RGA_IMAGE_S* src_image);
int rga_copy(RGA_IMAGE_S* src_image,RGA_IMAGE_S* dst_image);
int rga_resize(RGA_IMAGE_S* src_image,RGA_IMAGE_S* dst_image);
int rga_cvtColor(RGA_IMAGE_S* src_image,RGA_IMAGE_S* dst_image);
int rga_crop(RGA_IMAGE_S* src_image,RGA_IMAGE_S* dst_image, int x, int y);
int rga_rotate(RGA_IMAGE_S* src_image,RGA_IMAGE_S* dst_image, int rotate);
int rga_blend(RGA_IMAGE_S* src_image,RGA_IMAGE_S* dst_image);

unsigned int get_rga_vir_buffer_len(RgaSURF_FORMAT enType,unsigned int w,unsigned int h);
int rga_comm_create_image2(RGA_IMAGE_S* rga_stDst,unsigned char*vir, RgaSURF_FORMAT enType,unsigned int w,unsigned int h,
                           unsigned int stride_w, unsigned int stride_h);
void rga_release_frame_image2(RGA_IMAGE_S* src_image);

#ifdef __cplusplus
}
#endif

#endif

