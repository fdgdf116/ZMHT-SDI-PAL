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

#define ringbuffer_destory ringbuffer_deinit
#define PCIE_READ_IMAGE_DATA_ADDRESS (0x00000000)
#define PCIE_WRITE_IMAGE_DATA_ADDRESS (0x100000000)


#define FPGA2RK_VFIFO_SIZE_OFFSET (0x100)
#define RK2FPGA_VFIFO_SIZE_OFFSET (0x104)

#define FPGA_READ_DATA_ROW_MAX (200)
// #define FPGA_READ_ONE_DATA_MAX (FPGA_ROW_DATA_SIZE * FPGA_READ_DATA_ROW_MAX)
#define FPGA_READ_ONE_DATA_MAX (1*1024*1024)

#define FPGA_PCIE_RESET_OFFSET (0x3c)
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

extern char ssd_baseaddr[];
typedef struct {
	uint8_t *pcie_data;
    ring_buffer_t *pcie_data_rb;
    void* pcie_reg;
    void* pcie_pc_reg;
} pcie_fpga_info_t;

static pcie_fpga_info_t *g_fpga_pcie;
static void* pcie_reg;
void* pcie_common_param_reg;
static void* pcie_stack_char_reg;
static void* pcie_stack_image_reg;
void* pcie_vdma_config_reg;
void* fiber_rate_config_reg;
int chn_num = 1;

static cfg_info_t* cfg_info[DATA_CHANNEL_NUM_MAX] = {NULL};
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
    unsigned int reg_offset;

    pcie_bar_write_reg(g_fpga_pcie->pcie_reg, FPGA_PCIE_ENABLE_OFFSET, 0);
    pcie_bar_write_reg(g_fpga_pcie->pcie_pc_reg, FPGA_PCIE_ENABLE_OFFSET, 0);
    usleep(100*1000);
    for(int chn = 0; chn < 24; chn++) {
        reg_offset = ((chn + 1) * FPGA_PCIE_CHN_OFFSET);
        pcie_bar_write_reg(g_fpga_pcie->pcie_reg, (reg_offset + FPGA_SET_CHN_ENABLE_OFFSET), 0);
        pcie_bar_write_reg(g_fpga_pcie->pcie_pc_reg, (reg_offset + FPGA_SET_CHN_ENABLE_OFFSET), 0);
        usleep(1000);
    }

    pcie_bar_write_reg(g_fpga_pcie->pcie_reg, FPGA_PCIE_RESET_OFFSET, 0);
    usleep(500*1000);
    pcie_bar_write_reg(g_fpga_pcie->pcie_reg, FPGA_PCIE_RESET_OFFSET, 1);
    usleep(500*1000);

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
    unsigned int reg_offset;
    unsigned char chn;

    chn = reg_val_info->chn;
    // chn -= PCIE_SRC_START_CHANNEL;
    reg_offset = (chn  * 0x4);

    printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY + (0x640+reg_offset),1);
    pcie_bar_write_reg(pcie_common_param_reg, (0x640+reg_offset),1);
    usleep(1000);

    pcie_bar_write_reg(pcie_common_param_reg, (reg_offset + 0x2c), reg_val_info->draw_mode0);
    printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY + (reg_offset + 0x2c),reg_val_info->draw_mode0);
    usleep(1000);
    pcie_bar_write_reg(pcie_common_param_reg, (reg_offset + 0x5c), reg_val_info->center_point_x0);
    printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY + (reg_offset + 0x5c),reg_val_info->center_point_x0);
    usleep(1000);
    pcie_bar_write_reg(pcie_common_param_reg, (reg_offset + 0x8c), reg_val_info->center_point_y0);
    printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY + (reg_offset + 0x8c),reg_val_info->character_point_y0);
    usleep(1000);
    pcie_bar_write_reg(pcie_common_param_reg, (reg_offset + 0xbc), reg_val_info->center_point_r0);
    printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY + (reg_offset + 0xbc),reg_val_info->center_point_r0);
    usleep(1000);
    pcie_bar_write_reg(pcie_common_param_reg, (reg_offset + 0xec), reg_val_info->center_point_s0);
    printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY + (reg_offset + 0xec),reg_val_info->center_point_s0);
    usleep(1000);
    pcie_bar_write_reg(pcie_common_param_reg, (reg_offset + 0x11c), reg_val_info->center_point_l0);
    printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY + (reg_offset + 0x11c),reg_val_info->center_point_l0);
    usleep(1000);
    pcie_bar_write_reg(pcie_common_param_reg, (reg_offset + 0x14c), reg_val_info->center_point_w0);
    printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY + (reg_offset + 0x14c),reg_val_info->center_point_w0);
    usleep(1000);
    pcie_bar_write_reg(pcie_common_param_reg, (reg_offset + 0x17c), reg_val_info->image_length0);
    printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY + (reg_offset + 0x17c),reg_val_info->image_length0);
    usleep(1000);
    pcie_bar_write_reg(pcie_common_param_reg, (reg_offset + 0x1ac), reg_val_info->image_width0);
    printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY + (reg_offset + 0x1ac),reg_val_info->image_width0);
    usleep(1000);
    pcie_bar_write_reg(pcie_common_param_reg, (reg_offset + 0x1dc), reg_val_info->chan0_sel_mode);
    printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY + (reg_offset + 0x1dc),reg_val_info->chan0_sel_mode);
    usleep(1000);
    pcie_bar_write_reg(pcie_common_param_reg, (reg_offset + 0x20c), reg_val_info->image_fill_length0);
    printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY+(reg_offset + 0x20c),reg_val_info->image_fill_length0);
    usleep(1000);
    pcie_bar_write_reg(pcie_common_param_reg, (reg_offset + 0x23c), reg_val_info->image_fill_width0);
    printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY+(reg_offset + 0x23c),reg_val_info->image_fill_width0);
    usleep(1000);
    pcie_bar_write_reg(pcie_common_param_reg, (reg_offset + 0x26c), reg_val_info->image_cut_length0);
    printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY+(reg_offset + 0x26c),reg_val_info->image_cut_length0);
    usleep(1000);
    pcie_bar_write_reg(pcie_common_param_reg, (reg_offset + 0x29c), reg_val_info->image_out_format);
    printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY+(reg_offset + 0x29c),reg_val_info->image_out_format);
    usleep(1000);
    pcie_bar_write_reg(pcie_common_param_reg, (reg_offset + 0x2cc), reg_val_info->draw_line_width0);
    printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY+(reg_offset + 0x2cc),reg_val_info->draw_line_width0);
    usleep(1000);
    pcie_bar_write_reg(pcie_common_param_reg, (reg_offset + 0x2fc), reg_val_info->character_point_x0);
    printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY+(reg_offset + 0x2fc),reg_val_info->character_point_x0);
    usleep(1000);
    pcie_bar_write_reg(pcie_common_param_reg, (reg_offset + 0x32c), reg_val_info->character_point_y0);
    printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY+(reg_offset + 0x32c),reg_val_info->character_point_y0);
    usleep(1000);
    pcie_bar_write_reg(pcie_common_param_reg, (reg_offset + 0x35c), reg_val_info->image_point_x0);
    printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY+(reg_offset + 0x35c),reg_val_info->image_point_x0);
    usleep(1000);
    pcie_bar_write_reg(pcie_common_param_reg, (reg_offset + 0x38c), reg_val_info->image_point_y0);
    printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY+(reg_offset + 0x38c),reg_val_info->image_point_y0);
    usleep(1000);
    pcie_bar_write_reg(pcie_common_param_reg, (reg_offset + 0x3bc), reg_val_info->yuv422_crcb_sel0);
    printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY+(reg_offset + 0x3bc),reg_val_info->yuv422_crcb_sel0);
    usleep(1000);
    pcie_bar_write_reg(pcie_common_param_reg, (reg_offset + 0x3ec), reg_val_info->chan0_video_mode);
    printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY+(reg_offset + 0x3ec),reg_val_info->chan0_video_mode);
    usleep(1000);
    pcie_bar_write_reg(pcie_common_param_reg, (reg_offset + 0x41c), reg_val_info->image_length1);
    printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY+(reg_offset + 0x41c),reg_val_info->image_length1);
    usleep(1000);
    pcie_bar_write_reg(pcie_common_param_reg, (reg_offset + 0x44c), reg_val_info->image_width1);
    printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY+(reg_offset + 0x44c),reg_val_info->image_width1);
    usleep(1000);

    for(int index = 0;index < DATA_CHANNEL_NUM_MAX;index++)
    {
        if((reg_val_info->chan0_sel_mode >> index) & 0x01)
        {
            reg_offset = index*0x04;
            pcie_bar_write_reg(pcie_common_param_reg, (0x18+reg_offset),1);
            printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY + (0x18+reg_offset),1);
            usleep(1000);
            pcie_bar_write_reg(pcie_common_param_reg, (0x18+reg_offset),0);
            printf("offset:0x%x value:0x%x\n",FPGA_PCIE_COMMON_PARM_REG_PHY + (0x18+reg_offset),0);
            usleep(1000);
        }
    }
}

int decode_configuration_init(cfg_info_t* reg_val_info)
{
    int ret = 0;
    unsigned int reg_offset;
    // unsigned char chn;
    cfg_info_t* cfg_info = GetCfgInfo(reg_val_info->channel_param.chan0_sel_mode);
    memcpy(cfg_info, reg_val_info, sizeof(cfg_info_t));
    sprintf(cfg_info->in_filename_chn, "%s%s", ssd_baseaddr, reg_val_info->in_filename_chn);
    sprintf(cfg_info->out_filename_chn, "%s%s", ssd_baseaddr, reg_val_info->out_filename_chn);
    channel_param_t channel_param = cfg_info->channel_param;
    data_stream_t* data_stream = data_stream_get();
    image_chn_info_t* image_chn_info = data_stream->image_chn_info[reg_val_info->channel_param.chan0_sel_mode];
    mpp_info_t* mpp_info = &image_chn_info->mpp_info;
    MpiDecLoopData* loop_data = &mpp_info->dec_data;
    rga_param_t* rga_info = &image_chn_info->rga_info;
    data_stream_t* gs_data_stream = data_stream_get();

    if(!mpp_info->decode_state)
    {
        printf("decoding!!!\n");
        return -1;
    }
    mpp_info->mpp_chn = reg_val_info->channel_param.chan0_sel_mode;
    parse_stream_info(cfg_info->in_filename_chn,&mpp_info->decode_type,&mpp_info->fps,&mpp_info->width, &mpp_info->height);
    mpp_info->dec_data.fps = mpp_info->fps;
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
    
    rga_info->out_color = channel_param.out_color;
    rga_info->image_w0 = channel_param.image_w0;
    rga_info->image_y0 = channel_param.image_y0;
    rga_info->rotate_type = cfg_info->channel_param.rotate;
    rga_info->resize = cfg_info->channel_param.resize;
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
        rga_info->resize_width  = cfg_info->channel_param.image_length0;
        rga_info->resize_height = cfg_info->channel_param.image_width0;
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
    rga_info->out_width = rga_info->resize_width;
    rga_info->out_height = rga_info->resize_height;
    if((rga_info->out_width != rga_info->osd_width) || (rga_info->out_height != rga_info->osd_height))
    {
        rga_info->crop_flag = 1; //裁剪标志
    }
    switch (rga_info->out_color)
    {
    case 1: // RGB_888
      rga_info->pixel_byte = 3;
      break;
    case 2: // YUYV
      rga_info->pixel_byte = 2;
      break;
    default:
      break;
    }
    SLOGI("out_color:%d rotate_width:%d rotate_height:%d resize_width:%d resize_height:%d image_w0:%d image_y0:%d osd_width:%d osd_height:%d out_width:%d out_height:%d totate_type:%d resize:%d crop:%d pixel_byte:%d\n",
          rga_info->out_color, rga_info->rotate_width, rga_info->rotate_height,
          rga_info->resize_width, rga_info->resize_height, rga_info->image_w0, rga_info->image_y0,
          rga_info->osd_width, rga_info->osd_height, rga_info->out_width, rga_info->out_height,
          rga_info->rotate_type, rga_info->resize, rga_info->crop_flag, rga_info->pixel_byte);

    ret = writePL_chn_init(mpp_info->mpp_chn);
    if(ret != 0) 
    {
        SLOGI("writePL_chn_init error \n");
        goto err_1;
    }
    ret = gpu_osd_chn_image_init(mpp_info->mpp_chn);
	if (ret != 0) {
		SLOGI("gpu_osd_chn_image_init error \n");
        goto err_2;
	}
    ret = rga_chn_image_init(mpp_info->mpp_chn);
    if (ret != 0) {
        SLOGI("rga_chn_image_init error \n");
        goto err_3;
    }
    return A818_SUCCESS;
err_3:
    rga_pthread_chn_destory(mpp_info->mpp_chn);
    data_stream_chn_data_destory(mpp_info->mpp_chn);
err_2:
    gui_osd_overlapped_chn_destory(mpp_info->mpp_chn);
    gpu_osd_chn_image_destory(mpp_info->mpp_chn);
err_1:
    writePL_chn_destory(mpp_info->mpp_chn);
    return ret;    
}

void pcie_VdmaConfigInit(vdma_configuration_info_t *vdma_configuration_info){
    unsigned int reg_offset;
    unsigned char chn;

    chn = vdma_configuration_info->chn;
    // chn -= PCIE_SRC_START_CHANNEL;
    reg_offset = (chn  * 0x10000);
    // SLOGI("[Debug] %s reg offset:0x%x src_width:%d src_height:%d \n", __func__, reg_offset, reg_val_info->src_width, reg_val_info->src_height);

    pcie_bar_write_reg(pcie_vdma_config_reg, (reg_offset + 0x00), vdma_configuration_info->VDMA_MM2S_DMACR);
    printf("offset:%x write:%x\n",FPGA_PCIE_VDMA_CONFIG_REG_PHY+(reg_offset + 0x00),vdma_configuration_info->VDMA_MM2S_DMACR);
    usleep(1000);
    pcie_bar_write_reg(pcie_vdma_config_reg, (reg_offset + 0x04), vdma_configuration_info->VDMA_MM2S_DMASR);
    printf("offset:%x write:%x\n",FPGA_PCIE_VDMA_CONFIG_REG_PHY+(reg_offset + 0x04),vdma_configuration_info->VDMA_MM2S_DMASR);
    usleep(1000);
    pcie_bar_write_reg(pcie_vdma_config_reg, (reg_offset + 0x54), vdma_configuration_info->VDMA_MM2S_HSIZE);
    printf("offset:%x write:%x\n",FPGA_PCIE_VDMA_CONFIG_REG_PHY+(reg_offset + 0x54),vdma_configuration_info->VDMA_MM2S_HSIZE);
    usleep(1000);
    pcie_bar_write_reg(pcie_vdma_config_reg, (reg_offset + 0x58), vdma_configuration_info->VDMA_MM2S_DLYSTRIDE);
    printf("offset:%x write:%x\n",FPGA_PCIE_VDMA_CONFIG_REG_PHY+(reg_offset + 0x58),vdma_configuration_info->VDMA_MM2S_DLYSTRIDE);
    usleep(1000);
    pcie_bar_write_reg(pcie_vdma_config_reg, (reg_offset + 0x5c), vdma_configuration_info->VDMA_MM2S_START_ADDR1);
    printf("offset:%x write:%x\n",FPGA_PCIE_VDMA_CONFIG_REG_PHY+(reg_offset + 0x5c),vdma_configuration_info->VDMA_MM2S_START_ADDR1);
    usleep(1000);
    pcie_bar_write_reg(pcie_vdma_config_reg, (reg_offset + 0x60), vdma_configuration_info->VDMA_MM2S_START_ADDR2);
    printf("offset:%x write:%x\n",FPGA_PCIE_VDMA_CONFIG_REG_PHY+(reg_offset + 0x60),vdma_configuration_info->VDMA_MM2S_START_ADDR2);
    usleep(1000);
    pcie_bar_write_reg(pcie_vdma_config_reg, (reg_offset + 0x64), vdma_configuration_info->VDMA_MM2S_START_ADDR3);
    printf("offset:%x write:%x\n",FPGA_PCIE_VDMA_CONFIG_REG_PHY+(reg_offset + 0x64),vdma_configuration_info->VDMA_MM2S_START_ADDR3);
    usleep(1000);
    pcie_bar_write_reg(pcie_vdma_config_reg, (reg_offset + 0x30), vdma_configuration_info->VDMA_S2MM_DMACR);
    printf("offset:%x write:%x\n",FPGA_PCIE_VDMA_CONFIG_REG_PHY+(reg_offset + 0x30),vdma_configuration_info->VDMA_S2MM_DMACR);
    usleep(1000);
    pcie_bar_write_reg(pcie_vdma_config_reg, (reg_offset + 0x34), vdma_configuration_info->VDMA_S2MM_DMASR);
    printf("offset:%x write:%x\n",FPGA_PCIE_VDMA_CONFIG_REG_PHY+(reg_offset + 0x34),vdma_configuration_info->VDMA_S2MM_DMASR);
    usleep(1000);
    pcie_bar_write_reg(pcie_vdma_config_reg, (reg_offset + 0xa4), vdma_configuration_info->VDMA_S2MM_HSIZE);
    printf("offset:%x write:%x\n",FPGA_PCIE_VDMA_CONFIG_REG_PHY+(reg_offset + 0xa4),vdma_configuration_info->VDMA_S2MM_HSIZE);
    usleep(1000);
    pcie_bar_write_reg(pcie_vdma_config_reg, (reg_offset + 0xa8), vdma_configuration_info->VDMA_S2MM_DLYSTRIDE);
    printf("offset:%x write:%x\n",FPGA_PCIE_VDMA_CONFIG_REG_PHY+(reg_offset + 0xa8),vdma_configuration_info->VDMA_S2MM_DLYSTRIDE);
    usleep(1000);
    pcie_bar_write_reg(pcie_vdma_config_reg, (reg_offset + 0xac), vdma_configuration_info->VDMA_S2MM_START_ADDR1);
    printf("offset:%x write:%x\n",FPGA_PCIE_VDMA_CONFIG_REG_PHY+(reg_offset + 0xac),vdma_configuration_info->VDMA_S2MM_START_ADDR1);
    usleep(1000);
    pcie_bar_write_reg(pcie_vdma_config_reg, (reg_offset + 0xb0), vdma_configuration_info->VDMA_S2MM_START_ADDR2);
    printf("offset:%x write:%x\n",FPGA_PCIE_VDMA_CONFIG_REG_PHY+(reg_offset + 0xb0),vdma_configuration_info->VDMA_S2MM_START_ADDR2);
    usleep(1000);
    pcie_bar_write_reg(pcie_vdma_config_reg, (reg_offset + 0xb4), vdma_configuration_info->VDMA_S2MM_START_ADDR3);
    printf("offset:%x write:%x\n",FPGA_PCIE_VDMA_CONFIG_REG_PHY+(reg_offset + 0xb4),vdma_configuration_info->VDMA_S2MM_START_ADDR3);
    usleep(1000);
    pcie_bar_write_reg(pcie_vdma_config_reg, (reg_offset + 0x50), vdma_configuration_info->VDMA_MM2S_VSIZE);
    printf("offset:%x write:%x\n",FPGA_PCIE_VDMA_CONFIG_REG_PHY+(reg_offset + 0x50),vdma_configuration_info->VDMA_MM2S_VSIZE);
    usleep(1000);
    pcie_bar_write_reg(pcie_vdma_config_reg, (reg_offset + 0xa0), vdma_configuration_info->VDMA_S2MM_VSIZE);
    printf("offset:%x write:%x\n",FPGA_PCIE_VDMA_CONFIG_REG_PHY+(reg_offset + 0xa0),vdma_configuration_info->VDMA_S2MM_VSIZE);
    usleep(100000);
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
        // save_image_data(pcie_buf,ret,"/userdata/sda1/cvtcolor_char.raw");
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
        // save_image_data(gpu_image->over_image,ret,"/userdata/sda1/cvtcolor_image.raw");
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

void pcie_get_selftest_val(selftest_val_info_t* info)
{
   info->board_type = htonl(pcie_bar_read_reg(g_fpga_pcie->pcie_reg, 0x80));
   info->series_number = htonl(pcie_bar_read_reg(g_fpga_pcie->pcie_reg, 0x84));
   info->version1 = htonl(pcie_bar_read_reg(g_fpga_pcie->pcie_reg, 0x88));
   info->version2 = htonl(pcie_bar_read_reg(g_fpga_pcie->pcie_reg, 0x8c));
   info->link = htonl(pcie_bar_read_reg(g_fpga_pcie->pcie_reg, 0x90));
   info->pc_link = pcie_bar_read_reg(g_fpga_pcie->pcie_reg, 0x94);
   info->rk_link = pcie_bar_read_reg(g_fpga_pcie->pcie_reg, 0x98);

}

void pcie_set_rk_enable(unsigned char val)
{
    pcie_bar_write_reg(g_fpga_pcie->pcie_reg, FPGA_PCIE_ENABLE_OFFSET, val);
}

void pcie_set_pc_enable(unsigned char val)
{
    pcie_bar_write_reg(g_fpga_pcie->pcie_pc_reg, FPGA_PCIE_ENABLE_OFFSET, val);
}

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
    pcie_set_rk_enable(0);
    pcie_set_pc_enable(0);
    pcie_fpga_reg_reset();

    while(pcie_get_fpga2rk_vfifo_szie()){
        usleep(1000);
    }

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

    ret = pcie_bar_open(0);
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

    pcie_common_param_reg = pcie_bar_mmap(FPGA_PCIE_COMMON_PARM_REG_PHY, FPGA_PCIE_REG_LEN , 0);
    if(pcie_common_param_reg == NULL){
        printf("[ERROR] pcie reg bar mmap error \n");
        ret = A818_PCIE_REG_MMAP;
        goto err_2;
    }

    ret = pcie_bar_open(2);
    if(ret != 0){
        printf("[ERROR] pcie bar2 mmap error \n");
        return A818_PCIE_BAR_OPEN;
    }
    pcie_stack_char_reg =  pcie_bar_mmap(FPGA_PCIE_STACK_CHAR_REG_PHY, FPGA_PCIE_STACK_CHAR_REG_LEN , 2);
    if(pcie_stack_char_reg == NULL){
        printf("[ERROR] pcie reg bar mmap error \n");
        ret = A818_PCIE_REG_MMAP;
        goto err_2;
    }

    pcie_stack_image_reg =  pcie_bar_mmap(FPGA_PCIE_STACK_IMAGE_REG_PHY, FPGA_PCIE_STACK_IMAGE_REG_LEN, 2);
    if(pcie_stack_image_reg == NULL){
        printf("[ERROR] pcie reg bar mmap error \n");
        ret = A818_PCIE_REG_MMAP;
        goto err_2;
    }

    pcie_vdma_config_reg =  pcie_bar_mmap(FPGA_PCIE_VDMA_CONFIG_REG_PHY, FPGA_PCIE_VDMA_CONFIG_REG_LEN , 2);
    if(pcie_vdma_config_reg == NULL){
        printf("[ERROR] pcie reg bar mmap error \n");
        ret = A818_PCIE_REG_MMAP;
        goto err_2;
    }

    fiber_rate_config_reg =  pcie_bar_mmap(FIBER_RATE_CONFIG_REG_PHY, FIBER_RATE_CONFIG_REG_LEN , 2);
    if(fiber_rate_config_reg == NULL){
        printf("[ERROR] pcie reg bar mmap error \n");
        ret = A818_PCIE_REG_MMAP;
        goto err_2;
    }

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
    for(int index=0; index < DATA_CHANNEL_NUM_MAX; index++){
        cfg_info[index] = (cfg_info_t *)malloc(sizeof(cfg_info_t));
        memset(cfg_info[index], 0, sizeof(cfg_info_t));
        cfg_info[index]->channel_param.chan0_sel_mode = index;
    }

    g_fpga_pcie->pcie_data = align_read_buf_get();
    // g_fpga_pcie->pcie_data_rb = ringbuffer_create(PCIE_DATA_STREAM_FRAME_MAX*sizeof(image_frame_info_t));
    // if (!g_fpga_pcie->pcie_data_rb) {
    //     printf("[ERROR] create pcie data rb buffer failed\n");
    //     ret = A818_PCIE_DATA_RB;
    //     goto err_1;
    // }

    // g_fpga_pcie->pcie_reg = pcie_reg;

    // g_fpga_pcie->pcie_pc_reg = pcie_bar_mmap(FPGA_PCIE_PC_REG_PHY, FPGA_PCIE_REG_LEN);
    // if(g_fpga_pcie->pcie_pc_reg == NULL){
    //     printf("[ERROR] pcie reg bar mmap error \n");
    //     ret = A818_PCIE_PC_REG_MMAP;
    //     goto err_3;
    // }

    // /* 初始化相关线程 */
    // ret = pthread_create(&read_tid,NULL,&pcie_read_data_thread,NULL);;
    // if (ret != 0) {
    //     printf("[ERROR] Create pcie read data thread failed %d\n",__func__,ret);
    //     ret = A818_PCIE_READ_THREAD;
    //     goto err_4;
    // }

    // pcie_fpga_reg_reset();
    SLOGI("[%s] pcie init ok \n", __func__);
    usleep(200*1000);

    return A818_SUCCESS;

err_4:
    pthread_cancel(read_tid);
err_3:
    pcie_bar_munmap(g_fpga_pcie->pcie_pc_reg, FPGA_PCIE_REG_LEN);  
err_2:
    ringbuffer_destory(g_fpga_pcie->pcie_data_rb);
err_1:
    free(g_fpga_pcie);
    return ret;
}


void pcie_fpga_data_destory(void)
{
    if(g_fpga_pcie){
        pcie_set_rk_enable(0);
        pcie_set_pc_enable(0);
        ringbuffer_destory(g_fpga_pcie->pcie_data_rb);
        free(g_fpga_pcie);
        g_fpga_pcie = NULL;
    }
}

