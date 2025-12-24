#ifndef _VIVADO_COMMON_H
#define _VIVADO_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif
#include <sys/types.h>
// #define DEC_CHN (4)

#define PAL_RK3588_VERSION 0x10000001
#define PAL_RK3588_BUILD_TIME 0x20251224
#define TEST 0

enum video_ratio_e {
    VIDEO_RATIO_1 = 1,
    VIDEO_RATIO_3 = 3,	
	VIDEO_RATIO_4 = 4,
	VIDEO_RATIO_6 = 6,
	VIDEO_RATIO_9 = 9,			
};

enum video_common_e {
    FPGA_ROW_DATA_SIZE = 5184,/*5120 + 64*/

    VIDEO_OSD_WIDTH = 4096,
    VIDEO_OSD_HIGHT = 4096,

    VIDEO_FRAME_HEIGHT_4k = 4096,
    VIDEO_FRAME_WIDTH_4k = 4096,

    VIDEO_SPECIAL_FRAME_HEIGHT = 1088,
	VIDEO_SPECIAL_FRAME_WIDTH = 1920,

    VIDEO_FRAME_HEIGHT = 1080,
	VIDEO_FRAME_WIDTH = 1920,

    VIDEO_DP_WIDTH_MAX = 3840,
    VIDEO_DP_HEIGHT_MAX = 2160,

    VIDEO_RATIO_1x1_WIDTH = 1920,
    VIDEO_RATIO_1x1_HEIGHT = 1080,

    VIDEO_RATIO_3x2_WIDTH = 640,
    VIDEO_RATIO_3x2_HEIGHT = 540,

    VIDEO_RATIO_3x3_WIDTH = 640,
    VIDEO_RATIO_3x3_HEIGHT = 360,

    VIDEO_RATIO_2x2_WIDTH = 960,
    VIDEO_RATIO_2x2_HEIGHT = 540,
};


enum data_stream_e {
    PCIE_DATA_STREAM_FRAME_MAX = 16,

    DATA_4K_STREAM_FRAME_MAX = 1,  /* buf 缓冲区 */
    DATA_STREAM_FRAME_MAX = 5,  /* buf 缓冲区 */
    RINGBUFFER_FRAME_MAX = 5,

    PCIE_SRC_START_CHANNEL = 12,
    PCIE_SRC_END_CHANNEL = 23,

    DATA_CHANNEL_NUM_MAX = 4, /* 4路处理链路 */
    DATA_RATIO_CHANNEL_MAX = 12, /* PCIE缩放通道的最大的值 */
    MPP_DECODE_NTHREAD_MAX = 2, /* mpp进行解码的通道个数 */
    MPP_ENCODE_NTHREAD_MAX = 2, /* mpp进行编码的通道个数 */

    NO_SIGNAL_BUF_MAX = 24,
    NO_SIGNAL_IMGAE_MAX = 5,

    GPU_STITCHBASE_CHN = 24, /* 拼接所在的GPU数据队里的通道号 */
    MPP_STITCHBASE_CHN = 8, /* 拼接所在的编码通道是固定死的 */

    MPP_OUTPUT_HEAD_SIZE = 64, 
    MPP_OUTPUT_FRAME_SIZE = 65536,
    MPP_OUTPUT_FRAME_MAX = 128,
    MPP_OUTPUT_ENCODE_FRAME_MAX = 70,

    DATA_PAGE=4096,
};

enum uartlist_request_result_info_e {
    UARTLIST_FRAME_ENABLE = 0xAA,
    // UARTLIST_CMD_OK = 0x55,
    // UARTLIST_CMD_ERR = 0x5a,
    // UARTLIST_EXECUTIVE_CMD_OK = 0xaa,
    // UARTLIST_EXECUTIVE_CMD_ERR = 0xa5,
    // UARTLIST_EXECUTIVE_SEND_ERR = 0x0,
};

enum uartlist_data_type_info_e {
    UARTLIST_REQUEST_TYPE1 = 0x1,
    UARTLIST_REQUEST_TYPE2 = 0x2,
    UARTLIST_REQUEST_TYPE3 = 0x3,
    UARTLIST_REQUEST_TYPE4 = 0x4,
    UARTLIST_REQUEST_TYPE5 = 0x5,
    UARTLIST_REQUEST_TYPE6 = 0x6,
    UARTLIST_REQUEST_TYPE7 = 0x7,
    UARTLIST_REQUEST_TYPE8 = 0x8,
    UARTLIST_REQUEST_TYPE9 = 0x9,
    UARTLIST_REQUEST_TYPE10 = 0xa,
    UARTLIST_REQUEST_TYPE11 = 0xb,
    UARTLIST_REQUEST_TYPE12 = 0xc,
    UARTLIST_REQUEST_TYPE13 = 0xd,
    UARTLIST_REQUEST_TYPE14 = 0xe,
    UARTLIST_REQUEST_TYPE15 = 0xf,
    UARTLIST_REQUEST_TYPE16 = 0x10,
    UARTLIST_REQUEST_TYPE17 = 0x11,
    UARTLIST_REQUEST_TYPE18 = 0x12,
    UARTLIST_REQUEST_TYPE19 = 0x13,
    UARTLIST_REQUEST_TYPE20 = 0x14,
    // UARTLIST_CMD_OK_TYPE = 0x0,
    // UARTLIST_CMD_ERR_TYPE = 0x1,
    // UARTLIST_EXECUTIVE_CMD_OK_TYPE = 0x2,
    // UARTLIST_EXECUTIVE_CMD_ERR_TYPE = 0x3,

};

enum fpga_reg_out_type_e {
    NULL_OUT_TYPE = 0,
    MPP_OUT_TYPE = 1,
    PC_OUT_TYPE = 2,
    MPP_PC_OUT_TYPE = 3,
    DP_OUT_TYPE = 4,
    MPP_DP_OUT_TYPE = 5,
    DP_PC_OUT_TYPE = 6,
    MPP_DP_PC_OUT_TYPE = 7,
};

enum rga_out_buff_mode_e {
    RGB_BUFF_MODE = 0x1,
    ROTATE_BUFF_MODE = 0x2,
    RESIZE_BUFF_MODE = 0x3,
};

enum pcie_chn_init_flag_e{
    DATA_INIT = 0,
    OSD_INIT = 1,
    MPP_INIT = 2,
    INIT_MAX = 3,
};

enum stitchbase_osd_shape_type_e {
    STITCHBASE_OSD_ROUND_SHAPE = 0, /* 圆形 */
    STITCHBASE_OSD_CROSS_SHAPE = 1, /* 十字 */
    STITCHBASE_OSD_BLOCK_SHAPE = 2, /* 方框 */
    STITCHBASE_OSD_ACROSS_SHAPE = 3, /*横线*/
    STITCHBASE_OSD_VERTICAL_SHAPE = 4, /*竖线*/
    STITCHBASE_OSD_NO_SHAPE = 5, /*不叠加*/
};


enum fpga_chn_input_mode_e {
    EXTERNAL_SOURCE_MODE = 0,
    INTERNAL_SOURCE_MODE = 1,
};

enum rk3588_upgrade_info_e {
    RK3588_UPGRADE_DATA_START = 9, /*1个字节的帧类型+4个字节的包计数+4个字节的有些数据长度*/
    RK3588_UPGRADE_DATA_SIZE = 768//240,
};

typedef struct {
    u_int16_t   chn;            /*rga处理通路*/
    u_int16_t   out_color;      /*输出颜色选择*/
    u_int16_t   rotate;         /*旋转角度*/
    u_int16_t   odd_even_transform; /*奇数行偶数行变换标志*/
    u_int16_t   resize;         /*是否缩放*/
    u_int16_t   resize_width;  /*缩放宽*/
    u_int16_t   resize_higth;   /*缩放高*/
    u_int16_t   crop;         /*是否裁剪*/
    u_int16_t   image_length0;  /*图像长度*/
    u_int16_t   image_width0;   /*图像宽度*/
    u_int16_t   image_fps;      /*输出帧率*/
    u_int16_t   image_w0;       /*叠加图像的宽*/
    u_int16_t   image_y0;       /*叠加图像的高*/
}channel_param_t;

typedef struct __attribute__((packed)){
    channel_param_t channel_param; /*通道处理参数*/
    unsigned char in_filename_chn[256];
    unsigned char out_filename_chn[256];
}cfg_info_t;


typedef struct  __attribute__((packed)){
    unsigned char chn;
    unsigned char draw_mode0;
    unsigned short center_point_x0;
    unsigned short center_point_y0;
    unsigned short center_point_r0;
    unsigned short center_point_s0;
    unsigned short center_point_l0;
    unsigned short center_point_w0;
    unsigned short image_length0;
    unsigned short image_width0;
    unsigned short chan0_sel_mode;
    unsigned short image_fill_length0;
    unsigned short image_fill_width0;
    unsigned short image_cut_length0;
    unsigned short image_out_format;
    unsigned short draw_line_width0;
    unsigned short character_point_x0;
    unsigned short character_point_y0;
    unsigned short image_point_x0;
    unsigned short image_point_y0;
    unsigned short yuv422_crcb_sel0;
    unsigned short chan0_video_mode;
    unsigned short image_length1;
    unsigned short image_width1;
}stack_configuration_info_t;

typedef struct __attribute__((packed)){
    unsigned char chn;
    unsigned int VDMA_MM2S_DMACR;
    unsigned int VDMA_MM2S_DMASR;
    unsigned int VDMA_MM2S_HSIZE;
    unsigned int VDMA_MM2S_DLYSTRIDE;
    unsigned int VDMA_MM2S_START_ADDR1;
    unsigned int VDMA_MM2S_START_ADDR2;
    unsigned int VDMA_MM2S_START_ADDR3;
    unsigned int VDMA_S2MM_DMACR;
    unsigned int VDMA_S2MM_DMASR;
    unsigned int VDMA_S2MM_HSIZE;
    unsigned int VDMA_S2MM_DLYSTRIDE;
    unsigned int VDMA_S2MM_START_ADDR1;
    unsigned int VDMA_S2MM_START_ADDR2;
    unsigned int VDMA_S2MM_START_ADDR3;
    unsigned int VDMA_MM2S_VSIZE;
    unsigned int VDMA_S2MM_VSIZE;
}vdma_configuration_info_t;

typedef struct  __attribute__((packed)){
    u_int32_t a818_param_select;
    u_int32_t active_pixels_per_line;
    u_int32_t active_lines_per_video;
    u_int32_t num_of_advb_obj2_frames;
    u_int32_t obj0_advb_frame;
    u_int32_t obj2_advb_frame;
    u_int32_t sofi_to_sofi;
    u_int32_t sofi_to_sofn_firtst;
    u_int32_t sofn_to_sofn_next;
    u_int32_t sofnfirst_to_sofnlast;
    u_int32_t sofnlast_to_sofi;
    u_int32_t video_line_time;
    u_int32_t eofn_to_sofn;
    u_int32_t eoft_to_sofn;
    u_int32_t a818_image_format;
}temporal_parameters_t;

typedef struct {
    volatile unsigned char chn;
    volatile unsigned int frame_size;
    volatile unsigned int frame_index;
    volatile unsigned int frame_offset;
}image_frame_info_t;


typedef struct{
    unsigned char type;
    unsigned char data[2];
}string_info_t;

#pragma pack (1)
typedef struct {
    unsigned short head;
    unsigned int len;
    unsigned char mode;
    unsigned char data;
    unsigned char sum;
}uartlist_send_info_t;
#pragma pack ()

typedef struct {
    unsigned int board_type;
    unsigned int series_number;
    unsigned int version1;
    unsigned int version2;
    unsigned int link;
    unsigned char pc_link;
    unsigned char rk_link;
    unsigned int width;
    unsigned int height;
}selftest_val_info_t;

typedef struct {
    unsigned short head;
    unsigned char len;
    unsigned char mode;
    selftest_val_info_t selftest;
    unsigned char sum;
}uartlist_type6_info_t;

typedef struct {
    unsigned int year;
    unsigned char mon;
    unsigned char mday;
    unsigned char hour;
    unsigned char min;
    unsigned char sec;
}system_time_info_t;

typedef enum {
    Time_Osd,
    Char_Osd,
    Image_Over,
    Shape_Osd,
    OSD_TYPE_MAX
} Osd_Type;
#define OSD_CHAR_NUM 512
typedef struct {
    unsigned char chn;               /* 叠加图像所在的通道号 */
    unsigned char osd_type;             /* 叠加类型 */
    unsigned char shape;            /*图像形状*/
    unsigned char font_type;        /*字体的类型*/
    unsigned char font_format;      /*字体的格式*/
    unsigned char font_size;        /*字体的大小*/
    unsigned char line_width;        /*字体线宽*/
    unsigned char time_position;     /*时间标叠加的X位置*/
    unsigned int font_color;        /*字体颜色*/
    unsigned int color;             /*空白区域的颜色*/
    unsigned int string_x;          /*字符串的X坐标*/
    unsigned int string_y;          /*字符串的Y坐标*/
    unsigned int x;                 /*叠加中心点位置的X坐标*/
    unsigned int y;                 /*叠加中心点位置的Y坐标*/
    unsigned int x_off;                 /*叠加中心点位置的W坐标*/
    unsigned int y_off;                 /*叠加中心点位置的H坐标*/
    unsigned int image_x0;                 /*图像叠加x坐标*/
    unsigned int image_y0;                 /*图像叠加y坐标*/
    unsigned int data_size;         /*字符串的个数*/
    unsigned char data[OSD_CHAR_NUM];         /*字符串区域*/
    // string_info_t data[128];
}osd_image_info_t;

typedef struct {
    unsigned char chn;           /* 图像所在的通道号 */
    unsigned char mode;         /* 缩放模式 */
    unsigned char extraction;   /* 抽帧*/
    unsigned char fps;          /* 帧率 */
    unsigned int src_width;          /* 原图图像的宽*/
    unsigned int src_height;         /* 原图图像的高*/
    unsigned int width;          /* 本通道图像的宽*/
    unsigned int height;         /* 本通道图像的高*/
    unsigned char fill_column;
    unsigned char fill_row;
    unsigned char position;      /* 在拼接图中的块号 */
    unsigned char rotate;        /* 0:不旋转, 90:旋转90 180:旋转180 270:旋转270 */
}ratio_image_info_t;

// typedef struct {
// 	unsigned char chn;           /* 图像所在的通道号 */
// 	unsigned char fps;           /* 图像的帧率 */
// 	unsigned int width;          /* 本通道图像的宽 */
// 	unsigned int height;         /* 本通道图像的高 */
// 	unsigned char mpp_type;
// 	unsigned char mpp_chn;       /*图像进行h264/h265压缩所在的通道*/
// 	unsigned int mpp_bps;
// 	unsigned char position;         /*字符叠加位置*/
// 	unsigned char out_type;
// 	unsigned char extraction;		/*抽帧比*/
// 	unsigned char rotate;         /*旋转类型0/1/2/4*/
// 	unsigned char crop;
// 	unsigned int crop_width;
// 	unsigned int crop_height;
// }mpp_image_info_t;

typedef struct {
    unsigned char chn;
    unsigned int src_width;
    unsigned int src_height;
    unsigned int dst_width;
    unsigned int dst_height;
}resize_info_t;

#ifdef __cplusplus
}
#endif

#endif /* DATA_MANAGER_H */
