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

int crop_stream_init(void);
void crop_stream_destory(void);
int crop_chn_init(unsigned char chn);
void crop_chn_destory(unsigned char chn);
#ifdef __cplusplus
}
#endif
#endif /* CROP_ENGINE_H_ */
