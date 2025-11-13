/*
 * @author       : Xyl
 * @FilePath     : mpi_decode.c
 * @version      : v1.0
 * @Date         : 2024-02-06 09:14:19
 * @LastEditors  : Xyl
 * @LastEditTime : 2024-06-07 10:06:55
 * @Description  : 
 * Copyright (c) 2024 by Xyl, All Rights Reserved. 
 */
#include <string.h>
#include "mpi_decode.h"
#include "data_engine.h"
#include "rga_engine.h"
#include "ringbuffer.h"
#include "pthread.h"
#include "xutils.h"
#include "fps_calc.h"
#include "log.h"
// #include "types_def.h"

struct timespec start_tm,end_tm;
unsigned char output_filename[256];
unsigned char input_filename[256];

/**
 * @brief        设置输出文件名
 * @param        {unsigned char} *name 文件名
 * @return       {*}
 */
void SetOutputFilename(unsigned char *name){
    memset(output_filename,0,sizeof(output_filename));
	sprintf(output_filename,"%s",name);
}
/**
 * @brief        设置输入文件名(解码文件名)
 * @param        {unsigned char} *name 文件名
 * @return       {*}
 */
void SetInputFilename(unsigned char *name){
	sprintf(input_filename,"%s",name);
    printf("set decode file name [%s] \n",input_filename);
}

 void DumpYUYVtoFile(MppFrame frame,int chn){
    RK_U32 width    = 0;
    RK_U32 height   = 0;
    RK_U32 h_stride = 0;
    RK_U32 v_stride = 0;
    MppFrameFormat fmt  = MPP_FMT_YUV420SP;
    MppBuffer buffer    = NULL;
    RK_U8 *base = NULL;
    image_frame_info_t frame_info = {0};
    image_frame_info_t event_param = {0};
    struct timeval time;
    float buff_size = 0;
    data_stream_t* data_stream = data_stream_get();
    image_chn_info_t* image_chn_info = data_stream->image_chn_info[chn];
    fpga_chn_image_info_t *chn_image = data_stream->chn_image[chn];
    mpp_decode_info_t* mpp_info = data_stream->mpp_decode_info[chn%MPP_ENCODE_NTHREAD_MAX];

    if ( NULL == frame)
        return ;

    width    = mpp_frame_get_width(frame);
    height   = mpp_frame_get_height(frame);
    h_stride = mpp_frame_get_hor_stride(frame);
    v_stride = mpp_frame_get_ver_stride(frame);
    fmt      = mpp_frame_get_fmt(frame);
    buffer   = mpp_frame_get_buffer(frame);

    if (NULL == buffer)
        return ;
    base = (RK_U8 *)mpp_buffer_get_ptr(buffer);
    if (MPP_FRAME_FMT_IS_RGB(fmt) && MPP_FRAME_FMT_IS_LE(fmt)) {
        fmt &= MPP_FRAME_FMT_MASK;
    }
    ring_buffer_t *rb_t = GetInPutRingBufferPt(chn);
    switch (fmt & MPP_FRAME_FMT_MASK) {
        case MPP_FMT_YUV420SP : {
            RK_U32 i;
            RK_U8 *base_y = base;
            RK_U8 *base_c = base + h_stride * v_stride;
            while((!mpp_info->decode_state) && ringbuffer_is_full(rb_t)){
                // printf("InPutRingBuffer is_full\n");
                time.tv_sec=0;
                time.tv_usec=(1000);
                select(0,NULL,NULL,NULL,&time);
            }
            if(chn_image->rb_offset >= (chn_image->data_frame_max+1)){
                chn_image->rb_offset = 0;
            }
            uint8_t * chn_image_buf = chn_image->chn_data.yuv420_data[chn_image->rb_offset].vir_addr;
            if(!chn_image_buf){
                printf("-2-CHN:%d malloc buff error \n", chn);
                return ;
            }
            chn_image->chn_data.data_offset = 0;
            for (i = 0; i < height; i++, base_y += h_stride) {
                memcpy(chn_image_buf+chn_image->chn_data.data_offset,base_y,width);
                chn_image->chn_data.data_offset += width;
            }
            for (i = 0; i < height / 2; i++, base_c += h_stride) {
                memcpy(chn_image_buf+chn_image->chn_data.data_offset,base_c,width);
                chn_image->chn_data.data_offset += width;
            }
            if(!ringbuffer_is_full(rb_t)){
                event_param.frame_offset = chn_image->rb_offset;
                //chn_frame_info.frame_index = chn_image->rb_offset;
                event_param.frame_size = chn_image->chn_data.data_offset;
                ringbuffer_put(rb_t, (uint8_t *)&event_param, sizeof(event_param));
                chn_image->rb_offset += 1;
            }
            else
            {
                SLOGI("[debug] %s ringbuffer is full, chn:%d\n", __func__, chn);
            }
        }
    }
}

static int decode_simple(MpiDecLoopData *data)
{
    RK_U32 pkt_done = 0;
    RK_U32 err_info = 0;
	
    MPP_RET ret = MPP_OK;
    MppCtx ctx  = data->ctx;
    MppApi *mpi = data->mpi;
    char   *buf = data->buf;
    int chn = data->chn;
    data_stream_t* data_stream = data_stream_get();
    image_chn_info_t* image_chn_info = data_stream->image_chn_info[chn];
    mpp_decode_info_t* mpp_info = data_stream->mpp_decode_info[chn%MPP_ENCODE_NTHREAD_MAX];

    MppPacket packet = data->packet;
    MppFrame  frame  = NULL;
    size_t read_size = fread(buf, 1, data->packet_size, data->fp_input);
    // printf("packet_size:%d,read_size:%d\n",data->packet_size,read_size);
    if (read_size != data->packet_size || feof(data->fp_input)) {
        printf("found last packet\n");
        rewind(data->fp_input);
        read_size = fread(buf, 1, data->packet_size, data->fp_input);
        // data->eos = 1;
    }
    mpp_packet_write(packet, 0, buf, read_size);
    mpp_packet_set_pos(packet, buf);
    mpp_packet_set_length(packet, read_size);
    if (data->eos){
        mpp_packet_set_eos(packet);
    }
    do {
        if (!pkt_done) {
            ret = mpi->decode_put_packet(ctx, packet);
            if (MPP_OK == ret){
                pkt_done = 1;
			}
        }
 
        do {
            RK_S32 get_frm = 0;
            RK_U32 frm_eos = 0;
            ret = mpi->decode_get_frame(ctx, &frame);
    
            if (frame) {
                if (mpp_frame_get_info_change(frame)){
                    RK_U32 width = mpp_frame_get_width(frame);
                    RK_U32 height = mpp_frame_get_height(frame);
                    RK_U32 hor_stride = mpp_frame_get_hor_stride(frame);
                    RK_U32 ver_stride = mpp_frame_get_ver_stride(frame);
                    RK_U32 buf_size = mpp_frame_get_buf_size(frame);
                    printf("decoder require buffer w:h [%d:%d] stride [%d:%d] buf_size %d\n", width, height, hor_stride, ver_stride, buf_size);  
                    printf("%s:%d\n",__func__,__LINE__);
                
                    if (NULL == data->frm_grp) {
                        ret = mpp_buffer_group_get_internal(&data->frm_grp, MPP_BUFFER_TYPE_ION);
                        if (ret) {
                            printf("get mpp buffer group failed ret %d\n", ret);
                            break;
                        }							
                        ret = mpi->control(ctx, MPP_DEC_SET_EXT_BUF_GROUP, data->frm_grp);
                        if (ret) {
                            printf("set buffer group failed ret %d\n", ret);
                            break;
                        }
                    } else {                      
                        ret = mpp_buffer_group_clear(data->frm_grp);
                        if (ret){
                            printf("clear buffer group failed ret %d\n", ret);
                            break;
                        }
                    }
                    
                    ret = mpp_buffer_group_limit_config(data->frm_grp, buf_size, 24);
                    if (ret) {
                        printf("limit buffer group failed ret %d\n", ret);
                        break;
                    }                  
                    ret = mpi->control(ctx, MPP_DEC_SET_INFO_CHANGE_READY, NULL);
                    if (ret) {
                        printf("info change ready failed ret %d\n", ret);
                        break;
                    }
                }else{
                    err_info = mpp_frame_get_errinfo(frame) | mpp_frame_get_discard(frame);
                    data->frame_count++;
                    if (!err_info){
                        DumpYUYVtoFile(frame,chn+2);
					}
                    if(mpp_info->decode_state){
                        printf("decode_chn[%d] stop!\n",chn);
                        mpp_frame_deinit(&frame);
                        frame = NULL;
                        return -1;
                    }
				}
                frm_eos = mpp_frame_get_eos(frame);
                mpp_frame_deinit(&frame);
                frame = NULL;
                get_frm = 1;
			}
           
            if (data->frm_grp) {
                size_t usage = mpp_buffer_group_usage(data->frm_grp);
                if (usage > data->max_usage){
                    data->max_usage = usage;
				}	
            }
         
            if (data->eos && pkt_done && !frm_eos) {
                msleep(10);
                continue;
            }
            if (frm_eos) {
                printf("found last frame\n");
                break;
            }
            if (data->frame_num && data->frame_count >= data->frame_num) {
                data->eos = 1;
                break;
            }
            if (get_frm){
                continue;
			}
            break;
        } while (1);
 
        if (data->frame_num && data->frame_count >= data->frame_num){
            data->eos = 1;
            printf("reach max frame number %d\n", data->frame_count);
            break;
        }
        if (pkt_done){
            break;
		}
		
    } while (1);
    return ret;
}
 
// MpiDecLoopData * GetMpiData(int chn){
//     return &gs_data_stream->image_chn_info[chn].dec_data;
// }

void *thread_decode(void *arg){
    pthread_detach(pthread_self());
    int chn = *(int*)arg;
    int ret;
    printf("thread_decode chn :%d\n",chn);
        
    data_stream_t* data_stream = data_stream_get();
    image_chn_info_t* image_chn_info = data_stream->image_chn_info[chn];
    fpga_chn_image_info_t *chn_image = data_stream->chn_image[chn];
    mpp_decode_info_t* mpp_info = data_stream->mpp_decode_info[chn%MPP_ENCODE_NTHREAD_MAX];
    MpiDecLoopData * data = &mpp_info->dec_data;
    printf("chn:%d %s %d mpi_data:%p\n",chn,__func__,__LINE__, data);
    /* 获取文件大小 */
    // size_t file_size = 0;
    // fseek(data->fp_input, 0L, SEEK_END);
	// file_size = ftell(data->fp_input);
    // rewind(data->fp_input);
    // printf("input file size %ld\n", file_size);
    while (!data->eos)
	{
        ret = decode_simple(data);
        if(ret){
            break;
        }
    }
err:
    ret = data->mpi->reset(data->ctx);
    if (MPP_OK != ret) 
    {
        printf("mpi->reset failed\n");
    }
    printf("thread_decode chn:%d exit\n",chn);
    pthread_exit(NULL);
}
int mpi_dec_chn_init(int chn)
{
    MPP_RET ret         = MPP_OK;
    MppParam param      = NULL;
    RK_U32 need_split   = 1;

    data_stream_t* data_stream = data_stream_get();
    mpp_decode_info_t* mpp_info = data_stream->mpp_decode_info[chn];

    MpiDecLoopData * data = &mpp_info->dec_data;
    
    data->chn           = chn;
    data->eos            = 0;
    data->packet_size    = MPI_DEC_STREAM_SIZE;
    data->frame_count    = 0;

    data->fps = mpp_info->fps;

    /* 指定解码文件和输出文件 */
// #if TEST
//     data->frame_num		= 1; //指定解码帧数，0为解析整个文件
// #else
	data->frame_num		= 0; //指定解码帧数，0为解析整个文件
// #endif
    if ( NULL == data->fp_input ) {
        printf("failed to open input/output file \n");
        return -1;
    }

    data->buf = mpp_malloc(char, data->packet_size);
    ret = mpp_packet_init(&data->packet, data->buf, data->packet_size);
    if(MPP_OK != ret){
        printf("mpp_packet_init error\n");
        goto MPP_TEST_OUT;
    }
    
    ret = mpp_create(&data->ctx, &data->mpi);
    if(MPP_OK != ret){
        printf("mpp_create error\n");
        goto MPP_TEST_OUT;
    }

    ret = mpp_init(data->ctx, MPP_CTX_DEC, mpp_info->decode_type);
    if (MPP_OK != ret) 
    {
        printf("mpp_init failed\n");
        goto MPP_TEST_OUT;
    }

    param = &need_split;
    
    ret = data->mpi->control(data->ctx, MPP_DEC_SET_PARSER_SPLIT_MODE, param);
    if (MPP_OK != ret) {
        printf("mpi->control failed\n");
        goto MPP_TEST_OUT;
    }

    MppFrameFormat format = MPP_FMT_YUV420SP_VU;
    ret = data->mpi->control(data->ctx, MPP_DEC_SET_OUTPUT_FORMAT, &format);
    if (MPP_OK != ret) {
        printf("mpi->control failed\n");
        goto MPP_TEST_OUT;
    }

    mpp_info->decode_state = 0;
    mpp_info->decode_state = pthread_create(&mpp_info->pht_dec, NULL, thread_decode, &mpp_info->mpp_chn);
    if (mpp_info->decode_state) {
        printf("failed to create thread for input ret %d\n", ret);
        ret = -1;
        goto MPP_TEST_OUT;
    }
    return ret;
MPP_TEST_OUT:
    printf("MPP_TEST_OUT MpiDecDestroy!!!!\n");
    MpiDecodeDestroy(chn);
    return ret;
}

void MpiDecodeDestroy(int chn){
    // pthread_cancel(pht_dec[chn]);
    data_stream_t* data_stream = data_stream_get();
    mpp_decode_info_t* mpp_info = data_stream->mpp_decode_info[chn%MPP_ENCODE_NTHREAD_MAX];
    MpiDecLoopData* mpi_data = &mpp_info->dec_data;
    printf("chn:%d %s %d mpi_data:%p\n",chn,__func__,__LINE__, mpi_data);
    if (mpi_data->packet) {
        mpp_packet_deinit(&mpi_data->packet);
        mpi_data->packet = NULL;
    }
 
    if (mpi_data->ctx) {
        mpp_destroy(mpi_data->ctx);
        mpi_data->ctx= NULL;
    }
 
    if (mpi_data->buf) {
        mpp_free(mpi_data->buf);
        mpi_data->buf = NULL;
    }
 
    if (mpi_data->frm_grp) {
        mpp_buffer_group_put(mpi_data->frm_grp);
        mpi_data->frm_grp = NULL;
    }
 
    if (mpi_data->fp_output){
        fflush(mpi_data->fp_output);
        fclose(mpi_data->fp_output);
        mpi_data->fp_output = NULL;
    }
 
    if (mpi_data->fp_input) {
        fflush(mpi_data->fp_input);
        fclose(mpi_data->fp_input);
        mpi_data->fp_input = NULL;
    }
}


