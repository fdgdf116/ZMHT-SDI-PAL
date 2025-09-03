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
#include "net_engine.h"
#include "fps_calc.h"

extern void* pcie_common_param_reg;
static data_stream_t *gs_data_stream = NULL;
data_stream_t *data_stream_get(void)
{
	return gs_data_stream;
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

void *thread_crop(void *arg){
    pthread_detach(pthread_self());
    // pthread_mutex_lock(&mutex);
    image_frame_info_t frame_info;
    image_frame_info_t event_param = {0};
    uint8_t *send_buf = NULL; 
    unsigned int fifo_num;
    struct timeval time;
    int chn = *(int*)arg;
    struct timeval t1, t2;
    send_buf = align_write_buf_get(chn);
    data_stream_t* data_stream = data_stream_get();
    image_chn_info_t* image_chn_info = data_stream->image_chn_info[chn];
    rga_param_t* rga_info = &image_chn_info->rga_info;
    GPU_Frame_st* gpu_frame_stream = gpu_data_stream_get();
	gpu_image_info_t *gpu_image = &gpu_frame_stream->gpu_image[chn];

	uint8_t* out_yuv_addr = NULL;
    ring_buffer_t *rb_t = GetOutPutRingBufferPt(chn);
    ring_buffer_t *rb_in = gpu_frame_stream->image_over_rb[chn];
    printf("chn:[%d] %s start\n", chn, __func__);
    //计算每个通道的pcie发送耗时
    gettimeofday(&t1, NULL);
    while(!image_chn_info->pcie_info.crop_flag){
        while(ringbuffer_is_empty(rb_in)){
            time.tv_sec=0;
            time.tv_usec=(1000);
            select(0,NULL,NULL,NULL,&time);
            if(image_chn_info->pcie_info.crop_flag)
                goto stop;
        }
        while(ringbuffer_is_full(rb_t)){
            time.tv_sec=0;
            time.tv_usec=(1000);
            select(0,NULL,NULL,NULL,&time);
            if(image_chn_info->pcie_info.crop_flag)
                goto stop;
        }
        ringbuffer_get(rb_in, (uint8_t *)&frame_info, sizeof(image_frame_info_t));
        if(gpu_image->crop_rb_offset >= gpu_image->data_frame_max + 1){
            gpu_image->crop_rb_offset = 0;
		}
        out_yuv_addr = gpu_image->out_yuv_buffer[frame_info.frame_offset];
        if(out_yuv_addr == NULL)
        {
            SLOGI("[ERROR] %s chn:%d frame_offset:%d out_yuv_addr is NULL\n", __func__, chn, frame_info.frame_offset);
            continue;
        }
        if(rga_info->out_color == 1)
        {
            for(int i = 0; i < rga_info->out_height ; i++)
            {
                memcpy(send_buf + gpu_image->crop_rb_offset*rga_info->out_width*rga_info->out_height*3 + rga_info->out_width * i * 3, out_yuv_addr + rga_info->osd_width * i * 3,rga_info->out_width * 3);
            }
            event_param.frame_size = rga_info->out_width * rga_info->out_height *3;
        }
        else if(rga_info->out_color == 2)
        {
            for(int i = 0; i < rga_info->out_height ; i++)
            {
                memcpy(send_buf + gpu_image->crop_rb_offset*rga_info->out_width*rga_info->out_height*2 + rga_info->out_width * i * 2, out_yuv_addr + rga_info->osd_width * i * 2,rga_info->out_width*2);
            }
            event_param.frame_size = rga_info->out_width * rga_info->out_height*2;
        }
        else
        {
            printf("chn[%d] thread_crop out_color error!!!\n",chn);
        }
        if(!ringbuffer_is_full(rb_t)){
            event_param.frame_offset = gpu_image->crop_rb_offset;
            event_param.chn = chn;
            ringbuffer_put(rb_t, (uint8_t *)&event_param, sizeof(event_param));
            gpu_image->crop_rb_offset += 1;
        }
        else
        {
            SLOGI("[Debug] %s ringbuffer_is_full\n", __func__);
        }
    }
stop:
    printf("chn[%d] crop finish!!!\n",chn);
    return NULL;
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
    send_buf = align_write_buf_get(chn);
    rb_t = GetOutPutRingBufferPt(chn);
    data_stream_t* data_stream = data_stream_get();
    image_chn_info_t* image_chn_info = data_stream->image_chn_info[chn];
    rga_param_t rga_info = image_chn_info->rga_info;
    MpiDecLoopData* data = &image_chn_info->mpp_info.dec_data;
    printf("chn:[%d] %s start\n", chn, __func__);
    //计算每个通道的pcie发送耗时
    gettimeofday(&t1, NULL);
    // u_int8_t* send_tmp = (u_int8_t*)malloc(rga_info.out_width * rga_info.out_height * 2);
    // for(int j = 0;j<rga_info.out_height;j++)
    // {
    //     for(int i = 0;i<rga_info.out_width;i++)
    //     {
    //         send_tmp[j*rga_info.out_width*2 + i*2] = i;
    //     }
    // }
    while(!image_chn_info->pcie_info.send_flag){
        while(ringbuffer_is_empty(rb_t)){
            // pcie_dec_send(send_buf + frame_info.frame_size*frame_info.frame_offset,frame_info.frame_size,chn);
            time.tv_sec=0;
            time.tv_usec=(1000);
            select(0,NULL,NULL,NULL,&time);
            if(image_chn_info->pcie_info.send_flag)
                goto stop;
        }
        fifo_num = (pcie_bar_read_reg(pcie_common_param_reg,0x7d0+chn*4) * 32);
        if(fifo_num > (32*1024*1024))
        {
            time.tv_sec=0;
            time.tv_usec=(1000);
            select(0,NULL,NULL,NULL,&time);
            printf("fifo fulll\n");
            continue;
        }
        ringbuffer_get(rb_t, (uint8_t *)&frame_info, sizeof(image_frame_info_t));
        // pcie_dec_send(send_tmp, frame_info.frame_size,chn);
        pcie_dec_send(send_buf + frame_info.frame_offset*frame_info.frame_size, frame_info.frame_size,chn);
        if(data->fp_output != NULL)
        {
            fwrite(send_buf + frame_info.frame_offset*frame_info.frame_size,frame_info.frame_size,1,data->fp_output);
        }

        if(rga_info.out_height < 4096){
            if(data->fps <= 35)
            {
                time.tv_sec=0;
                time.tv_usec=(29000);
            }
            else
            {
                time.tv_sec=0;
                time.tv_usec=(9000);
            }
        }
        else{
            time.tv_sec=0;
            time.tv_usec=(970000);
        }
        select(0,NULL,NULL,NULL,&time);
        // save_image_data(send_buf,resize_width[chn]*resize_height[chn]*2,"/userdata/sda1/pcie_send.raw");
    }
stop:
    printf("chn[%d] writePL finish!!!\n",chn);
    return NULL;
}

int writePL_chn_destory(unsigned char chn)
{
    struct timeval time_val;
    data_stream_t *data_stream = data_stream_get();
    GPU_Frame_st* gpu_frame_stream = gpu_data_stream_get();
    fpga_chn_image_info_t* chn_image = data_stream->chn_image[chn];
    image_chn_info_t* image_chn_info = data_stream->image_chn_info[chn];
    ring_buffer_t* pcie_data_rb = chn_image->pcie_data_rb;
    ring_buffer_t* image_over_rb = gpu_frame_stream->image_over_rb[chn];
    GPU_Frame_st* frame_stream = gpu_data_stream_get();
    gpu_image_info_t *gpu_image = &frame_stream->gpu_image[chn];

    while(!ringbuffer_is_empty(image_over_rb)){
        // printf("InPutRingBuffer is_empty\n");
        time_val.tv_sec=0;
        time_val.tv_usec=(1000);
        select(0,NULL,NULL,NULL,&time_val);
    }
    while(!ringbuffer_is_empty(pcie_data_rb)){
        // printf("InPutRingBuffer is_empty\n");
        time_val.tv_sec=0;
        time_val.tv_usec=(1000);
        select(0,NULL,NULL,NULL,&time_val);
    }
    if(!image_chn_info->pcie_info.crop_flag){
        image_chn_info->pcie_info.crop_flag = 1;
        printf("chn[%d] crop thread destoryed\n",chn);
    }
    if(!image_chn_info->pcie_info.send_flag){
        image_chn_info->pcie_info.send_flag = 1;
        printf("chn[%d] writePL thread destoryed\n",chn);
    }
    for(int index = 0; index < gpu_image->data_frame_max + 1; index++)
    {
        if(gpu_image->out_yuv_buffer[index]){
            free(gpu_image->out_yuv_buffer[index]);
            gpu_image->out_yuv_buffer[index] = NULL;
        }
    }
    return 0;
}

int writePL_chn_init(unsigned char chn)
{
    data_stream_t* data_stream = data_stream_get();
    image_chn_info_t* image_chn_info = data_stream->image_chn_info[chn];
    image_chn_info->pcie_info.send_flag = 0;
    image_chn_info->pcie_info.crop_flag = 0;
    int ret = 0;
    ret = pthread_create(&image_chn_info->pht_up,NULL,thread_writePL,&image_chn_info->mpp_info.mpp_chn);
    if(ret != 0){
        printf("[%s]Create WritePL thread failed\n",__func__);
        return -1;
    }
    ret = pthread_create(&image_chn_info->pht_crop,NULL,thread_crop,&image_chn_info->mpp_info.mpp_chn);
    if(ret != 0){
        printf("[%s]Create WritePL thread failed\n",__func__);
        return -1;
    }
    return 0;
}

void MpiDecDestroy(){
    for(int chn = 0;chn < DATA_CHANNEL_NUM_MAX;chn++)
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
        gs_data_stream->chn_image[chn] = (fpga_chn_image_info_t*)malloc(sizeof(fpga_chn_image_info_t));
        if(!gs_data_stream->chn_image[chn]){
            printf("[ERROR] chn:%d malloc chn image failed \n", chn);
            ret = -1;
            goto err_1; 
        }
        memset(gs_data_stream->chn_image[chn], 0x0, sizeof(fpga_chn_image_info_t));
 
        gs_data_stream->image_chn_info[chn] = (image_chn_info_t*)malloc(sizeof(image_chn_info_t));
        if(!gs_data_stream->image_chn_info[chn]){
            printf("[ERROR] chn:%d malloc image_chn_info failed \n", chn);
            ret = -1;
            goto err_1; 
        }
        memset(gs_data_stream->image_chn_info[chn], 0x0, sizeof(image_chn_info_t));
        gs_data_stream->image_chn_info[chn]->mpp_info.decode_state = 1;

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