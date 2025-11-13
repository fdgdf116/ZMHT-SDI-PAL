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
#include <string.h>

#include "log.h"
#include "xutils.h"
#include "pcie_engine.h"
#include "data_engine.h"
#include "pcie_impl.h"
#include "pcie_reg_rw.h"
#include "a818_error.h"
#include "mpi_decode.h"
#include "fps_calc.h"
#include "rga_engine.h"
#include "osd_engine.h"
#include "gpu_interface.h"
#include "crop_engine.h"
#include "mpi_encode.h"
#include "data_engine.h"

#define ringbuffer_destory ringbuffer_deinit
#define PCIE_READ_IMAGE_DATA_ADDRESS (0x400000000)
#define PCIE_WRITE_IMAGE_DATA_ADDRESS (0x700000000)


#define FPGA2RK_VFIFO_SIZE_OFFSET (0x28)
#define RK2FPGA_VFIFO_SIZE_OFFSET (0x104)

#define FPGA_READ_DATA_ROW_MAX (200)
#define FPGA_READ_ONE_DATA_MAX (FPGA_ROW_DATA_SIZE * FPGA_READ_DATA_ROW_MAX)
// #define FPGA_READ_ONE_DATA_MAX (1*1024*1024)

#define FPGA_SYSTEM_RESETN (0x4)
#define MULTIPLEX_CHN_EN (0x14)
#define MULTIPLEX_RECV_EN (0x18)
#define IN_SOURCE_EN (0x1c)
#define FPGA_VFIFO_RESET_OFFSET (0x3c)

// #define FPGA_PCIE_RESET_OFFSET (0x3c)
#define FPGA_PCIE_ENABLE_OFFSET (0x40)
#define FPGA_PCIE_CHN_OFFSET (0x400)


#define FPGA_SET_CHN_LENGTH_OFFSET (0x0)
#define FPGA_SET_CHN_INTERVAL_OFFSET (0x04)
#define FPGA_SET_CHN_TYPE_OFFSET (0x08)
#define FPGA_SET_CHN_MODE_OFFSET (0xc)
#define FPGA_SET_CHN_ENABLE_OFFSET (0x10)
#define FPGA_SET_CHN_IMAGE_RATIO_OFFSET (0x14)
#define FPGA_SET_CHN_IMAGE_EXTRACTION_OFFSET (0x18)

#define FPGA_GET_CHN_IMAGE_WIDTH_OFFSET (0x1c)
#define FPGA_GET_CHN_IMAGE_HIGHT_OFFSET (0x20)

#define FPGA_GET_CHN_IMAGE_FILL_COLUMN_OFFSET (0x24)
#define FPGA_GET_CHN_IMAGE_FILL_ROW_OFFSET (0x28)
#define FPGA_GET_CHN_IMAGE_RATIO_WIDTH_OFFSET (0x2c)
#define FPGA_GET_CHN_IMAGE_RATIO_HIGHT_OFFSET (0x30)

#define FPGA_PCIE_REG_PHY (0x10000)
#define FPGA_PCIE_PC_REG_PHY (0x20000)
#define FPGA_PCIE_REG_LEN (0x10000)

#define FPGA_PCIE_COMMON_PARM_REG_PHY (0x00010000)

#define FPGA_PCIE_STACK_CHAR_REG_PHY  (0x00100000)
#define FPGA_PCIE_STACK_CHAR_REG_LEN  (0xc0000)
#define CHANNEL_LEN_CHAR              (0x8000)
#define CHANNEL_OFFSET                (0x10000)

#define FPGA_PCIE_STACK_IMAGE_REG_PHY (0x00200000)
#define FPGA_PCIE_STACK_IMAGE_REG_LEN (0x170000)
#define CHANNEL_LEN_IMG               (0x20000)

#define FPGA_PCIE_VDMA_CONFIG_REG_PHY (0x00400000)
#define FPGA_PCIE_VDMA_CONFIG_REG_LEN (0xc0000)

#define FIBER_RATE_CONFIG_REG_PHY (0x00060000)
#define FIBER_RATE_CONFIG_REG_LEN (0x10000)

#define FPGA_PCIE_DATA_HEAD_SIZE (64)
#define PCIE_HEAD_CHN_OFFSET (17)

#define PCIE_GET_DATA_HEAD(buffer) ({ \
	unsigned char *bd_buffer = buffer;	            \
	unsigned int val;               \
	val = bd_buffer[0] << 24 | bd_buffer[1] << 16 | bd_buffer[2] << 8 | bd_buffer[3]; val;})

#define SET_INTERNAL_IN_RESOLUTION(src_width, src_height, src_fps, set_row_width, set_interval) { 	             \
	.width = src_width,   				                       \
	.height = src_height,					                   \
	.fps = src_fps, \
	.row_width = set_row_width, \
	.interval = set_interval, \
}

extern char ssd_baseaddr[];
typedef struct {
	uint8_t *pcie_data;
    ring_buffer_t *pcie_data_rb;
    void* pcie_reg;
    // void* pcie_pc_reg;
} pcie_fpga_info_t;

static pcie_fpga_info_t *g_fpga_pcie;
static void* pcie_reg;
void* pcie_common_param_reg;
static void* pcie_stack_char_reg;
static void* pcie_stack_image_reg;
void* pcie_vdma_config_reg;
void* fiber_rate_config_reg;
int chn_num = 1;

static cfg_info_t* cfg_info[MPP_DECODE_NTHREAD_MAX] = {NULL};
cfg_info_t* GetCfgInfo(int chan0_sel_mode )
{
    if(chan0_sel_mode < 0 || chan0_sel_mode >= DATA_CHANNEL_NUM_MAX || cfg_info[chan0_sel_mode] == NULL)
    {
        SLOGI("chan0_sel_mode:%d cfg_info is NULL\n", chan0_sel_mode);
        return NULL;
    }
    return cfg_info[chan0_sel_mode];
}

void ChnSet(int num){
    chn_num = num;
}
int Get_Chn(){
    return chn_num;
}
uint8_t * GetPcieData(){
    return g_fpga_pcie->pcie_data;
}
void pcie_fpga_reg_reset(void)
{
    // pcie_bar_write_reg(g_fpga_pcie->pcie_reg, FPGA_PCIE_ENABLE_OFFSET, 0);
    // pcie_bar_write_reg(g_fpga_pcie->pcie_pc_reg, FPGA_PCIE_ENABLE_OFFSET, 0);
    // usleep(100*1000);
    // for(int chn = 0; chn < 24; chn++) {
    //     reg_offset = ((chn + 1) * FPGA_PCIE_CHN_OFFSET);
    //     pcie_bar_write_reg(g_fpga_pcie->pcie_reg, (reg_offset + FPGA_SET_CHN_ENABLE_OFFSET), 0);
    //     pcie_bar_write_reg(g_fpga_pcie->pcie_pc_reg, (reg_offset + FPGA_SET_CHN_ENABLE_OFFSET), 0);
    //     usleep(1000);
    // }

    // pcie_bar_write_reg(g_fpga_pcie->pcie_reg, FPGA_PCIE_RESET_OFFSET, 0);
    // usleep(500*1000);
    // pcie_bar_write_reg(g_fpga_pcie->pcie_reg, FPGA_PCIE_RESET_OFFSET, 1);
    // usleep(500*1000);

    //系统复位
    pcie_bar_write_reg(g_fpga_pcie->pcie_reg, FPGA_SYSTEM_RESETN, 0);
    usleep(1000);
    pcie_bar_write_reg(g_fpga_pcie->pcie_reg, FPGA_SYSTEM_RESETN, 1);
    usleep(1000);

    SLOGI("[%s] pcie reset end \n", __func__);
}

void pcie_close_chn_enable(unsigned int chn)
{
    unsigned int reg_offset;
    reg_offset = ((chn + 1) * FPGA_PCIE_CHN_OFFSET);
    pcie_bar_write_reg(g_fpga_pcie->pcie_reg, (reg_offset + FPGA_SET_CHN_ENABLE_OFFSET), 0);
    usleep(1000);
}

void pcie_chn_stack_configuration_reg_init(stack_configuration_info_t* reg_val_info)
{
    // unsigned int reg_offset;
    // unsigned char chn;

    // chn = reg_val_info->chn;
    // // chn -= PCIE_SRC_START_CHANNEL;
    // reg_offset = (chn  * 0x4);

    // printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY + (0x640+reg_offset),1);
    // pcie_bar_write_reg(pcie_common_param_reg, (0x640+reg_offset),1);
    // usleep(1000);

    // pcie_bar_write_reg(pcie_common_param_reg, (reg_offset + 0x2c), reg_val_info->draw_mode0);
    // printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY + (reg_offset + 0x2c),reg_val_info->draw_mode0);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_common_param_reg, (reg_offset + 0x5c), reg_val_info->center_point_x0);
    // printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY + (reg_offset + 0x5c),reg_val_info->center_point_x0);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_common_param_reg, (reg_offset + 0x8c), reg_val_info->center_point_y0);
    // printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY + (reg_offset + 0x8c),reg_val_info->character_point_y0);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_common_param_reg, (reg_offset + 0xbc), reg_val_info->center_point_r0);
    // printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY + (reg_offset + 0xbc),reg_val_info->center_point_r0);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_common_param_reg, (reg_offset + 0xec), reg_val_info->center_point_s0);
    // printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY + (reg_offset + 0xec),reg_val_info->center_point_s0);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_common_param_reg, (reg_offset + 0x11c), reg_val_info->center_point_l0);
    // printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY + (reg_offset + 0x11c),reg_val_info->center_point_l0);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_common_param_reg, (reg_offset + 0x14c), reg_val_info->center_point_w0);
    // printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY + (reg_offset + 0x14c),reg_val_info->center_point_w0);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_common_param_reg, (reg_offset + 0x17c), reg_val_info->image_length0);
    // printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY + (reg_offset + 0x17c),reg_val_info->image_length0);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_common_param_reg, (reg_offset + 0x1ac), reg_val_info->image_width0);
    // printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY + (reg_offset + 0x1ac),reg_val_info->image_width0);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_common_param_reg, (reg_offset + 0x1dc), reg_val_info->chan0_sel_mode);
    // printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY + (reg_offset + 0x1dc),reg_val_info->chan0_sel_mode);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_common_param_reg, (reg_offset + 0x20c), reg_val_info->image_fill_length0);
    // printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY+(reg_offset + 0x20c),reg_val_info->image_fill_length0);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_common_param_reg, (reg_offset + 0x23c), reg_val_info->image_fill_width0);
    // printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY+(reg_offset + 0x23c),reg_val_info->image_fill_width0);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_common_param_reg, (reg_offset + 0x26c), reg_val_info->image_cut_length0);
    // printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY+(reg_offset + 0x26c),reg_val_info->image_cut_length0);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_common_param_reg, (reg_offset + 0x29c), reg_val_info->image_out_format);
    // printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY+(reg_offset + 0x29c),reg_val_info->image_out_format);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_common_param_reg, (reg_offset + 0x2cc), reg_val_info->draw_line_width0);
    // printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY+(reg_offset + 0x2cc),reg_val_info->draw_line_width0);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_common_param_reg, (reg_offset + 0x2fc), reg_val_info->character_point_x0);
    // printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY+(reg_offset + 0x2fc),reg_val_info->character_point_x0);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_common_param_reg, (reg_offset + 0x32c), reg_val_info->character_point_y0);
    // printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY+(reg_offset + 0x32c),reg_val_info->character_point_y0);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_common_param_reg, (reg_offset + 0x35c), reg_val_info->image_point_x0);
    // printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY+(reg_offset + 0x35c),reg_val_info->image_point_x0);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_common_param_reg, (reg_offset + 0x38c), reg_val_info->image_point_y0);
    // printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY+(reg_offset + 0x38c),reg_val_info->image_point_y0);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_common_param_reg, (reg_offset + 0x3bc), reg_val_info->yuv422_crcb_sel0);
    // printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY+(reg_offset + 0x3bc),reg_val_info->yuv422_crcb_sel0);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_common_param_reg, (reg_offset + 0x3ec), reg_val_info->chan0_video_mode);
    // printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY+(reg_offset + 0x3ec),reg_val_info->chan0_video_mode);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_common_param_reg, (reg_offset + 0x41c), reg_val_info->image_length1);
    // printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY+(reg_offset + 0x41c),reg_val_info->image_length1);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_common_param_reg, (reg_offset + 0x44c), reg_val_info->image_width1);
    // printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY+(reg_offset + 0x44c),reg_val_info->image_width1);
    // usleep(1000);

    // for(int index = 0;index < DATA_CHANNEL_NUM_MAX;index++)
    // {
    //     if((reg_val_info->chan0_sel_mode >> index) & 0x01)
    //     {
    //         reg_offset = index*0x04;
    //         pcie_bar_write_reg(pcie_common_param_reg, (0x18+reg_offset),1);
    //         printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY + (0x18+reg_offset),1);
    //         usleep(1000);
    //         pcie_bar_write_reg(pcie_common_param_reg, (0x18+reg_offset),0);
    //         printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY + (0x18+reg_offset),0);
    //         usleep(1000);
    //     }
    // }
}

int decode_configuration_init(cfg_info_t* reg_val_info)
{
    int ret = 0;
    unsigned int reg_offset;
    if(reg_val_info->channel_param.chn < MPP_DECODE_NTHREAD_MAX)
    {
        printf("decode_configuration_init chn:%d err\n",reg_val_info->channel_param.chn);
        return 1;
    }
    cfg_info_t* cfg_info = GetCfgInfo(reg_val_info->channel_param.chn%MPP_DECODE_NTHREAD_MAX);
    memcpy(cfg_info, reg_val_info, sizeof(cfg_info_t));
    sprintf(cfg_info->in_filename_chn, "%s%s", ssd_baseaddr, reg_val_info->in_filename_chn);
    sprintf(cfg_info->out_filename_chn, "%s%s", "/tmp/", reg_val_info->out_filename_chn);
    channel_param_t channel_param = cfg_info->channel_param;
    data_stream_t* data_stream = data_stream_get();
    image_chn_info_t* image_chn_info = data_stream->image_chn_info[reg_val_info->channel_param.chn];
    mpp_decode_info_t* mpp_info = data_stream->mpp_decode_info[reg_val_info->channel_param.chn%MPP_DECODE_NTHREAD_MAX];
    pcie_info_t* pcie_info = data_stream->pcie_info[reg_val_info->channel_param.chn%MPP_DECODE_NTHREAD_MAX];

    mpp_info->mpp_chn = reg_val_info->channel_param.chn%MPP_DECODE_NTHREAD_MAX;
    MpiDecLoopData* loop_data = &mpp_info->dec_data;
    rga_param_t* rga_info = &image_chn_info->rga_info;
    data_stream_t* gs_data_stream = data_stream_get();

    if(!mpp_info->decode_state)
    {
        printf("decoding!!!\n");
        return -1;
    }
    parse_stream_info(cfg_info->in_filename_chn,&mpp_info->decode_type,&mpp_info->fps,&mpp_info->width, &mpp_info->height);
    loop_data->fps = mpp_info->fps;
    loop_data->fp_input = fopen(cfg_info->in_filename_chn, "rb");
    if (loop_data->fp_input == NULL) {
        SLOGI("open input file %s failed\n", cfg_info->in_filename_chn);
        return -1;
    }
    
    if(strlen(reg_val_info->out_filename_chn) != 0)
    {
        printf("out_filename_chn:%s\n",reg_val_info->out_filename_chn);
        loop_data->fp_output = fopen(cfg_info->out_filename_chn, "wb+");
        if (loop_data->fp_output == NULL) {
            SLOGI("open output file %s failed\n", cfg_info->out_filename_chn);
            fclose(loop_data->fp_output);
            return -1;
        }
    }
    
    rga_info->rga_chn = reg_val_info->channel_param.chn;
    rga_info->out_color = 2;
    rga_info->image_w0 = channel_param.image_w0;
    rga_info->image_y0 = channel_param.image_y0;
    rga_info->rotate_type = cfg_info->channel_param.rotate;
    rga_info->resize = cfg_info->channel_param.resize;
    rga_info->crop_flag = cfg_info->channel_param.crop;
    rga_info->src_width = mpp_info->width;
    rga_info->src_height = mpp_info->height;
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
        rga_info->resize_width  = cfg_info->channel_param.resize_width;
        rga_info->resize_height = cfg_info->channel_param.resize_higth;
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
        rga_info->out_width = cfg_info->channel_param.image_length0;
        rga_info->out_height = cfg_info->channel_param.image_width0;
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
    pcie_info->out_width = rga_info->out_width;
    pcie_info->out_height = rga_info->out_height;

    SLOGI("out_color:%d rotate_width:%d rotate_height:%d resize_width:%d resize_height:%d image_w0:%d image_y0:%d osd_width:%d osd_height:%d out_width:%d out_height:%d totate_type:%d resize:%d crop:%d pixel_byte:%f\n",
          rga_info->out_color, rga_info->rotate_width, rga_info->rotate_height,
          rga_info->resize_width, rga_info->resize_height, rga_info->image_w0, rga_info->image_y0,
          rga_info->osd_width, rga_info->osd_height, rga_info->out_width, rga_info->out_height,
          rga_info->rotate_type, rga_info->resize, rga_info->crop_flag, rga_info->pixel_byte);

    ret = writePL_chn_init(rga_info->rga_chn);
    if(ret != 0) 
    {
        SLOGI("writePL_chn_init error \n");
        goto err_1;
    }
    ret = crop_chn_init(rga_info->rga_chn);
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
    ret = rga_chn_image_init(rga_info->rga_chn);
    if (ret != 0) {
        SLOGI("rga_chn_image_init error \n");
        goto err_4;
    }
    return A818_SUCCESS;
err_4:
    rga_pthread_chn_destory(rga_info->rga_chn);
    data_stream_chn_data_destory(rga_info->rga_chn);
err_3:
    gui_osd_overlapped_chn_destory(rga_info->rga_chn);
    gpu_osd_chn_image_destory(rga_info->rga_chn);
err_2:
    crop_chn_destory(rga_info->rga_chn);
    crop_chn_image_destory(rga_info->rga_chn);
err_1:
    writePL_chn_destory(mpp_info->mpp_chn);
    return ret;    
}

void pcie_VdmaConfigInit(vdma_configuration_info_t *vdma_configuration_info){
    // unsigned int reg_offset;
    // unsigned char chn;

    // chn = vdma_configuration_info->chn;
    // // chn -= PCIE_SRC_START_CHANNEL;
    // reg_offset = (chn  * 0x10000);
    // // SLOGI("[Debug] %s reg offset:0x%x src_width:%d src_height:%d \n", __func__, reg_offset, reg_val_info->src_width, reg_val_info->src_height);

    // pcie_bar_write_reg(pcie_vdma_config_reg, (reg_offset + 0x00), vdma_configuration_info->VDMA_MM2S_DMACR);
    // printf("offset:%x write:%x\n",FPGA_PCIE_VDMA_CONFIG_REG_PHY+(reg_offset + 0x00),vdma_configuration_info->VDMA_MM2S_DMACR);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_vdma_config_reg, (reg_offset + 0x04), vdma_configuration_info->VDMA_MM2S_DMASR);
    // printf("offset:%x write:%x\n",FPGA_PCIE_VDMA_CONFIG_REG_PHY+(reg_offset + 0x04),vdma_configuration_info->VDMA_MM2S_DMASR);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_vdma_config_reg, (reg_offset + 0x54), vdma_configuration_info->VDMA_MM2S_HSIZE);
    // printf("offset:%x write:%x\n",FPGA_PCIE_VDMA_CONFIG_REG_PHY+(reg_offset + 0x54),vdma_configuration_info->VDMA_MM2S_HSIZE);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_vdma_config_reg, (reg_offset + 0x58), vdma_configuration_info->VDMA_MM2S_DLYSTRIDE);
    // printf("offset:%x write:%x\n",FPGA_PCIE_VDMA_CONFIG_REG_PHY+(reg_offset + 0x58),vdma_configuration_info->VDMA_MM2S_DLYSTRIDE);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_vdma_config_reg, (reg_offset + 0x5c), vdma_configuration_info->VDMA_MM2S_START_ADDR1);
    // printf("offset:%x write:%x\n",FPGA_PCIE_VDMA_CONFIG_REG_PHY+(reg_offset + 0x5c),vdma_configuration_info->VDMA_MM2S_START_ADDR1);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_vdma_config_reg, (reg_offset + 0x60), vdma_configuration_info->VDMA_MM2S_START_ADDR2);
    // printf("offset:%x write:%x\n",FPGA_PCIE_VDMA_CONFIG_REG_PHY+(reg_offset + 0x60),vdma_configuration_info->VDMA_MM2S_START_ADDR2);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_vdma_config_reg, (reg_offset + 0x64), vdma_configuration_info->VDMA_MM2S_START_ADDR3);
    // printf("offset:%x write:%x\n",FPGA_PCIE_VDMA_CONFIG_REG_PHY+(reg_offset + 0x64),vdma_configuration_info->VDMA_MM2S_START_ADDR3);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_vdma_config_reg, (reg_offset + 0x30), vdma_configuration_info->VDMA_S2MM_DMACR);
    // printf("offset:%x write:%x\n",FPGA_PCIE_VDMA_CONFIG_REG_PHY+(reg_offset + 0x30),vdma_configuration_info->VDMA_S2MM_DMACR);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_vdma_config_reg, (reg_offset + 0x34), vdma_configuration_info->VDMA_S2MM_DMASR);
    // printf("offset:%x write:%x\n",FPGA_PCIE_VDMA_CONFIG_REG_PHY+(reg_offset + 0x34),vdma_configuration_info->VDMA_S2MM_DMASR);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_vdma_config_reg, (reg_offset + 0xa4), vdma_configuration_info->VDMA_S2MM_HSIZE);
    // printf("offset:%x write:%x\n",FPGA_PCIE_VDMA_CONFIG_REG_PHY+(reg_offset + 0xa4),vdma_configuration_info->VDMA_S2MM_HSIZE);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_vdma_config_reg, (reg_offset + 0xa8), vdma_configuration_info->VDMA_S2MM_DLYSTRIDE);
    // printf("offset:%x write:%x\n",FPGA_PCIE_VDMA_CONFIG_REG_PHY+(reg_offset + 0xa8),vdma_configuration_info->VDMA_S2MM_DLYSTRIDE);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_vdma_config_reg, (reg_offset + 0xac), vdma_configuration_info->VDMA_S2MM_START_ADDR1);
    // printf("offset:%x write:%x\n",FPGA_PCIE_VDMA_CONFIG_REG_PHY+(reg_offset + 0xac),vdma_configuration_info->VDMA_S2MM_START_ADDR1);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_vdma_config_reg, (reg_offset + 0xb0), vdma_configuration_info->VDMA_S2MM_START_ADDR2);
    // printf("offset:%x write:%x\n",FPGA_PCIE_VDMA_CONFIG_REG_PHY+(reg_offset + 0xb0),vdma_configuration_info->VDMA_S2MM_START_ADDR2);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_vdma_config_reg, (reg_offset + 0xb4), vdma_configuration_info->VDMA_S2MM_START_ADDR3);
    // printf("offset:%x write:%x\n",FPGA_PCIE_VDMA_CONFIG_REG_PHY+(reg_offset + 0xb4),vdma_configuration_info->VDMA_S2MM_START_ADDR3);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_vdma_config_reg, (reg_offset + 0x50), vdma_configuration_info->VDMA_MM2S_VSIZE);
    // printf("offset:%x write:%x\n",FPGA_PCIE_VDMA_CONFIG_REG_PHY+(reg_offset + 0x50),vdma_configuration_info->VDMA_MM2S_VSIZE);
    // usleep(1000);
    // pcie_bar_write_reg(pcie_vdma_config_reg, (reg_offset + 0xa0), vdma_configuration_info->VDMA_S2MM_VSIZE);
    // printf("offset:%x write:%x\n",FPGA_PCIE_VDMA_CONFIG_REG_PHY+(reg_offset + 0xa0),vdma_configuration_info->VDMA_S2MM_VSIZE);
    // usleep(100000);
}

void pcie_chn_stack_char(unsigned char* reg_val_info, unsigned int frame_size)
{
    unsigned int reg_offset;
    unsigned char chn;
    int ret = 0;

    chn = *(reg_val_info+1);
    reg_offset = (chn  * CHANNEL_OFFSET);
    int data_len = ntohl(*(unsigned int *)(reg_val_info+2));
    data_stream_t *data_stream = data_stream_get();
    image_chn_info_t* image_chn_info = data_stream->image_chn_info[chn];

    uint8_t *pcie_buf = g_fpga_pcie->pcie_data;
    ret = dma_memory_mapped_read(pcie_buf,PCIE_READ_IMAGE_DATA_ADDRESS, data_len,PCIE_C2H_0);

    if((ret == data_len) && (ret < OSD_CHAR_NUM))
    {
        image_chn_info->osd_info.data_size = ret;
        memcpy(image_chn_info->osd_info.data, pcie_buf, ret);
        // save_image_data(pcie_buf,ret,"/tmp/cvtcolor_char.raw");
        // pcie_write_buf(pcie_stack_char_reg, reg_offset, pcie_buf,ret);   
    }
    else
    {
        perror("dma_memory_mapped_read error!!!");
    }
}

int pcie_chn_stack_char_test_w()
{
    printf("[%s]write char\n",__func__);
    unsigned int base_address;
    unsigned int reg_offset;
    unsigned char chn = 0;
    unsigned char fp_buf[100*50*4] = {0};

    // FILE *fp = fopen("200x50.raw","r+");
    FILE *fp = fopen("100x50.bmp","r+");

    int times = 100 * 50;
    if(fp == NULL){
        perror("fopen");
        return -1;
    }
    for(int i = 0; i < times; i++){
        fp_buf[i*4] = 0x0;
        int len = fread(fp_buf + i * 4 + 1,1,3,fp);
        if(len != 3){
            printf("error len (%d)\n",len);
            return -1;
        }
        *((int *)fp_buf + i) = htonl(*((int *)fp_buf + i));
    }
    // int len = fread(fp_buf,1,200*50*2,fp);
    // if(len != 200*50*2){
    //     printf("error len (%d)\n",len);
    //     return -1;
    // }

    for(int i = 0; i < 12; i++){
        chn = i;
        printf("chn %d\n",chn);
        reg_offset = (chn  * 0x10000);
        pcie_write_buf(pcie_stack_char_reg,reg_offset,fp_buf,100*50*4);
    }
    fclose(fp);
    return 0;
}

int pcie_chn_stack_char_test_r()
{
    printf("[%s]write char\n",__func__);
    unsigned int base_address = 0;
    unsigned int reg_offset = 0;
    unsigned char chn = 4;
    unsigned char fp_buf[100*50*2] = {0};

    reg_offset = (chn  * 0x10000);
    FILE *fp = fopen("out_200x50.raw","w+");
    if(fp == NULL){
        perror("fopen");
        return -1;
    }
    printf("write chnnal [%d]\n",chn);
    pcie_read_buf(pcie_stack_char_reg,reg_offset,fp_buf,100*50*2);

    int len = fwrite(fp_buf,1,100*50*2,fp);
    if(len != 100*50*2){
        printf("fwrite error!\n");
        fclose(fp);

        return -1;
    }
    fclose(fp);
    return 0;
}

void pcie_chn_stack_image(unsigned char* reg_val_info, unsigned int frame_size)
{
    unsigned int reg_offset;
    unsigned char chn;
    int ret = 0;
    chn = *(reg_val_info+1);
    reg_offset = (chn  * CHANNEL_OFFSET * 2);
    int data_len = ntohl(*(unsigned int *)(reg_val_info+2));
    GPU_Frame_st* gpu_frame_stream = gpu_data_stream_get();
	gpu_image_info_t *gpu_image = &gpu_frame_stream->gpu_image[chn];
    data_stream_t* data_stream = data_stream_get();
    image_chn_info_t* image_chn_info = data_stream->image_chn_info[chn];
    rga_param_t* rga_info = &image_chn_info->rga_info;

    uint8_t *pcie_buf = g_fpga_pcie->pcie_data;
    ret = dma_memory_mapped_read(pcie_buf,PCIE_READ_IMAGE_DATA_ADDRESS, data_len,PCIE_C2H_0);

    if(ret == rga_info->image_w0 * rga_info->image_y0 *3)
    {
        memcpy(gpu_image->over_image, pcie_buf, rga_info->image_w0 * rga_info->image_y0 *3);
        // save_image_data(gpu_image->over_image,ret,"/tmp/cvtcolor_image.raw");
        // pcie_write_buf(pcie_stack_image_reg,reg_offset,pcie_buf,ret);
    }
    else
        printf("dma_memory_mapped_read err len:%d ret:%d !!!!",rga_info->image_w0 * rga_info->image_y0 *3,ret);
}

int pcie_chn_stack_image_test_w()
{
    printf("[%s]write img\n",__func__);
    unsigned int base_address;
    unsigned int reg_offset;
    unsigned char chn;
    unsigned char fp_buf[100*100*4] = {0};

    // FILE *fp = fopen("200x200.raw","r+");
    FILE *fp = fopen("100x100.bmp","r+");

    // if(fp == NULL){
    //     perror("fopen");
    //     return -1;
    // }
    // int len = fread(fp_buf,1,200*200*2,fp);
    // if(len != 200*200*2){
    //     printf("error len (%d)\n",len);
    //     return -1;
    // }
    int times = 100 * 100;
    if(fp == NULL){
        perror("fopen");
        return -1;
    }
    for(int i = 0; i < times; i++){
        fp_buf[i*4] = 0x0;
        int len = fread(fp_buf + i * 4 + 1,1,3,fp);
        if(len != 3){
            printf("error len (%d)\n",len);
            return -1;
        }
        *((int *)fp_buf + i) = htonl(*((int *)fp_buf + i));
    }

    for(int i = 0; i < 12; i++){
        chn = i;
        printf("chn %d\n",chn);
        reg_offset = (chn  * 0x20000);
        pcie_write_buf(pcie_stack_image_reg,reg_offset ,fp_buf,100*100*4);/*80KB字符数据*/
    }
    fclose(fp);
    return 0;
}

int pcie_chn_stack_image_test_r()
{
    printf("[%s]write img\n",__func__);
    unsigned int base_address = 0;
    unsigned int reg_offset = 0;
    unsigned char chn = 1;
    unsigned char fp_buf[100*100*4] = {0};

    reg_offset = (chn  * 0x20000);
    FILE *fp = fopen("out_200x200.raw","w+");
    if(fp == NULL){
        perror("fopen");
        return -1;
    }
    printf("write chnnal [%d]\n",chn);
    pcie_read_buf(pcie_stack_image_reg,reg_offset,fp_buf,100*100*4);

    int len = fwrite(fp_buf,1,100*100*4,fp);
    if(len != 100*100*4){
        printf("fwrite error!\n");
        fclose(fp);

        return -1;
    }
    fclose(fp);
    return 0;
}

// void pcie_get_selftest_val(selftest_val_info_t* info)
// {
//    info->board_type = htonl(pcie_bar_read_reg(g_fpga_pcie->pcie_reg, 0x80));
//    info->series_number = htonl(pcie_bar_read_reg(g_fpga_pcie->pcie_reg, 0x84));
//    info->version1 = htonl(pcie_bar_read_reg(g_fpga_pcie->pcie_reg, 0x88));
//    info->version2 = htonl(pcie_bar_read_reg(g_fpga_pcie->pcie_reg, 0x8c));
//    info->link = htonl(pcie_bar_read_reg(g_fpga_pcie->pcie_reg, 0x90));
//    info->pc_link = pcie_bar_read_reg(g_fpga_pcie->pcie_reg, 0x94);
//    info->rk_link = pcie_bar_read_reg(g_fpga_pcie->pcie_reg, 0x98);

// }

int pcie_set_rk_enable()
{
    //vfifo复位
    pcie_bar_write_reg(g_fpga_pcie->pcie_reg, FPGA_VFIFO_RESET_OFFSET, 1);
    usleep(1000);
    pcie_bar_write_reg(g_fpga_pcie->pcie_reg, FPGA_VFIFO_RESET_OFFSET, 0);
    usleep(1000);
    //接收通道使能
    pcie_bar_write_reg(g_fpga_pcie->pcie_reg, MULTIPLEX_CHN_EN, 1);
    usleep(1000);
    //接收使能
    pcie_bar_write_reg(g_fpga_pcie->pcie_reg, MULTIPLEX_RECV_EN, 1);
    usleep(1000);
    //内源使能
    pcie_bar_write_reg(g_fpga_pcie->pcie_reg, IN_SOURCE_EN, 1);
    usleep(1000);
}

int pcie_set_rk_disable()
{
    //接收通道使能
    pcie_bar_write_reg(g_fpga_pcie->pcie_reg, MULTIPLEX_CHN_EN, 0);
    usleep(1000);
    //接收使能
    pcie_bar_write_reg(g_fpga_pcie->pcie_reg, MULTIPLEX_RECV_EN, 0);
    usleep(1000);
    //内源使能
    pcie_bar_write_reg(g_fpga_pcie->pcie_reg, IN_SOURCE_EN, 0);
    usleep(1000);
    //vfifo复位
    pcie_bar_write_reg(g_fpga_pcie->pcie_reg, FPGA_VFIFO_RESET_OFFSET, 1);
    usleep(1000);
    pcie_bar_write_reg(g_fpga_pcie->pcie_reg, FPGA_VFIFO_RESET_OFFSET, 0);
    usleep(1000);
}

// void pcie_set_pc_enable(unsigned char val)
// {
//     pcie_bar_write_reg(g_fpga_pcie->pcie_pc_reg, FPGA_PCIE_ENABLE_OFFSET, val);
// }

unsigned int pcie_get_fpga2rk_vfifo_szie(void)
{
    return pcie_bar_read_reg(g_fpga_pcie->pcie_reg, FPGA2RK_VFIFO_SIZE_OFFSET);
}

static void pcie_data_to_queue(image_frame_info_t *frame_info, ring_buffer_t *rb)
{
	image_frame_info_t frame_param, tmp_param;
	if (ringbuffer_is_full(rb)) {					
		//ringbuffer_get(rb, (uint8_t *)&tmp_param, sizeof(tmp_param));
        SLOGI("[Debug] %s ringbuffer_is_full", __func__);
	}

    memcpy(&frame_param,frame_info,sizeof(image_frame_info_t));		
	ringbuffer_put(rb, (uint8_t *)&frame_param, sizeof(frame_param));

    return ;
}


void pcie_fpga_device_stop(void)
{
    pcie_set_rk_disable();
    // pcie_set_pc_enable(0);
    // pcie_fpga_reg_reset();

    // while(pcie_get_fpga2rk_vfifo_szie()){
    //     usleep(1000);
    // }

    while(!ringbuffer_is_empty(g_fpga_pcie->pcie_data_rb)){
        usleep(100);  
    }
}

/* pcie 读取数据线程 */
static void *pcie_read_data_thread(void* parameter)
{
    int ret;
    struct timeval time;
    unsigned int pcie_buf_offset = 0;
    unsigned int pcie_read_frame = 0;
    unsigned int fpga_vfifo_size = 0;
    unsigned int frame_size = FPGA_READ_ONE_DATA_MAX;
    unsigned int pcie_offset = ((frame_size / DATA_PAGE) + 1) * DATA_PAGE;
    unsigned int pcie_offset_max = pcie_offset * PCIE_DATA_STREAM_FRAME_MAX;

    // data_stream_t *data_stream = data_stream_get();
    ring_buffer_t *rb = g_fpga_pcie->pcie_data_rb;
    uint8_t *pcie_buf = g_fpga_pcie->pcie_data;
    image_frame_info_t frame_info;

    while(1) {
        fpga_vfifo_size = pcie_get_fpga2rk_vfifo_szie();
        if(fpga_vfifo_size >= frame_size) {
            if(pcie_buf_offset >= pcie_offset_max){
                pcie_buf_offset = 0;
            }
            
            ret = dma_memory_mapped_read(pcie_buf + pcie_buf_offset,PCIE_READ_IMAGE_DATA_ADDRESS, frame_size,PCIE_C2H_0);
            if(ret == frame_size){
                // save_image_data(pcie_buf + pcie_buf_offset, ret, "/tmp/pcie_data.raw");
                frame_info.frame_offset = pcie_buf_offset;
                //frame_info.frame_index=pcie_read_frame;
                if(ringbuffer_is_full(rb)){
                    SLOGI("[%s] ringbuffer full \n", __func__);
                }
                pcie_data_to_queue(&frame_info, rb);
                pcie_buf_offset += pcie_offset;
                pcie_read_frame += 1;

            } else {
                SLOGI("[ERROR] PCIE READ DATA ERROR \n");
            }
        } else {
            time.tv_sec=0;
	        time.tv_usec=1;
            select(0,NULL,NULL,NULL,&time);
        }

    }
    return NULL;
}

/*奇偶行拼接*/
void odd_transform_yuv(unsigned char* yuv_data, int chn)
{
	data_stream_t *data_stream = data_stream_get();
	image_chn_info_t* image_chn_info = data_stream->image_chn_info[chn];
	int src_width = image_chn_info->rga_info.src_width;
	int src_height = image_chn_info->rga_info.src_height;

	uint16_t row_len =  src_width * 2;
    // 还原奇偶场数据
    uint8_t* temp_row = (uint8_t*)malloc(row_len*sizeof(row_len));
    // 建立一个映射，防止因为闭环导致出现未交换的行
    uint8_t* visit_map = malloc(src_height*sizeof(uint8_t));
	memset(visit_map, 0x00, src_height);
	
    uint32_t start = 1;
    uint32_t current = start;
    memcpy(temp_row, &yuv_data[current * row_len], row_len); // 保存第1行数据
    visit_map[start] = 1;
    
    while(1)
    {
        uint32_t next = (current % 2) ? (current + src_height) / 2 : current / 2;
        if (next == start)
        {
            // 先找到 start 对应的地址，完成这个闭环
            uint16_t temp_target = start < src_height / 2 ? start * 2 : 2 * (start - src_height / 2) + 1;
            memcpy(&yuv_data[row_len * temp_target], temp_row, row_len); // 保存第1行数据
            visit_map[temp_target] = 1;
	
            // 检查是否有闭环
            uint32_t unvisited = 0;
            for (uint16_t i = 1; i < src_height - 1; ++i)
                if (!visit_map[i])
                {
                    unvisited = i;
                    break;
                }
            if (unvisited)
            {
                start = unvisited;
                current = start;
                memcpy(temp_row, &yuv_data[start * row_len], row_len); // 保存第1行数据
                continue;
            }
            else
                break;
        }
        else
            visit_map[current] = 1;
        // 数据向目标索引移动
        memcpy(&yuv_data[current * row_len], &yuv_data[next * row_len], row_len);
        current = next;
    }
	
    // 释放内存
    free(temp_row);
	free(visit_map);
}

void pcie_fpga2rga_image_to_queue(unsigned char chn, image_frame_info_t *frame_info)
{
	image_frame_info_t event_param, tmp_param;
	data_stream_t *data_stream = data_stream_get();
	ring_buffer_t *rb = data_stream->chn_image[chn]->rga_data_rb;
	
	if (ringbuffer_is_full(rb)) {					
		ringbuffer_get(rb, (uint8_t *)&tmp_param, sizeof(tmp_param));
		SLOGI("[Debug] %s ringbuffer_is_full", __func__);
	}

	memcpy(&event_param,frame_info,sizeof(image_frame_info_t));
			
	ringbuffer_put(rb, (uint8_t *)&event_param, sizeof(event_param));
}

/*
	函数: int pcie_chn_data_analysis(unsigned char chn, unsigned char* chn_buf_l)
	作用: 高通道数据解析(基本上与低通道的解析是一致的，只是两者保存的一帧图的buffer是不一样的)
	参数:
	  chn:通道
	  chn_buf_l：一行数据(5184)
	返回值:
	  正确返回0，失败返回其他
*/
int pcie_chn_data_analysis(unsigned char chn, unsigned char* chn_buf_l)
{
	ring_buffer_t *chn_rb;
	uint32_t memcpy_offset; 
	uint32_t memcpy_len;
	uint8_t *chn_image_buf;
	image_frame_info_t chn_frame_info;
	gpu_image_info_t *gpu_image;
	fpga_chn_image_info_t* chn_image;
	GPU_Frame_st* gpu_frame_stream;
	data_stream_t *data_stream = data_stream_get();
	
	gpu_frame_stream = gpu_data_stream_get();
	if((!gpu_frame_stream)){
		printf("[Debug] %s chn:%d get gpu_frame_stream error \n", __func__, chn);
		return 1;
	}

	gpu_image = &gpu_frame_stream->gpu_image[chn];
	if((!gpu_image)){
		printf("[Debug] %s chn:%d get gpu_image error \n", __func__, chn);
		return 2;
	}

	chn_image = data_stream->chn_image[chn];
	if(!chn_image){
		SLOGI("-1-CHN:%d malloc chn_image error \n", chn);
		return 4;
	}
    chn_data_info_t* chn_data = &chn_image->chn_data;
    if (!chn_data)
    {
        SLOGI("-1-CHN:%d malloc chn_data error \n", chn);
		return 5;
    } 
    

	chn_image_buf = (uint8_t*)chn_image->chn_data.yuv420_data[chn_data->rb_offset].vir_addr;
	if((!chn_image_buf)){
		printf("[Debug] %s chn:%d get chn_image_buf error \n", __func__, chn);
		return 3;
	}

	/* 判断本行是不是图像的结束行，如果是结束行，则对数据进行拷贝 */
	if((chn_image->chn_data.data_offset + (FPGA_ROW_DATA_SIZE - FPGA_PCIE_DATA_HEAD_SIZE)) >= chn_image->chn_data.frame_size){

		memcpy_len = chn_data->frame_size - chn_data->data_offset; /* 剩余的进行拷贝 */
		memcpy(chn_image_buf + chn_data->data_offset, (chn_buf_l + FPGA_PCIE_DATA_HEAD_SIZE),memcpy_len);
        if(chn == 0)
            odd_transform_yuv(chn_image_buf,chn);
		// save_image_data(chn_image_buf,chn_data->frame_size,"/tmp/chn_image_buf.raw");
		chn_rb = GetInPutRingBufferPt(chn);
		if(chn_rb){
			if(!ringbuffer_is_full(chn_rb)){
				chn_frame_info.frame_offset = chn_data->rb_offset;
				//chn_frame_info.frame_index = gpu_image->rb_offset;
				chn_frame_info.frame_size = chn_data->frame_size;
				pcie_fpga2rga_image_to_queue(chn, &chn_frame_info);
				chn_data->rb_offset += 1;
				if(chn_data->rb_offset >= (chn_image->data_frame_max + 1)){
					chn_data->rb_offset = 0;
				}
			}
		} else {
			SLOGI("[Debug] chn:%d ringbuffer is NULL \n", chn);
		}

		chn_data->data_offset = 0;

		chn_image_buf = (uint8_t*)chn_image->chn_data.yuv420_data[chn_data->rb_offset].vir_addr;
		if((!chn_image_buf)){
			printf("[Debug] %s chn:%d get chn_image_buf error \n", __func__, chn);
			return 4;
		}

		/* 判断整行判断这一行是不是正好结束 */
		memcpy_offset = (memcpy_len + FPGA_PCIE_DATA_HEAD_SIZE);

		if(memcpy_offset != FPGA_ROW_DATA_SIZE){
			memcpy(chn_image_buf + chn_data->data_offset, (chn_buf_l + memcpy_offset), (FPGA_ROW_DATA_SIZE - memcpy_offset));
			chn_data->data_offset += (FPGA_ROW_DATA_SIZE - memcpy_offset); /* 更新拷贝的数据的大小 */
		}

	} else {
		memcpy(chn_image_buf + chn_data->data_offset, (chn_buf_l + FPGA_PCIE_DATA_HEAD_SIZE), (FPGA_ROW_DATA_SIZE - FPGA_PCIE_DATA_HEAD_SIZE));
		chn_data->data_offset += (FPGA_ROW_DATA_SIZE - FPGA_PCIE_DATA_HEAD_SIZE); /* 更新拷贝的数据的大小 */
	}

	return 0;
}

/*
	函数: static void *pcie_data_analysis_thread(void* parameter)
	作用: 读取到的数据解析线程
	参数:
		线程参数
	返回值:
		线程返回值
*/
/*
	说明:
		fpga传递的数据是有协议的，协议为64 + 5120 = 5184，每个5184会打一次帧头，
		因此按照5184的整数倍来读取数据，这样方便确认是不是丢数据。
*/
#if 0
static void *pcie_data_analysis_thread(void* parameter)
{
	u_int32_t ret;
	struct timeval time;
	uint8_t *pcie_buf;
	unsigned int chn, frame_index;
	chn = 0;
	image_frame_info_t frame_info;
	uint8_t *chn_buf_l;
	ring_buffer_t *rb = g_fpga_pcie->pcie_data_rb;
	unsigned int pcie_data_head = 0x499602d2;
	uint8_t *chn_image_buf = NULL;
	ring_buffer_t *chn_rb = NULL;
	image_frame_info_t chn_frame_info;
	data_stream_t *data_stream = data_stream_get();
	fpga_chn_image_info_t* chn_image = data_stream->chn_image[chn];
	GPU_Frame_st* gpu_frame_stream = gpu_data_stream_get();;
	gpu_image_info_t *gpu_image = &gpu_frame_stream->gpu_image[chn];;

	FILE* fp_datasrc = fopen("/userdata/output_720x576.yuyv","rb");
	if(fp_datasrc == NULL)
	{
		SLOGI("fp_datasrc open fail!");
		return NULL;
	}

	cpu_set_t mask;
	CPU_ZERO(&mask);
	// 限大核调度
	CPU_SET(4, &mask), CPU_SET(5, &mask);
	CPU_SET(6, &mask), CPU_SET(7, &mask);
	pthread_setaffinity_np(pthread_self( ), sizeof(mask), &mask);

	// chn_image_buf = (uint8_t*)malloc(gpu_image->frame_size);
	while(1){
		chn_image_buf = (uint8_t*)chn_image->chn_data.yuv422_data[gpu_image->rb_offset].vir_addr;
		if(chn_image_buf == NULL)
		{
			continue;
		}
		ret = fread(chn_image_buf,1,gpu_image->frame_size,fp_datasrc);
		fflush(fp_datasrc);
		if(ret != gpu_image->frame_size)
		{
			SLOGI("fp_datasrc read over ret:%d frame_size:%d,come back\n",ret,gpu_image->frame_size);
			rewind(fp_datasrc);
			continue;
		}
		chn_rb = chn_image->chn_data_rb;
		if(chn_rb){
			if(!ringbuffer_is_full(chn_rb)){
				chn_frame_info.frame_offset = gpu_image->rb_offset;
				//chn_frame_info.frame_index = gpu_image->rb_offset;
				chn_frame_info.frame_size = gpu_image->frame_size;
				chn_frame_info.t0 = what_time_is_it_now();
				pcie_fpga2rga_image_to_queue(chn, &chn_frame_info);
				gpu_image->rb_offset += 1;
				if(gpu_image->rb_offset >= (chn_image->data_frame_max + 1)){
					gpu_image->rb_offset = 0;
				}
			}
		} else {
			SLOGI("[Debug] chn:%d ringbuffer is NULL \n", chn);
		}
		usleep(40000);
	}
err1:
	fclose(fp_datasrc);
	return NULL;
}
#else
static void *pcie_data_analysis_thread(void* parameter)
{
	int ret;
	struct timeval time;
	uint8_t *pcie_buf;
	unsigned int chn, frame_index;
	image_frame_info_t frame_info;
	uint8_t *chn_buf_l;
	ring_buffer_t *rb = g_fpga_pcie->pcie_data_rb;
	unsigned int pcie_data_head = 0x499602d2;

	cpu_set_t mask;
	CPU_ZERO(&mask);
	// 限大核调度
	CPU_SET(4, &mask), CPU_SET(5, &mask);
	CPU_SET(6, &mask), CPU_SET(7, &mask);
	pthread_setaffinity_np(pthread_self( ), sizeof(mask), &mask);

	while(1){
		while(ringbuffer_is_empty(rb)) {
			time.tv_sec=0;
			time.tv_usec=1;
			select(0,NULL,NULL,NULL,&time);
		}
		ringbuffer_get(rb, (uint8_t *)&frame_info, sizeof(image_frame_info_t));

		if(ringbuffer_is_full(rb)){
			SLOGI("[%s] ringbuffer full \n", __func__);
		}

		pcie_buf = g_fpga_pcie->pcie_data + frame_info.frame_offset;

		/*对每行数据进行解析*/
		for(frame_index = 0; frame_index < FPGA_READ_DATA_ROW_MAX; frame_index++){
			chn_buf_l = (pcie_buf + ((frame_index) * FPGA_ROW_DATA_SIZE));
            // save_image_data(chn_buf_l, FPGA_ROW_DATA_SIZE , "/tmp/chn_buf_l.raw");

			if(PCIE_GET_DATA_HEAD(chn_buf_l) != pcie_data_head){ /*帧头错误，则跳过本帧解析*/
				SLOGI("pcie index (0x%x != 0x%x )error \n", PCIE_GET_DATA_HEAD(chn_buf_l), pcie_data_head);
				continue;
			}

			chn = chn_buf_l[PCIE_HEAD_CHN_OFFSET]; /* DATA_CHANNEL_OFFSET:通道偏移 */
			if(chn >= MPP_ENCODE_NTHREAD_MAX){/* 帧序号超过最大的帧序号，则跳出本帧解析 */
				SLOGI("get pcie data chn(%d - %d) error \n", chn, chn_buf_l[16]);
				continue;
			}

			pcie_chn_data_analysis(chn, chn_buf_l);
		}
	}
	return NULL;
}
#endif

int pcie_mpp_data_send(unsigned char* mpp_data, unsigned int size)
{
    int ret = 0;
    ret = dma_memory_mapped_write(mpp_data, PCIE_WRITE_IMAGE_DATA_ADDRESS,size, PCIE_H2C_0);
    if(ret != size){
        SLOGI("rk2fpga size:(%d-%d) data error \n", ret, size);
    }

    return ret;
}


/*帧头填充*/
void mpp_data_assemble_frame(uint8_t* mpp_data, unsigned char src_chn, unsigned char chn, unsigned int frame_index)
{
    unsigned int framer_head = 0xfa3ce6d5;
    mpi_encode_info_t* mpi_encode = mpi_encode_info_get();

    mpp_data[0] = (framer_head >> 24) & 0xff;
    mpp_data[1] = (framer_head >> 16) & 0xff;
    mpp_data[2] = (framer_head >> 8) & 0xff;
    mpp_data[3] = framer_head & 0xff;

    mpp_data[4] = (MPP_OUTPUT_FRAME_SIZE >> 24) & 0xff;
    mpp_data[5] = (MPP_OUTPUT_FRAME_SIZE >> 16) & 0xff;
    mpp_data[6] = (MPP_OUTPUT_FRAME_SIZE >> 8) & 0xff;
    mpp_data[7] = MPP_OUTPUT_FRAME_SIZE & 0xff;

    mpp_data[8] = (frame_index >> 24) & 0xff;
    mpp_data[9] = (frame_index >> 16) & 0xff;
    mpp_data[10] = (frame_index >> 8) & 0xff;
    mpp_data[11] = frame_index & 0xff;

    mpp_data[16] = src_chn;
    mpp_data[17] = chn;
    
}
/*有效数据填充*/
void mpp_data_frame_effective_length(uint8_t* mpp_data, unsigned int length)
{
    mpp_data[60] = (length >> 24) & 0xff;
    mpp_data[61] = (length >> 16) & 0xff;
    mpp_data[62] = (length >> 8) & 0xff;
    mpp_data[63] = length & 0xff;
}


int pcie_send_mpp_data(int chn, image_frame_info_t *frame_info)
{
    int ret;
    mpi_image_encode_info_t *image_encode;
    mpi_encode_info_t* mpi_encode = mpi_encode_info_get();
    uint8_t *buffer = align_write_mpp_buf_get(chn);
    image_encode = &mpi_encode->image_encode[chn];

    memset(buffer, 0x0, (MPP_OUTPUT_FRAME_SIZE + MPP_OUTPUT_HEAD_SIZE));
    
    mpp_data_assemble_frame(buffer, frame_info->chn, chn, frame_info->frame_index);
    memcpy(buffer + MPP_OUTPUT_HEAD_SIZE, image_encode->encode_data + frame_info->frame_offset, frame_info->frame_size);
    mpp_data_frame_effective_length(buffer, frame_info->frame_size);

    ret = pcie_mpp_data_send(buffer, (MPP_OUTPUT_FRAME_SIZE + MPP_OUTPUT_HEAD_SIZE));
    if(ret != (MPP_OUTPUT_FRAME_SIZE + MPP_OUTPUT_HEAD_SIZE)){
        return 1;
    }

    // SLOGI("rk2fpga chn:%d f_chn:%d index:%d offset:0x%x size:%d data ok \n", chn, frame_info->chn, frame_info->frame_index,frame_info->frame_offset, frame_info->frame_size);

    return 0;
}

/*
	函数: static void *pcie_mpp_send_thread(void* parameter)
	作用: 编码数据发送线程
	参数:
		线程参数
	返回值:
		线程返回值
*/
static void *pcie_mpp_send_thread(void* parameter)
{
    int ret;
    ring_buffer_t *rb;
    image_frame_info_t frame_info;

    mpi_image_encode_info_t *image_encode;
    mpi_encode_info_t* mpi_encode = mpi_encode_info_get();

    while(1){
        /*遍历编码通道，有数据则通过PCIE发送出去*/
        for(int chn = 0; chn < MPP_ENCODE_NTHREAD_MAX; chn++){
            rb =mpi_encode->mpp_encode_rb[chn];
            image_encode = &mpi_encode->image_encode[chn];
            if(!ringbuffer_is_empty(rb)) {
                if(ringbuffer_is_full(rb)){
                    SLOGI("[%s] chn:%d ringbuffer full \n", __func__, chn);
                }
                ringbuffer_get(rb, (uint8_t *)&frame_info, sizeof(image_frame_info_t));
                
                ret = pcie_send_mpp_data(chn, &frame_info);
                if(ret != 0){
                    SLOGI("rk2fpga chn:%d offset:0x%x size:(%d) data error \n", chn, frame_info.frame_offset, frame_info.frame_size);
                }
            }
        }
        //usleep(100);
        xt_nanosleep(0, (100));
    }

    return NULL;
}

int pcie_read_data(uint32_t frame_size,FILE *fp){
    int ret;
    struct timeval time;
    unsigned int pcie_buf_offset = 0;
    unsigned int pcie_read_frame = 0;
    unsigned int fpga_vfifo_size = 0;

    uint8_t *pcie_buf = g_fpga_pcie->pcie_data;
    image_frame_info_t frame_info;

    // fpga_vfifo_size = pcie_get_fpga2rk_vfifo_szie();

    ret = dma_memory_mapped_read(pcie_buf,PCIE_READ_IMAGE_DATA_ADDRESS, frame_size,PCIE_C2H_0);
    if(ret == frame_size){
        ret = fwrite(pcie_buf,1,frame_size,fp);
        if(ret != frame_size){
            perror("fwrite");
            return -1;
        }
        ret = 0;
    } else {
        ret = -1;
        SLOGI("[ERROR] PCIE READ DATA ERROR \n");
    }
    return ret;
}

int pcie_dec_send(uint8_t *buf,uint64_t frame_size,int chn)
{
#if 1
    int ret;
    ret = dma_memory_mapped_write(buf, PCIE_WRITE_IMAGE_DATA_ADDRESS * (chn + 1), frame_size, PCIE_H2C_0);
    if(ret != frame_size){
        ret = -1;
        printf("pcie send size error\n");
        // SLOGI("rk2fpga size:(%d-%d) data error \n",  ret, frame_size);
    }

    // usleep(100);

#endif
    return 0;
}

int fpga_pcie_device_open(void)
{
    int ret;

    ret = pcie_bar_open(1);
    if(ret != 0){
        printf("[ERROR] pcie bar0 mmap error \n");
        return A818_PCIE_BAR_OPEN;
    }

    ret = pcie_impl_init();
    if(ret != 0){
        printf("[ERROR] pcie impl init error \n");
        ret = A818_PCIE_IMPL_INIT;
        goto err_1;
    }

    // pcie_reg = pcie_bar_mmap(FPGA_PCIE_REG_PHY, FPGA_PCIE_REG_LEN , 0);
    // if(pcie_reg == NULL){
    //     printf("[ERROR] pcie reg bar mmap error \n");
    //     ret = A818_PCIE_REG_MMAP;
    //     goto err_2;
    // }

    pcie_common_param_reg = pcie_bar_mmap(FPGA_PCIE_COMMON_PARM_REG_PHY, FPGA_PCIE_REG_LEN , 1);
    if(pcie_common_param_reg == NULL){
        printf("[ERROR] pcie reg bar mmap error \n");
        ret = A818_PCIE_REG_MMAP;
        goto err_2;
    }

    // ret = pcie_bar_open(2);
    // if(ret != 0){
    //     printf("[ERROR] pcie bar2 mmap error \n");
    //     return A818_PCIE_BAR_OPEN;
    // }
    // pcie_stack_char_reg =  pcie_bar_mmap(FPGA_PCIE_STACK_CHAR_REG_PHY, FPGA_PCIE_STACK_CHAR_REG_LEN , 2);
    // if(pcie_stack_char_reg == NULL){
    //     printf("[ERROR] pcie reg bar mmap error \n");
    //     ret = A818_PCIE_REG_MMAP;
    //     goto err_2;
    // }

    // pcie_stack_image_reg =  pcie_bar_mmap(FPGA_PCIE_STACK_IMAGE_REG_PHY, FPGA_PCIE_STACK_IMAGE_REG_LEN, 2);
    // if(pcie_stack_image_reg == NULL){
    //     printf("[ERROR] pcie reg bar mmap error \n");
    //     ret = A818_PCIE_REG_MMAP;
    //     goto err_2;
    // }

    // pcie_vdma_config_reg =  pcie_bar_mmap(FPGA_PCIE_VDMA_CONFIG_REG_PHY, FPGA_PCIE_VDMA_CONFIG_REG_LEN , 2);
    // if(pcie_vdma_config_reg == NULL){
    //     printf("[ERROR] pcie reg bar mmap error \n");
    //     ret = A818_PCIE_REG_MMAP;
    //     goto err_2;
    // }

    // fiber_rate_config_reg =  pcie_bar_mmap(FIBER_RATE_CONFIG_REG_PHY, FIBER_RATE_CONFIG_REG_LEN , 2);
    // if(fiber_rate_config_reg == NULL){
    //     printf("[ERROR] pcie reg bar mmap error \n");
    //     ret = A818_PCIE_REG_MMAP;
    //     goto err_2;
    // }

    // set_pcie_timestamp_addr(pcie_reg);

    return A818_SUCCESS;

err_2:
    pcie_impl_destory();
err_1:
    pcie_bar_close();
    return ret;
}

void fpga_pcie_device_close(void)
{
    pcie_bar_munmap(pcie_reg, FPGA_PCIE_REG_LEN);
    pcie_impl_destory();
    pcie_bar_close();
}

int pcie_fpga_data_init(void)
{
	int ret;
	pthread_t read_tid, analysis_tid, send_tid;

	g_fpga_pcie = (pcie_fpga_info_t*)malloc(sizeof(pcie_fpga_info_t));
	if(NULL == g_fpga_pcie){
		printf("[ERROR] fpga pcie malloc error \n");
		return A818_PCIE_DATA_MALLOC;
	}
    //初始化cfg_info
    for(int index=0; index < MPP_DECODE_NTHREAD_MAX; index++){
        cfg_info[index] = (cfg_info_t *)malloc(sizeof(cfg_info_t));
        memset(cfg_info[index], 0, sizeof(cfg_info_t));
    }

	g_fpga_pcie->pcie_data = align_read_buf_get();
	g_fpga_pcie->pcie_data_rb = ringbuffer_create(PCIE_DATA_STREAM_FRAME_MAX*sizeof(image_frame_info_t), sizeof(image_frame_info_t));
	if (!g_fpga_pcie->pcie_data_rb) {
		printf("[ERROR] create pcie data rb buffer failed\n");
		ret = A818_PCIE_DATA_RB;
		goto err_1;
	}

	g_fpga_pcie->pcie_reg = pcie_common_param_reg;

	// g_fpga_pcie->pcie_pc_reg = pcie_bar_mmap(FPGA_PCIE_PC_REG_PHY, FPGA_PCIE_REG_LEN,1);
	// if(g_fpga_pcie->pcie_pc_reg == NULL){
	// 	printf("[ERROR] pcie reg bar mmap error \n");
	// 	ret = A818_PCIE_PC_REG_MMAP;
	// 	goto err_2;
	// }

	/* 初始化相关线程 */
	ret = pthread_create(&read_tid,NULL,&pcie_read_data_thread,NULL);
	if (ret != 0) {
		printf("[ERROR] Create pcie read data thread failed %d\n",__func__,ret);
		ret = A818_PCIE_READ_THREAD;
		goto err_3;
	}

	ret = pthread_create(&analysis_tid,NULL,&pcie_data_analysis_thread,NULL);
	if (ret != 0) {
		printf("[ERROR] Create pcie data analysis thread failed %d\n",__func__,ret);
		ret = A818_PCIE_ANALYSIS_THREAD;
		goto err_4;
	}

	ret = pthread_create(&send_tid,NULL,&pcie_mpp_send_thread,NULL);
	if (ret != 0) {
		printf("[ERROR] Create pcie data send thread failed %d\n",__func__,ret);
		ret = A818_PCIE_SEND_THREAD;
		goto err_5;
	}

	// pcie_fpga_reg_reset();
    SLOGI("[%s] pcie init ok \n", __func__);
    usleep(200*1000);

    return A818_SUCCESS;
err_5:
	pthread_cancel(analysis_tid);
err_4:
    pthread_cancel(read_tid);
err_3:
    // pcie_bar_munmap(g_fpga_pcie->pcie_pc_reg, FPGA_PCIE_REG_LEN);  
err_2:
    ringbuffer_destory(g_fpga_pcie->pcie_data_rb);
err_1:
    free(g_fpga_pcie);
    return ret;
}


void pcie_fpga_data_destory(void)
{
    if(g_fpga_pcie){
        pcie_set_rk_disable();
        // pcie_set_pc_enable(0);
        ringbuffer_destory(g_fpga_pcie->pcie_data_rb);
        free(g_fpga_pcie);
        g_fpga_pcie = NULL;
    }
}

