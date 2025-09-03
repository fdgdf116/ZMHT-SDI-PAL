/*
 * @author       : Xyl
 * @FilePath     : rga_engine.c
 * @version      : v1.0
 * @Date         : 2024-02-27 14:10:15
 * @LastEditors  : Xyl
 * @LastEditTime : 2024-06-06 18:31:31
 * @Description  : 
 * Copyright (c) 2024 by Xyl, All Rights Reserved. 
 */
#include "stdint.h"
#include "stdlib.h"

#include "xutils.h"
#include "rga_engine.h"
#include "rga_interface.h"
#include "ringbuffer.h"
#include "data_engine.h"
#include "osd_engine.h"
#include "gpu_interface.h"
#include "mpi_decode.h"
#include "a818_error.h"
#include "log.h"

static char *rga_test_file = "output.yuv";
static char *cvtcolor_test_file = "/userdata/sda1/cvtcolor_1920x1080.raw";

void* rga_deal(void* arg){
    pthread_detach(pthread_self());
    int ret;
    int chn = *(int *)arg;
    data_stream_t* data_stream = data_stream_get();
    image_chn_info_t* image_chn_info = data_stream->image_chn_info[chn];
    rga_param_t rga_info = image_chn_info->rga_info;
    osd_image_info_t* osd_info = &image_chn_info->osd_info;
    fpga_chn_image_info_t *chn_image = data_stream->chn_image[chn];
	GPU_Frame_st* gpu_frame_stream = gpu_data_stream_get();
	gpu_image_info_t *gpu_image = &gpu_frame_stream->gpu_image[chn];
    osd_data_stream_t* gs_osd_stream = GetOsdDataStream();
    image_frame_info_t event_param = {0};
    struct timeval time_val;
    int resize_size = image_chn_info->rga_info.resize_width * image_chn_info->rga_info.resize_height * 3;
    ring_buffer_t *rb_in = GetInPutRingBufferPt(chn);
    ring_buffer_t *rb_out = NULL;

    printf("chn:[%d] %s start\n", chn, __func__);
    image_frame_info_t frame_info = {0};
    while (!image_chn_info->rga_info.rga_flag)
    {
        while(ringbuffer_is_empty(rb_in)){
            // printf("InPutRingBuffer is_empty\n");
            time_val.tv_sec=0;
            time_val.tv_usec=(1000);
            select(0,NULL,NULL,NULL,&time_val);
            if(image_chn_info->rga_info.rga_flag){
				goto stop;
			}
        }
        if(osd_info->osd_type)
        {
            rb_out = GetResizeRingBufferPt(chn);
        }
        else
        {
            rb_out = gpu_frame_stream->osd_data_rb[chn];
        }
        while(ringbuffer_is_full(rb_out)){
            // printf("OutPutRingBuffer is_full\n");
            time_val.tv_sec=0;
            time_val.tv_usec=(1000);
            select(0,NULL,NULL,NULL,&time_val);
            if(image_chn_info->rga_info.rga_flag){
				goto stop;
			}
        }
        ringbuffer_get(rb_in, (uint8_t *)&frame_info, sizeof(image_frame_info_t));
        if(chn_image->chn_data.yuv420_data[frame_info.frame_offset].vir_addr == NULL)
        {
            SLOGI("[ERROR] %s chn:%d frame_offset:%d yuv420_data is NULL\n", __func__, chn, frame_info.frame_offset);
            continue;
        }
        if(chn_image->chn_data.resize_rb_offset >= 2*(chn_image->data_frame_max + 1)){
            chn_image->chn_data.resize_rb_offset = 0;
        }
        // gettimeofday(&t1, NULL);
        // pthread_mutex_lock(&data_stream->rga_lock);
        if(rga_info.rotate_type)
        {
            rga_cvtColor(&chn_image->chn_data.yuv420_data[frame_info.frame_offset],\
                &chn_image->chn_data.rgb888_data);
            if(rga_info.resize)
            {
                rga_rotate(&chn_image->chn_data.rgb888_data,\
                    &chn_image->chn_data.rotate_data,image_chn_info->rga_info.rotate_type);
                rga_resize(&chn_image->chn_data.rotate_data, &gpu_image->in_rgb_buffer[chn_image->chn_data.resize_rb_offset]);
            }
            else
            {
                rga_rotate(&chn_image->chn_data.rgb888_data,\
                    &gpu_image->in_rgb_buffer[chn_image->chn_data.resize_rb_offset],image_chn_info->rga_info.rotate_type);
            }
        }
        else
        {
            rga_cvtColor(&chn_image->chn_data.yuv420_data[frame_info.frame_offset],\
                &gpu_image->in_rgb_buffer[chn_image->chn_data.resize_rb_offset]);
        }
        // pthread_mutex_unlock(&data_stream->rga_lock);
        if(!ringbuffer_is_full(rb_out)){
            event_param.chn = chn;
            event_param.frame_offset = chn_image->chn_data.resize_rb_offset;
            event_param.frame_size = resize_size;
            ringbuffer_put(rb_out, (uint8_t *)&event_param, sizeof(event_param));
            chn_image->chn_data.resize_rb_offset += 1;
        } else {
            SLOGI("[ERROR] %s ringbuffer is full, chn:%d\n", __func__, chn);
        }
    }
stop:
    printf("chn[%d] rga_deal finish!!!\n", chn);
	return NULL;
}

/*
	函数: unsigned int data_stream_low_chn_data_init(unsigned char chn)
	作用: 对通道进行初始化
	参数:
		通道号
	返回值:
		正确返回0，错误返回其他
*/
unsigned int rga_chn_image_init(unsigned char chn)
{
	int ret, index;
	unsigned int buffer_len;
	unsigned int ret_val = 0;
    data_stream_t* data_stream = data_stream_get();
    image_chn_info_t* image_chn_info = data_stream->image_chn_info[chn];
    fpga_chn_image_info_t *chn_image = data_stream->chn_image[chn];
    rga_param_t* rga_info = &image_chn_info->rga_info;
	chn_image->chn = chn;
	chn_image->frame_num = 0;
    unsigned int w_stride = rga_info->src_width%16 == 0 ? rga_info->src_width : (rga_info->src_width/16 + 1)*16;
    unsigned int h_stride = rga_info->src_height%16 == 0 ? rga_info->src_height : (rga_info->src_height/16 + 1)*16;
    printf("src_width:%d src_height:%d w_stride:%d h_stride:%d\n", rga_info->src_width, rga_info->src_height, w_stride, h_stride);

    chn_image->data_frame_max = 0;
    // if((rga_info->resize_width >= VIDEO_FRAME_WIDTH_4k) && (rga_info->resize_height >= VIDEO_FRAME_HEIGHT_4k)){
    //     chn_image->data_frame_max = DATA_4K_STREAM_FRAME_MAX;
    // } else {
        chn_image->data_frame_max = DATA_STREAM_FRAME_MAX;
    // }
    chn_image->rga_addr_offset_max = ((2+chn_image->data_frame_max+1)*3*w_stride* h_stride + 4096);
    chn_image->rga_addr_offset = 0;
    chn_image->rga_vir_addr = (unsigned char*)malloc(chn_image->rga_addr_offset_max);
    if(!chn_image->rga_vir_addr){
        printf("chn:%d rga_vir_addr malloc error \n",chn);
        ret_val = A818_DATA_CHN_IMAGE_MALLOC;
        goto err_4;
    }

	memset(&chn_image->chn_data, 0x0, sizeof(chn_image->chn_data));
	chn_image->chn_data.data_offset = 0;
	chn_image->chn_data.frame_size = 0;
	chn_image->chn_data.rb_offset = 0;

	SLOGI("[DEBUG] %s chn:%d frame_size:%d (%d-%d-%d-%d-%d-%d)\n", __func__, chn, chn_image->chn_data.frame_size,rga_info->src_width,rga_info->src_height,\
		rga_info->rotate_width, rga_info->rotate_height, rga_info->resize_width, rga_info->resize_height);

	/*初始化各种缓存的申请，这里将输入的yuv数据多申请一帧的图像，因为防止在特殊情况下(ring buffer满)，导致
		缓冲区图像被覆盖
	*/
    buffer_len = get_rga_vir_buffer_len(RK_FORMAT_RGB_888, rga_info->src_width%16 == 0 ? rga_info->src_width : (rga_info->src_width/16 + 1)*16,rga_info->src_height);
    ret = rga_comm_create_image2(&chn_image->chn_data.rgb888_data,chn_image->rga_vir_addr +  chn_image->rga_addr_offset,RK_FORMAT_RGB_888, \
        rga_info->src_width,rga_info->src_height, rga_info->src_width%16 == 0 ? rga_info->src_width : (rga_info->src_width/16 + 1)*16, rga_info->src_height);
    if(ret != 0){
        printf("[ERROR] int rga rgb888_data[%d] error \n", index);
        ret_val = A818_DATA_RGB_DATA;
        ret_val |= (ret << 8);
        goto err_2;
    }
    chn_image->rga_addr_offset += buffer_len;
    if(chn_image->rga_addr_offset >= chn_image->rga_addr_offset_max){
        SLOGI("[ERROR] int rga rgb888_data[%d] buffer_len:%d rga_addr_offset error \n", index, buffer_len);
        ret_val = A818_DATA_RGA_ADDR_OFFSET;
        goto err_2;
    }

    buffer_len = get_rga_vir_buffer_len(RK_FORMAT_RGB_888, rga_info->rotate_width%16 == 0 ? rga_info->rotate_width : (rga_info->rotate_width/16 + 1)*16, rga_info->rotate_height);
    ret = rga_comm_create_image2(&chn_image->chn_data.rotate_data,chn_image->rga_vir_addr +  chn_image->rga_addr_offset,RK_FORMAT_RGB_888, \
        rga_info->rotate_width,rga_info->rotate_height, rga_info->rotate_width%16 == 0 ? rga_info->rotate_width : (rga_info->rotate_width/16 + 1)*16, rga_info->rotate_height);
    if(ret != 0){
        printf("[ERROR] int rga rotate_data[%d] buffer_len:%d error \n", index, buffer_len);
        ret_val = A818_DATA_ROTATE_DATA;
        ret_val |= (ret << 8);
        goto err_3;
    }
    chn_image->rga_addr_offset += buffer_len;
    if(chn_image->rga_addr_offset >= chn_image->rga_addr_offset_max){
        SLOGI("[ERROR] int rga rotate_data[%d] rga_addr_offset error \n", index);
        ret_val = A818_DATA_RGA_ADDR_OFFSET;
        goto err_3;
    }

    // buffer_len = get_rga_vir_buffer_len(RK_FORMAT_RGB_888, rga_info.rotate_width, rga_info.rotate_height);
    // ret = rga_comm_create_image2(&chn_image->chn_data.tmp_data,data_stream->rga_vir_addr +  data_stream->rga_addr_offset,RK_FORMAT_RGB_888, \
    //     rga_info.rotate_width,rga_info.rotate_height);
    // if(ret != 0){
    //     printf("[ERROR] int rga rotate_data[%d] buffer_len:%d error \n", index, buffer_len);
    //     ret_val = A818_DATA_ROTATE_DATA;
    //     ret_val |= (ret << 8);
    //     goto err_4;
    // }
    // data_stream->rga_addr_offset += buffer_len;
    // if(data_stream->rga_addr_offset >= data_stream->rga_addr_offset_max){
    //     SLOGI("[ERROR] int rga rotate_data[%d] rga_addr_offset error \n", index);
    //     ret_val = A818_DATA_RGA_ADDR_OFFSET;
    //     goto err_4;
    // }
    printf("data_frame_max:%d\n", chn_image->data_frame_max);
	for(index = 0; index < chn_image->data_frame_max + 1; index++){
        // buffer_len = get_rga_vir_buffer_len(RK_FORMAT_RGB_888, rga_info.resize_width%16 == 0 ? rga_info.resize_width : (rga_info.resize_width/16+1)*16,rga_info.resize_height);
        // ret = rga_comm_create_image2(&chn_image->chn_data.resize_data[index],data_stream->rga_vir_addr +  data_stream->rga_addr_offset,RK_FORMAT_RGB_888, \
        //     rga_info.resize_width,rga_info.resize_height, rga_info.resize_width%16 == 0 ? rga_info.resize_width : (rga_info.resize_width/16+1)*16, rga_info.resize_height);
        // if(ret != 0){
        //     printf("[ERROR] int rga rgb888_data[%d] error \n", index);
        //     ret_val = A818_DATA_RGB_DATA;
        //     ret_val |= (ret << 8);
        //     goto err_5;
        // }
        // data_stream->rga_addr_offset += buffer_len;
        // if(data_stream->rga_addr_offset >= data_stream->rga_addr_offset_max){
        //     SLOGI("[ERROR] int rga rgb888_data[%d] buffer_len:%d rga_addr_offset error \n", index, buffer_len);
        //     ret_val = A818_DATA_RGA_ADDR_OFFSET;
        //     goto err_5;
        // }

		buffer_len = get_rga_vir_buffer_len(RK_FORMAT_YCbCr_420_SP, rga_info->src_width%16 == 0 ? rga_info->src_width : (rga_info->src_width/16 + 1)*16,rga_info->src_height%2 == 0 ? rga_info->src_height : (rga_info->src_height/2 + 1)*2);
		ret = rga_comm_create_image2(&chn_image->chn_data.yuv420_data[index],chn_image->rga_vir_addr +  chn_image->rga_addr_offset,RK_FORMAT_YCbCr_420_SP, \
			rga_info->src_width,rga_info->src_height, rga_info->src_width%16 == 0 ? rga_info->src_width : (rga_info->src_width/16 + 1)*16, rga_info->src_height%2 == 0 ? rga_info->src_height : (rga_info->src_height/2 + 1)*2);
		if(ret != 0){
			printf("[ERROR] int rga yuv422_data[%d] error \n", index);
			ret_val = A818_DATA_YUV_DATA;
			ret_val |= (ret << 8);
			goto err_1;
		}
		chn_image->rga_addr_offset += buffer_len;
		if(chn_image->rga_addr_offset >= chn_image->rga_addr_offset_max){
			SLOGI("[ERROR] int rga yuv422_data[%d] buffer_len:%d rga_addr_offset error \n", index, buffer_len);
			ret_val = A818_DATA_RGA_ADDR_OFFSET;
			goto err_1;
		}
	}
	usleep(200*1000);
    image_chn_info->rga_info.rga_flag = 0;
    ret = pthread_create(&image_chn_info->pht_rga, NULL, &rga_deal, &chn_image->chn);
    if (ret != 0) {
      printf("[ERROR] Create rga_deal failed %d\n",__func__,ret);
      ret_val = A818_DATA_LOW_CHN_THREAD;
      goto err_5;
    }
    SLOGI("[Debug] %s chn:%d \n", __func__, chn);
	return A818_SUCCESS;

err_5:
err_1:
	for(index = 0; index < (chn_image->data_frame_max + 1); index++){
		if(chn_image->chn_data.yuv420_data[index].vir_addr){
			rga_release_frame_image2(&chn_image->chn_data.yuv420_data[index]);
			chn_image->chn_data.yuv420_data[index].vir_addr = NULL;
		}
	}
err_3:
    if(chn_image->chn_data.rotate_data.vir_addr){
        rga_release_frame_image2(&chn_image->chn_data.rotate_data);
        chn_image->chn_data.rotate_data.vir_addr = NULL;
    }
err_2:
    if(chn_image->chn_data.rgb888_data.vir_addr){
        rga_release_frame_image2(&chn_image->chn_data.rgb888_data);
        chn_image->chn_data.rgb888_data.vir_addr = NULL;
    }
err_4:
    if(chn_image->rga_vir_addr){
        free(chn_image->rga_vir_addr);
        chn_image->rga_vir_addr = NULL;
        chn_image->rga_addr_offset_max = 0;
    }
	return ret_val;
}

void rga_pthread_chn_destory(unsigned char chn)
{
    data_stream_t *data_stream = data_stream_get();
    image_chn_info_t* image_chn_info = data_stream->image_chn_info[chn];
    ring_buffer_t *rb_in = GetInPutRingBufferPt(chn);
    struct timeval time_val;
    printf("chn:%d %s line:%d test\n", chn, __func__, __LINE__);
    while(!ringbuffer_is_empty(rb_in)){
        // printf("InPutRingBuffer is_empty\n");
        time_val.tv_sec=0;
        time_val.tv_usec=(1000);
        select(0,NULL,NULL,NULL,&time_val);
    }
    printf("chn:%d %s line:%d test\n", chn, __func__, __LINE__);
    if(!image_chn_info->rga_info.rga_flag){
        //pthread_cancel(gs_osd_stream->chn_info[chn].osd_tid);
        image_chn_info->rga_info.rga_flag = 1;
        SLOGI("[Debug] %s chn:%d \n", __func__, chn);
        xt_nanosleep(0, 1000*1000);
    }
}

/**************************************************************/
/*
	函数: void data_stream_chn_data_destory(void)
	作用: 数据流销毁函数
	参数:
		无
	返回值:
		正确返回0，失败返回其他
*/
void data_stream_chn_data_destory(unsigned char chn)
{
	int index;
	data_stream_t *data_stream = data_stream_get();
	fpga_chn_image_info_t* chn_image = data_stream->chn_image[chn];
    printf("chn:%d %s line:%d test\n", chn, __func__, __LINE__);
    if(chn_image->chn_data.rgb888_data.vir_addr){
        rga_release_frame_image2(&chn_image->chn_data.rgb888_data);
        chn_image->chn_data.rgb888_data.vir_addr = NULL;
    }
    printf("chn:%d %s line:%d test\n", chn, __func__, __LINE__);
    if(chn_image->chn_data.rotate_data.vir_addr){
        rga_release_frame_image2(&chn_image->chn_data.rotate_data);
        chn_image->chn_data.rotate_data.vir_addr = NULL;
    }
    // if(chn_image->chn_data.tmp_data.vir_addr){
    //     rga_release_frame_image2(&chn_image->chn_data.tmp_data);
    //     chn_image->chn_data.tmp_data.vir_addr = NULL;
    // }
    printf("chn:%d %s line:%d test\n", chn, __func__, __LINE__);
	for(index = 0; index < (chn_image->data_frame_max + 1); index++){
		if(chn_image->chn_data.yuv420_data[index].vir_addr){
			rga_release_frame_image2(&chn_image->chn_data.yuv420_data[index]);
			chn_image->chn_data.yuv420_data[index].vir_addr = NULL;
		}
	}
    printf("chn:%d %s line:%d test\n", chn, __func__, __LINE__);
    if(chn_image->rga_vir_addr){
        free(chn_image->rga_vir_addr);
        chn_image->rga_vir_addr = NULL;
        chn_image->rga_addr_offset_max = 0;
    }
	SLOGI("[DEBUG] %s chn:%d end \n", __func__, chn);
}
