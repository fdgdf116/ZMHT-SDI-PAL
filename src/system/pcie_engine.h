#ifndef PCIE_ENGINE_H_
#define PCIE_ENGINE_H_

#include "types_def.h"
#include "ringbuffer.h"
#include "vivado_common.h"

#ifdef __cplusplus
extern "C" {
#endif

uint8_t * GetPcieData();
void ChnSet(int num);
void pcie_fpga_reg_reset(void);
cfg_info_t* GetCfgInfo(int chan0_sel_mode );

void pcie_chn_stack_configuration_reg_init(stack_configuration_info_t* reg_val_info);
int decode_configuration_init(cfg_info_t* reg_val_info);
void pcie_chn_stack_char(unsigned char* reg_val_info, unsigned int frame_size);
int pcie_chn_stack_image(unsigned char* reg_val_info, unsigned int frame_size);
void pcie_VdmaConfigInit(vdma_configuration_info_t *vdma_configuration_info);
void vfifo_reg_reset(int chn);
int pcie_set_rk_enable();
// void pcie_set_pc_enable(unsigned char val);

int fpga_pcie_device_open(void);
void fpga_pcie_device_close(void);

int pcie_fpga_data_init(void);
void pcie_fpga_data_destory(void);

void pcie_close_chn_enable(unsigned int chn);
void pcie_fpga_device_stop(void);

// void pcie_get_selftest_val(selftest_val_info_t* info);

int pcie_dec_send(uint8_t *buf,uint64_t frame_size,int chn);
int pcie_read_data(uint32_t frame_size,FILE *fp);

int pcie_chn_stack_char_test_w();
int pcie_chn_stack_char_test_r();

int pcie_chn_stack_image_test_w();
int pcie_chn_stack_image_test_r();
#ifdef __cplusplus
}
#endif
#endif /* GPU_GPU_INTERFACE_H_ */
