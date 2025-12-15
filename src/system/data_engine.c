/*
 * File      : data_engine.c
 * This file is data engine file  
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-1-4     fengyong    first version
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/ioctl.h>
#include <sched.h>

#include "rga_interface.h"
#include "fifo_engine.h"
#include "pcie_engine.h"
#include "pcie_reg_rw.h"
#include "data_engine.h"
#include "mpi_decode.h"
#include "xutils.h"
#include "log.h"
#include "gpu_interface.h"
#include "osd_engine.h"
#include "data_engine.h"
#include "fps_calc.h"
#include "crop_engine.h"

extern void* pcie_common_param_reg;
static data_stream_t *gs_data_stream = NULL;
data_stream_t *data_stream_get(void)
{
	return gs_data_stream;
}

/*
	函数: void data_stream_chn_ringbuffer_is_empty(unsigned char chn)
	作用: 判断本通道的ring buffer是否被使用完成
	参数:
		小图通道
	返回值:
		无
*/
void data_stream_chn_ringbuffer_is_empty(unsigned char chn)
{
	data_stream_t *data_stream = data_stream_get();
	fpga_chn_image_info_t* chn_image = data_stream->chn_image[chn];

	while(!ringbuffer_is_empty(chn_image->rga_data_rb)){
		xt_nanosleep(0, 100);
	}
	while(!ringbuffer_is_empty(chn_image->resize_data_rb)){
		xt_nanosleep(0, 100);
	}
	while(!ringbuffer_is_empty(chn_image->pcie_data_rb)){
		xt_nanosleep(0, 100);
	}
}

ring_buffer_t *GetInPutRingBufferPt(int chn){
    data_stream_t* data_stream = data_stream_get();
    fpga_chn_image_info_t *chn_image = data_stream->chn_image[chn];
    return chn_image->rga_data_rb;
}
ring_buffer_t *GetResizeRingBufferPt(int chn){
    data_stream_t* data_stream = data_stream_get();
    fpga_chn_image_info_t *chn_image = data_stream->chn_image[chn];
    return chn_image->resize_data_rb;
}
ring_buffer_t *GetOutPutRingBufferPt(int chn){
    data_stream_t* data_stream = data_stream_get();
    fpga_chn_image_info_t *chn_image = data_stream->chn_image[chn];
    return chn_image->pcie_data_rb;
}

void *thread_picture_decode(void *arg){
    int chn = (int)arg;
    unsigned int fifo_num = 0;
    struct timeval time;
    data_stream_t* data_stream = data_stream_get();
    picture_info_t *picture_info = &data_stream->picture_info[chn%MPP_DECODE_NTHREAD_MAX];
    while(!picture_info->send_flag){
        fifo_num = (pcie_bar_read_reg(pcie_common_param_reg,0x2c+chn*4) * 16);
        if(fifo_num > (8*1024*1024))
        {
            time.tv_sec=0;
            time.tv_usec=(1000);
            select(0,NULL,NULL,NULL,&time);
            // printf("fifo fulll\n");
            continue;
        }
        pcie_dec_send(picture_info->pitture_buf, picture_info->weidth*picture_info->height*2,chn);
        // save_image_data(picture_info->pitture_buf, picture_info->weidth*picture_info->height*2, "/tmp/pitture_buf.raw");
    }
}

int picture_init(u_int32_t chn)
{
    unsigned int picture_head = 0;
    data_stream_t* data_stream = data_stream_get();
    picture_info_t *picture_info = &data_stream->picture_info[chn%MPP_DECODE_NTHREAD_MAX];
    mpp_decode_info_t* mpp_info = data_stream->mpp_decode_info[chn%MPP_DECODE_NTHREAD_MAX];
    if(!mpp_info->decode_state)
    {
        printf("decoding!!!\n");
        return 1;
    }

    picture_info->pitture_fp = fopen(picture_info->picture_name, "rb");
    if(picture_info->pitture_fp == NULL){
        printf("open %s failed\n", picture_info->picture_name);
        return 2;
    }
    fread(&picture_head, 4, 1, picture_info->pitture_fp);
    fread(&picture_info->weidth, 4, 1, picture_info->pitture_fp);
    fread(&picture_info->height, 4, 1, picture_info->pitture_fp);
    fread(&picture_info->fps, 4, 1, picture_info->pitture_fp);
    fflush(picture_info->pitture_fp);

    if(picture_head != 0x56fa3751){
        printf("picture head error\n");
        return 3;
    }
    picture_info->weidth = ntohl(picture_info->weidth);
    picture_info->height = ntohl(picture_info->height);
    picture_info->fps = ntohl(picture_info->fps);
    SLOGI("picture weidth:%d, height:%d, fps:%d", picture_info->weidth, picture_info->height, picture_info->fps);
    picture_info->pitture_buf = align_write_buf_get(chn+2);;
    if(picture_info->pitture_buf == NULL){
        printf("malloc %s failed\n", picture_info->picture_name);
        return 4;
    }
    fread(picture_info->pitture_buf, picture_info->weidth * picture_info->height * 2, 1, picture_info->pitture_fp);
    fflush(picture_info->pitture_fp);
    fclose(picture_info->pitture_fp);
    if(picture_info->chn == 0)
        FAPI_image_odd_transform_to_target(picture_info->weidth, picture_info->height, 2, data_stream->image_chn_info[chn+2]->rga_info.odd_even_transform, picture_info->pitture_buf, picture_info->pitture_buf);
    picture_info->send_flag = 0;
    mpp_info->decode_state = pthread_create(&picture_info->pht_picture, NULL, thread_picture_decode, &picture_info->chn);
    return 0;
}

void *thread_writePL(void *arg){
    pthread_detach(pthread_self());
    // pthread_mutex_lock(&mutex);
    int rb_len,get_len;
    image_frame_info_t frame_info;
    ring_buffer_t * rb_t = NULL;
    uint8_t *send_buf = NULL; 
    unsigned int fifo_num;
    struct timeval time;
    int chn = *(int*)arg;
    struct timeval t1, t2;
    send_buf = align_write_buf_get(chn+2);
    rb_t = GetOutPutRingBufferPt(chn);
    data_stream_t* data_stream = data_stream_get();
    pcie_info_t* pcie_info = data_stream->pcie_info[chn];
    mpp_decode_info_t* mpp_info = data_stream->mpp_decode_info[chn];
    rga_param_t rga_info = data_stream->image_chn_info[chn+2]->rga_info;
    MpiDecLoopData* data = &mpp_info->dec_data;
    printf("chn:[%d] %s start\n", chn, __func__);
    //计算每个通道的pcie发送耗时
    gettimeofday(&t1, NULL);
    u_int8_t* send_tmp = (u_int8_t*)malloc(rga_info.out_width * rga_info.out_height * 2);
    for(int j = 0;j<rga_info.out_height;j++)
    {
        for(int i = 0;i<rga_info.out_width;i++)
        {
            send_tmp[j*rga_info.out_width*2 + i*2] = i;
            send_tmp[j*rga_info.out_width*2 + i*2+1] = 128;
        }
    }
    while(!pcie_info->send_flag){
        while(ringbuffer_is_empty(rb_t)){
            // pcie_dec_send(send_buf + frame_info.frame_size*frame_info.frame_offset,frame_info.frame_size,chn);
            time.tv_sec=0;
            time.tv_usec=(1000);
            select(0,NULL,NULL,NULL,&time);
            if(pcie_info->send_flag)
                goto stop;
        }
        fifo_num = (pcie_bar_read_reg(pcie_common_param_reg,0x2c+chn*4) * 16);
        if(fifo_num > (8*1024*1024))
        {
            time.tv_sec=0;
            time.tv_usec=(1000);
            select(0,NULL,NULL,NULL,&time);
            // printf("fifo fulll\n");
            continue;
        }
        ringbuffer_get(rb_t, (uint8_t *)&frame_info, sizeof(image_frame_info_t));
        pcie_dec_send(send_buf + frame_info.frame_offset*frame_info.frame_size, frame_info.frame_size,chn);
        if(data->fp_output != NULL)
        {
            fwrite(send_buf + frame_info.frame_offset*frame_info.frame_size,frame_info.frame_size,1,data->fp_output);
        }
        // pcie_dec_send(send_tmp, rga_info.out_width * rga_info.out_height * 2,chn);
        // if(data->fp_output != NULL)
        // {
        //     fwrite(send_tmp, rga_info.out_width * rga_info.out_height * 2,1,data->fp_output);
        // }
        // gettimeofday(&t2, NULL);
        // printf("[%s %d]speed:%f MBps frame_size:%d time: %ld us\n", __func__, __LINE__, frame_info.frame_size*1.0/(1024*1024) /((t2.tv_sec - t1.tv_sec) * 1000000 + (t2.tv_usec - t1.tv_usec)) * 1000000, frame_info.frame_size, (t2.tv_sec - t1.tv_sec) * 1000000 + (t2.tv_usec - t1.tv_usec));
        // memcpy(&t1, &t2, sizeof(struct timeval));
        // if(pcie_info->out_height < 4096){
        //     if(data->fps <= 35)
        //     {
        //         time.tv_sec=0;
        //         time.tv_usec=(29000);
        //     }
        //     else
        //     {
        //         time.tv_sec=0;
        //         time.tv_usec=(9000);
        //     }
        // }
        // else{
        //     time.tv_sec=0;
        //     time.tv_usec=(970000);
        // }
        if(chn == 0)
        {
            time.tv_sec=0;
            time.tv_usec=(30000);
            select(0,NULL,NULL,NULL,&time);
        }
        else
        {
            time.tv_sec=0;
            time.tv_usec=(5000);
            select(0,NULL,NULL,NULL,&time);
        }
    }
stop:
    printf("chn[%d] writePL finish!!!\n",chn);
    return NULL;
}

int writePL_chn_destory(unsigned char chn)
{
    struct timeval time_val;
    data_stream_t *data_stream = data_stream_get();
    fpga_chn_image_info_t* chn_image = data_stream->chn_image[chn];
    pcie_info_t* pcie_info = data_stream->pcie_info[chn%MPP_DECODE_NTHREAD_MAX];
    ring_buffer_t* pcie_data_rb = chn_image->pcie_data_rb;
	Crop_Frame_st* crop_stream = crop_data_stream_get();
	crop_image_info_t *crop_image = &crop_stream->crop_image[chn];
    image_frame_info_t frame_info = {0};

    while(!ringbuffer_is_empty(pcie_data_rb)){
        ringbuffer_get(pcie_data_rb, (uint8_t *)&frame_info, sizeof(image_frame_info_t));
        // printf("InPutRingBuffer is_empty\n");
        time_val.tv_sec=0;
        time_val.tv_usec=(1000);
        select(0,NULL,NULL,NULL,&time_val);
    }
    if(!pcie_info->send_flag){
        pcie_info->send_flag = 1;
        printf("chn[%d] writePL thread destoryed\n",chn);
    }
	for(int index = 0; index < crop_image->data_frame_max+1; index++)
	{
		if(crop_image->crop_yuv_buffer[index]){
			free(crop_image->crop_yuv_buffer[index]);
			crop_image->crop_yuv_buffer[index] = NULL;
		}
	}
    return 0;
}

int writePL_chn_init(unsigned char chn)
{
    data_stream_t* data_stream = data_stream_get();
    mpp_decode_info_t* mpp_decode_info = data_stream->mpp_decode_info[chn%MPP_DECODE_NTHREAD_MAX];
    pcie_info_t* pcie_info = data_stream->pcie_info[chn%MPP_DECODE_NTHREAD_MAX];

    pcie_info->send_flag = 0;
    int ret = 0;
    ret = pthread_create(&pcie_info->pht_up,NULL,thread_writePL,&mpp_decode_info->mpp_chn);
    if(ret != 0){
        printf("[%s]Create WritePL thread failed\n",__func__);
        return -1;
    }
    return 0;
}

void MpiDecDestroy(){
    for(int chn = MPP_DECODE_NTHREAD_MAX;chn < DATA_CHANNEL_NUM_MAX;chn++)
    {
        MpiDecodeDestroy(chn);
    }
}

int data_stream_init(void)
{
    int ret;
    int chn;
    gs_data_stream = (data_stream_t*)malloc(sizeof(data_stream_t));
    memset(gs_data_stream,0x00,sizeof(data_stream_t));
    pthread_mutex_init(&gs_data_stream->osd_lock, NULL);
    pthread_mutex_init(&gs_data_stream->rga_lock, NULL);

    /* 初始化各个需要拼接通道的 ringbuffer, 这个是不需要跟着停止释放的 */
    for(chn = 0; chn < DATA_CHANNEL_NUM_MAX;chn++){
        printf("[%s %d] test\n",__func__,__LINE__);
        gs_data_stream->chn_image[chn] = (fpga_chn_image_info_t*)malloc(sizeof(fpga_chn_image_info_t));
        if(!gs_data_stream->chn_image[chn]){
            printf("[ERROR] chn:%d malloc chn image failed \n", chn);
            ret = -1;
            goto err_1; 
        }
        memset(gs_data_stream->chn_image[chn], 0x0, sizeof(fpga_chn_image_info_t));
 
        gs_data_stream->image_chn_info[chn] = (image_chn_info_t*)malloc(sizeof(image_chn_info_t));
        if(!gs_data_stream->image_chn_info[chn]){
            printf("[ERROR] chn:%d malloc image_chn _info failed \n", chn);
            ret = -1;
            goto err_1; 
        }
        memset(gs_data_stream->image_chn_info[chn], 0x0, sizeof(image_chn_info_t));

        if(chn < MPP_ENCODE_NTHREAD_MAX)
        {
            gs_data_stream->mpp_encode_info[chn%MPP_ENCODE_NTHREAD_MAX] = (mpp_encode_info_t*)malloc(sizeof(mpp_encode_info_t));
            if(!gs_data_stream->mpp_encode_info[chn%MPP_ENCODE_NTHREAD_MAX]){
                printf("[ERROR] chn:%d malloc image_chn _info failed \n", chn);
                ret = -1;
                goto err_1; 
            }
            memset(gs_data_stream->mpp_encode_info[chn%MPP_ENCODE_NTHREAD_MAX], 0x0, sizeof(mpp_encode_info_t));   
        }
        else
        {
            gs_data_stream->mpp_decode_info[chn%MPP_DECODE_NTHREAD_MAX] = (mpp_decode_info_t*)malloc(sizeof(mpp_decode_info_t));
            if(!gs_data_stream->mpp_decode_info[chn%MPP_DECODE_NTHREAD_MAX]){
                printf("[ERROR] chn:%d malloc image_chn _info failed \n", chn);
                ret = -1;
                goto err_1; 
            }
            memset(gs_data_stream->mpp_decode_info[chn%MPP_DECODE_NTHREAD_MAX], 0x0, sizeof(mpp_decode_info_t)); 
            gs_data_stream->mpp_decode_info[chn%MPP_DECODE_NTHREAD_MAX]->decode_state = 1;

            gs_data_stream->pcie_info[chn%MPP_DECODE_NTHREAD_MAX] = (pcie_info_t*)malloc(sizeof(pcie_info_t));
            if(!gs_data_stream->pcie_info[chn%MPP_DECODE_NTHREAD_MAX]){
                printf("[ERROR] chn:%d malloc pcie_info failed \n", chn);
                ret = -1;
                goto err_1; 
            }
            memset(gs_data_stream->pcie_info[chn%MPP_DECODE_NTHREAD_MAX], 0x0, sizeof(pcie_info_t));   
        }

        gs_data_stream->chn_image[chn]->rga_data_rb = ringbuffer_create(RINGBUFFER_FRAME_MAX*sizeof(image_frame_info_t),sizeof(image_frame_info_t));
        if (!gs_data_stream->chn_image[chn]->rga_data_rb) {
            printf("[ERROR] chn:%d init rga data ringbuffer failed\n", chn);
            ret = -2;
            goto err_1;
        }

        gs_data_stream->chn_image[chn]->resize_data_rb = ringbuffer_create(RINGBUFFER_FRAME_MAX*sizeof(image_frame_info_t),sizeof(image_frame_info_t));
        if (!gs_data_stream->chn_image[chn]->resize_data_rb) {
            printf("[ERROR] chn:%d init resize data ringbuffer failed\n", chn);
            ret = -2;
            goto err_1;
        }

        gs_data_stream->chn_image[chn]->pcie_data_rb = ringbuffer_create(RINGBUFFER_FRAME_MAX*sizeof(image_frame_info_t),sizeof(image_frame_info_t));
        if (!gs_data_stream->chn_image[chn]->pcie_data_rb) {
            printf("[ERROR] chn:%d init stitchbase data ringbuffer failed\n", chn);
            ret = -3;
            goto err_1;
        }
    }

    return 0;
err_2:
err_1:
    data_stream_destory();
    return ret;
}

void data_stream_destory(void)
{
    for(int chn = 0; chn < DATA_CHANNEL_NUM_MAX;chn++){
        data_stream_t* data_stream = data_stream_get();
        image_chn_info_t *image_chn_info = data_stream->image_chn_info[chn];
        fpga_chn_image_info_t *chn_image = data_stream->chn_image[chn];
        mpp_decode_info_t* mpp_decode_info = data_stream->mpp_decode_info[chn%MPP_DECODE_NTHREAD_MAX];
        mpp_encode_info_t* mpp_encode_info = data_stream->mpp_encode_info[chn%MPP_ENCODE_NTHREAD_MAX];
        pcie_info_t* pcie_info = data_stream->pcie_info[chn%MPP_DECODE_NTHREAD_MAX];

        if(chn_image){

            if(chn_image->rga_data_rb){
                ringbuffer_deinit(chn_image->rga_data_rb);
            }
            if(chn_image->resize_data_rb){
                ringbuffer_deinit(chn_image->resize_data_rb);
            }
            if(chn_image->pcie_data_rb){
                ringbuffer_deinit(chn_image->pcie_data_rb);
            }
            free(chn_image);
            chn_image = NULL;
        }
        if(mpp_decode_info)
        {
            free(mpp_decode_info);
            mpp_decode_info = NULL;
        }
        if(mpp_encode_info)
        {
            free(mpp_encode_info);
            mpp_encode_info = NULL;
        }
        if(pcie_info)
        {
            free(pcie_info);
            pcie_info = NULL;
        }
        if(image_chn_info)
        {
            free(image_chn_info);
            image_chn_info = NULL;
        }
        if(data_stream)
        {
            free(data_stream);
            data_stream = NULL;
        }
    }
}