#ifndef _PCIE_REG_RW_H
#define _PCIE_REG_RW_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    unsigned int phy_address;
    unsigned int size;
    void* user_addr;
} pcie_bar_mem_info_t;

typedef struct {
    unsigned int time_h;
    unsigned int time_l;
} pcie_timestamp_t;

unsigned char *GetBarDevice();

void SetBarDevice(int num);
int pcie_bar_open(int bar_x);

void* pcie_bar_mmap(unsigned int phy_address, unsigned int size,int bar_num);

unsigned int pcie_bar_read_reg(void* addr, unsigned int offset);
void pcie_bar_write_reg(void* addr, unsigned int offset, unsigned int value);

void pcie_write_buf(void* addr, unsigned int offset, unsigned char * buf,unsigned int len);
void pcie_read_buf(void* addr, unsigned int offset, unsigned char * buf,unsigned int len);

void pcie_bar_munmap(void* user_addr, unsigned int size);
void pcie_bar_close(void);


void set_pcie_timestamp_addr(void* addr);
void get_pcie_timestamp(pcie_timestamp_t* timestamp);
unsigned long pcie_diff_timestamp(pcie_timestamp_t *end_tm, pcie_timestamp_t *start_tm);

int test_main(int argc,char **argv);
#ifdef __cplusplus
}
#endif

#endif