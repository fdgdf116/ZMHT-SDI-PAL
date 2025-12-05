#include <string.h>

#include "mpi_encode.h"
#include "utils.h"
#include "gpu_interface.h"
#include "log.h"
#include "pcie_impl.h"
#include "pcie_reg_rw.h"
#include "xutils.h"
#include "a818_error.h"
#include "crop_engine.h"
#include "gpu_interface.h"

static mpi_encode_info_t *gs_mpi_encode;
#define ringbuffer_destory ringbuffer_deinit

const char *encode_name[MPP_ENCODE_NTHREAD_MAX] = {"yuv_encode0","yuv_encode1"};

static unsigned char test_file_flag = 0;

int  video_data_wait_and_get(uint8_t chn, uint8_t *buf, uint32_t width, uint32_t height,
				   uint32_t hor_stride, uint32_t ver_stride)
{   
	struct timeval time;

	mpi_encode_info_t* mpi_encode = mpi_encode_info_get();
	GPU_Frame_st* gpu_frame_stream = gpu_data_stream_get();
	data_stream_t *data_stream = data_stream_get();
	image_chn_info_t* image_chn_info = data_stream->image_chn_info[chn];
	ring_buffer_t *rb = gpu_frame_stream->mpp_data_rb[chn];
	image_frame_info_t frame_info = {0}; 
	Crop_Frame_st* crop_stream = crop_data_stream_get();
	crop_image_info_t* crop_image = &crop_stream->crop_image[chn];

	if((!mpi_encode) || (!gpu_frame_stream) || (!rb)){
		SLOGI("[Debug] %s get buff error \n", __func__);
		return 0;
	}
	while(ringbuffer_is_empty(rb)){
		time.tv_sec=0;
		time.tv_usec=(10);
		select(0,NULL,NULL,NULL,&time); 
		if(mpi_encode->image_encode[chn].encode_flag){
			return 1;
		}       
	}

	ringbuffer_get(rb, (uint8_t *)&frame_info, sizeof(image_frame_info_t));
	// memcpy(buf,crop_image->crop_yuv_buffer[frame_info.frame_offset],width*height*3/2);
	for(int i = 0; i < height; i++){
		memcpy(buf + i*hor_stride,crop_image->crop_yuv_buffer[frame_info.frame_offset] + i*width,width);
	}
	for(int i = 0; i < height/2; i++){
		memcpy(buf + hor_stride*ver_stride + i*hor_stride,crop_image->crop_yuv_buffer[frame_info.frame_offset] + width*height + i*width,width);
	}
	// save_image_data((char*)buf, hor_stride*ver_stride*3/2, "/tmp/mpp_input.raw");
	return 0;
}



mpi_encode_info_t* mpi_encode_info_get(void)
{
	return gs_mpi_encode;
}



static void mpp_data_to_queue(unsigned char chn, image_frame_info_t *frame_info)
{
	image_frame_info_t frame_param, tmp_param;
	mpi_encode_info_t* mpi_encode = mpi_encode_info_get();
	ring_buffer_t *rb =mpi_encode->mpp_encode_rb[chn];


	if (ringbuffer_is_full(rb)) {					
		ringbuffer_get(rb, (uint8_t *)&tmp_param, sizeof(tmp_param));
		SLOGI("[Debug] %s chn:%d ringbuffer_is_full\n", __func__, chn);
	}

	frame_param.chn = frame_info->chn;
	frame_param.frame_offset = frame_info->frame_offset;
	frame_param.frame_index = frame_info->frame_index;
	frame_param.frame_size = frame_info->frame_size;

	ringbuffer_put(rb, (uint8_t *)&frame_param, sizeof(frame_param));
}

static void mpp_encode_output(unsigned char chn,unsigned char* pkt_data,int pkt_size)
{
    unsigned int rem;
    unsigned int byte_count;
    unsigned int memcpy_len;
    image_frame_info_t frame_param;

    mpi_encode_info_t* mpi_encode = mpi_encode_info_get();
    mpi_image_encode_info_t *image_encode = &mpi_encode->image_encode[chn];

    byte_count = MPP_OUTPUT_FRAME_SIZE; /*发送给上位机的一帧数据的最大数据量*/
    memcpy_len = 0;
    while(1){
        rem = pkt_size - memcpy_len;
        if(rem == 0){
            break;
        }

        if(rem < MPP_OUTPUT_FRAME_SIZE){
            byte_count = rem;
        }

        if ((image_encode->encode_data_offset + pkt_size) >= image_encode->encode_total_size) {
            image_encode->encode_data_offset = 0;
        }

        // SLOGI("[Debug] %s chn:%d encode_data_offset:%d encode_total_size:%d \n", __func__, chn, 
        //     image_encode->encode_data_offset, image_encode->encode_total_size);

        memcpy(image_encode->encode_data + image_encode->encode_data_offset, pkt_data + memcpy_len, byte_count);

        frame_param.frame_offset = image_encode->encode_data_offset; 
        frame_param.frame_size  = byte_count;
        frame_param.frame_index = image_encode->frame_index;
        frame_param.chn = image_encode->image_chn;
        
        mpp_data_to_queue(chn,&frame_param);
        image_encode->encode_data_offset += byte_count;
        image_encode->frame_index += 1;

        memcpy_len += byte_count;
    }
}

static MPP_RET encode_app_ctx_init(MpiEncMultiCtxInfo *info)
{
	MpiEncAppArgs *cmd = info->cmd;
	MpiEncAppData *p = &info->ctx;
	MPP_RET ret = MPP_OK;

	// get paramter from cmd
	p->width        = cmd->width;
	p->height       = cmd->height;
	p->hor_stride   = (cmd->hor_stride) ? (cmd->hor_stride) :
					  (MPP_ALIGN(cmd->width, 16));
	p->ver_stride   = (cmd->ver_stride) ? (cmd->ver_stride) :
					  (MPP_ALIGN(cmd->height, 16));
	p->fmt          = cmd->format;
	p->type         = cmd->type;
	p->bps          = cmd->bps_target;
	p->bps_min      = cmd->bps_min;
	p->bps_max      = cmd->bps_max;
	p->rc_mode      = cmd->rc_mode;
	p->frame_num    = cmd->frame_num;
	// if (cmd->type == MPP_VIDEO_CodingMJPEG && p->frame_num == 0) {
	// 	SLOGI("jpege default encode only one frame. Use -n [num] for rc case\n");
	// 	p->frame_num = 1;
	// }
	p->gop_mode     = cmd->gop_mode;
	p->gop_len      = cmd->gop_len;
	p->vi_len       = cmd->vi_len;

	p->fps_in_flex  = cmd->fps_in_flex;
	p->fps_in_den   = cmd->fps_in_den;
	p->fps_in_num   = cmd->fps_in_num;
	p->fps_out_flex = cmd->fps_out_flex;
	p->fps_out_den  = cmd->fps_out_den;
	p->fps_out_num  = cmd->fps_out_num;
	p->mdinfo_size  = (MPP_VIDEO_CodingHEVC == cmd->type) ?
					  (MPP_ALIGN(p->hor_stride, 32) >> 5) *
					  (MPP_ALIGN(p->ver_stride, 32) >> 5) * 16 :
					  (MPP_ALIGN(p->hor_stride, 64) >> 6) *
					  (MPP_ALIGN(p->ver_stride, 16) >> 4) * 16;

	if (cmd->file_input) {        
		p->fp_input = fopen(cmd->file_input, "rb");
		if (NULL == p->fp_input) {
			SLOGI("Error:failed to open input file %s\n", cmd->file_input);            
		}
	}

	if (cmd->file_output) {
		p->fp_output = fopen(cmd->file_output, "w+b");
		if (NULL == p->fp_output) {
			SLOGI("Error:failed to open output file %s\n", cmd->file_output);
			ret = MPP_ERR_OPEN_FILE;
		}
	}

	// update resource parameter
	switch (p->fmt & MPP_FRAME_FMT_MASK) {
	case MPP_FMT_YUV420SP:
	case MPP_FMT_YUV420P: {
		p->frame_size = MPP_ALIGN(p->hor_stride, 64) * MPP_ALIGN(p->ver_stride, 64) * 3 / 2;
	} break;

	case MPP_FMT_YUV422_YUYV :
	case MPP_FMT_YUV422_YVYU :
	case MPP_FMT_YUV422_UYVY :
	case MPP_FMT_YUV422_VYUY :
	case MPP_FMT_YUV422P :
	case MPP_FMT_YUV422SP : {
		p->frame_size = MPP_ALIGN(p->hor_stride, 64) * MPP_ALIGN(p->ver_stride, 64) * 2;
	} break;
	case MPP_FMT_RGB444 :
	case MPP_FMT_BGR444 :
	case MPP_FMT_RGB555 :
	case MPP_FMT_BGR555 :
	case MPP_FMT_RGB565 :
	case MPP_FMT_BGR565 :
	case MPP_FMT_RGB888 :
	case MPP_FMT_BGR888 :
	case MPP_FMT_RGB101010 :
	case MPP_FMT_BGR101010 :
	case MPP_FMT_ARGB8888 :
	case MPP_FMT_ABGR8888 :
	case MPP_FMT_BGRA8888 :
	case MPP_FMT_RGBA8888 : {
		p->frame_size = MPP_ALIGN(p->hor_stride, 64) * MPP_ALIGN(p->ver_stride, 64);
	} break;

	default: {
		p->frame_size = MPP_ALIGN(p->hor_stride, 64) * MPP_ALIGN(p->ver_stride, 64) * 4;
	} break;
	}

	if (MPP_FRAME_FMT_IS_FBC(p->fmt)) {
		if ((p->fmt & MPP_FRAME_FBC_MASK) == MPP_FRAME_FBC_AFBC_V1)
			p->header_size = MPP_ALIGN(MPP_ALIGN(p->width, 16) * MPP_ALIGN(p->height, 16) / 16, SZ_4K);
		else
			p->header_size = MPP_ALIGN(p->width, 16) * MPP_ALIGN(p->height, 16) / 16;
	} else {
		p->header_size = 0;
	}

	return ret;
}

static MPP_RET app_ctx_deinit(MpiEncAppData *p)
{
	if (p) {        
		if (p->fp_input) {
			fclose(p->fp_input);
			p->fp_input = NULL;
		}
		if (p->fp_output) {
			fclose(p->fp_output);
			p->fp_output = NULL;
		}        
	}

	return MPP_OK;
}

static MPP_RET app_mpp_enc_cfg_setup(MpiEncMultiCtxInfo *info)
{
	MpiEncAppArgs *cmd = info->cmd;
	MpiEncAppData *p = &info->ctx;
	MppApi *mpi = p->mpi;
	MppCtx ctx = p->ctx;
	MppEncCfg cfg = p->cfg;
	RK_U32 quiet = cmd->quiet;
	MPP_RET ret;
	RK_U32 rotation;
	RK_U32 mirroring;
	RK_U32 flip;
	RK_U32 gop_mode = p->gop_mode;
	MppEncRefCfg ref = NULL;

	/* setup default parameter */
	if (p->fps_in_den == 0)
		p->fps_in_den = 1;
	if (p->fps_in_num == 0)
		p->fps_in_num = 30;
	if (p->fps_out_den == 0)
		p->fps_out_den = 1;
	if (p->fps_out_num == 0)
		p->fps_out_num = 30;

	if (!p->bps)
		p->bps = p->width * p->height / 8 * (p->fps_out_num / p->fps_out_den);

	mpp_enc_cfg_set_s32(cfg, "prep:width", p->width);
	mpp_enc_cfg_set_s32(cfg, "prep:height", p->height);
	mpp_enc_cfg_set_s32(cfg, "prep:hor_stride", p->hor_stride);
	mpp_enc_cfg_set_s32(cfg, "prep:ver_stride", p->ver_stride);
	mpp_enc_cfg_set_s32(cfg, "prep:format", p->fmt);

	mpp_enc_cfg_set_s32(cfg, "rc:mode", p->rc_mode);

	/* fix input / output frame rate */
	mpp_enc_cfg_set_s32(cfg, "rc:fps_in_flex", p->fps_in_flex);
	mpp_enc_cfg_set_s32(cfg, "rc:fps_in_num", p->fps_in_num);
	mpp_enc_cfg_set_s32(cfg, "rc:fps_in_denorm", p->fps_in_den);
	mpp_enc_cfg_set_s32(cfg, "rc:fps_out_flex", p->fps_out_flex);
	mpp_enc_cfg_set_s32(cfg, "rc:fps_out_num", p->fps_out_num);
	mpp_enc_cfg_set_s32(cfg, "rc:fps_out_denorm", p->fps_out_den);

	/* drop frame or not when bitrate overflow */
	mpp_enc_cfg_set_u32(cfg, "rc:drop_mode", MPP_ENC_RC_DROP_FRM_DISABLED);
	mpp_enc_cfg_set_u32(cfg, "rc:drop_thd", 20);        /* 20% of max bps */
	mpp_enc_cfg_set_u32(cfg, "rc:drop_gap", 1);         /* Do not continuous drop frame */

	/* setup bitrate for different rc_mode */
	mpp_enc_cfg_set_s32(cfg, "rc:bps_target", p->bps);
	switch (p->rc_mode) {
	case MPP_ENC_RC_MODE_FIXQP : {
		/* do not setup bitrate on FIXQP mode */
	} break;
	case MPP_ENC_RC_MODE_CBR : {
		/* CBR mode has narrow bound */
		mpp_enc_cfg_set_s32(cfg, "rc:bps_max", p->bps_max ? p->bps_max : p->bps * 17 / 16);
		mpp_enc_cfg_set_s32(cfg, "rc:bps_min", p->bps_min ? p->bps_min : p->bps * 15 / 16);
	} break;
	case MPP_ENC_RC_MODE_VBR :
	case MPP_ENC_RC_MODE_AVBR : {
		/* VBR mode has wide bound */
		mpp_enc_cfg_set_s32(cfg, "rc:bps_max", p->bps_max ? p->bps_max : p->bps * 17 / 16);
		mpp_enc_cfg_set_s32(cfg, "rc:bps_min", p->bps_min ? p->bps_min : p->bps * 1 / 16);
	} break;
	default : {
		/* default use CBR mode */
		mpp_enc_cfg_set_s32(cfg, "rc:bps_max", p->bps_max ? p->bps_max : p->bps * 17 / 16);
		mpp_enc_cfg_set_s32(cfg, "rc:bps_min", p->bps_min ? p->bps_min : p->bps * 15 / 16);
	} break;
	}

	/* setup qp for different codec and rc_mode */
	switch (p->type) {
	case MPP_VIDEO_CodingAVC :
	case MPP_VIDEO_CodingHEVC : {
		switch (p->rc_mode) {
		case MPP_ENC_RC_MODE_FIXQP : {
			RK_S32 fix_qp = cmd->qp_init;

			mpp_enc_cfg_set_s32(cfg, "rc:qp_init", fix_qp);
			mpp_enc_cfg_set_s32(cfg, "rc:qp_max", fix_qp);
			mpp_enc_cfg_set_s32(cfg, "rc:qp_min", fix_qp);
			mpp_enc_cfg_set_s32(cfg, "rc:qp_max_i", fix_qp);
			mpp_enc_cfg_set_s32(cfg, "rc:qp_min_i", fix_qp);
			mpp_enc_cfg_set_s32(cfg, "rc:qp_ip", 0);
		} break;
		case MPP_ENC_RC_MODE_CBR :
		case MPP_ENC_RC_MODE_VBR :
		case MPP_ENC_RC_MODE_AVBR : {
			mpp_enc_cfg_set_s32(cfg, "rc:qp_init", cmd->qp_init ? cmd->qp_init : -1);
			mpp_enc_cfg_set_s32(cfg, "rc:qp_max", cmd->qp_max ? cmd->qp_max : 51);
			mpp_enc_cfg_set_s32(cfg, "rc:qp_min", cmd->qp_min ? cmd->qp_min : 10);
			mpp_enc_cfg_set_s32(cfg, "rc:qp_max_i", cmd->qp_max_i ? cmd->qp_max_i : 51);
			mpp_enc_cfg_set_s32(cfg, "rc:qp_min_i", cmd->qp_min_i ? cmd->qp_min_i : 10);
			mpp_enc_cfg_set_s32(cfg, "rc:qp_ip", 2);
		} break;
		default : {
			SLOGI("Error:unsupport encoder rc mode %d\n", p->rc_mode);
		} break;
		}
	} break;
	case MPP_VIDEO_CodingVP8 : {
		/* vp8 only setup base qp range */
		mpp_enc_cfg_set_s32(cfg, "rc:qp_init", cmd->qp_init ? cmd->qp_init : 40);
		mpp_enc_cfg_set_s32(cfg, "rc:qp_max",  cmd->qp_max ? cmd->qp_max : 127);
		mpp_enc_cfg_set_s32(cfg, "rc:qp_min",  cmd->qp_min ? cmd->qp_min : 0);
		mpp_enc_cfg_set_s32(cfg, "rc:qp_max_i", cmd->qp_max_i ? cmd->qp_max_i : 127);
		mpp_enc_cfg_set_s32(cfg, "rc:qp_min_i", cmd->qp_min_i ? cmd->qp_min_i : 0);
		mpp_enc_cfg_set_s32(cfg, "rc:qp_ip", 6);
	} break;
	case MPP_VIDEO_CodingMJPEG : {
		/* jpeg use special codec config to control qtable */
		mpp_enc_cfg_set_s32(cfg, "jpeg:q_factor", cmd->qp_init ? cmd->qp_init : 80);
		mpp_enc_cfg_set_s32(cfg, "jpeg:qf_max", cmd->qp_max ? cmd->qp_max : 99);
		mpp_enc_cfg_set_s32(cfg, "jpeg:qf_min", cmd->qp_min ? cmd->qp_min : 1);
	} break;
	default : {
	} break;
	}

	/* setup codec  */
	mpp_enc_cfg_set_s32(cfg, "codec:type", p->type);
	switch (p->type) {
	case MPP_VIDEO_CodingAVC : {
		RK_U32 constraint_set;

		/*
		 * H.264 profile_idc parameter
		 * 66  - Baseline profile
		 * 77  - Main profile
		 * 100 - High profile
		 */
		mpp_enc_cfg_set_s32(cfg, "h264:profile", 100);
		/*
		 * H.264 level_idc parameter
		 * 10 / 11 / 12 / 13    - qcif@15fps / cif@7.5fps / cif@15fps / cif@30fps
		 * 20 / 21 / 22         - cif@30fps / half-D1@@25fps / D1@12.5fps
		 * 30 / 31 / 32         - D1@25fps / 720p@30fps / 720p@60fps
		 * 40 / 41 / 42         - 1080p@30fps / 1080p@30fps / 1080p@60fps
		 * 50 / 51 / 52         - 4K@30fps
		 */
		mpp_enc_cfg_set_s32(cfg, "h264:level", 40);
		mpp_enc_cfg_set_s32(cfg, "h264:cabac_en", 1);
		mpp_enc_cfg_set_s32(cfg, "h264:cabac_idc", 0);
		mpp_enc_cfg_set_s32(cfg, "h264:trans8x8", 1);

		mpp_env_get_u32("constraint_set", &constraint_set, 0);
		if (constraint_set & 0x3f0000)
			mpp_enc_cfg_set_s32(cfg, "h264:constraint_set", constraint_set);
	} break;
	case MPP_VIDEO_CodingHEVC :
	case MPP_VIDEO_CodingMJPEG :
	case MPP_VIDEO_CodingVP8 : {
	} break;
	default : {
		SLOGI("Error:unsupport encoder coding type %d\n", p->type);
	} break;
	}

	p->split_mode = 0;
	p->split_arg = 0;
	p->split_out = 0;

	mpp_env_get_u32("split_mode", &p->split_mode, MPP_ENC_SPLIT_NONE);
	mpp_env_get_u32("split_arg", &p->split_arg, 0);
	mpp_env_get_u32("split_out", &p->split_out, 0);

	if (p->split_mode) {
		SLOGI("%p split mode %d arg %d out %d\n", ctx,
				  p->split_mode, p->split_arg, p->split_out);
		mpp_enc_cfg_set_s32(cfg, "split:mode", p->split_mode);
		mpp_enc_cfg_set_s32(cfg, "split:arg", p->split_arg);
		mpp_enc_cfg_set_s32(cfg, "split:out", p->split_out);
	}

	mpp_env_get_u32("mirroring", &mirroring, 0);
	mpp_env_get_u32("rotation", &rotation, 0);
	mpp_env_get_u32("flip", &flip, 0);

	mpp_enc_cfg_set_s32(cfg, "prep:mirroring", mirroring);
	mpp_enc_cfg_set_s32(cfg, "prep:rotation", rotation);
	mpp_enc_cfg_set_s32(cfg, "prep:flip", flip);

	// config gop_len and ref cfg
	mpp_enc_cfg_set_s32(cfg, "rc:gop", p->gop_len ? p->gop_len : p->fps_out_num * 2);

	mpp_env_get_u32("gop_mode", &gop_mode, gop_mode);

	if (gop_mode) {
		mpp_enc_ref_cfg_init(&ref);

		if (p->gop_mode < 4)
			mpi_enc_gen_ref_cfg(ref, gop_mode);
		else
			mpi_enc_gen_smart_gop_ref_cfg(ref, p->gop_len, p->vi_len);

		mpp_enc_cfg_set_ptr(cfg, "rc:ref_cfg", ref);
	}

	ret = mpi->control(ctx, MPP_ENC_SET_CFG, cfg);
	if (ret) {
		SLOGI("Error:mpi control enc set cfg failed ret %d\n", ret);
		goto RET;
	}

	if (ref)
		mpp_enc_ref_cfg_deinit(&ref);

	/* optional */
	{
		RK_U32 sei_mode;

		mpp_env_get_u32("sei_mode", &sei_mode, MPP_ENC_SEI_MODE_ONE_FRAME);
		p->sei_mode = sei_mode;
		ret = mpi->control(ctx, MPP_ENC_SET_SEI_CFG, &p->sei_mode);
		if (ret) {
			SLOGI("Error:mpi control enc set sei cfg failed ret %d\n", ret);
			goto RET;
		}
	}

	if (p->type == MPP_VIDEO_CodingAVC || p->type == MPP_VIDEO_CodingHEVC) {
		p->header_mode = MPP_ENC_HEADER_MODE_EACH_IDR;
		ret = mpi->control(ctx, MPP_ENC_SET_HEADER_MODE, &p->header_mode);
		if (ret) {
		   SLOGI("Error:mpi control enc set header mode failed ret %d\n", ret);
			goto RET;
		}
	}

	/* setup test mode by env */
	mpp_env_get_u32("osd_enable", &p->osd_enable, 0);
	mpp_env_get_u32("osd_mode", &p->osd_mode, MPP_ENC_OSD_PLT_TYPE_DEFAULT);
	mpp_env_get_u32("roi_enable", &p->roi_enable, 0);
	mpp_env_get_u32("user_data_enable", &p->user_data_enable, 0);

	if (p->roi_enable) {
		mpp_enc_roi_init(&p->roi_ctx, p->width, p->height, p->type, 4);
		mpp_assert(p->roi_ctx);
	}

RET:
	return ret;
}

static MPP_RET app_mpp_run(MpiEncMultiCtxInfo *info)
{
	MpiEncAppArgs *cmd = info->cmd;
	MpiEncAppData *p = &info->ctx;
	MppApi *mpi = p->mpi;
	MppCtx ctx = p->ctx;
	RK_U32 quiet = cmd->quiet;
	RK_S32 chn = info->chn;
	RK_U32 cap_num = 0;
	DataCrc checkcrc;
	MPP_RET ret = MPP_OK;
	mpi_encode_info_t* mpi_encode = mpi_encode_info_get();
	//struct timespec start_tm,end_tm;
	
	memset(&checkcrc, 0, sizeof(checkcrc));
	checkcrc.sum = mpp_malloc(RK_ULONG, 512);

	if (p->type == MPP_VIDEO_CodingAVC || p->type == MPP_VIDEO_CodingHEVC) {
		MppPacket packet = NULL;

		/*
		 * Can use packet with normal malloc buffer as input not pkt_buf.
		 * Please refer to vpu_api_legacy.cpp for normal buffer case.
		 * Using pkt_buf buffer here is just for simplifing demo.
		 */
		mpp_packet_init_with_buffer(&packet, p->pkt_buf);
		/* NOTE: It is important to clear output packet length!! */
		mpp_packet_set_length(packet, 0);

		ret = mpi->control(ctx, MPP_ENC_GET_HDR_SYNC, packet);
		if (ret) {
			mpp_err("mpi control enc get extra info failed\n");
			goto RET;
		} else {
			/* get and write sps/pps for H.264 */

			//void *ptr   = mpp_packet_get_pos(packet);
			unsigned char*ptr = (unsigned char*)mpp_packet_get_pos(packet);
			size_t len  = mpp_packet_get_length(packet);

			mpp_encode_output(chn, ptr, len);
			if (p->fp_output)
			{
				fwrite(ptr, 1, len, p->fp_output);
			}
		}

		mpp_packet_deinit(&packet);
	}
	while (!p->pkt_eos) {
		MppMeta meta = NULL;
		MppFrame frame = NULL;
		MppPacket packet = NULL;
		void *buf = mpp_buffer_get_ptr(p->frm_buf);
		RK_S32 cam_frm_idx = -1;
		MppBuffer cam_buf = NULL;
		RK_U32 eoi = 1;

		if (p->fp_input) {
			ret = read_image(buf, p->fp_input, p->width, p->height,
							 p->hor_stride, p->ver_stride, p->fmt);
			if (ret == MPP_NOK || feof(p->fp_input)) {
				p->frm_eos = 1;

				if (p->frame_num < 0 || p->frame_count < p->frame_num) {
					clearerr(p->fp_input);
					rewind(p->fp_input);
					p->frm_eos = 0;
					SLOGI("chn %d loop times %d\n", chn, ++p->loop_times);
					continue;
				}
				SLOGI("chn %d found last frame. feof %d\n", chn, feof(p->fp_input));
			} else if (ret == MPP_ERR_VALUE)
				goto RET;
		} else {
#if 0            
			if (p->cam_ctx == NULL) {
				ret = fill_image(buf, p->width, p->height, p->hor_stride,
								 p->ver_stride, p->fmt, p->frame_count);
				if (ret)
					goto RET;
			} else {
				cam_frm_idx = camera_source_get_frame(p->cam_ctx);
				mpp_assert(cam_frm_idx >= 0);

				/* skip unstable frames */
				if (cap_num++ < 50) {
					camera_source_put_frame(p->cam_ctx, cam_frm_idx);
					continue;
				}

				cam_buf = camera_frame_to_buf(p->cam_ctx, cam_frm_idx);
				mpp_assert(cam_buf);
			}
#endif  
			//cam_buf = camera_video_frame_get();

			if(mpi_encode->image_encode[chn].encode_flag){
				SLOGI("[Debug] video_data_wait_and_get stop -1- \n");
				ret = MPP_OK;
				goto RET;
			}

			ret = video_data_wait_and_get(chn, buf,p->width, p->height, p->hor_stride,p->ver_stride);
			if((ret) || (mpi_encode->image_encode[chn].encode_flag)){
				SLOGI("[Debug] video_data_wait_and_get stop -2- \n");
				ret = MPP_OK;
				goto RET;
			}
			//clock_gettime(CLOCK_REALTIME, &start_tm);
		   /* SLOGI("++Encode step 0\n");*/
		}

		/* SLOGI("++Encode step 1\n"); */
		ret = mpp_frame_init(&frame);
		if (ret) {
			SLOGI("Error:mpp_frame_init failed\n");
			goto RET;
		}
	   /* SLOGI("++Encode step 2\n");*/
		mpp_frame_set_width(frame, p->width);
		mpp_frame_set_height(frame, p->height);
		mpp_frame_set_hor_stride(frame, p->hor_stride);
		mpp_frame_set_ver_stride(frame, p->ver_stride);
		mpp_frame_set_fmt(frame, p->fmt);
		mpp_frame_set_eos(frame, p->frm_eos);

		/*SLOGI("++Encode step 3\n");*/

		if (p->fp_input && feof(p->fp_input))
			mpp_frame_set_buffer(frame, NULL);
		else if (cam_buf)
			mpp_frame_set_buffer(frame, cam_buf);
		else
			mpp_frame_set_buffer(frame, p->frm_buf);

		/*SLOGI("++Encode step 4\n");*/

		meta = mpp_frame_get_meta(frame);
		mpp_packet_init_with_buffer(&packet, p->pkt_buf);
		/* NOTE: It is important to clear output packet length!! */
		mpp_packet_set_length(packet, 0);
		mpp_meta_set_packet(meta, KEY_OUTPUT_PACKET, packet);
		mpp_meta_set_buffer(meta, KEY_MOTION_INFO, p->md_info);

		/*SLOGI("++Encode step 5\n");*/
		if (p->osd_enable || p->user_data_enable || p->roi_enable) {
			if (p->user_data_enable) {
				MppEncUserData user_data;
				char *str = "this is user data\n";

				if ((p->frame_count & 10) == 0) {
					user_data.pdata = str;
					user_data.len = strlen(str) + 1;
					mpp_meta_set_ptr(meta, KEY_USER_DATA, &user_data);
				}
				static RK_U8 uuid_debug_info[16] = {
					0x57, 0x68, 0x97, 0x80, 0xe7, 0x0c, 0x4b, 0x65,
					0xa9, 0x06, 0xae, 0x29, 0x94, 0x11, 0xcd, 0x9a
				};

				MppEncUserDataSet data_group;
				MppEncUserDataFull datas[2];
				char *str1 = "this is user data 1\n";
				char *str2 = "this is user data 2\n";
				data_group.count = 2;
				datas[0].len = strlen(str1) + 1;
				datas[0].pdata = str1;
				datas[0].uuid = uuid_debug_info;

				datas[1].len = strlen(str2) + 1;
				datas[1].pdata = str2;
				datas[1].uuid = uuid_debug_info;

				data_group.datas = datas;

				mpp_meta_set_ptr(meta, KEY_USER_DATAS, &data_group);
			}

			if (p->osd_enable) {
				/* gen and cfg osd plt */
				mpi_enc_gen_osd_plt(&p->osd_plt, p->frame_count);

				p->osd_plt_cfg.change = MPP_ENC_OSD_PLT_CFG_CHANGE_ALL;
				p->osd_plt_cfg.type = MPP_ENC_OSD_PLT_TYPE_USERDEF;
				p->osd_plt_cfg.plt = &p->osd_plt;

				ret = mpi->control(ctx, MPP_ENC_SET_OSD_PLT_CFG, &p->osd_plt_cfg);
				if (ret) {
					SLOGI("Error:mpi control enc set osd plt failed ret %d\n", ret);
					goto RET;
				}

				/* gen and cfg osd plt */
				mpi_enc_gen_osd_data(&p->osd_data, p->buf_grp, p->width,
									 p->height, p->frame_count);
				mpp_meta_set_ptr(meta, KEY_OSD_DATA, (void*)&p->osd_data);
			}

			if (p->roi_enable) {
				RoiRegionCfg *region = &p->roi_region;

				/* calculated in pixels */
				region->x = MPP_ALIGN(p->width / 8, 16);
				region->y = MPP_ALIGN(p->height / 8, 16);
				region->w = 128;
				region->h = 256;
				region->force_intra = 0;
				region->qp_mode = 1;
				region->qp_val = 24;

				mpp_enc_roi_add_region(p->roi_ctx, region);

				region->x = MPP_ALIGN(p->width / 2, 16);
				region->y = MPP_ALIGN(p->height / 4, 16);
				region->w = 256;
				region->h = 128;
				region->force_intra = 1;
				region->qp_mode = 1;
				region->qp_val = 10;

				mpp_enc_roi_add_region(p->roi_ctx, region);

				/* send roi info by metadata */
				mpp_enc_roi_setup_meta(p->roi_ctx, meta);
			}
		}

		/*SLOGI("++Encode step 6\n");*/
		if (!p->first_frm)
			p->first_frm = mpp_time();
		/*SLOGI("++Encode step 7\n");*/    
		/*
		 * NOTE: in non-block mode the frame can be resent.
		 * The default input timeout mode is block.
		 *
		 * User should release the input frame to meet the requirements of
		 * resource creator must be the resource destroyer.
		 */
		ret = mpi->encode_put_frame(ctx, frame);
		if (ret) {
			SLOGI("Error:chn %d encode put frame failed\n", chn);
			mpp_frame_deinit(&frame);
			goto RET;
		}
	   /* SLOGI("++Encode step 8\n");    */
		mpp_frame_deinit(&frame);
		/*SLOGI("++Encode step 9\n");    */
		do {
			ret = mpi->encode_get_packet(ctx, &packet);
			if (ret) {
				SLOGI("Error:chn %d encode get packet failed\n", chn);
				goto RET;
			}

			mpp_assert(packet);
		   /* SLOGI("++Encode step 10\n");*/
			if (packet) {
				// write packet to file here
				//void *ptr   = mpp_packet_get_pos(packet);
				unsigned char *ptr   = (unsigned char*)mpp_packet_get_pos(packet);
				size_t len  = mpp_packet_get_length(packet);
				char log_buf[256];
				RK_S32 log_size = sizeof(log_buf) - 1;
				RK_S32 log_len = 0;

				if (!p->first_pkt)
					p->first_pkt = mpp_time();

				p->pkt_eos = mpp_packet_get_eos(packet);
				//clock_gettime(CLOCK_REALTIME, &end_tm);

				mpp_encode_output(chn, ptr, len);
				if (p->fp_output)
				{
					fwrite(ptr, 1, len, p->fp_output);
				}

				/* clock_gettime(CLOCK_REALTIME, &end_tm);*/
			   /* SLOGI("++Encode step 11\n");*/

			   /* SLOGI("Encode%d data frame:%d--diff_tm:%u\n",chn, len,diff_timespec(&end_tm,&start_tm));*/
				//SLOGI("Encode data frame:%d\n",len);    

				log_len += snprintf(log_buf + log_len, log_size - log_len,
									"encoded frame %-4d", p->frame_count);

				/* for low delay partition encoding */
				if (mpp_packet_is_partition(packet)) {
					eoi = mpp_packet_is_eoi(packet);

					log_len += snprintf(log_buf + log_len, log_size - log_len,
										" pkt %d", p->frm_pkt_cnt);
					p->frm_pkt_cnt = (eoi) ? (0) : (p->frm_pkt_cnt + 1);
				}

				log_len += snprintf(log_buf + log_len, log_size - log_len,
									" size %-7zu", len);

				if (mpp_packet_has_meta(packet)) {
					meta = mpp_packet_get_meta(packet);
					RK_S32 temporal_id = 0;
					RK_S32 lt_idx = -1;
					RK_S32 avg_qp = -1;

					if (MPP_OK == mpp_meta_get_s32(meta, KEY_TEMPORAL_ID, &temporal_id))
						log_len += snprintf(log_buf + log_len, log_size - log_len,
											" tid %d", temporal_id);

					if (MPP_OK == mpp_meta_get_s32(meta, KEY_LONG_REF_IDX, &lt_idx))
						log_len += snprintf(log_buf + log_len, log_size - log_len,
											" lt %d", lt_idx);

					if (MPP_OK == mpp_meta_get_s32(meta, KEY_ENC_AVERAGE_QP, &avg_qp))
						log_len += snprintf(log_buf + log_len, log_size - log_len,
											" qp %d", avg_qp);
				}

			   /* SLOGI("chn %d %s\n", chn, log_buf);*/

				mpp_packet_deinit(&packet);
				fps_calc_inc(cmd->fps);

				p->stream_size += len;
				p->frame_count += eoi;

				if (p->pkt_eos) {
					SLOGI("chn %d found last packet\n", chn);
					mpp_assert(p->frm_eos);
				}
			}
		} while (!eoi);

#if 0
		if (cam_frm_idx >= 0)
			camera_source_put_frame(p->cam_ctx, cam_frm_idx);
#endif             

		if (p->frame_num > 0 && p->frame_count >= p->frame_num)
		{
			printf("chn %d frame_num:%d frame_count:%d \n", chn, p->frame_num, p->frame_count);
			break;
		}

		if (p->loop_end)
		{
			printf("chn %d loop_end:%d \n", chn, p->loop_end);
			break;
		}

		if (p->frm_eos && p->pkt_eos)
		{
			printf("chn %d frm_eos:%d pkt_eos:%d \n", chn, p->frm_eos, p->pkt_eos);
			break;
		}
	}
RET:
	MPP_FREE(checkcrc.sum);

	return ret;
}

static void *mpp_frame_encode_thread(void *arg)
{
	MpiEncMultiCtxInfo *info = (MpiEncMultiCtxInfo *)arg;
	MpiEncAppArgs *cmd = info->cmd;
	MpiEncAppData *p = &info->ctx;
	MpiEncMultiCtxRet *enc_ret = &info->ret;
	MppPollType timeout = MPP_POLL_BLOCK;
	RK_U32 quiet = cmd->quiet;
	MPP_RET ret = MPP_OK;
   
	SLOGI("%s start\n", info->name);

	ret = encode_app_ctx_init(info);
	if (ret) {
		SLOGI("Error:test data init failed ret %d\n", ret);
		goto FAILED_OUT;
	}

	ret = mpp_buffer_group_get_internal(&p->buf_grp, MPP_BUFFER_TYPE_DRM);
	if (ret) {
		SLOGI("Error:failed to get mpp buffer group ret %d\n", ret);
		goto FAILED_OUT;
	}

	ret = mpp_buffer_get(p->buf_grp, &p->frm_buf, p->frame_size + p->header_size);
	if (ret) {
		SLOGI("Error:failed to get buffer for input frame ret %d\n", ret);
		goto FAILED_OUT;
	}

	ret = mpp_buffer_get(p->buf_grp, &p->pkt_buf, p->frame_size);
	if (ret) {
		SLOGI("Error:failed to get buffer for output packet ret %d\n", ret);
		goto FAILED_OUT;
	}

	ret = mpp_buffer_get(p->buf_grp, &p->md_info, p->mdinfo_size);
	if (ret) {
		SLOGI("Error:failed to get buffer for motion info output packet ret %d\n", ret);
		goto FAILED_OUT;
	}

	// encoder demo
	ret = mpp_create(&p->ctx, &p->mpi);
	if (ret) {
		SLOGI("Error:mpp_create failed ret %d\n", ret);
		goto FAILED_OUT;
	}

	SLOGI("%p encoder test start w %d h %d type %d\n",
			  p->ctx, p->width, p->height, p->type);

	ret = p->mpi->control(p->ctx, MPP_SET_OUTPUT_TIMEOUT, &timeout);
	if (MPP_OK != ret) {
		SLOGI("Error:mpi control set output timeout %d ret %d\n", timeout, ret);
		goto FAILED_OUT;
	}

	ret = mpp_init(p->ctx, MPP_CTX_ENC, p->type);
	if (ret) {
		SLOGI("Error:mpp_init failed ret %d\n", ret);
		goto FAILED_OUT;
	}

	ret = mpp_enc_cfg_init(&p->cfg);
	if (ret) {
		SLOGI("Error:mpp_enc_cfg_init failed ret %d\n", ret);
		goto FAILED_OUT;
	}

	ret = p->mpi->control(p->ctx, MPP_ENC_GET_CFG, p->cfg);
	if (ret) {
		SLOGI("Error:get enc cfg failed ret %d\n", ret);
		goto FAILED_OUT;
	}

	ret = app_mpp_enc_cfg_setup(info);
	if (ret) {
		SLOGI("Error:test mpp setup failed ret %d\n", ret);
		goto FAILED_OUT;
	}

	ret = app_mpp_run(info);    
	if (ret) {
		SLOGI("Error:test mpp run failed ret %d\n", ret);
		goto FAILED_OUT;
	}

	ret = p->mpi->reset(p->ctx);
	if (ret) {
		SLOGI("Error:mpi->reset failed\n");
		goto FAILED_OUT;
	}

#if 0
	enc_ret->elapsed_time = t_e - t_s;
	enc_ret->frame_count = p->frame_count;
	enc_ret->stream_size = p->stream_size;
	enc_ret->frame_rate = (float)p->frame_count * 1000000 / enc_ret->elapsed_time;
	enc_ret->bit_rate = (p->stream_size * 8 * (p->fps_out_num / p->fps_out_den)) / p->frame_count;
	enc_ret->delay = p->first_pkt - p->first_frm;
#endif     

	SLOGI("[%s-%d]----mpp_frame_encode_thread end----\n", __func__, __LINE__);

FAILED_OUT:
	if (p->ctx) {
		mpp_destroy(p->ctx);
		p->ctx = NULL;
	}

	if (p->cfg) {
		mpp_enc_cfg_deinit(p->cfg);
		p->cfg = NULL;
	}

	if (p->frm_buf) {
		mpp_buffer_put(p->frm_buf);
		p->frm_buf = NULL;
	}

	if (p->pkt_buf) {
		mpp_buffer_put(p->pkt_buf);
		p->pkt_buf = NULL;
	}

	if (p->md_info) {
		mpp_buffer_put(p->md_info);
		p->md_info = NULL;
	}

	if (p->osd_data.buf) {
		mpp_buffer_put(p->osd_data.buf);
		p->osd_data.buf = NULL;
	}

	if (p->buf_grp) {
		mpp_buffer_group_put(p->buf_grp);
		p->buf_grp = NULL;
	}

	if (p->roi_ctx) {
		mpp_enc_roi_deinit(p->roi_ctx);
		p->roi_ctx = NULL;
	}

	app_ctx_deinit(p);

	return NULL;
}

unsigned int mpp_encode_chn_init(unsigned int chn, unsigned char image_chn, MpiEncAppArgs *encode_cmd)
{
	int ret;
	mpi_encode_info_t* mpi_encode = mpi_encode_info_get();

	mpi_encode->encode_cmd[chn].width = encode_cmd->width;
	mpi_encode->encode_cmd[chn].height = encode_cmd->height;
	mpi_encode->encode_cmd[chn].format =  encode_cmd->format;

	mpi_encode->encode_cmd[chn].fps_in_flex = 0;
	mpi_encode->encode_cmd[chn].fps_in_den = 1;
	mpi_encode->encode_cmd[chn].fps_in_num = encode_cmd->fps_in_num;
	mpi_encode->encode_cmd[chn].fps_out_flex = 0;
	mpi_encode->encode_cmd[chn].fps_out_num = encode_cmd->fps_out_num;
	mpi_encode->encode_cmd[chn].fps_out_den = 1;

	mpi_encode->encode_cmd[chn].type = encode_cmd->type;
#if TEST
	if(encode_cmd->type == MPP_VIDEO_CodingAVC)
		mpi_encode->encode_cmd[chn].file_output = "/tmp/output.h264";
	else if(encode_cmd->type == MPP_VIDEO_CodingHEVC)
		mpi_encode->encode_cmd[chn].file_output = "/tmp/output.h265";
	else if(encode_cmd->type == MPP_VIDEO_CodingMJPEG)
		mpi_encode->encode_cmd[chn].file_output = "/tmp/output.mjpeg";
#endif

	mpi_encode->encode_cmd[chn].bps_target = encode_cmd->bps_target;

	mpi_encode->encode_ctxs[chn].cmd = &mpi_encode->encode_cmd[chn];
	mpi_encode->encode_ctxs[chn].name = encode_name[chn];
	mpi_encode->encode_ctxs[chn].chn = chn;

	/******************************************************************/
	SLOGI("[Debug] %s chn:%d encode_size_max:%d \n", __func__, chn, mpi_encode->encode_size_max);
	memset(&mpi_encode->image_encode[chn], 0x0, sizeof(mpi_encode->image_encode[chn]));
	mpi_encode->image_encode[chn].image_chn = image_chn;
	mpi_encode->image_encode[chn].frame_index = 1;
	mpi_encode->image_encode[chn].encode_flag = 0;
	mpi_encode->image_encode[chn].frame_size = 0;
	mpi_encode->image_encode[chn].encode_data_offset = 0;
	mpi_encode->image_encode[chn].offset_size = (((mpi_encode->encode_size_max + MPP_OUTPUT_HEAD_SIZE) / DATA_PAGE) + 1)*DATA_PAGE;
	mpi_encode->image_encode[chn].encode_total_size = MPP_OUTPUT_FRAME_MAX * gs_mpi_encode->image_encode[chn].offset_size;
	mpi_encode->image_encode[chn].encode_data =align_write_buf_get(chn);
	if(mpi_encode->image_encode[chn].encode_data == NULL) {
		printf("[ERROR] failed to malloc mpp encode data for app\n");
		return A818_MPI_INIT_DATA_MALLOC;
	}
	ret = pthread_create(&mpi_encode->encode_ctxs[chn].thd, NULL, mpp_frame_encode_thread, & mpi_encode->encode_ctxs[chn]);
	if (ret != 0) {
		printf("failed to create mpp encode thread %d\n", 0);
		return A818_MPI_INIT_ENCODE_THREAD;
	}
	usleep(200*1000);
	
	return A818_SUCCESS;
}

void mpp_chn_ringbuffer_is_empty(unsigned char chn)
{
	mpi_encode_info_t* mpi_encode = mpi_encode_info_get();
	ring_buffer_t *rb = mpi_encode->mpp_encode_rb[chn];

	while(!ringbuffer_is_empty(rb)){
		xt_nanosleep(0, 100);
	}
}

void mpp_encode_chn_destory(unsigned int chn)
{
	struct timeval time_val;
	mpi_encode_info_t* mpi_encode = mpi_encode_info_get();
	GPU_Frame_st* gpu_frame_stream = gpu_data_stream_get();
	Crop_Frame_st* crop_stream = crop_data_stream_get();
	crop_image_info_t* crop_image = &crop_stream->crop_image[chn];
	image_frame_info_t frame_info = {0};

	while(!ringbuffer_is_empty(gpu_frame_stream->mpp_data_rb[chn%MPP_ENCODE_NTHREAD_MAX])){
		ringbuffer_get(gpu_frame_stream->mpp_data_rb[chn%MPP_ENCODE_NTHREAD_MAX], (uint8_t *)&frame_info, sizeof(image_frame_info_t));
		// printf("InPutRingBuffer is_empty\n");
		time_val.tv_sec=0;
		time_val.tv_usec=(1000);
		select(0,NULL,NULL,NULL,&time_val);
	}
	mpi_encode->image_encode[chn%MPP_ENCODE_NTHREAD_MAX].encode_flag = 1;
	usleep(100*1000);
	SLOGI("[DEBUG] %s chn:%d end \n", __func__, chn);
	for(int index = 0; index < crop_image->data_frame_max+1; index++)
	{
		if(crop_image->crop_yuv_buffer[index]){
			free(crop_image->crop_yuv_buffer[index]);
			crop_image->crop_yuv_buffer[index] = NULL;
		}
	}
	return ;
}

int mpp_encode_init(void)
{
	int chn, i;
	MPP_RET ret = MPP_OK; 

	gs_mpi_encode = (mpi_encode_info_t*)malloc(sizeof(mpi_encode_info_t));
	if(gs_mpi_encode == NULL){
		printf("[ERROR] mpi encode info malloc error \n");
		return A818_MPI_INIT_ENCODE_INFO;
	}

	memset(gs_mpi_encode, 0x0, sizeof(mpi_encode_info_t));

	gs_mpi_encode->encode_ctxs = mpp_calloc(MpiEncMultiCtxInfo, MPP_ENCODE_NTHREAD_MAX);
	if (NULL == gs_mpi_encode->encode_ctxs) {
		printf("[ERROR] failed to alloc mpp encode context for app\n");
		ret = A818_MPI_INIT_ENCODE_CONTEXT;
		goto err_1;

	}

	gs_mpi_encode->encode_cmd = mpp_calloc(MpiEncAppArgs, MPP_ENCODE_NTHREAD_MAX);
	if (NULL == gs_mpi_encode->encode_cmd) {
		printf("[ERROR] failed to alloc mpp encode cmd for app\n");
		ret = A818_MPI_INIT_ENCODE_CMD;
		goto err_2;
	}


	gs_mpi_encode->encode_size_max = MPP_OUTPUT_FRAME_SIZE;

	for(chn = 0; chn < MPP_ENCODE_NTHREAD_MAX; chn++) {
		gs_mpi_encode->mpp_encode_rb[chn] = ringbuffer_create((MPP_OUTPUT_ENCODE_FRAME_MAX+1)*sizeof(image_frame_info_t), sizeof(image_frame_info_t));
		if (!gs_mpi_encode->mpp_encode_rb[chn]) {
			printf("[ERROR] init mpp encode rb failed\n");
			ret = A818_MPI_INIT_ENCODE_RB;
			goto err_3;
		}
		//SLOGI("chn:%d offset_size:%d encode_total_size:%d \n", chn, gs_mpi_encode->image_encode[chn].offset_size, gs_mpi_encode->image_encode[chn].encode_total_size);
	}

	return A818_SUCCESS;

err_3:
	for(i = 0; chn < chn; i++) {
		if(gs_mpi_encode->mpp_encode_rb[i]){
			ringbuffer_destory(gs_mpi_encode->mpp_encode_rb[i]); 
			gs_mpi_encode->mpp_encode_rb[i] = NULL;
		}     
	}
	MPP_FREE(gs_mpi_encode->encode_cmd);

err_2:
	MPP_FREE(gs_mpi_encode->encode_ctxs);

err_1:
	free(gs_mpi_encode);

	return ret;

}


void mpp_encode_destory(void)
{
	int chn;
	mpi_encode_info_t* mpi_encode = mpi_encode_info_get();
	if(mpi_encode){
		gs_mpi_encode = NULL;

		for(chn = 0; chn < MPP_ENCODE_NTHREAD_MAX; chn++) {
			if(mpi_encode->mpp_encode_rb[chn]){
				ringbuffer_destory(mpi_encode->mpp_encode_rb[chn]); 
				mpi_encode->mpp_encode_rb[chn] = NULL;
			}     
		}

		MPP_FREE(mpi_encode->encode_cmd);
		MPP_FREE(mpi_encode->encode_ctxs);
		free(mpi_encode);
	}

	return ;
}

