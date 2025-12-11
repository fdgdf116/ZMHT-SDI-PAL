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
#include <opencv2/opencv.hpp>

#include "gpu_interface.h"
#include "data_engine.h"
#include "crop_engine.h"
#include "xutils.h"
#include "log.h"
#include "a818_error.h"

#define ringbuffer_destory ringbuffer_deinit
static Crop_Frame_st *g_crop_frame_stream = NULL;

Crop_Frame_st* crop_data_stream_get(void)
{
	return g_crop_frame_stream;
}

void crop_to_queue(unsigned char chn, image_frame_info_t *frame_info)
{
	Crop_Frame_st* crop_stream = crop_data_stream_get();
	ring_buffer_t *rb = crop_stream->crop_data_rb[chn];
	image_frame_info_t event_param, tmp_param;

	if (ringbuffer_is_full(rb)) {					
		ringbuffer_get(rb, (uint8_t *)&tmp_param, sizeof(tmp_param));
		SLOGI("[Debug] %s ringbuffer_is_full", __func__);
	}

	memcpy(&event_param,frame_info,sizeof(image_frame_info_t));
	ringbuffer_put(rb, (uint8_t *)&event_param, sizeof(event_param));
}

/*
	函数: int crop_chn_image_init(unsigned char chn, unsigned int src_width, unsigned int src_hight)
	作用: 初始化通道相关的crop buffer
	参数:
		chn:需要初始化的通道
		src_width:图像宽度
		src_hight:图像高度
	返回值:
		正确返回0，错误返回其他
*/
int crop_chn_image_init(unsigned char chn)
{
	int ret;
	int index;
	// GPU_Frame_st* frame_stream = gpu_data_stream_get();
	// gpu_image_info_t *gpu_image = &frame_stream->gpu_image[chn];

	Crop_Frame_st* crop_stream = crop_data_stream_get();
	crop_image_info_t* crop_image = &crop_stream->crop_image[chn];
	data_stream_t *data_stream = data_stream_get();
	image_chn_info_t* image_chn_info = data_stream->image_chn_info[chn];

	unsigned int crop_width = image_chn_info->rga_info.out_width;
	unsigned int crop_hight = image_chn_info->rga_info.out_height;
	SLOGI("[Debug] %s chn:%d width:%d hight:%d \n", __func__, chn, crop_width, crop_hight);

	crop_image->data_frame_max = 0;
	crop_image->crop_rb_offset = 0;

	/* 根据分辨率决定初始化的每种buffer的个数 */
	// if((crop_width== VIDEO_FRAME_WIDTH_4k) && (crop_hight == VIDEO_FRAME_HEIGHT_4k)){
	// 	crop_image->data_frame_max = DATA_4K_STREAM_FRAME_MAX;
	// } else {
		crop_image->data_frame_max = DATA_STREAM_FRAME_MAX;
	// }

	/* 初始化输入的crop buffer*/
	for(index = 0; index < crop_image->data_frame_max+1; index++)
	{
		crop_image->crop_yuv_buffer[index] = (uint8_t*)aligned_alloc(4096, (crop_width * crop_hight*2));
		if(crop_image->crop_yuv_buffer[index] == NULL){
			printf("[ERROR] crop yuv buffer(%d) acquire error \n", index);
			ret = A818_CROP_DATA_BUFFER;
			goto err_1;
		}
		memset(crop_image->crop_yuv_buffer[index], 0x0, (crop_width * crop_hight*2));
	}

	crop_chn_init(chn);

	return A818_SUCCESS;
err_1:
	for(index = 0; index < crop_image->data_frame_max; index++)
	{
		if(crop_image->crop_yuv_buffer[index]){
			free(crop_image->crop_yuv_buffer[index]);
		}
	}

	return ret;
}

/*
	函数: void crop_chn_ringbuffer_is_empty(unsigned char chn)
	作用: 判断本通道的 ring buffer是否处理完
	参数:
		chn:需要初始化的通道
	返回值:
		无
*/
void crop_chn_ringbuffer_is_empty(unsigned char chn)
{
	Crop_Frame_st* crop_stream = crop_data_stream_get();
	image_frame_info_t frame_info = {0};
	while(!ringbuffer_is_empty(crop_stream->crop_data_rb[chn])){
		ringbuffer_get(crop_stream->crop_data_rb[chn], (uint8_t *)&frame_info, sizeof(image_frame_info_t));
		xt_nanosleep(0, 100);
	}
}

/*
	函数: void crop_chn_destory(unsigned char chn)
	作用: 相同的通道crop资源的释放
	参数:
			chn：释放资源的通道号
	返回值:
			无
*/
void crop_chn_destory(unsigned char chn)
{
	Crop_Frame_st* crop_stream = crop_data_stream_get();
	crop_chn_ringbuffer_is_empty(chn);
	if(crop_stream->crop_image[chn].crop_flag){
		crop_stream->crop_image[chn].crop_flag = 0;
		SLOGI("[Debug] %s chn:%d \n", __func__, chn);
		xt_nanosleep(0, 100*1000);
	}
	usleep(1000);
}

/*
	函数: void crop_chn_image_destory(unsigned char chn)
	作用: 释放通道申请的各种buffer
	参数:
		chn:需要初始化的通道
	返回值:
		无
*/
void crop_chn_image_destory(unsigned char chn)
{
	int index;
	Crop_Frame_st* crop_stream = crop_data_stream_get();
	crop_image_info_t *crop_image = &crop_stream->crop_image[chn];
	GPU_Frame_st* frame_stream = gpu_data_stream_get();
	gpu_image_info_t *gpu_image = &frame_stream->gpu_image[chn];

    for(int index = 0; index < gpu_image->data_frame_max + 1; index++)
    {
        if(gpu_image->out_yuv_buffer[index]){
            free(gpu_image->out_yuv_buffer[index]);
            gpu_image->out_yuv_buffer[index] = NULL;
        }
    }

	crop_image->data_frame_max = 0;
	SLOGI("[DEBUG] %s chn:%d end \n", __func__, chn);
	return ;
}

/*
	函数: int crop_init(void)
	作用: gpu 初始化
	参数:
		无
	返回值:
		正确返回0，错误返回其他
*/
int crop_stream_init(void)
{
	int ret;
	int chn;

	/* 构建GPU相关结构体 */
	g_crop_frame_stream = (Crop_Frame_st*)malloc(sizeof(Crop_Frame_st));
	if(!g_crop_frame_stream){
		printf("[ERROR] malloc crop frame error \n");
		ret = A818_CROP_FRAME_MALLOC;
		goto err_1;
	}

	memset(g_crop_frame_stream, 0x00, sizeof(Crop_Frame_st));

	/* 对每个通道的队列进行初始化 */
	for(chn = 0; chn < DATA_CHANNEL_NUM_MAX; chn++){

		g_crop_frame_stream->crop_data_rb[chn]= ringbuffer_create(RINGBUFFER_FRAME_MAX*sizeof(image_frame_info_t), sizeof(image_frame_info_t));
		if (!g_crop_frame_stream->crop_data_rb[chn]) {
			printf("ERROR] create crop data rb failed\n");
			ret = A818_CROP_DATA_RB;
			goto err_2;
		}
	}

	return A818_SUCCESS;

err_2:
	for(chn = 0; chn < DATA_CHANNEL_NUM_MAX; chn++){
		if(g_crop_frame_stream->crop_data_rb[chn]){
			ringbuffer_destory(g_crop_frame_stream->crop_data_rb[chn]); 
			g_crop_frame_stream->crop_data_rb[chn] = NULL;
		}
	}
err_1:
	free(g_crop_frame_stream);
	g_crop_frame_stream = NULL;
	return ret;
}

/*
	函数: void crop_stream_destory(void)
	作用: crop 释放
	参数:
		无
	返回值:
		无
*/
void crop_stream_destory(void)
{
	int chn;
	Crop_Frame_st* crop_frame_stream = crop_data_stream_get();
	if(crop_frame_stream){
		for(chn = 0; chn < DATA_CHANNEL_NUM_MAX; chn++){
			if(crop_frame_stream->crop_data_rb[chn]){
				ringbuffer_destory(crop_frame_stream->crop_data_rb[chn]); 
				crop_frame_stream->crop_data_rb[chn] = NULL;
			}
		}
		free(crop_frame_stream);
		crop_frame_stream = NULL;
	}

	return ;
}

void data_stream_crop_src_to_new_nv12(unsigned int index, unsigned char *src, unsigned char *dst, rga_param_t rga_info, int crop_x, int crop_y)
{
	unsigned int offset;
	int src_width = rga_info.osd_width;
	int src_height = rga_info.osd_height;
	int crop_width = rga_info.out_width;
	int crop_height = rga_info.out_height;

	if(!index){
		offset = 0;
	}else {
		offset = 8;/*因为对齐填充宽/高*/
	}

	if (!src || !dst || crop_x < 0 || crop_y < 0 || crop_width <= 0 || crop_height <= 0 ||
	crop_x + crop_width > src_width || crop_y + crop_height > src_height) {
	fprintf(stderr, "Invalid crop parameters!\n");
	return;
	}

	// 计算Y平面
	for (int i = 0; i < crop_height; i++) {
		memcpy(dst + i * crop_width, src + (crop_y + i) * src_width + crop_x, crop_width);
	}

	// 计算UV平面 (NV12格式，每两个Y像素对应一个UV像素)
	unsigned char *src_uv = src + src_width * src_height;
	unsigned char *dst_uv = dst + crop_width * crop_height;
	for (int i = 0; i < crop_height / 2; i++) {
		memcpy(dst_uv + i * crop_width, src_uv + ((crop_y / 2) + i) * src_width + crop_x, crop_width);
	}
}

void data_stream_crop_src_to_new_yuyv(unsigned int index, unsigned char *src, unsigned char *dst, 
                                rga_param_t rga_info, int crop_x, int crop_y) {
    // 从rga_info提取参数
    int src_width = rga_info.osd_width;
    int src_height = rga_info.osd_height;
    int crop_width = rga_info.out_width;
    int crop_height = rga_info.out_height;

    // 保留原index的offset逻辑（根据实际需求调整用途）
    unsigned int offset = (index == 0) ? 0 : 8;

    // 参数校验
    if (!src || !dst) {
        std::cerr << "Invalid source or destination pointer!" << std::endl;
        return;
    }
    if (crop_x < 0 || crop_y < 0 || crop_width <= 0 || crop_height <= 0) {
        std::cerr << "Invalid crop size or position!" << std::endl;
        return;
    }
    if (crop_x + crop_width > src_width || crop_y + crop_height > src_height) {
        std::cerr << "Crop region exceeds source image!" << std::endl;
        return;
    }
    if (crop_width % 2 != 0) {  // YUYV要求宽度为偶数
        std::cerr << "Crop width must be even for YUYV format!" << std::endl;
        return;
    }

    // 1. 将原始src数据包装为OpenCV矩阵（YUYV对应CV_8UC2，不拷贝数据）
    cv::Mat src_mat(src_height, src_width, CV_8UC2, src + offset);

    // 2. 定义裁剪区域ROI
    cv::Rect crop_roi(crop_x, crop_y, crop_width, crop_height);

    // 3. 直接将dst包装为目标矩阵（映射到dst指向的内存，不拷贝数据）
    cv::Mat dst_mat(crop_height, crop_width, CV_8UC2, dst);

    // 4. 裁剪并直接写入dst_mat（利用OpenCV的矩阵赋值，内部会高效复制数据到dst内存）
    src_mat(crop_roi).copyTo(dst_mat);
}

bool FAPI_imagebase_odd_transform_to_origin(uint32_t width, uint32_t height, uint8_t px_size, uint8_t* img, uint8_t* temp_boost)
{
	if (!img || !width || !height || !px_size)
		return false;

	uint16_t row_len = width * px_size;

	uint8_t* temp_row = nullptr; // 还原奇偶场数据
	bool* visit_map = nullptr;   // 建立一个映射，防止因为闭环导致出现未交换的行
	if (temp_boost)
	{
		temp_row = temp_boost;
		visit_map = (bool*)(temp_boost + row_len);
	}
	else
	{
		temp_row = new uint8_t[row_len];
		visit_map = new bool[height];
	}
	memset(visit_map, false, height);

	uint32_t start = 1;
	uint32_t current = start;
	memcpy(temp_row, &img[current * row_len], row_len); // 保存第1行数据
	visit_map[start] = true;

	while (true)
	{
		uint32_t next = (current % 2) ? (current + height) / 2 : current / 2;
		if (next == start)
		{
			// 先找到 start 对应的地址，完成这个闭环
			uint16_t temp_target = start < height / 2 ? start * 2 : 2 * (start - height / 2) + 1;
			memcpy(&img[row_len * temp_target], temp_row, row_len); // 保存第1行数据
			visit_map[temp_target] = true;

			// 检查是否有闭环
			uint32_t unvisited = 0;
			for (uint16_t i = 1; i < height - 1; ++i)
				if (!visit_map[i])
				{
					unvisited = i;
					break;
				}
			if (unvisited)
			{
				start = unvisited;
				current = start;
				memcpy(temp_row, &img[start * row_len], row_len); // 保存第1行数据
				continue;
			}
			else
				break;
		}
		else
			visit_map[current] = true;
		// 数据向目标索引移动
		memcpy(&img[current * row_len], &img[next * row_len], row_len);
		current = next;
	}

	// 释放内存
	if (!temp_boost)
	{
		delete[] temp_row;
		delete[] visit_map;
	}
}

bool FAPI_image_odd_transform_to_target(uint32_t width, uint32_t height, uint8_t px_size, const uint8_t* src, uint8_t* target)
{
	if (!src || !target)
		return false;
	// 典型场图像要求偶数高度
	if (height % 2 != 0)
		return false;
	const size_t width_size = (size_t)width * px_size;
	for (size_t src_row = 0; src_row < height; ++src_row)
	{
		size_t target_row;
#if 1 // 奇在上，偶在下
		target_row = (src_row % 2) ? src_row / 2 : src_row / 2 + height / 2;
#else // 偶在上，奇在下
		target_row = (src_row % 2) ? src_row / 2 + height / 2 : src_row / 2;
#endif

		memcpy(target + width_size * target_row, src + width_size * src_row, width_size);
	}

	return true;
}

/*
	函数: static void* crop_thread(void* parameter)
	作用: 编码图进行画布做画线程
	参数:
			parameter:线程参数
	返回值:
			线程返回值
*/
static void* crop_thread(void* parameter)
{
	ring_buffer_t *rb;
	unsigned char crop_chn;
	ring_buffer_t *rb_t;
	struct timeval time;

	gpu_image_info_t *gpu_image;
	GPU_Frame_st* gpu_frame_stream = gpu_data_stream_get();
	Crop_Frame_st* crop_stream = crop_data_stream_get();
	data_stream_t* data_stream = data_stream_get();

	image_frame_info_t frame_info = {0};
	image_frame_info_t crop_frame_info = {0};

	crop_chn = *((unsigned char*)parameter);
	SLOGI("[debug] %s %d chn:%d \n", __func__, __LINE__, crop_chn);

	rb = crop_stream->crop_data_rb[crop_chn];
	fpga_chn_image_info_t* chn_image = data_stream->chn_image[crop_chn];
	image_chn_info_t* image_chn_info = data_stream->image_chn_info[crop_chn];
	gpu_image = &gpu_frame_stream->gpu_image[crop_chn];
	crop_image_info_t *crop_image = &crop_stream->crop_image[crop_chn];

    uint8_t* send_buf = align_write_buf_get(crop_chn);
	unsigned char mpp_chn = crop_chn%MPP_ENCODE_NTHREAD_MAX;
	if(crop_chn < MPP_ENCODE_NTHREAD_MAX)
		rb_t = gpu_frame_stream->mpp_data_rb[mpp_chn];
	else
		rb_t = GetOutPutRingBufferPt(mpp_chn);

	uint8_t* crop_yuv_addr = NULL;
	uint8_t* out_yuv_addr = NULL;
	uint8_t* send_yuv_addr = NULL;
	unsigned int width = image_chn_info->rga_info.out_width;
	unsigned int height = image_chn_info->rga_info.out_height;
	
	while(1){
		if(!crop_image->crop_flag){
			break;
		}
		while(ringbuffer_is_empty(rb)){
			time.tv_sec=0;
			time.tv_usec=(100);
			select(0,NULL,NULL,NULL,&time);
			if(!crop_image->crop_flag){
				goto stop;
			}
		}
		while(ringbuffer_is_full(rb_t)){
			// printf("rb_t chn:%d is_full\n", crop_chn);
			time.tv_sec=0;
			time.tv_usec=(100);
			select(0,NULL,NULL,NULL,&time);
			if(!crop_image->crop_flag){
				goto stop;
			}
		}
		ringbuffer_get(rb, (uint8_t *)&frame_info, sizeof(image_frame_info_t));

		if(crop_image->crop_rb_offset >= crop_image->data_frame_max+1){
				crop_image->crop_rb_offset = 0;
		}

		out_yuv_addr = gpu_image->out_yuv_buffer[frame_info.frame_offset];
		crop_yuv_addr = crop_image->crop_yuv_buffer[crop_image->crop_rb_offset];
		send_yuv_addr = send_buf + crop_image->crop_rb_offset*width*height*2;
		if(image_chn_info->rga_info.crop_flag)
		{
			if(image_chn_info->rga_info.out_color == 3)
				data_stream_crop_src_to_new_nv12(0,out_yuv_addr,crop_yuv_addr,image_chn_info->rga_info,0,0);
			else
				data_stream_crop_src_to_new_yuyv(0,out_yuv_addr,crop_yuv_addr,image_chn_info->rga_info,0,0);
			if(image_chn_info->rga_info.odd_even_transform && (crop_chn >= MPP_ENCODE_NTHREAD_MAX))
			{
				FAPI_image_odd_transform_to_target(width, height, 2, crop_yuv_addr, send_yuv_addr);
			}
			else if(crop_chn >= MPP_ENCODE_NTHREAD_MAX)
			{
				memcpy(send_yuv_addr,crop_yuv_addr,width*height*2);
			}
		}
		else
		{
			if(crop_chn >= MPP_ENCODE_NTHREAD_MAX)
			{
				if(image_chn_info->rga_info.odd_even_transform)
				{
					FAPI_image_odd_transform_to_target(width, height, 2, out_yuv_addr, send_yuv_addr);
				}
				else
				{
					memcpy(send_yuv_addr,out_yuv_addr,frame_info.frame_size);
				}
			}
			else
			{
				memcpy(crop_yuv_addr,out_yuv_addr,frame_info.frame_size);
			}
		}
		// if(crop_chn < MPP_ENCODE_NTHREAD_MAX)
		// 	save_image_data((char*)crop_yuv_addr,width*height*3/2,"/tmp/crop_buf.raw");
		// else
		// 	save_image_data((char*)crop_yuv_addr,width*height*2,"/tmp/crop_buf.raw");

		/*放到编码的ring buffer中，后续进行编码输出*/
		if(!ringbuffer_is_full(rb_t)){
			crop_frame_info.frame_offset = crop_image->crop_rb_offset;
			crop_frame_info.frame_size = width*height*2;
			crop_frame_info.chn = mpp_chn;
			ringbuffer_put(rb_t, (uint8_t *)&crop_frame_info, sizeof(image_frame_info_t));
			crop_image->crop_rb_offset += 1;
		}
		else {
			SLOGI("[Debug]:%s %d chn:%d pl_out_rb ringbuffer_is_full \n", __func__, __LINE__, crop_chn);
		}
	}
stop:
	return NULL;
}

/*
	函数: int crop_chn_init(unsigned char chn)
	作用: 根据不同的通道，初始化不同的作画线程
	参数:
			无
	返回值:
			正常返回0，失败返回其他
*/
int crop_chn_init(unsigned char chn)
{
	int ret;
	Crop_Frame_st *crop_stream = crop_data_stream_get();
	crop_image_info_t* crop_image = &crop_stream->crop_image[chn];

	crop_image->chn = chn;
	crop_image->crop_flag = 1;

	ret = pthread_create(&crop_image->tid, NULL, &crop_thread, &crop_image->chn);
	if (ret != 0) {
		printf("[ERROR]:%s Create crop_thread_thread failed %d\n",__func__,ret);
		return A818_CROP_THREAD;
	}
	return ret;
}