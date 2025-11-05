#ifndef GPU_GPU_INTERFACE_H_
#define GPU_GPU_INTERFACE_H_

//#include "data_engine.h"
#include "hetero_interface.h"
//using namespace cv;
#include "types_def.h"
#include "ringbuffer.h"
#include "vivado_common.h"
#include "rga_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RK_GPU_LIB_PATH      "/tmp/real_part/lib/libhetero.so"

typedef struct {

    // uint32_t data_offset; /* 在一帧图像中的行偏移 */
    // uint32_t frame_size;
    // uint8_t rb_offset;
    unsigned char* rga_vir_addr;
	unsigned int rga_addr_offset;
	unsigned int rga_addr_offset_max;

    unsigned char data_frame_max;
    unsigned int osd_rb_offset; /*拼接时的ring_buffer 计数*/
    unsigned int crop_rb_offset;
    unsigned char* over_image;
    RGA_IMAGE_S in_rgb_buffer[2*(DATA_STREAM_FRAME_MAX + 1)];
    uint8_t* gui_osd_buffer[DATA_STREAM_FRAME_MAX + 1];
    uint8_t* out_yuv_buffer[DATA_STREAM_FRAME_MAX+1];
}gpu_image_info_t;

typedef struct {
    ring_buffer_t *osd_data_rb[DATA_CHANNEL_NUM_MAX]; /*osd 编码的ring_buffer*/
    ring_buffer_t *image_over_rb[DATA_CHANNEL_NUM_MAX]; /*osd 编码的ring_buffer*/
    ring_buffer_t *mpp_data_rb[MPP_ENCODE_NTHREAD_MAX]; /*mpp 编码的ring_buffer*/
    gpu_image_info_t gpu_image[DATA_CHANNEL_NUM_MAX]; /*GPU相关图像的存储*/

    void* hetero_hdr;
    hetero_world_st htr;
}GPU_Frame_st;


GPU_Frame_st* gpu_data_stream_get(void);

void gpu_osd_to_queue(unsigned char chn, image_frame_info_t *frame_info);
void gpu_mpp_to_queue(unsigned char chn, image_frame_info_t *frame_info);

int gpu_yuv_to_rgb(uint8_t* rgb_buffer, uint8_t* yuv_buffer, unsigned int width, unsigned hight);

//int gpu_buf_osd(void* gpu_osd, void *gpu_addr,unsigned int width, unsigned int height);
int gpu_cooperate_overlap_osd(uint8_t* osd_buffer,uint8_t *rgb_buffer, uint8_t* out_rgb, unsigned int width, unsigned int height);
int gpu_cooperate_encode_yuyv(uint8_t* osd_buffer,uint8_t *rgb_buffer, uint8_t* yuv_buffer, unsigned int width, unsigned int height);
int gpu_cooperate_encode_nv12(uint8_t* osd_buffer,uint8_t *rgb_buffer, uint8_t* yuv_buffer, unsigned int width, unsigned int height);
void gpu_chn_ringbuffer_is_empty(unsigned char chn);
int gpu_lib_init(void);
void gpu_lib_destory(void);


#ifdef __cplusplus
}
#endif
#endif /* GPU_GPU_INTERFACE_H_ */
