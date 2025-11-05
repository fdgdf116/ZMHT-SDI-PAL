#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/ioctl.h>
#include <sched.h>
#include <dlfcn.h>

#include "gpu_interface.h"
#include "mpi_decode.h"
#include "xutils.h"
#include "log.h"
#include "a818_error.h"

#define ringbuffer_destory ringbuffer_deinit
static GPU_Frame_st *g_gpu_frame_stream = NULL;

GPU_Frame_st* gpu_data_stream_get(void)
{
    return g_gpu_frame_stream;
}

void gpu_osd_to_queue(unsigned char chn, image_frame_info_t *frame_info)
{
    GPU_Frame_st* frame_stream = gpu_data_stream_get();
	ring_buffer_t *rb = frame_stream->osd_data_rb[chn];
	image_frame_info_t event_param, tmp_param;

	if (ringbuffer_is_full(rb)) {					
		//ringbuffer_get(rb, (uint8_t *)&tmp_param, sizeof(tmp_param));
        SLOGI("[Debug] %s ringbuffer_is_full", __func__);
	}

	memcpy(&event_param,frame_info,sizeof(image_frame_info_t));	
	ringbuffer_put(rb, (uint8_t *)&event_param, sizeof(event_param));
}

void gpu_mpp_to_queue(unsigned char chn, image_frame_info_t *frame_info)
{
	GPU_Frame_st* frame_stream = gpu_data_stream_get();
	ring_buffer_t *rb = frame_stream->mpp_data_rb[chn];
	image_frame_info_t event_param, tmp_param;

	if (ringbuffer_is_full(rb)) {					
		ringbuffer_get(rb, (uint8_t *)&tmp_param, sizeof(tmp_param));
		SLOGI("[Debug] %s ringbuffer_is_full", __func__);
	}

	memcpy(&event_param,frame_info,sizeof(image_frame_info_t));
	ringbuffer_put(rb, (uint8_t *)&event_param, sizeof(event_param));
}

/*******************************************************/
int gpu_yuv_to_rgb(uint8_t* rgb_buffer, uint8_t* yuv_buffer, unsigned int width, unsigned hight)
{
    GPU_Frame_st* frame_stream = gpu_data_stream_get();
    return frame_stream->htr.cooperate_decode_yuv(width,hight, 0, e_yuv_yuyv, yuv_buffer, 0, rgb_buffer, 0);
}


int gpu_cooperate_encode_yuyv(uint8_t* osd_buffer,uint8_t *rgb_buffer, uint8_t* yuv_buffer, unsigned int width, unsigned int height)
{
    GPU_Frame_st* frame_stream = gpu_data_stream_get();
    return frame_stream->htr.cooperate_encode_yuv(width, height, e_yuv_yuyv, (uint16_t*)osd_buffer, rgb_buffer, yuv_buffer, 0);
    // return frame_stream->htr.cooperate_encode_yuv(width, height, e_yuv_nv61, (uint16_t*)osd_buffer, rgb_buffer, yuv_buffer, 0);
}

int gpu_cooperate_encode_nv12(uint8_t* osd_buffer,uint8_t *rgb_buffer, uint8_t* yuv_buffer, unsigned int width, unsigned int height)
{
    GPU_Frame_st* frame_stream = gpu_data_stream_get();
    return frame_stream->htr.cooperate_encode_yuv(width, height, e_yuv_nv12, (uint16_t*)osd_buffer, rgb_buffer, yuv_buffer, 0);
    // return frame_stream->htr.cooperate_encode_yuv(width, height, e_yuv_nv61, (uint16_t*)osd_buffer, rgb_buffer, yuv_buffer, 0);
}

int gpu_cooperate_overlap_osd(uint8_t* osd_buffer,uint8_t *rgb_buffer, uint8_t* out_rgb, unsigned int width, unsigned int height)
{
    GPU_Frame_st* frame_stream = gpu_data_stream_get();
    return frame_stream->htr.cooperate_overlap_osd(width, height, (uint16_t*)osd_buffer, rgb_buffer, out_rgb, 0);
}

void gpu_chn_ringbuffer_is_empty(unsigned char chn)
{
   
    GPU_Frame_st* frame_stream = gpu_data_stream_get();
    while(!ringbuffer_is_empty(frame_stream->osd_data_rb[chn])){
        xt_nanosleep(0, 100);
    }
    while(!ringbuffer_is_empty(frame_stream->image_over_rb[chn])){
        xt_nanosleep(0, 100);
    }
    while(!ringbuffer_is_empty(frame_stream->mpp_data_rb[chn])){
        xt_nanosleep(0, 100);
    }
}

int gpu_lib_init(void)
{
    int ret;
    int chn;
    int index;

    PATH_TO_HETERO* f_path_to_hetero;

    g_gpu_frame_stream = (GPU_Frame_st*)malloc(sizeof(GPU_Frame_st));
    if(!g_gpu_frame_stream){
        printf("[ERROR] malloc GPU frame error \n");
        return A818_GUI_FRAME_MALLOC;
    }

    memset(g_gpu_frame_stream, 0x0, sizeof(GPU_Frame_st));

    void* libhetero = dlopen(RK_GPU_LIB_PATH, RTLD_NOW | RTLD_DEEPBIND);
    if (libhetero == 0) {
        printf("[ERROR] Could not find libhetero.so.\n");
        ret = A818_GUI_DLOPEN;
        goto err_1;
    }

    f_path_to_hetero = (PATH_TO_HETERO*)dlsym(libhetero, "path_to_hetero");
    if (f_path_to_hetero == 0) {
        printf("[ERROR] Loading function failed.\n");
        ret = A818_GUI_DLSYM;
        goto err_1;
    } 

    g_gpu_frame_stream->htr.library_version =HETERO_INTERFACE_VER;
        // 联通异构计算库
    if (f_path_to_hetero(&(g_gpu_frame_stream->htr)) < 0) {
        printf("[ERROR] Library hetero interface not match.\n");
        ret = A818_GUI_LIBRARY;
        goto err_1;
    }

    // 获取异构计算环境句柄
    g_gpu_frame_stream->hetero_hdr = g_gpu_frame_stream->htr.compute_acquire(0);
    if (g_gpu_frame_stream->hetero_hdr == 0) {
        printf("[ERROR] get htr compute acquire error.\n");
        ret = A818_GUI_GET_HDL;
        goto err_1;
    }

    /* 对每个通道的队列进行初始化 */
    for(chn = 0; chn < DATA_CHANNEL_NUM_MAX; chn++){
        g_gpu_frame_stream->osd_data_rb[chn]= ringbuffer_create(RINGBUFFER_FRAME_MAX*sizeof(image_frame_info_t),sizeof(image_frame_info_t));
        if (!g_gpu_frame_stream->osd_data_rb[chn]) {
            printf("ERROR] create osd data rb failed\n");
            ret = A818_GUI_OSD_DATA_RB;
            goto err_2;
        }
        g_gpu_frame_stream->image_over_rb[chn]= ringbuffer_create(RINGBUFFER_FRAME_MAX*sizeof(image_frame_info_t),sizeof(image_frame_info_t));
        if (!g_gpu_frame_stream->image_over_rb[chn]) {
            printf("ERROR] create osd data rb failed\n");
            ret = A818_GUI_OSD_DATA_RB;
            goto err_3;
        }
		g_gpu_frame_stream->mpp_data_rb[chn]= ringbuffer_create(RINGBUFFER_FRAME_MAX*sizeof(image_frame_info_t), sizeof(image_frame_info_t));
		if (!g_gpu_frame_stream->mpp_data_rb[chn]) {
			printf("ERROR] create mpp data rb failed\n");
			ret = A818_GUI_MPP_DATA_RB;
			goto err_4;
		}
    }
    return A818_SUCCESS;
err_4:
    for(chn = 0; chn < DATA_CHANNEL_NUM_MAX; chn++){
        if(g_gpu_frame_stream->mpp_data_rb[chn]){
            ringbuffer_destory(g_gpu_frame_stream->mpp_data_rb[chn]); 
            g_gpu_frame_stream->mpp_data_rb[chn] = NULL ;
        }
    }
err_3:
    for(chn = 0; chn < DATA_CHANNEL_NUM_MAX; chn++){
        if(g_gpu_frame_stream->image_over_rb[chn]){
            ringbuffer_destory(g_gpu_frame_stream->image_over_rb[chn]); 
            g_gpu_frame_stream->image_over_rb[chn] = NULL;
        }
    }
err_2:
    for(chn = 0; chn < DATA_CHANNEL_NUM_MAX; chn++){
        if(g_gpu_frame_stream->osd_data_rb[chn]){
            ringbuffer_destory(g_gpu_frame_stream->osd_data_rb[chn]); 
            g_gpu_frame_stream->osd_data_rb[chn] = NULL;
        }
    }

    g_gpu_frame_stream->htr.compute_release(g_gpu_frame_stream->hetero_hdr);
err_1:
    free(g_gpu_frame_stream);
    g_gpu_frame_stream = NULL;
    return ret;
}

/*
void gpu_lib_destory(void)
GPU 库相关的释放
*/
void gpu_lib_destory(void)
{
    int chn;
    GPU_Frame_st* gpu_frame_stream = gpu_data_stream_get();
    if(gpu_frame_stream){
        for(chn = 0; chn < DATA_CHANNEL_NUM_MAX; chn++){
            if(gpu_frame_stream->osd_data_rb[chn]){
                ringbuffer_destory(gpu_frame_stream->osd_data_rb[chn]); 
                gpu_frame_stream->osd_data_rb[chn] = NULL;
            }
        }
        
        gpu_frame_stream->htr.compute_release(gpu_frame_stream->hetero_hdr);
        free(gpu_frame_stream);
        gpu_frame_stream = NULL;
    }

    return ;
}
