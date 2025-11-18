#include <stdint.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <poll.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <memory.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <arpa/inet.h>
#include <inttypes.h>
#include <sys/time.h>
#include <dirent.h>

#include "vivado_common.h"
#include "mpi_decode.h"
#include "mpi_encode.h"
#include "pcie_reg_rw.h"
#include "pcie_engine.h"
#include "data_engine.h"
#include "stream_fifo.h"
#include "fifo_engine.h"
#include "rga_interface.h"
#include "rga_engine.h"
#include "gpu_interface.h"
#include "a818_error.h"
#include "my_tree.h"
#include "xutils.h"
#include "osd_engine.h"
#include "crop_engine.h"

#define SLOGI printf

#define FPGA_PCIE_AXIFIFO_PHY (0x20000)
#define FPGA_PCIE_AXIFIFO_LEN (0x10000)


#define FPGA_PCIE_DATA_PHY (0x30000)
#define FPGA_PCIE_DATA_LEN (0x10000)
#define FPGA_PCIE_READ_DATA_OFFSET (0x1000)


#define AXIFIFO_RECV_DATA_SIZE (95*1024)
#define PCIE_AXIFIFO_BUF_SIZE_MAX (95*1024)
#define AXIFIFO_HEAD_SIZE (2)
#define AXIFIFO_LENGH_SIZE (4)

#define BUILD_TIME 0x20250826
#define VERSION 0x10000010

#define FILE_NAME_MAX_SIZE 256
#if TEST
#define SDD_ADDRESS "/userdata/"
#else
#define SDD_ADDRESS "/userdata/sda1/"
#endif
#define FPGA_PCIE_COMMON_PARM_REG_PHY (0x00010000)

typedef enum {
    FiberRate_1_0625,
    FiberRate_2_125,
    FiberRate_3_1875,
    FiberRate_4_25,
    FiberRate_8,
    FiberRate_10,
    FiberRate_overrall,
} FiberRate;

typedef struct {
    uint8_t axififo_data[81*1024];
    unsigned char pc_enable;
    unsigned char rk_enable;
    stream_fifo_reg_t fifo_reg;
} axififo_info_t;

typedef struct {
    uint32_t capacity_total;
    uint32_t capacity_used;
    uint32_t capacity_unused;
    uint32_t file_count;
} dev_disk_capacity_t;

static axififo_info_t *g_axififo_info;

extern u_int32_t rotate_type[4];
FILE *fp = NULL;
const char ssd_baseaddr[] = SDD_ADDRESS;
char filename[FILE_NAME_MAX_SIZE] = {0};

static unsigned int axififo_count = 0;
static unsigned int axififo_index = 0;
static unsigned char msg_head_1 = 0xec;
static unsigned char msg_head_2 = 0x91;
static unsigned char local_buffer[PCIE_AXIFIFO_BUF_SIZE_MAX];
extern void* pcie_common_param_reg;
extern void* pcie_vdma_config_reg;
extern void* fiber_rate_config_reg;

static unsigned int upgrade_num;
static unsigned int upgrade_size;
static unsigned char* upgrade_buffer = NULL;
static char upgrade_file_name[128];
static FILE * upgrade_fp;
static unsigned int upgrade_state;
static unsigned char* cmd_buff = NULL;
static cfg_info_t* cfg_info = NULL;
static uint8_t* osd_tmp_info = NULL;

static char * ZPK_PWD  = "/userdata/zpk/";
static unsigned char* zpk_upgrade_state_patch = "/tmp/sd_update";



unsigned char check_sum(unsigned char* buf, int len)
{
    int i;
    uint32_t sum = 0;
    for(i = 0; i < len; i++){
        sum += buf[i];
    }

    return (sum & 0xff);
}

#include <sys/statfs.h>

/*
	函数: static void axififo_data_send(unsigned char mode, unsigned int* data,int len)
	作用: axififo 发送任意长度数据函数
	参数:
		mode：帧类型
		data:发送的数据
	返回值:
		无
*/
static int axififo_data_send(unsigned char mode, unsigned char* data,int len)
{
	int ret;
	unsigned int send_size;
	uint8_t* buff = (char*)malloc(len + 8);
	uartlist_send_info_t* send_info = (uartlist_send_info_t*)buff;

	send_info->head = htons(0xec91);
	send_info->len = htonl(len);
	send_info->mode = mode;
	memcpy(&send_info->data,data,len);
	*(buff + 7 + len) = check_sum((unsigned char*)&send_info->data,len);

	send_size = 0;
	while(1){
		ret = stream_fifo_write_data(&g_axififo_info->fifo_reg,(uint8_t *)(buff + send_size),len + 8 - send_size);
		if(ret == (len + 8 - send_size)){
			break;
		}
		send_size += ret;
		xt_nanosleep(0, 100);
	}

	return 0;
}

/*
	函数: char read_zpk_upgrade_state(void)
	作用: 读取rk3588升级的状态
	参数:
        无
	返回值:
        返回升级状态码
*/
char read_zpk_upgrade_state(void)
{
    FILE* fp;
    int ret;
    unsigned char state;

    fp = fopen(zpk_upgrade_state_patch, "rb");
    if(NULL == fp) {
        printf("open %s error \n", zpk_upgrade_state_patch);
        return -1;
    }

    ret = fread(&state, sizeof(unsigned char), 1, fp);
    if(ret != 1){
        printf("read %s data error \n", zpk_upgrade_state_patch);
        fclose(fp);
        return -2;
    }

    fclose(fp);
    return state - '0';
}

int getstatfs(dev_disk_capacity_t *disk_info) {
    struct statfs buf;
    DIR *p_dir = NULL;
    struct dirent *p_dirent = NULL;
    disk_info->file_count = 0;

    char* path = "/userdata/sda1";

    p_dir = opendir(path);
    if(p_dir == NULL) {
        printf("opendir error");
        return -1;
    }

    if (statfs(path, &buf) != 0) {
        perror("statfs");
        return -1;
    }

    while((p_dirent = readdir(p_dir)) != NULL){
		if((strcmp(p_dirent->d_name,"..") == 0) || (strcmp(p_dirent->d_name,".") == 0))
			continue;
        disk_info->file_count++;
    }
    disk_info->capacity_total = ((unsigned long long)buf.f_blocks * buf.f_frsize)/1024;
    disk_info->capacity_unused = ((unsigned long long)buf.f_bfree * buf.f_frsize)/1024;
    disk_info->capacity_used = disk_info->capacity_total - disk_info->capacity_unused;
    printf("capacity_total:%d,capacity_unused:%d,capacity_used:%d,file_count:%d\n",disk_info->capacity_total,disk_info->capacity_unused,disk_info->capacity_used,disk_info->file_count);
    disk_info->capacity_total = htonl(disk_info->capacity_total);
    disk_info->capacity_unused = htonl(disk_info->capacity_unused);
    disk_info->capacity_used = htonl(disk_info->capacity_used);
    disk_info->file_count = htonl(disk_info->file_count);
    closedir(p_dir);
 
    return 0;
}

void convert_to_timeval(struct tm *time_elements, struct timeval *tv) {
 
    // // 初始化 tm 结构体
    // time_elements.tm_year = year - 1900;
    // time_elements.tm_mon = month - 1;
    // time_elements.tm_mday = day;
    // time_elements.tm_hour = hour;
    // time_elements.tm_min = min;
    // time_elements.tm_sec = sec;
    // time_elements.tm_isdst = -1; // 自动设置夏令时
 
    // 将日期转换为自1970年1月1日以来的秒数
    tv->tv_sec = mktime(time_elements);
 
    // tv->tv_usec = time_elements.tm_sec * 1000000; // 将秒转换为微秒
}

static void mpp_encode_cmd_init(MpiEncAppArgs *encode_cmd, mpp_encode_info_t *mpp_info)
{
	encode_cmd->width = mpp_info->width; 
	encode_cmd->height = mpp_info->height;

	// encode_cmd->format = MPP_FMT_RGB888;/*之前的编码颜色格式为yuv422sp*/
	// if(mpp_info->mpp_chn == 0)
		encode_cmd->format = MPP_FMT_YUV420SP;
	// else if (mpp_info->mpp_chn == 1)
	// 	encode_cmd->format = MPP_FMT_YUV400;
	// else
	// {
	// 	printf("[Debug] %s mpp_chn error \n", __func__);
	// 	return;
	// }
	
	if(mpp_info->mpp_type == 1){
		encode_cmd->type = MPP_VIDEO_CodingHEVC; /**< H.265/HEVC */
	} else if(mpp_info->mpp_type == 0){
		encode_cmd->type = MPP_VIDEO_CodingAVC;  /**< H.264/AVC */
	}else if(mpp_info->mpp_type == 2){
		encode_cmd->type = MPP_VIDEO_CodingMJPEG; /**< MJPEG */
	}
	
	encode_cmd->fps_in_num = mpp_info->fps;
	encode_cmd->fps_out_num = mpp_info->fps;
	encode_cmd->bps_target = mpp_info->mpp_bps * 1024;
    SLOGI("[%s %d] width:%d height:%d format:%d type:%d fps_in_num:%d fps_out_num:%d bps_target:%d\n", __func__, __LINE__, mpp_info->width, mpp_info->height, encode_cmd->format, encode_cmd->type, encode_cmd->fps_in_num, encode_cmd->fps_out_num, encode_cmd->bps_target);
}

/*
	函数: static int request_type4_mpp_init(fpga_chn_image_info_t* chn_image)
	作用: 帧类型4编码相关的初始化
	参数:
		chn_image:通道结构体
	返回值:
		正确返回0，失败返回其他
*/
static int request_type2_mpp_init(int chn)
{
	int ret;
	MpiEncAppArgs encode_cmd;
    data_stream_t * data_stream = data_stream_get();
    mpp_encode_info_t* mpp_info = data_stream->mpp_encode_info[chn];

	/*mpp相关的资源初始化*/
	mpp_encode_cmd_init(&encode_cmd, mpp_info);
	ret = mpp_encode_chn_init(mpp_info->mpp_chn, mpp_info->mpp_chn, &encode_cmd);
	if (ret != 0) {
		SLOGI("encode frame chn init error \n");
		return ret;
	}

	return 0;
}

/*
	函数: void request_type2_mpp_destory(fpga_chn_image_info_t* chn_image)
	作用: 帧类型4编码相关的释放
	参数:
		chn_image:通道结构体
	返回值:
		无
*/
static void request_type2_mpp_destory(int chn)
{
	mpp_encode_chn_destory(chn%MPP_ENCODE_NTHREAD_MAX);
}

int EncodeStart(int chn){
    int ret = rga_chn_image_init(chn);
    if (ret != 0) {
        SLOGI("rga_chn_image_init error \n");
        goto err_4;
    }
    if(g_axififo_info->rk_enable) {
        pcie_set_rk_enable();
    }
    // if(g_axififo_info->pc_enable) {
    //     pcie_set_pc_enable(1);
    // }
    return 0;
err_4:
    rga_pthread_chn_destory(chn);
    data_stream_chn_data_destory(chn);
    return 1;
}

int DecodeStart(int chn){
    data_stream_t* data_stream = data_stream_get();
    mpp_decode_info_t* mpp_info = data_stream->mpp_decode_info[chn%MPP_DECODE_NTHREAD_MAX];

    if(!mpp_info->decode_state)
    {
        printf("decoding!!!\n");
        return -1;
    }
    int ret = 0;

#if 1
    printf("DecodeStart chn:%d\n",chn);
    ret = mpi_dec_chn_init(mpp_info->mpp_chn);
    if(ret != MPP_OK)
        return -1;
    return ret;
#endif
}

void DecodeStop(int chn){
    data_stream_t* data_stream = data_stream_get();
    mpp_decode_info_t *mpp_info = data_stream->mpp_decode_info[chn%MPP_DECODE_NTHREAD_MAX];

    if(mpp_info->decode_state)
    {
        printf("no decoding!!!\n");
        return;
    }

    mpp_info->decode_state = 1;
    MpiDecodeDestroy(chn);
    rga_pthread_chn_destory(chn);
    data_stream_chn_data_destory(chn);
    gui_osd_overlapped_chn_destory(chn);
    gpu_osd_chn_image_destory(chn);
    crop_chn_destory(chn);
    crop_chn_image_destory(chn);
    writePL_chn_destory(chn);

	memset(&data_stream->image_chn_info[chn]->osd_info, 0x0, sizeof(osd_image_info_t));
    printf("DecodeStop finished!!!\n");
}

/***********************************************************************/
/**************************axififo_frame_request_type9******************/
/*
	函数: void clean_system_buff_cache(void)
	作用: 释放系统缓存
	参数:
		无
	返回值:
		无
*/
void clean_system_buff_cache(void)
{
	char cmd[128];
	sprintf(cmd, "echo %d > /proc/sys/vm/drop_caches", 1);
	system(cmd);
	usleep(200*1000);

	sprintf(cmd, "echo %d > /proc/sys/vm/drop_caches", 2);
	system(cmd);
	usleep(200*1000);

	sprintf(cmd, "echo %d > /proc/sys/vm/drop_caches", 3);
	system(cmd);
	usleep(200*1000);
	SLOGI("[Debug] %s ok \n", __func__);
}

void EncodeStop(int chn)
{
	fpga_chn_image_info_t* chn_image;
	data_stream_t *data_stream = data_stream_get();
	mpi_encode_info_t* mpi_encode = mpi_encode_info_get();

	pcie_fpga_device_stop();
	sleep(1);

    rga_pthread_chn_destory(chn);
    data_stream_chn_data_destory(chn);
    gui_osd_overlapped_chn_destory(chn);
    gpu_osd_chn_image_destory(chn);
    crop_chn_destory(chn);
    crop_chn_image_destory(chn);

    /*释放申请的编码资源*/
    mpp_encode_chn_destory(chn);
	SLOGI("[Debug] %s mpp_encode_chn_destory ok \n", __func__);
	usleep(100*1000);

	g_axififo_info->rk_enable = 0;
	// g_axififo_info->pc_enable = 0;

	mpi_encode->encode_size_max = MPP_OUTPUT_FRAME_SIZE;

	memset(&data_stream->image_chn_info[chn]->osd_info, 0x0, sizeof(osd_image_info_t));
	clean_system_buff_cache();
	// pcie_fpga_reg_reset();
}

/********************通用参数寄存器，用bar0配置**************************************/
/* 帧类型1 (叠加配置) */
static int axififo_frame_request_type1(unsigned char* frame_buff, unsigned int frame_size)
{
    // char date[128];
    // stack_configuration_info_t * osd_info = (stack_configuration_info_t *)(frame_buff+1);
    // unsigned short * temp_pt = (unsigned short *)((unsigned char *)osd_info + 2);
    // for(int i = 1;i < sizeof(stack_configuration_info_t)/sizeof(unsigned short); i++)
    // {
    //     *((unsigned short*)osd_info+i) = ntohs(*((unsigned short*)osd_info+i));
    // }
    // pcie_chn_stack_configuration_reg_init(osd_info);
    // return COMD_PROC_SUCCESS;
    return COMD_PROC_FAILED;
}
/* 帧类型17（图像生成配置） */
static int axififo_frame_request_type17(unsigned char* frame_buff, unsigned int frame_size)
{
    if(frame_size > (sizeof(cfg_info_t) + 1))
    {
        SLOGI("transform frame_buff too much, frame_size = %d\n", frame_size);
        return -1;
    }
    u_int16_t * tmp_val_info = (u_int16_t *)(frame_buff+1);
    for(int i = 0;i < sizeof(channel_param_t)/sizeof(u_int16_t); i++)
    {
        tmp_val_info[i] = ntohs(tmp_val_info[i]);
    }
    cfg_info_t* reg_val_info = (cfg_info_t*)tmp_val_info;
    decode_configuration_init(reg_val_info);
    return COMD_PROC_SUCCESS;
}
/* 帧类型2 (图像采集配置) */
static unsigned int axififo_frame_request_type2(unsigned char* frame_buff, unsigned int frame_size)
{
	int ret;
	unsigned char chn;
	unsigned char mpp_type, pc_type;
	data_stream_t *data_stream = data_stream_get();
	if(!data_stream){
		printf("[Debug] %s data_stream error \n", __func__);
		return 1;
	}

	chn = frame_buff[1];
    if(chn >= MPP_DECODE_NTHREAD_MAX)
    {
        printf("axififo_frame_request_type2 chn:%d err\n",chn);
        return 1;
    }
	fpga_chn_image_info_t* chn_image = data_stream->chn_image[chn];
	if(!chn_image){
		printf("[Debug] %s chn_image error chn:%d\n", __func__,chn);
		return 1;
	}
    image_chn_info_t*  image_chn_info = data_stream->image_chn_info[chn];
	if(!image_chn_info){
		printf("[Debug] %s image_chn_info error chn:%d\n", __func__,chn);
		return 1;
	}
    mpp_encode_info_t* mpp_info = data_stream->mpp_encode_info[chn%MPP_ENCODE_NTHREAD_MAX];
	memcpy(mpp_info, (frame_buff + 1), (frame_size - 1));
	mpp_info->mpp_chn = chn%MPP_ENCODE_NTHREAD_MAX;
    rga_param_t* rga_info = &image_chn_info->rga_info;
#if TEST
#else
	mpp_info->width = ntohl(mpp_info->width);
	mpp_info->height = ntohl(mpp_info->height);
	mpp_info->mpp_bps = ntohl(mpp_info->mpp_bps);
	mpp_info->resize_width = ntohl(mpp_info->resize_width);
	mpp_info->resize_height = ntohl(mpp_info->resize_height);
	mpp_info->crop_width = ntohl(mpp_info->crop_width);
	mpp_info->crop_height = ntohl(mpp_info->crop_height);
#endif
    rga_info->rga_chn = chn;
    rga_info->pixel_byte = 1.5;
    rga_info->out_color = 3;
	rga_info->rotate_type = mpp_info->rotate;
    rga_info->resize = mpp_info->resize;
	rga_info->crop_flag = mpp_info->crop;
    rga_info->src_width = mpp_info->width;
    rga_info->src_height = mpp_info->height;
    rga_info->image_w0 = mpp_info->image_w0;
    rga_info->image_y0 = mpp_info->image_y0;

    if( rga_info->rotate_type == IM_HAL_TRANSFORM_ROT_90 || rga_info->rotate_type == IM_HAL_TRANSFORM_ROT_270 )
    {
        rga_info->rotate_width = rga_info->src_height;
        rga_info->rotate_height = rga_info->src_width;
    }
    else
    {
        rga_info->rotate_width = rga_info->src_width;
        rga_info->rotate_height = rga_info->src_height;
    }
    if(rga_info->resize)
    {
        rga_info->resize_width  = mpp_info->resize_width;
        rga_info->resize_height = mpp_info->resize_height;
    }
    else
    {
        rga_info->resize_width  = rga_info->rotate_width;
        rga_info->resize_height = rga_info->rotate_height;
    }

    if(rga_info->resize_width % 32)
    {
        rga_info->osd_width = (rga_info->resize_width/32 + 1)*32;
    }
    else
    {
        rga_info->osd_width = rga_info->resize_width;
    }
    if(rga_info->resize_height % 2)
    {
        rga_info->osd_height = (rga_info->resize_height/2 + 1)*2;
    }
    else
    {
        rga_info->osd_height = rga_info->resize_height;
    }
    if(rga_info->crop_flag == 1)
    {
        rga_info->out_width = mpp_info->crop_width;
        rga_info->out_height = mpp_info->crop_height;
    }
    else
    {
        rga_info->out_width = rga_info->resize_width;
        rga_info->out_height = rga_info->resize_height;        
    }
    if((rga_info->out_width != rga_info->osd_width) || (rga_info->out_height != rga_info->osd_height))
    {
        rga_info->crop_flag = 1; //裁剪标志
    }
    else
    {
        rga_info->crop_flag = 0; //裁剪标志
    }
    mpp_info->width = rga_info->out_width;
    mpp_info->height = rga_info->out_height;

    SLOGI("out_color:%d rotate_width:%d rotate_height:%d resize_width:%d resize_height:%d image_w0:%d image_y0:%d osd_width:%d osd_height:%d out_width:%d out_height:%d totate_type:%d resize:%d crop:%d pixel_byte:%d\n",
          rga_info->out_color, rga_info->rotate_width, rga_info->rotate_height,
          rga_info->resize_width, rga_info->resize_height, rga_info->image_w0, rga_info->image_y0,
          rga_info->osd_width, rga_info->osd_height, rga_info->out_width, rga_info->out_height,
          rga_info->rotate_type, rga_info->resize, rga_info->crop_flag, rga_info->pixel_byte);

	/*判断输出的类型*/
	switch(mpp_info->out_type){
		case NULL_OUT_TYPE:
			mpp_type = 0;
			pc_type = 0;
		break;
		case MPP_OUT_TYPE:
			mpp_type = 1;
			pc_type = 0;
		break;

		case PC_OUT_TYPE:
			mpp_type = 0;
			pc_type = 1;
		break;

		case MPP_PC_OUT_TYPE:
			mpp_type = 1;
			pc_type = 1;
		break;
		default:
			mpp_type = 0;
			pc_type = 0;
			return 1;
	}
	SLOGI("[DEBUG] mpp_type:%d pc_type%d chn:(%d-%d) \n", mpp_type, pc_type, chn, mpp_info->mpp_chn);
	/*是否是编码输出*/
	if(mpp_type){
        ret = crop_chn_image_init(rga_info->rga_chn);
        if(ret != 0) 
        {
            SLOGI("writePL_chn_init error \n");
            goto err_2;
        }
        ret = gpu_osd_chn_image_init(rga_info->rga_chn);
        if (ret != 0) {
            SLOGI("gpu_osd_chn_image_init error \n");
            goto err_3;
        }

		/*如果编码输出，初始化编码资源*/
        ret = request_type2_mpp_init(rga_info->rga_chn);
        if(ret != 0){
            SLOGI("request_type2_mpp_init ERROR \n");
            goto err_4;
        }
        usleep(10*1000);

		printf("rgb rk_enable\n");
		g_axififo_info->rk_enable = 1;
	}
	return COMD_PROC_SUCCESS;

err_4:
    request_type2_mpp_destory(rga_info->rga_chn);
err_3:
    gui_osd_overlapped_chn_destory(rga_info->rga_chn);
    gpu_osd_chn_image_destory(rga_info->rga_chn);
err_2:
    crop_chn_destory(rga_info->rga_chn);
    crop_chn_image_destory(rga_info->rga_chn);
	return COMD_PROC_FAILED;
}
/* 帧类型3 (图像叠加数据传输) */
static unsigned int axififo_frame_request_type3(unsigned char* frame_buff, unsigned int frame_size)
{
    int ret = 0;

    pcie_chn_stack_image(frame_buff,frame_size);
    return COMD_PROC_SUCCESS;
}
/*****************************************************************************/
/* 帧类型4（存储文件配置） */
int axififo_frame_request_type4(unsigned char* frame_buff, unsigned int frame_size)
{
    int ret = 0;
    snprintf(filename,frame_size + strlen(ssd_baseaddr),"%s%s",ssd_baseaddr,frame_buff+1);
	fp = fopen(filename,"w+");
	if(fp == NULL){
		perror("fopen err!!!");
        ret = -1;
		goto error;
	}
    // SetOutputFilename(filename);
	printf("create file ok\n");

    // axififo_send(frame_buff[0], COMD_PROC_SUCCESS);
    return COMD_PROC_SUCCESS;
    error:
    	fclose(fp);
        fp = NULL;
		// axififo_send(frame_buff[0], COMD_PROC_FAILED);
        return COMD_PROC_FAILED;
}
/* 帧类型5（删除文件配置） */
int axififo_frame_request_type5(unsigned char* frame_buff, unsigned int frame_size)
{
    int ret = 0;
    snprintf(filename,frame_size + strlen(ssd_baseaddr),"%s%s",ssd_baseaddr,frame_buff+1);
	ret = remove(filename);
	if(ret != 0){
        printf("Error deleting file %s.\n", filename);
        goto error;
    }
	printf("destroy file %s ok\n",filename);

    return COMD_PROC_SUCCESS;
	error:
		// axififo_send(frame_buff[0], COMD_PROC_FAILED);
        return COMD_PROC_FAILED;
}

/* 帧类型6（获取文件列表） */
int axififo_frame_request_type6(unsigned char* frame_buff, unsigned int frame_size)
{
    int ret = 0;
    if(frame_buff[1]==0xfa&&frame_buff[2]==0x3c&&frame_buff[3]==0x55&&frame_buff[4]==0xaa)
    {
		ret = write_dirent_struct_file(SDD_ADDRESS);
		if(ret != 0){
            perror("write_dirent_struct_file err!!!");
			goto error;
		}
        ret = trans_dirent_struct_file(0x07);
        if(ret != 0){
            perror("trans_dirent_struct_file err!!!");
            goto error;
        }
	}else{
		printf("error Tail\n");
		goto error;
	}
    return ret;
	error:
	    return COMD_PROC_FAILED;
}
/* 帧类型7（数据传输配置） */
int axififo_frame_request_type7(unsigned char* frame_buff, unsigned int frame_size)
{
    int ret = 0;
    int len = ntohl(*(unsigned int *)(frame_buff+1));
    if(fp == NULL){
        perror("fopen");
        ret = -1;
        goto error;
    }
	printf("recv len is %d\n",len);
	if(len > 0){
		ret = pcie_read_data(len,fp);
        if(ret != 0)
        {
            perror("pcie_read_data err!!!");
            goto error;
        }
	}else if(len == 0){
		printf("recv complete\n");
        if(fp == NULL){
            printf("[error]no file open!\n");
            goto error;
        }else{
            fflush(fp);
            fclose(fp);
            fp = NULL;
        }
	}
	return COMD_PROC_SUCCESS;
	error:
	    return COMD_PROC_FAILED;
}

/* 帧类型9（解码开始配置） */
int axififo_frame_request_type9(unsigned char* frame_buff, unsigned int frame_size)
{
    int ret = 0;
    int chn = frame_buff[1];
    printf("%s:chn%d\n",__func__,chn);
    if(chn >= MPP_DECODE_NTHREAD_MAX)
    {
        ret = DecodeStart(chn);
        if(ret < 0){
            goto error;
        }
    }
    else
    {
        ret = EncodeStart(chn);
        if(ret < 0){
            goto error;
        }
    }
    usleep(100000);
    return COMD_PROC_SUCCESS;
    error:
		// axififo_send(frame_buff[0], COMD_PROC_FAILED);
        return COMD_PROC_FAILED;
}

/* 帧类型8（fpgaVDMA配置） */
int axififo_frame_request_type8(unsigned char* frame_buff, unsigned int frame_size)
{
    int ret = 0;
    vdma_configuration_info_t * vdma_info = (vdma_configuration_info_t *)(frame_buff+1);
    unsigned int* vdma_temp = (unsigned int *)((unsigned char *)vdma_info + 1);
    for(int i = 0;i < sizeof(vdma_configuration_info_t)/sizeof(unsigned int); i++)
    {
        *(vdma_temp + i) = ntohl(*(vdma_temp + i));
    }
	pcie_VdmaConfigInit((vdma_configuration_info_t *)vdma_info);
    return COMD_PROC_SUCCESS;
	error:
		// axififo_send(frame_buff[0], COMD_PROC_FAILED);
	    return COMD_PROC_FAILED;
}
/* 帧类型10（解码停止配置） */
int axififo_frame_request_type10(unsigned char* frame_buff, unsigned int frame_size)
{
#if 0
    int ret = 0;
    if(frame_buff[1]==0xeb&&frame_buff[2]==0x90&&frame_buff[3]==0x3c&&frame_buff[4]==0xfa)
    {
		DecodeStop();
	}else{
		printf("error Tail\n");
		goto error;
	}

	axififo_send(frame_buff[0], COMD_PROC_SUCCESS);
	return ret;
	error:
		// axififo_send(frame_buff[0], COMD_PROC_FAILED);
	return COMD_PROC_FAILED;
#else
    int ret = 0;
    int reg_offset ;
    int chn = frame_buff[1];

    if(chn >= MPP_DECODE_NTHREAD_MAX)
    {
        DecodeStop(chn);
    }
    else
    {
        EncodeStop(chn);
    }

    return COMD_PROC_SUCCESS;
    error:
        return COMD_PROC_FAILED;
#endif
}
/* 帧类型11（系统自检） */
int axififo_frame_request_type11(unsigned char* frame_buff, unsigned int frame_size)
{
    int ret = 0;
    int len = frame_size - 1;
    int send_size = 0;
    uartlist_send_info_t head;
    if( len%4 != 0)
    {
        printf("type12 frame_size abnormal\n");
        len = (len/4+1)*4;
    }
    uint8_t* send_buf = malloc(frame_size+7);
    unsigned int* value_offset = (unsigned int*)(frame_buff+1);

    ret = getstatfs((dev_disk_capacity_t *)value_offset);
    if(ret != 0)
    {
        perror("getstatfs err!!!");
        goto error;
    }
    value_offset[4] = htonl(BUILD_TIME);
    value_offset[5] = htonl(VERSION);
    // for(int j = 6;j<len/4;j++)
    // {
    //     printf("read reg:%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY+ntohl(value_offset[j]));
    //     value_offset[j] = htonl(pcie_bar_read_reg(pcie_common_param_reg, ntohl(value_offset[j])));
    //     printf("value:%x\n",value_offset[j]);
    //     usleep(10);
    // }

    head.head = htons(0xec91);
    head.len = htonl(frame_size);
    head.mode = 0x0c&0xff;
    memcpy(send_buf,&head,7);
    memcpy(send_buf+7,value_offset,len);
    *(send_buf+len+7) = check_sum(send_buf+6, frame_size);

    send_size = stream_fifo_write_data(&g_axififo_info->fifo_reg,send_buf,len+8);
    if(send_size != len+8)
        goto error;

    free(send_buf);
    return ret;
    error:
        free(send_buf);
        return COMD_PROC_FAILED;
}
/* 帧类型12（叠加配置） */
int axififo_frame_request_type12(unsigned char* frame_buff, unsigned int frame_size)
{
    int ret = 0;
    int chn = frame_buff[1];
    uint32_t reg_offset = 0;
    data_stream_t* data_stream = data_stream_get();
    image_chn_info_t* image_chn_info = data_stream->image_chn_info[chn];
    rga_param_t* rga_info = &image_chn_info->rga_info;
    osd_image_info_t* osd_info = &image_chn_info->osd_info;

    memcpy(osd_info, frame_buff + 1, sizeof(osd_image_info_t));
#if TEST
#else
    osd_info->font_color = ntohl(osd_info->font_color);
    osd_info->color = ntohl(osd_info->color);
    osd_info->string_x = ntohl(osd_info->string_x);
    osd_info->string_y = ntohl(osd_info->string_y);
    osd_info->x = ntohl(osd_info->x);
    osd_info->y = ntohl(osd_info->y);
    osd_info->x_off = ntohl(osd_info->x_off);
    osd_info->y_off = ntohl(osd_info->y_off);
    osd_info->image_x0 = ntohl(osd_info->image_x0);
    osd_info->image_y0 = ntohl(osd_info->image_y0);
    osd_info->data_size = ntohl(osd_info->data_size);

#endif
    u_int16_t canvas_width = rga_info->out_width;
    u_int16_t canvas_height = rga_info->out_height;
    if(canvas_width > VIDEO_OSD_WIDTH)
    {
        canvas_width = VIDEO_OSD_WIDTH;
    }
    if(canvas_height > VIDEO_OSD_HIGHT)
    {
        canvas_height = VIDEO_OSD_HIGHT;
    }
    printf("chn:%d,osd_type:%d,shape:%d,font_type:%d,,font_format:%d,font_size:%d,line_width:%d,time_position:%d\n",
           chn, osd_info->osd_type, osd_info->shape, osd_info->font_type,osd_info->font_format, osd_info->font_size,osd_info->line_width,osd_info->time_position);
    printf("font_color:%d,color:0x%x,string_x:%x,string_y:%x,x:%x,y:%x,x_off:%x,y_off:%x,image_x0:%x,image_y0:%x,data_size:%d\n",
           osd_info->font_color,osd_info->color,osd_info->string_x,osd_info->string_y,
           osd_info->x,osd_info->y,osd_info->x_off,osd_info->y_off,
           osd_info->image_x0,osd_info->image_y0,osd_info->data_size);
    if(osd_info->font_format >= OSD_GUI_FONT_FORMAT_MAX || osd_info->font_size >= OSD_GUI_FONT_SIZE_MAX)
    {
        printf("font_format or font_size error Use default parameters, osd_info->font_format:%d,osd_info->font_size:%d\n",osd_info->font_format,osd_info->font_size);
        goto error;
    }
    if((osd_info->image_x0 + rga_info->image_w0 > canvas_width) || (osd_info->image_y0 + rga_info->image_y0 > canvas_height))
    {
        printf("image_x0 image_y0 too large, image_x0:%d,image_y0:%d canvas_width:%d canvas_height:%d\n",
               osd_info->image_x0+rga_info->image_w0, osd_info->image_y0+rga_info->image_y0,canvas_width,canvas_height);
        goto error;
    }
    if((osd_info->string_x + (osd_info->font_size+1)*11*osd_info->data_size > canvas_width) || (osd_info->string_y + (osd_info->font_size+1)*11 > canvas_height))
    {
        printf("string_x string_y too large, string_x:%d,string_y:%d\n",
               osd_info->string_x + (osd_info->font_size+1)*11*osd_info->data_size, osd_info->string_y + (osd_info->font_size+1)*11);
        goto error;
    }
    if((osd_info->x + osd_info->x_off > canvas_width) || (osd_info->y + osd_info->y_off > canvas_height))
    {
        printf("x_off y_off too large, x_off:%d,y_off:%d\n",
            osd_info->x + osd_info->x_off, osd_info->y + osd_info->y_off);
        goto error;
    }
    return COMD_PROC_SUCCESS;
    error:
        memset(osd_info,0x00,sizeof(osd_image_info_t));
        return COMD_PROC_FAILED;
}

int axififo_frame_request_type13(unsigned char* frame_buff, unsigned int frame_size)
{
    struct tm time_elements;
    struct timeval tv;

    time_elements.tm_year = ((frame_buff[1]>>4) & 0xf) *10 + (frame_buff[1]&0xf) +2000 -1900;
    time_elements.tm_mon = ((frame_buff[2]>>4) & 0xf) *10 + (frame_buff[2]&0xf) - 1;
    time_elements.tm_mday = ((frame_buff[3]>>4) & 0xf) *10 + (frame_buff[3]&0xf);
    time_elements.tm_hour = ((frame_buff[4]>>4) & 0xf) *10 + (frame_buff[4]&0xf);
    time_elements.tm_min = ((frame_buff[5]>>4) & 0xf) *10 + (frame_buff[5]&0xf);
    time_elements.tm_sec = ((frame_buff[6]>>4) & 0xf) *10 + (frame_buff[6]&0xf);
    time_elements.tm_isdst = -1; // 自动设置夏令时
    tv.tv_usec = ((frame_buff[7]>>4) & 0xf) *100000 + (frame_buff[7]&0xf)*10000 + ((frame_buff[8]>>4) & 0xf)*1000;
    convert_to_timeval(&time_elements,&tv);
    if (settimeofday(&tv, NULL) == -1) {
        perror("settimeofday error");
        goto error;
    }

    return COMD_PROC_SUCCESS;
    error:
        return COMD_PROC_FAILED;
}

int axififo_frame_request_type14(unsigned char* frame_buff, unsigned int frame_size)
{
    u_int8_t bank = 0;
    u_int8_t fiberate = 0;
    // pcie_bar_write_reg(pcie_common_param_reg,523*4,1);
    // usleep(10);
    // int value = pcie_bar_read_reg(pcie_common_param_reg,523*4);
    // usleep(10);
    // printf("reg:%x value:%d\n",0x523*4,value);
    // for(int i = 0;i < frame_size/2;i++)
    // {
    //     bank = *(++frame_buff);
    //     fiberate = *(++frame_buff);
    //     switch (fiberate)
    //     {
    //     case FiberRate_1_0625:
    //     {
    //         for(int j = 0;j < 4;j++)
    //         {
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x2b4 + bank*0x5000 + j*0x1000, 0x190);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x2f0 + bank*0x5000 + j*0x1000, 0x0007);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x40 + bank*0x5000 + j*0x1000, 0x736);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x18c + bank*0x5000 + j*0x1000, 0x80c3);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x274 + bank*0x5000 + j*0x1000, 0x5668);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x148 + bank*0x5000 + j*0x1000, 0x2);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x198 + bank*0x5000 + j*0x1000, 0x3039);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x1f0 + bank*0x5000 + j*0x1000, 0x3e0);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x270 + bank*0x5000 + j*0x1000, 0xacc);
    //             usleep(10);
    //         }
    //         pcie_bar_write_reg(fiber_rate_config_reg, 0x2c0 + bank*0x5000 + 0x4000, 0x0);
    //         usleep(10);
    //         pcie_bar_write_reg(fiber_rate_config_reg, 0x258 + bank*0x5000 + 0x4000, 0x7f);
    //         usleep(10);
    //         pcie_bar_write_reg(fiber_rate_config_reg, 0x250 + bank*0x5000 + 0x4000, 0x42);
    //         usleep(10);
    //         pcie_bar_write_reg(fiber_rate_config_reg, 0x248 + bank*0x5000 + 0x4000, 0x21e8);
    //         usleep(10);
    //         pcie_bar_write_reg(fiber_rate_config_reg, 0x274 + bank*0x5000 + 0x4000, 0x21e8);
    //         usleep(10);
    //         break;
    //     }
    //     case FiberRate_2_125:
    //     {
    //         for(int j = 0;j < 4;j++)
    //         {
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x2b4 + bank*0x5000 + j*0x1000, 0x190);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x2f0 + bank*0x5000 + j*0x1000, 0x0007);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x40 + bank*0x5000 + j*0x1000, 0x746);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x18c + bank*0x5000 + j*0x1000, 0x80c2);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x274 + bank*0x5000 + j*0x1000, 0x5668);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x148 + bank*0x5000 + j*0x1000, 0x2);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x198 + bank*0x5000 + j*0x1000, 0x3039);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x1f0 + bank*0x5000 + j*0x1000, 0x2e0);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x270 + bank*0x5000 + j*0x1000, 0xacc);
    //             usleep(10);
    //         }
    //         pcie_bar_write_reg(fiber_rate_config_reg, 0x2c0 + bank*0x5000 + 0x4000, 0x0);
    //         usleep(10);
    //         pcie_bar_write_reg(fiber_rate_config_reg, 0x258 + bank*0x5000 + 0x4000, 0x7f);
    //         usleep(10);
    //         pcie_bar_write_reg(fiber_rate_config_reg, 0x250 + bank*0x5000 + 0x4000, 0x42);
    //         usleep(10);
    //         pcie_bar_write_reg(fiber_rate_config_reg, 0x248 + bank*0x5000 + 0x4000, 0x21e8);
    //         usleep(10);
    //         pcie_bar_write_reg(fiber_rate_config_reg, 0x274 + bank*0x5000 + 0x4000, 0x21e8);
    //         usleep(10);
    //         break;
    //     }
    //     case FiberRate_3_1875:
    //     {
    //         for(int j = 0;j < 4;j++)
    //         {
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x2b4 + bank*0x5000 + j*0x1000, 0x190);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x2f0 + bank*0x5000 + j*0x1000, 0xf007);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x40 + bank*0x5000 + j*0x1000, 0x746);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x18c + bank*0x5000 + j*0x1000, 0x80c2);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x274 + bank*0x5000 + j*0x1000, 0x0);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x148 + bank*0x5000 + j*0x1000, 0x2);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x198 + bank*0x5000 + j*0x1000, 0x3039);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x1f0 + bank*0x5000 + j*0x1000, 0x2e0);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x270 + bank*0x5000 + j*0x1000, 0x0);
    //             usleep(10);
    //         }
    //         pcie_bar_write_reg(fiber_rate_config_reg, 0x2c0 + bank*0x5000 + 0x4000, 0x9);
    //         usleep(10);
    //         pcie_bar_write_reg(fiber_rate_config_reg, 0x258 + bank*0x5000 + 0x4000, 0x1ff);
    //         usleep(10);
    //         pcie_bar_write_reg(fiber_rate_config_reg, 0x250 + bank*0x5000 + 0x4000, 0x64);
    //         usleep(10);
    //         pcie_bar_write_reg(fiber_rate_config_reg, 0x248 + bank*0x5000 + 0x4000, 0x25e8);
    //         usleep(10);
    //         pcie_bar_write_reg(fiber_rate_config_reg, 0x274 + bank*0x5000 + 0x4000, 0x25e8);
    //         usleep(10);
    //         break;
    //     }
    //     case FiberRate_4_25:
    //     {
    //         for(int j = 0;j < 4;j++)
    //         {
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x2b4 + bank*0x5000 + j*0x1000, 0x190);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x2f0 + bank*0x5000 + j*0x1000, 0x0007);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x40 + bank*0x5000 + j*0x1000, 0x756);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x18c + bank*0x5000 + j*0x1000, 0x80c1);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x274 + bank*0x5000 + j*0x1000, 0x5668);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x148 + bank*0x5000 + j*0x1000, 0x2);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x198 + bank*0x5000 + j*0x1000, 0x3039);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x1f0 + bank*0x5000 + j*0x1000, 0x1e0);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x270 + bank*0x5000 + j*0x1000, 0xacc);
    //             usleep(10);
    //         }
    //         pcie_bar_write_reg(fiber_rate_config_reg, 0x2c0 + bank*0x5000 + 0x4000, 0x0);
    //         usleep(10);
    //         pcie_bar_write_reg(fiber_rate_config_reg, 0x258 + bank*0x5000 + 0x4000, 0x7f);
    //         usleep(10);
    //         pcie_bar_write_reg(fiber_rate_config_reg, 0x250 + bank*0x5000 + 0x4000, 0x42);
    //         usleep(10);
    //         pcie_bar_write_reg(fiber_rate_config_reg, 0x248 + bank*0x5000 + 0x4000, 0x21e8);
    //         usleep(10);
    //         pcie_bar_write_reg(fiber_rate_config_reg, 0x274 + bank*0x5000 + 0x4000, 0x21e8);
    //         usleep(10);
    //         break;
    //     }
    //     case FiberRate_8:
    //     {
    //         for(int j = 0;j < 4;j++)
    //         {
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x2b4 + bank*0x5000 + j*0x1000, 0x1f0);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x2f0 + bank*0x5000 + j*0x1000, 0x0007);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x40 + bank*0x5000 + j*0x1000, 0x766);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x18c + bank*0x5000 + j*0x1000, 0x80c0);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x274 + bank*0x5000 + j*0x1000, 0x5668);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x148 + bank*0x5000 + j*0x1000, 0x2);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x198 + bank*0x5000 + j*0x1000, 0x3039);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x1f0 + bank*0x5000 + j*0x1000, 0x0e0);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x270 + bank*0x5000 + j*0x1000, 0xacc);
    //             usleep(10);
    //         }
    //         pcie_bar_write_reg(fiber_rate_config_reg, 0x2c0 + bank*0x5000 + 0x4000, 0x0);
    //         usleep(10);
    //         pcie_bar_write_reg(fiber_rate_config_reg, 0x258 + bank*0x5000 + 0x4000, 0x7f);
    //         usleep(10);
    //         pcie_bar_write_reg(fiber_rate_config_reg, 0x250 + bank*0x5000 + 0x4000, 0x3e);
    //         usleep(10);
    //         pcie_bar_write_reg(fiber_rate_config_reg, 0x248 + bank*0x5000 + 0x4000, 0x21e8);
    //         usleep(10);
    //         pcie_bar_write_reg(fiber_rate_config_reg, 0x274 + bank*0x5000 + 0x4000, 0x21e8);
    //         usleep(10);
    //         break;
    //     }
    //     case FiberRate_10:
    //     {
    //         for(int j = 0;j < 4;j++)
    //         {
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x2b4 + bank*0x5000 + j*0x1000, 0x1f0);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x2f0 + bank*0x5000 + j*0x1000, 0x0007);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x40 + bank*0x5000 + j*0x1000, 0x766);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x18c + bank*0x5000 + j*0x1000, 0x80c0);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x274 + bank*0x5000 + j*0x1000, 0x5568);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x148 + bank*0x5000 + j*0x1000, 0x42);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x198 + bank*0x5000 + j*0x1000, 0x303d);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x1f0 + bank*0x5000 + j*0x1000, 0x0e0);
    //             usleep(10);
    //             pcie_bar_write_reg(fiber_rate_config_reg, 0x270 + bank*0x5000 + j*0x1000, 0xacc);
    //             usleep(10);
    //         }
    //         pcie_bar_write_reg(fiber_rate_config_reg, 0x2c0 + bank*0x5000 + 0x4000, 0x0);
    //         usleep(10);
    //         pcie_bar_write_reg(fiber_rate_config_reg, 0x258 + bank*0x5000 + 0x4000, 0x1ff);
    //         usleep(10);
    //         pcie_bar_write_reg(fiber_rate_config_reg, 0x250 + bank*0x5000 + 0x4000, 0x4e);
    //         usleep(10);
    //         pcie_bar_write_reg(fiber_rate_config_reg, 0x248 + bank*0x5000 + 0x4000, 0x21e8);
    //         usleep(10);
    //         pcie_bar_write_reg(fiber_rate_config_reg, 0x274 + bank*0x5000 + 0x4000, 0x21e8);
    //         usleep(10);
    //         break;
    //     }
    //     default:
    //         break;
    //     }
    // }
    // pcie_bar_write_reg(pcie_common_param_reg,523*4,0);
    // usleep(10);
    // pcie_bar_write_reg(pcie_common_param_reg,0x04,0x1);
    // usleep(10);
    // pcie_bar_write_reg(pcie_common_param_reg,0x04,0x0);
    // usleep(10);
    // return COMD_PROC_SUCCESS;
    // error:
        return COMD_PROC_FAILED;
}
/* 帧类型18（时序参数配置） */
static int axififo_frame_request_type18(unsigned char* frame_buff, unsigned int frame_size)
{
    // temporal_parameters_t * temporal_par = (temporal_parameters_t *)(frame_buff+1);
    // for(int i = 1;i < sizeof(temporal_parameters_t)/sizeof(u_int32_t); i++)
    // {
    //     *((u_int32_t*)temporal_par+i) = ntohl(*((u_int32_t*)temporal_par+i));
    // }

    // pcie_bar_write_reg(pcie_common_param_reg, 0x960, temporal_par->a818_param_select);
    // printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY + 0x960,temporal_par->a818_param_select);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_common_param_reg, 0x964, temporal_par->active_pixels_per_line);
    // printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY + 0x964,temporal_par->active_pixels_per_line);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_common_param_reg, 0x968, temporal_par->active_lines_per_video);
    // printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY + 0x968,temporal_par->active_lines_per_video);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_common_param_reg, 0x96c, temporal_par->num_of_advb_obj2_frames);
    // printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY + 0x96c,temporal_par->num_of_advb_obj2_frames);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_common_param_reg, 0x970, temporal_par->obj0_advb_frame);
    // printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY + 0x970,temporal_par->obj0_advb_frame);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_common_param_reg, 0x974, temporal_par->obj2_advb_frame);
    // printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY + 0x974,temporal_par->obj2_advb_frame);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_common_param_reg, 0x978, temporal_par->sofi_to_sofi);
    // printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY + 0x978,temporal_par->sofi_to_sofi);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_common_param_reg, 0x97c, temporal_par->sofi_to_sofn_firtst);
    // printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY + 0x97c,temporal_par->sofi_to_sofn_firtst);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_common_param_reg, 0x980, temporal_par->sofn_to_sofn_next);
    // printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY + 0x980,temporal_par->sofn_to_sofn_next);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_common_param_reg, 0x984, temporal_par->sofnlast_to_sofi);
    // printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY + 0x984,temporal_par->sofnlast_to_sofi);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_common_param_reg, 0x988, temporal_par->eofn_to_sofn);
    // printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY+0x988,temporal_par->eofn_to_sofn);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_common_param_reg, 0x98c, temporal_par->eoft_to_sofn);
    // printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY+0x98c,temporal_par->eoft_to_sofn);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_common_param_reg, 0x990, temporal_par->a818_image_format);
    // printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY+0x990, temporal_par->a818_image_format);
    // usleep(1000);
    // return COMD_PROC_SUCCESS;
    return COMD_PROC_FAILED;
}

static void axififo_send(unsigned char mode, unsigned int data)
{
#if 1
    // printf("axifofo_send!!!!!\n");
    int ret;
    unsigned int send_size;
    uartlist_send_info_t send_info;
    // time_t current_time;
    // struct tm* time_info;
    // char time_string[30];

    send_info.head = htons(0xec91);
    send_info.len = htonl(2);
    send_info.mode = mode;
    send_info.data = data;
    send_info.sum = check_sum((unsigned char*)&send_info.mode, sizeof(send_info.data)+1);

    send_size = 0;
    while(1){
        // time(&current_time);
        // time_info = localtime(&current_time);
        // strftime(time_string, sizeof(time_string), "%Y-%m-%d %H:%M:%S", time_info);
        // printf("当前时间：%s\n", time_string);
        
        ret = stream_fifo_write_data(&g_axififo_info->fifo_reg,(uint8_t *)(&send_info + send_size),sizeof(send_info) - send_size);
        if(ret == (sizeof(send_info) - send_size)){
            break;
        }
        send_size += ret;
        // usleep(1000);
    }
#endif
    return ;
}


void axififo_frame_data_analyse(unsigned char* frame_buffer, unsigned int frame_size)
{
    unsigned int cmd_ok;
    unsigned char swap1,swap2;

    cmd_ok = COMD_PROC_FAILED;

    switch(frame_buffer[0]){
        case UARTLIST_REQUEST_TYPE1:/* 帧类型1 (fpga通道配置) */
            SLOGI("[Debug]++++UARTLIST_REQUEST_TYPE1+++++\n");
            cmd_ok = axififo_frame_request_type1(frame_buffer, frame_size);
            break;

        case UARTLIST_REQUEST_TYPE2:/* 帧类型2 (图像采集配置) */
            SLOGI("[Debug]++++UARTLIST_REQUEST_TYPE2+++++\n");
            cmd_ok = axififo_frame_request_type2(frame_buffer, frame_size);
            break;

        case UARTLIST_REQUEST_TYPE3:/* 帧类型3 (图像叠加数据传输) */
            SLOGI("[Debug]++++UARTLIST_REQUEST_TYPE3+++++\n");
            cmd_ok = axififo_frame_request_type3(frame_buffer, frame_size);
            usleep(1000);
            break;

        case UARTLIST_REQUEST_TYPE4:/* 帧类型4（存储文件配置） */
            SLOGI("[Debug]++++UARTLIST_REQUEST_TYPE4+++++\n");
            cmd_ok = axififo_frame_request_type4(frame_buffer, frame_size);
            break;

        case UARTLIST_REQUEST_TYPE5:/* 帧类型5（删除文件配置） */
            SLOGI("[Debug]++++UARTLIST_REQUEST_TYPE5+++++\n");
            cmd_ok = axififo_frame_request_type5(frame_buffer,frame_size);
            break;

        case UARTLIST_REQUEST_TYPE6:/* 帧类型6（获取文件列表） */
            SLOGI("[Debug]++++UARTLIST_REQUEST_TYPE6+++++\n");
            cmd_ok = axififo_frame_request_type6(frame_buffer,frame_size);
            if(cmd_ok == 0)
                return ;
            break; 

        case UARTLIST_REQUEST_TYPE7:/* 帧类型7（数据传输配置） */
            SLOGI("[Debug]++++UARTLIST_REQUEST_TYPE7+++++\n");
            cmd_ok = axififo_frame_request_type7(frame_buffer,frame_size);
            break; 

        case UARTLIST_REQUEST_TYPE8:/* 帧类型8（fpgaVDMA配置） */
            SLOGI("[Debug]++++UARTLIST_REQUEST_TYPE8+++++\n");
            cmd_ok = axififo_frame_request_type8(frame_buffer,frame_size);
            break;

        case UARTLIST_REQUEST_TYPE9:/* 帧类型9（编解码开始配置）*/
            SLOGI("[Debug]++++UARTLIST_REQUEST_TYPE9+++++\n");
            cmd_ok = axififo_frame_request_type9(frame_buffer,frame_size);
            break;
        case UARTLIST_REQUEST_TYPE10:/* 帧类型10（编解码停止配置） */
            SLOGI("[Debug]++++UARTLIST_REQUEST_TYPE10+++++\n");        
            cmd_ok = axififo_frame_request_type10(frame_buffer,frame_size);
            sleep(2);
            break;
        case UARTLIST_REQUEST_TYPE11:/* 帧类型11（系统自检） */
            SLOGI("[Debug]++++UARTLIST_REQUEST_TYPE11+++++\n");
            cmd_ok = axififo_frame_request_type11(frame_buffer,frame_size);
            if(cmd_ok == 0)
                return ;
            break;
        case UARTLIST_REQUEST_TYPE12:/* 帧类型12（叠加配置） */
            SLOGI("[Debug]++++UARTLIST_REQUEST_TYPE12+++++\n");
            cmd_ok = axififo_frame_request_type12(frame_buffer,frame_size);
            break;
        case UARTLIST_REQUEST_TYPE13:/* 帧类型13（设置板卡时间） */
            SLOGI("[Debug]++++UARTLIST_REQUEST_TYPE13++++\n");
            cmd_ok = axififo_frame_request_type13(frame_buffer,frame_size);
            break;
        case UARTLIST_REQUEST_TYPE14:/* 帧类型14（光纤速率配置） */
            SLOGI("[Debug]++++UARTLIST_REQUEST_TYPE14++++\n");
            cmd_ok = axififo_frame_request_type14(frame_buffer,frame_size);
            break;
        case UARTLIST_REQUEST_TYPE15:/* 帧类型15（升级文件传输  ） */
            cmd_ok = COMD_PROC_SUCCESS;
            SLOGI("[Debug]++++UARTLIST_REQUEST_TYPE15++++\n");
            upgrade_num = frame_buffer[1] << 24 | frame_buffer[2] << 16 | frame_buffer[3] << 8 | frame_buffer[4];
            // upgrade_num = htonl(upgrade_num);
            if(!upgrade_num){
                upgrade_size = frame_buffer[5] << 24 | frame_buffer[6] << 16 | frame_buffer[7] << 8 | frame_buffer[8];
                // upgrade_size = htonl(upgrade_size);

                if(upgrade_buffer){
                    free(upgrade_buffer);
                }

                upgrade_buffer = (unsigned char*)malloc(upgrade_size);
                if(upgrade_buffer == NULL){
                    cmd_ok = COMD_PROC_FAILED;
                    break;
                }
                memset(upgrade_file_name, 0x0, sizeof(upgrade_file_name));
                snprintf(upgrade_file_name,47,"%s%s", ZPK_PWD, frame_buffer+9);
                SLOGI("[Debug] upgrade file name:%s \n", upgrade_file_name);
                printf("[Debug]:upgrade num:%u size:%u pkgsize:%u \n",upgrade_num, upgrade_size, frame_size);
            } else {
                /*如果总长度为RK3588_UPGRADE_DATA_START，表示没有有效数据，则表示升级数据传输完成*/
                if(frame_size == RK3588_UPGRADE_DATA_START){
                    if(!upgrade_buffer){
                        break;
                    }

                    upgrade_fp = fopen(upgrade_file_name, "ab+");
                    if (upgrade_fp == NULL) {
                        free(upgrade_buffer);
                        upgrade_buffer = NULL;
                        cmd_ok = COMD_PROC_FAILED;
                        break;
                    }
                    
                    fwrite(upgrade_buffer, upgrade_size, 1, upgrade_fp);
	                fclose(upgrade_fp);
                    free(upgrade_buffer);
                    upgrade_buffer = NULL;
                    SLOGI("[DEBUG]-- UPGRADE FILE WRITE OK --- \n");
                    sync();
                    sleep(1);
                    system("/tmp/real_part/script/unpacker.sh");
                }else {
                    memcpy(upgrade_buffer + ((upgrade_num - 1)*RK3588_UPGRADE_DATA_SIZE), frame_buffer+RK3588_UPGRADE_DATA_START, (frame_size - RK3588_UPGRADE_DATA_START));
                }
            }
            break;
        case UARTLIST_REQUEST_TYPE16:/* 帧类型16（升级状态查询 ） */
            SLOGI("[Debug]++++UARTLIST_REQUEST_TYPE16++++\n");
            cmd_ok = read_zpk_upgrade_state();
            break;
        case UARTLIST_REQUEST_TYPE17:/* 帧类型17（图像生成配置） */
            SLOGI("[Debug]++++UARTLIST_REQUEST_TYPE17++++\n");
            cmd_ok = axififo_frame_request_type17(frame_buffer, frame_size);
            break;
        case UARTLIST_REQUEST_TYPE18:/* 帧类型18（时序参数配置） */
            SLOGI("[Debug]++++UARTLIST_REQUEST_TYPE18++++\n");
            cmd_ok = axififo_frame_request_type18(frame_buffer,frame_size);
            break;
        default: 
            SLOGI("set uartlist request type%d error \n", frame_buffer[0]);
            cmd_ok = RECV_PAK_ERROR;
            break;
    }
    axififo_send(frame_buffer[0], cmd_ok);
}
/******************************************************************************************/

static int axififo_recv_data_analysis(unsigned char *buffer, unsigned int length)
{
    uint8_t sum;
    unsigned int cmd_val;
	int i = 0, j = 0, index = 0;
    int ret = -1;
    unsigned int frame_length;
    unsigned char *frame_buff;
    image_frame_info_t frame_info;
    
	for(index = 0; index < length; index++) {
		local_buffer[(axififo_count + index) % PCIE_AXIFIFO_BUF_SIZE_MAX] = buffer[index];
	}

	axififo_count += length;

    if((axififo_count - axififo_index) < (AXIFIFO_HEAD_SIZE + AXIFIFO_LENGH_SIZE)){
        return 4;
    }
	for(i = axififo_index; i < (axififo_count - 2); i++) {
		if(local_buffer[i % PCIE_AXIFIFO_BUF_SIZE_MAX] == msg_head_1 && local_buffer[(i + 1) % PCIE_AXIFIFO_BUF_SIZE_MAX] == msg_head_2) {    
            frame_length = (local_buffer[(i + 2) % PCIE_AXIFIFO_BUF_SIZE_MAX] << 24) + (local_buffer[(i + 3) % PCIE_AXIFIFO_BUF_SIZE_MAX] << 16) + (local_buffer[(i + 4) % PCIE_AXIFIFO_BUF_SIZE_MAX] << 8) + local_buffer[(i + 5) % PCIE_AXIFIFO_BUF_SIZE_MAX];
            // printf("frame_length:%d,local_buffer[2]:0x%x,local_buffer[3]:0x%x,local_buffer[4]:0x%x,local_buffer[5]:0x%x\n",frame_length,local_buffer[(i + 2) % PCIE_AXIFIFO_BUF_SIZE_MAX], \
            // local_buffer[(i + 3) % PCIE_AXIFIFO_BUF_SIZE_MAX],local_buffer[(i + 4) % PCIE_AXIFIFO_BUF_SIZE_MAX],local_buffer[(i + 5) % PCIE_AXIFIFO_BUF_SIZE_MAX]); 
            if(frame_length < (axififo_count - i - 6)){
                frame_buff = g_axififo_info->axififo_data;
                for(j = 0; j < frame_length; j++){
                    frame_buff[j] = local_buffer[(i + j + 6) % PCIE_AXIFIFO_BUF_SIZE_MAX];/*frame_buff 帧类型 帧数据*/
                }

                sum = check_sum(frame_buff, frame_length);
                if(sum == local_buffer[(i + frame_length + 6) % PCIE_AXIFIFO_BUF_SIZE_MAX]){
                    axififo_frame_data_analyse(frame_buff, frame_length);
                } else {
                    SLOGI("uartlist frame data sum error,len:%d sum(0x%x 0x%x) \n", frame_length, \
                        sum, local_buffer[(i + frame_length + 6) % PCIE_AXIFIFO_BUF_SIZE_MAX]);
                    cmd_val = RECV_PAK_ERROR;
                    axififo_send(frame_buff[0], cmd_val);
                }
     
				axififo_index = i + frame_length + 7;
				i = axififo_index - 1;
				ret = 0;
			} else {
				axififo_index = i;
                ret = 2;
				break;
			}
            
		} else {
            ret = 3;
        }
	}

    return ret;
}


static void *axififo_recv_data_pthread(void* parameter)
{

    printf("axififo_recv_data_pthread start\n");
	u_int32_t recv_cnt;
    unsigned char RecvBuffer[AXIFIFO_RECV_DATA_SIZE];
    while(1){
        recv_cnt = stream_fifo_read_data_len(&g_axififo_info->fifo_reg);
        if(recv_cnt > 0){
            printf("fifo have data\n");
            stream_fifo_read_data(&g_axififo_info->fifo_reg, RecvBuffer, recv_cnt);
            for(int i = 0;i<recv_cnt;i++){
                printf("%#x ",RecvBuffer[i]);
            }
            printf("\n");
            axififo_recv_data_analysis(RecvBuffer,recv_cnt);
        }
        xt_nanosleep(0,100);
    }
	return NULL;
}


int axififo_data_init(void)
{
	int ret;
	pthread_t recv_tid;
    char temp[100] = {0};
    
	g_axififo_info = (axififo_info_t*)malloc(sizeof(axififo_info_t));
	if(NULL == g_axififo_info){
		printf("[ERROR] axififo info malloc error \n");
        ret = A818_FIFO_DATA_INFO;
        goto err_0;
	}

    memset(g_axififo_info, 0, sizeof(axififo_info_t));
#if OLD_AXI_FIFO
    g_axififo_info->fifo_reg.cfg = (config_reg_t *)pcie_bar_mmap(0x40000, FPGA_PCIE_AXIFIFO_LEN,1);
    if(g_axififo_info->fifo_reg.cfg == NULL){
        printf("[ERROR] pcie axififo config reg error \n");
        ret = A818_FIFO_CONFIG_REG_MMAP;
        goto err_1;
    }

    g_axififo_info->fifo_reg.send_data = (config_reg_t *)pcie_bar_mmap(0x50000, FPGA_PCIE_DATA_LEN,1);
    if(g_axififo_info->fifo_reg.send_data == NULL){
        printf("[ERROR] pcie axififo data reg error \n");
        ret = A818_FIFO_DATA_REG_MMAP;
        goto err_2;
    }
    g_axififo_info->fifo_reg.recv_data = (void*)((unsigned long)g_axififo_info->fifo_reg.send_data + FPGA_PCIE_READ_DATA_OFFSET);
#else
    g_axififo_info->fifo_reg.cfg = (config_reg_t *)pcie_bar_mmap(FPGA_PCIE_AXIFIFO_PHY, FPGA_PCIE_AXIFIFO_LEN,0);
    if(g_axififo_info->fifo_reg.cfg == NULL){
        printf("[ERROR] pcie axififo config reg error \n");
        ret = A818_FIFO_CONFIG_REG_MMAP;
        goto err_2;
    }
#endif
    stream_fifo_reset(&g_axififo_info->fifo_reg);

	ret = pthread_create(&recv_tid,NULL,&axififo_recv_data_pthread,NULL);
    if (ret != 0) {
        printf("[ERROR] Create axififo_recv_data_pthread thread failed %d\n",__func__,ret);
        ret = A818_FIFO_RECV_THREAD;
        goto err_3;
    }

	return A818_SUCCESS;
err_3:
err_2:
    pcie_bar_munmap(g_axififo_info->fifo_reg.send_data, FPGA_PCIE_DATA_LEN);
err_1:
    pcie_bar_munmap(g_axififo_info->fifo_reg.cfg, FPGA_PCIE_AXIFIFO_LEN);
err_0:
	free(g_axififo_info);
    return ret;
}
int axi_fifo_write_buf(unsigned char *buffer, int length){
    int len = 0;
    len = stream_fifo_write_data(&g_axififo_info->fifo_reg,buffer,length);
    return len;
}
int axi_fifo_write_file_buf(FILE *tree_fp,int mode){
    int ret = 0;
    unsigned int len,free_size,ddr_len = 0;
    uartlist_send_info_t head;
    fseek(tree_fp, 0L, SEEK_END);
	free_size = ftell(tree_fp);
    rewind(tree_fp);
    unsigned char* buf = (unsigned char*)malloc(free_size+8);

    head.head = htons(0xec91);
    head.len = htonl(free_size+1);
    head.mode = mode&0xff;
    memcpy(buf,&head,7);
    free_size = fread(buf+7,sizeof(char),free_size,tree_fp);
    *(buf+free_size+7) = check_sum((unsigned char*)buf+6, free_size+1);

    len = dma_memory_mapped_write(buf+7,0,free_size,PCIE_H2C_0);
    printf("send len:%d,freesize:%d\n",len,free_size);
    if(len != (free_size))
        goto err;
    ddr_len = htonl(len);
    head.len = htonl(4);
    memcpy(buf,&head,7);
    memcpy(buf+7,&ddr_len,4);
    *(buf+4+7) = check_sum((unsigned char*)buf+6, 5);
    stream_fifo_write_data(&g_axififo_info->fifo_reg, buf , 12);

    free(buf);
    return ret;
err:
    free(buf);
    return -1;
}

int run_test()
{
    int ret = 0;

	cmd_buff = (unsigned char*)malloc(sizeof(mpp_encode_info_t) + 1);
	mpp_encode_info_t* mpp_info = (mpp_encode_info_t*)(cmd_buff + 1);
	mpp_info->chn = 0;
	mpp_info->fps = 25;
	mpp_info->width = 1920;
	mpp_info->height = 1080;
    mpp_info->image_w0 = 256;
	mpp_info->image_y0 = 256;
	mpp_info->mpp_bps = 1;
	mpp_info->extraction = 0;
	mpp_info->mpp_type = 0;
	mpp_info->out_type = MPP_OUT_TYPE;
	mpp_info->rotate = 0;
	mpp_info->position = 0;
	mpp_info->resize = 1;
	mpp_info->resize_width = 1920;
	mpp_info->resize_height = 1080;
	mpp_info->crop = 0;
	mpp_info->crop_width = 768;
	mpp_info->crop_height = 768;
	axififo_frame_request_type2(cmd_buff,sizeof(mpp_encode_info_t) + 1);
    ret = EncodeStart(mpp_info->chn);

    cfg_info = (cfg_info_t*)malloc(sizeof(cfg_info_t));
    if(cfg_info == NULL){
        printf("[ERROR] cfg_info malloc error \n");
        ret = A818_PCIE_DATA_MALLOC;
        return -1;
    }
    FILE* fp = NULL;
    cfg_info->channel_param.chn = 3;
    cfg_info->channel_param.rotate = 0;
    cfg_info->channel_param.image_length0 = 1920;
    cfg_info->channel_param.image_width0 = 1080;
    cfg_info->channel_param.image_fps = 60;
    cfg_info->channel_param.image_w0 = 256;
    cfg_info->channel_param.image_y0 = 256;
    printf("cfg_info_image_w0:%d, cfg_info_image_y0:%d\n",cfg_info->channel_param.image_w0,cfg_info->channel_param.image_y0);
    strcpy(cfg_info->in_filename_chn, "h264test.h264");
    sprintf(cfg_info->out_filename_chn, "output%d.raw",cfg_info->channel_param.chn);
    decode_configuration_init(cfg_info);
    ret = DecodeStart(cfg_info->channel_param.chn);
    // cfg_info->channel_param.chn = 2;
    // sprintf(cfg_info->out_filename_chn, "output%d.raw",cfg_info->channel_param.chn);
    // decode_configuration_init(cfg_info);
    // ret = DecodeStart(cfg_info->channel_param.chn);

    osd_tmp_info = (uint8_t *)malloc(sizeof(osd_image_info_t) + 1);
    if(osd_tmp_info == NULL){
        printf("[ERROR] osd_tmp_info malloc error \n");
        ret = A818_PCIE_DATA_MALLOC;
        return -1;
    }
    osd_image_info_t* osd_info = (osd_image_info_t *)(osd_tmp_info + 1);
    osd_info->chn = cfg_info->channel_param.chn;
    // osd_info->chn = mpp_info->chn;
    osd_info->osd_type = 0x0f;
    osd_info->shape = STITCHBASE_OSD_ROUND_SHAPE;
    osd_info->font_type = 0;
    osd_info->font_format = 0;
    osd_info->font_size = 2;
    osd_info->time_position = 0;
    osd_info->font_color = 0xffff00;
    osd_info->color =  0x000000;
    osd_info->string_x = 600;
    osd_info->string_y = 600;
    osd_info->x = 640;
    osd_info->y = 512;
    osd_info->x_off = 30;
    osd_info->y_off = 30;
    osd_info->image_x0 = 576;
    osd_info->image_y0 = 576;
    osd_info->data_size = 0x0f;
    strcpy(osd_info->data, "1920*1080@60Hz");
    GPU_Frame_st* gpu_frame_stream = gpu_data_stream_get();
    gpu_image_info_t *gpu_image = &gpu_frame_stream->gpu_image[cfg_info->channel_param.chn];
    // gpu_image_info_t *gpu_image = &gpu_frame_stream->gpu_image[mpp_info->chn];
    fp = fopen("/userdata/castle-256.rgb24", "rb");
    if(fp == NULL){
        printf("open file error\n");
        return -1;
    }
    fread(gpu_image->over_image,cfg_info->channel_param.image_w0 * cfg_info->channel_param.image_y0 *3 ,sizeof(unsigned char), fp);
    // fread(gpu_image->over_image, mpp_info->image_w0 * mpp_info->image_y0 *3, sizeof(unsigned char), fp);
    fflush(fp);
    fclose(fp);
    axififo_frame_request_type12(osd_tmp_info, sizeof(osd_image_info_t) + 1);
	return 0;
}

int run_destory()
{
    EncodeStop(0);
	free(cmd_buff);

    // DecodeStop(2);
	// free(cfg_info);
    // free(osd_tmp_info);
	return 0;
}

#if 0
void axififo_data_destory(void)
{
	int index;
	if(NULL != g_axififo_info){
        pcie_bar_munmap(g_axififo_info->fifo_reg.cfg, FPGA_PCIE_AXIFIFO_LEN);
		free(g_axififo_info);
	}
	return ;
}
int SendType4(){
    int ret = 0;
    unsigned char send_buf[] = {0xec,0x91,0x5,0x4,'t','e','s','t',0xc4};
    ret = stream_fifo_write_data(&g_axififo_info->fifo_reg,send_buf,sizeof(send_buf));
    if(ret != sizeof(send_buf)){
        printf("send error!\n");
        return -1;
    }
    return 0;
}
int SendType5(){
    int ret = 0;
    unsigned char send_buf[] = {0xec,0x91,0x5,0x5,'t','e','s','t',0xc5};
    ret = stream_fifo_write_data(&g_axififo_info->fifo_reg,send_buf,sizeof(send_buf));
    if(ret != sizeof(send_buf)){
        printf("send error!\n");
        return -1;
    }
    return 0;
}
int SendType9(){
    int ret = 0;
    unsigned char send_buf[] = {0xec,0x91,0x5,0x9,0x90,0xeb,0x3c,0xfa,0xba};
    ret = stream_fifo_write_data(&g_axififo_info->fifo_reg,send_buf,sizeof(send_buf));
    if(ret != sizeof(send_buf)){
        printf("send error!\n");
        return -1;
    }
    return 0;
}
int SendType6(){
    int ret = 0;
    unsigned char send_buf[] = {0xec,0x91,0xa,0x6,0,'0','0','1','.','h','2','6','4',0xc9};
    ret = stream_fifo_write_data(&g_axififo_info->fifo_reg,send_buf,sizeof(send_buf));
    if(ret != sizeof(send_buf)){
        printf("send error!\n");
        return -1;
    }
    return 0;
}

int main(int argc,char **argv){
    if(argc !=2 ){
        printf("Usage:[%s] typenum\n",argv[0]);
        return -1;
    }
    printf("********FIFO_TEST*************\n");
    printf("argv[1]:%d",atoi(argv[1]));
    int num;
    pcie_bar_open(0);
    axififo_data_init();
    // g_axififo_info->fifo_reg.cfg = (config_reg_t *)pcie_bar_mmap(FPGA_PCIE_AXIFIFO_PHY, FPGA_PCIE_AXIFIFO_LEN,0);
    // if(g_axififo_info->fifo_reg.cfg == NULL){
    //     printf("[ERROR] pcie axififo config reg error \n");
    // }
    while(1)
    {
        num = atoi(argv[1]);
        switch (num)
        {
        case 4:
            printf("send type4\n");
            SendType4();
            break;
        case 5:
            printf("send type5\n");
            SendType5();
            break;
        case 6:
            SendType6();
            break;
        case 9:
            SendType9();
            break;
        default:
            break;
        }
        sleep(1);
    }
    return 0;
}
// void test(){
//     pcie_bar_open(0);
//     axififo_data_init();
// }
#endif
