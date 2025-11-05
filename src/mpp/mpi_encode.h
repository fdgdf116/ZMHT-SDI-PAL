/*
 * File      : mpp_encode.h
 * This file is RK3399PRO MPP encode interface header  
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-12-18     fengyong    first version
 */

#ifndef ZM_MPPENCODE_H
#define ZM_MPPENCODE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mpp_env.h"
#include "mpp_mem.h"
#include "mpp_time.h"
#include "mpp_debug.h"
#include "mpp_common.h"
#include "rk_mpi.h"
#include "mpi_enc_utils.h"
#include "mpp_enc_roi_utils.h"
#include "data_engine.h"

typedef struct {
    // base flow context
    MppCtx ctx;
    MppApi *mpi;
    RK_S32 chn;

    // global flow control flag
    RK_U32 frm_eos;
    RK_U32 pkt_eos;
    RK_U32 frm_pkt_cnt;
    RK_S32 frame_num;
    RK_S32 frame_count;
    RK_U64 stream_size;
    /* end of encoding flag when set quit the loop */
    volatile RK_U32 loop_end;

    // src and dst
    FILE *fp_input;
    FILE *fp_output;
    
    /* encoder config set */
    MppEncCfg       cfg;
    MppEncPrepCfg   prep_cfg;
    MppEncRcCfg     rc_cfg;
    MppEncCodecCfg  codec_cfg;
    MppEncSliceSplit split_cfg;
    MppEncOSDPltCfg osd_plt_cfg;
    MppEncOSDPlt    osd_plt;
    MppEncOSDData   osd_data;
    RoiRegionCfg    roi_region;
    MppEncROICfg    roi_cfg;

    // input / output
    MppBufferGroup buf_grp;
    MppBuffer frm_buf;
    MppBuffer pkt_buf;
    MppBuffer md_info;
    MppEncSeiMode sei_mode;
    MppEncHeaderMode header_mode;

    // paramter for resource malloc
    RK_U32 width;
    RK_U32 height;
    RK_U32 hor_stride;
    RK_U32 ver_stride;
    MppFrameFormat fmt;
    MppCodingType type;
    RK_S32 loop_times;
    
    MppEncRoiCtx roi_ctx;

    // resources
    size_t header_size;
    size_t frame_size;
    size_t mdinfo_size;
    /* NOTE: packet buffer may overflow */
    size_t packet_size;

    RK_U32 osd_enable;
    RK_U32 osd_mode;
    RK_U32 split_mode;
    RK_U32 split_arg;
    RK_U32 split_out;

    RK_U32 user_data_enable;
    RK_U32 roi_enable;

    // rate control runtime parameter
    RK_S32 fps_in_flex;
    RK_S32 fps_in_den;
    RK_S32 fps_in_num;
    RK_S32 fps_out_flex;
    RK_S32 fps_out_den;
    RK_S32 fps_out_num;
    RK_S32 bps;
    RK_S32 bps_max;
    RK_S32 bps_min;
    RK_S32 rc_mode;
    RK_S32 gop_mode;
    RK_S32 gop_len;
    RK_S32 vi_len;

    RK_S64 first_frm;
    RK_S64 first_pkt;
} MpiEncAppData;

/* For each instance thread return value */
typedef struct {
    float           frame_rate;
    RK_U64          bit_rate;
    RK_S64          elapsed_time;
    RK_S32          frame_count;
    RK_S64          stream_size;
    RK_S64          delay;
} MpiEncMultiCtxRet;

typedef struct {
    MpiEncAppArgs      *cmd;        // pointer to global command line info
    const char          *name;
    RK_S32              chn;

    pthread_t           thd;        // thread for for each instance
    MpiEncAppData       ctx;         // context of encoder
    MpiEncMultiCtxRet   ret;        // return of encoder
} MpiEncMultiCtxInfo;



typedef struct {
    uint8_t image_chn;
    uint8_t encode_flag;
    uint8_t *encode_data;
    uint32_t encode_total_size;
    uint32_t encode_data_offset;
    uint32_t frame_size;
    uint32_t frame_index;
    uint32_t offset_size;
}mpi_image_encode_info_t;


typedef struct {
    MpiEncMultiCtxInfo *encode_ctxs;
    MpiEncAppArgs *encode_cmd;
    ring_buffer_t *mpp_encode_rb[MPP_ENCODE_NTHREAD_MAX];
    mpi_image_encode_info_t image_encode[MPP_ENCODE_NTHREAD_MAX];;
    unsigned int encode_size_max;
}mpi_encode_info_t;

mpi_encode_info_t* mpi_encode_info_get(void);

int mpp_encode_init(void);
void mpp_encode_destory(void);

void *camera_video_frame_get(void);
unsigned int mpp_encode_chn_init(unsigned int chn, unsigned char image_chn, MpiEncAppArgs *encode_cmd);
void mpp_encode_chn_destory(unsigned int chn);;
void mpp_chn_ringbuffer_is_empty(unsigned char chn);

// extern unsigned char klv_buf2[188];
// extern unsigned char  head_video[1920*1080];
// extern  int head_len_video;
#ifdef __cplusplus
}
#endif

#endif /* ZM_MPPENCODE_H */
