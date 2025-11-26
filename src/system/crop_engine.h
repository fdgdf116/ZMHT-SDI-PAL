#ifndef CROP_ENGINE_H_
#define CROP_ENGINE_H_


#include "types_def.h"
#include "ringbuffer.h"
#include "vivado_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	unsigned char chn;
	pthread_t tid;
	unsigned char crop_flag;
	unsigned int crop_width;          /* 裁剪后图像的宽*/
	unsigned int crop_height;         /* 裁剪后图像的高*/

	unsigned char data_frame_max; /*申请缓存区的最大个数*/
	unsigned int crop_rb_offset;  /*裁剪是的ring_buf计数*/

	uint8_t *crop_yuv_buffer[DATA_STREAM_FRAME_MAX+1]; /*输出的yuv buffer */
}crop_image_info_t;

typedef struct {
	ring_buffer_t *crop_data_rb[DATA_CHANNEL_NUM_MAX]; /*mpp 编码的ring_buffer*/
	crop_image_info_t crop_image[DATA_CHANNEL_NUM_MAX]; /*图像的相关信息*/
}Crop_Frame_st;

Crop_Frame_st* crop_data_stream_get(void);

void crop_to_queue(unsigned char chn, image_frame_info_t *frame_info);

int crop_chn_image_init(unsigned char chn);
void crop_chn_image_destory(unsigned char chn);
void crop_chn_ringbuffer_is_empty(unsigned char chn);

/**
 * @brief 将行场分割的图像数据转换成单帧图像数据
 * @param  width      宽度
 * @param  height     高度
 * @param  px_size    像素大小，单位字节
 * @param  img        处理图像数据
 * @param  temp_boost 临时缓冲区,可以为空。如果不为空，要求有效长度是 width * px_size + height。
 */
bool FAPI_imagebase_odd_transform_to_origin(uint32_t width, uint32_t height, uint8_t px_size, uint8_t* img, uint8_t* temp_boost);
// @note  将单帧图像数据转换成行场分割的图像数据（参数同上）
bool FAPI_imagebase_odd_transform_to_target(uint32_t width, uint32_t height, uint8_t px_size, uint8_t* img, uint8_t* temp_boost);
int crop_stream_init(void);
void crop_stream_destory(void);
int crop_chn_init(unsigned char chn);
void crop_chn_destory(unsigned char chn);
#ifdef __cplusplus
}
#endif
#endif /* CROP_ENGINE_H_ */
