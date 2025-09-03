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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <math.h>
#include <fcntl.h>
#include <memory.h>
#include <unistd.h>
#include <sys/time.h>

#include "im2d.hpp"
#include "RockchipRga.h"
#include "RgaUtils.h"
#include "rga_interface.h"
#include "a818_error.h"

extern "C"
{
     #include "log.h"
}

#define DEBUG_RGA     0

struct timeval start, end;
long usec1;

int rga_comm_create_image(RGA_IMAGE_S* rga_stDst,RgaSURF_FORMAT enType,unsigned int w,unsigned int h)
{

	int ret;
	//复位
	memset(rga_stDst,0,sizeof(RGA_IMAGE_S));

	rga_stDst->enType = enType;

	//宽高需要对齐
	rga_stDst->Width = w;
	rga_stDst->Height = h;

	// 虚拟地址类型

	rga_stDst->vir_addr = (unsigned char*)malloc(rga_stDst->Width * rga_stDst->Height * get_bpp_from_format(enType));
	if(rga_stDst->vir_addr == NULL){
		printf("[ERROR] Failed vir_addr malloc !\n");
		return A818_RGA_VIR_ADDR_MALLOC;
	}
	rga_stDst->src_handle = importbuffer_virtualaddr(rga_stDst->vir_addr, rga_stDst->Width, rga_stDst->Height, enType);
	if (rga_stDst->src_handle <= 0) {
		printf("Failed to import virtualaddr for src channel!\n");
		ret = A818_RGA_INIT_HANDLE;
		goto err_1;
	}
	
	memset(rga_stDst->vir_addr, 0x0, rga_stDst->Width * rga_stDst->Height * get_bpp_from_format(enType));

	rga_stDst->handle_buff = wrapbuffer_handle(rga_stDst->src_handle, rga_stDst->Width, rga_stDst->Height, enType);
	if(rga_stDst->handle_buff.width == 0) {
		printf("%s, %s", __FUNCTION__, imStrError());
		ret = A818_RGA_INIT_HANDLE_BUFFER;
		goto err_2;
		 
	}

	return 0;

err_2:
	releasebuffer_handle(rga_stDst->src_handle);
err_1:
	free(rga_stDst->vir_addr);
	rga_stDst->vir_addr = NULL;
	
	return ret;
}

void rga_release_frame_image(RGA_IMAGE_S* src_image)
{
	releasebuffer_handle(src_image->src_handle);
	usleep(10*1000);
	if(src_image->vir_addr != NULL){
		free(src_image->vir_addr);
		src_image->vir_addr = NULL;
	}

	return;
}



/*
	函数: unsigned int get_rga_vir_buffer_len(RgaSURF_FORMAT enType,unsigned int w,unsigned int h)
	作用: 获取rga需要的内存长度
	参数:
		enType:帧类型
		w:图像宽
		h:图像高
	返回值:
		返回计算出来的长度
*/
unsigned int get_rga_vir_buffer_len(RgaSURF_FORMAT enType,unsigned int w,unsigned int h)
{
	return w * h * get_bpp_from_format(enType);
}

/*
	函数: int rga_comm_create_image2(RGA_IMAGE_S* rga_stDst,unsigned char*vir, RgaSURF_FORMAT enType,unsigned int w,unsigned int h)
	作用: 进行rga的内存的赋值和绑定
	参数:
		rga_stDst:需要初始化rga结构体
		vir:需要赋值的虚拟地址
		enType:图像格式
		w:图像宽
		h:图像高
	返回值:
		正确返回0，失败返回其他
*/
int rga_comm_create_image2(RGA_IMAGE_S* rga_stDst,unsigned char*vir, RgaSURF_FORMAT enType,unsigned int w,unsigned int h,unsigned int stride_w,unsigned int stride_h)
{

	int ret;
	//复位
	memset(rga_stDst,0,sizeof(RGA_IMAGE_S));

	rga_stDst->enType = enType;

	//宽高需要对齐
	rga_stDst->Width = w;
	rga_stDst->Height = h;
    rga_stDst->Stride_W = stride_w; // 宽度对齐
    rga_stDst->Stride_H = stride_h; // 高度对齐
    // printf("width:%d, height:%d, stride_w:%d, stride_h:%d\n", rga_stDst->Width, rga_stDst->Height, rga_stDst->Stride_W, rga_stDst->Stride_H);
	// 虚拟地址类型
	rga_stDst->vir_addr = vir;
	if(rga_stDst->vir_addr == NULL){
		printf("[ERROR] Failed vir_addr malloc !\n");
		return A818_RGA_VIR_ADDR_MALLOC;
	}
	rga_stDst->src_handle = importbuffer_virtualaddr(rga_stDst->vir_addr, rga_stDst->Stride_W, rga_stDst->Stride_H, enType);
	if (rga_stDst->src_handle <= 0) {
		printf("Failed to import virtualaddr for src channel!\n");
		ret = A818_RGA_INIT_HANDLE;
		goto err_1;
	}
	rga_stDst->handle_buff = wrapbuffer_handle(rga_stDst->src_handle, rga_stDst->Width, rga_stDst->Height, enType, rga_stDst->Stride_W, rga_stDst->Stride_H);
	if(rga_stDst->handle_buff.width == 0) {
		printf("%s, %s", __FUNCTION__, imStrError());
		ret = A818_RGA_INIT_HANDLE_BUFFER;
		goto err_2;
		 
	}
	return 0;

err_2:
	releasebuffer_handle(rga_stDst->src_handle);
err_1:
    if(rga_stDst->vir_addr)
    {
        free(rga_stDst->vir_addr);
        rga_stDst->vir_addr = NULL;
    }
	
	return ret;
}

/*
	函数: void rga_release_frame_image2(RGA_IMAGE_S* src_image)
	作用: 进行rga的解绑
	参数:
		rga_stDst:需要初始化rga结构体
	返回值:
		无
*/
void rga_release_frame_image2(RGA_IMAGE_S* src_image)
{
	releasebuffer_handle(src_image->src_handle);
	return;
}

int rga_copy(RGA_IMAGE_S* src_image,RGA_IMAGE_S* dst_image)
{
    IM_STATUS         STATUS;

    im_rect src_rect = {0};
    im_rect dst_rect = {0};

#if DEBUG_RGA
    int ret = imcheck(src_image->handle_buff, dst_image->handle_buff, src_rect, dst_rect);
    if (IM_STATUS_NOERROR != ret) 
    {
        printf("%d, check error! %s\n", __LINE__, imStrError((IM_STATUS)ret));
        return -1;
    }

    gettimeofday(&start, NULL);
#endif

    STATUS = imcopy(src_image->handle_buff, dst_image->handle_buff);

#if DEBUG_RGA
    gettimeofday(&end, NULL);
    usec1 = 1000000 * (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec);
    printf("copying .... cost time %ld us, %s\n", usec1, imStrError(STATUS));
#endif

    return 0;
}    

int rga_resize(RGA_IMAGE_S* src_image,RGA_IMAGE_S* dst_image)
{
    IM_STATUS         STATUS;

    im_rect src_rect = {0};
    im_rect dst_rect = {0};

#if DEBUG_RGA
    int ret = imcheck(src_image->handle_buff, dst_image->handle_buff, src_rect, dst_rect);
    if (IM_STATUS_NOERROR != ret) 
    {
        printf("%d, check error! %s\n", __LINE__, imStrError((IM_STATUS)ret));
        return -1;
    }

    gettimeofday(&start, NULL);
#endif

    STATUS = imresize(src_image->handle_buff, dst_image->handle_buff);

#if DEBUG_RGA
    gettimeofday(&end, NULL);
    usec1 = 1000000 * (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec);
    printf("resize .... cost time %ld us, %s\n", usec1, imStrError(STATUS));
#endif

    return 0;
}

int rga_cvtColor(RGA_IMAGE_S* src_image,RGA_IMAGE_S* dst_image)
{
    IM_STATUS         STATUS;

    im_rect src_rect = {0};
    im_rect dst_rect = {0};

#if DEBUG_RGA
    int ret = imcheck(src_image->handle_buff, dst_image->handle_buff, src_rect, dst_rect);
    if (IM_STATUS_NOERROR != ret) 
    {
        printf("%d, check error! %s\n", __LINE__, imStrError((IM_STATUS)ret));
        return -1;
    }

    gettimeofday(&start, NULL);
#endif
    STATUS = imcvtcolor(src_image->handle_buff, dst_image->handle_buff,src_image->enType,dst_image->enType);

#if DEBUG_RGA
    gettimeofday(&end, NULL);
    usec1 = 1000000 * (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec);
    printf("cvtColor .... cost time %ld us, %s\n", usec1, imStrError(STATUS));
#endif

    return 0;
}  
  
int rga_crop(RGA_IMAGE_S* src_image,RGA_IMAGE_S* dst_image, int x, int y)
{
    IM_STATUS         STATUS;

    im_rect src_rect = {0};
    im_rect dst_rect = {0};

    src_rect.width = dst_image->Width;
    src_rect.height = dst_image->Height;

    if((src_image->Width < src_rect.width) || (src_image->Height < src_rect.height))
    {
        return -1;
    }

    if (x + src_rect.width > src_image->Width)
    {
        src_rect.x = src_image->Width - src_rect.width;

        SLOGI("change x to %d\n ",src_rect.x);
    }
    else
    {
        src_rect.x = x;
    }

    if (y + src_rect.height > src_image->Height)
    {
        src_rect.y = src_image->Height - src_rect.height;

        SLOGI("change y to %d\n ",src_rect.y);
    }
    else
    {
        src_rect.y = y;
    }

#if DEBUG_RGA
    int ret = imcheck(src_image->handle_buff, dst_image->handle_buff, src_rect, dst_rect,IM_CROP);
    if (IM_STATUS_NOERROR != ret) 
    {
        printf("%d, check error! %s\n", __LINE__, imStrError((IM_STATUS)ret));
        return -1;
    }

    gettimeofday(&start, NULL);
#endif

    STATUS = imcrop(src_image->handle_buff, dst_image->handle_buff,src_rect);

#if DEBUG_RGA
    gettimeofday(&end, NULL);
    usec1 = 1000000 * (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec);
    printf("imcrop .... cost time %ld us, %s\n", usec1, imStrError(STATUS));
#endif

    return 0;
} 


int rga_rotate(RGA_IMAGE_S* src_image,RGA_IMAGE_S* dst_image, int rotate)
{
    IM_STATUS         STATUS;
    // if ((IM_HAL_TRANSFORM_ROT_90 ==  rotate) || (IM_HAL_TRANSFORM_ROT_270 == rotate)) {
    //     if((src_image->Width == 1920) && (src_image->Height == 1080))
    //     {
    //         dst_image->handle_buff.width = src_image->handle_buff.height+8;
    //         dst_image->handle_buff.height = src_image->handle_buff.width;
    //         dst_image->handle_buff.wstride = src_image->handle_buff.hstride+8;
    //         dst_image->handle_buff.hstride = src_image->handle_buff.wstride;
    //     }
    //     else
    //     {
    //         dst_image->handle_buff.width = src_image->handle_buff.height;
    //         dst_image->handle_buff.height = src_image->handle_buff.width;
    //         dst_image->handle_buff.wstride = src_image->handle_buff.hstride;
    //         dst_image->handle_buff.hstride = src_image->handle_buff.wstride;
    //     }
    // }
    // else
    // {
    //     dst_image->handle_buff.width = src_image->handle_buff.width;
    //     dst_image->handle_buff.height = src_image->handle_buff.height;
    //     dst_image->handle_buff.wstride = src_image->handle_buff.wstride;
    //     dst_image->handle_buff.hstride = src_image->handle_buff.hstride;
    // }

    im_rect src_rect = {0};
    im_rect dst_rect = {0};

#if DEBUG_RGA
    int ret = imcheck(src_image->handle_buff, dst_image->handle_buff, src_rect, dst_rect);
    if (IM_STATUS_NOERROR != ret) 
    {
        printf("%d, check error! %s\n", __LINE__, imStrError((IM_STATUS)ret));
        return -1;
    }

    gettimeofday(&start, NULL);
#endif

    STATUS = imrotate(src_image->handle_buff, dst_image->handle_buff, rotate);

#if DEBUG_RGA
    gettimeofday(&end, NULL);
    usec1 = 1000000 * (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec);
    printf("imrotate .... cost time %ld us, %s\n", usec1, imStrError(STATUS));
#endif

    return STATUS;
} 

int rga_blend(RGA_IMAGE_S* src_image,RGA_IMAGE_S* dst_image)
{
    IM_STATUS         STATUS;
    im_rect src_rect = {0};
    im_rect dst_rect = {0};

#if DEBUG_RGA
    int ret = imcheck(src_image->handle_buff, dst_image->handle_buff, src_rect, dst_rect);
    if (IM_STATUS_NOERROR != ret) 
    {
        printf("%d, check error! %s\n", __LINE__, imStrError((IM_STATUS)ret));
        return -1;
    }

    gettimeofday(&start, NULL);
#endif

    STATUS = imblend(src_image->handle_buff, dst_image->handle_buff);

#if DEBUG_RGA
    gettimeofday(&end, NULL);
    usec1 = 1000000 * (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec);
    printf("imrotate .... cost time %ld us, %s\n", usec1, imStrError(STATUS));
#endif

    return STATUS;
} 

