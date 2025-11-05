#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <getopt.h> 
#include <fcntl.h> 
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <linux/videodev2.h>

#include "ringbuffer.h"
#include "mpi_decode.h"
#include "data_engine.h"
#include "fifo_engine.h"
#include "pcie_engine.h"
#include "rga_engine.h"
#include "osd_engine.h"
#include "rga_engine.h"
#include "my_tree.h"
#include "log.h"
#include "pcie_reg_rw.h"
#include "gpu_interface.h"
#include "crop_engine.h"
#include "mpi_encode.h"

int rga_test_main();
static void sig_handler(int sig)
{
    for(int chn = MPP_DECODE_NTHREAD_MAX; chn < DATA_CHANNEL_NUM_MAX; chn++){
        DecodeStop(chn);
    }
    for(int chn = 0; chn < MPP_ENCODE_NTHREAD_MAX; chn++){
        EncodeStop(chn);
    }
    crop_stream_destory();
    data_stream_destory();
    gui_osd_destory();
    exit(0);
}
#if 1
int main(int argc, char *argv[])
{
    int ret = 0; 
    printf("Test Mpp...\n");
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, sig_handler);
    signal(SIGKILL, sig_handler);
    signal(SIGTERM, sig_handler);

    ret = gpu_lib_init();
    if(ret != 0){
        printf("LIB GPU init error ret = %d \n", ret);
        return 1;
    } else {
        printf("LIB GPU init ok \n");
    }

#if 1
    ret = fpga_pcie_device_open(); /* pcie初始化 */
    if(ret != 0){
        printf("fpga pcie open error \n");
    }
    ret = pcie_fpga_data_init();
    if(ret != 0){
        printf("fpga pcie data init error \n");
    }
#endif
    ret = data_stream_init();
    if(ret != 0){
        printf("data stream init error ret = %d \n", ret);
    }

    ret = gui_osd_init();
    if(ret != 0){
        printf("a818 osd init error ret = %d \n", ret);
        goto err_4;
    } else {
        printf("a818 osd init ok \n");
    }

    ret = crop_stream_init();
    if(ret != 0){
        printf("crop stream init error ret = %d \n", ret);
    }

	ret = pcie_fpga_data_init();
	if(ret != 0){
		printf("pcie data init error ret = %d \n", ret);
		goto err_6;
	} else {
		printf("fpga pcie data init ok \n");
	}

    ret = pcie_bar_open(0);
    ret = axififo_data_init();/* fifo初始化 */
    if(ret != 0){
        printf("axififo init error \n");
    }
    printf("init finished!!!\n");

    while(1){
        sleep(1);
    }
err_6:
	mpp_encode_destory();
err_4:
    gui_osd_destory();
    return ret;
}
#endif