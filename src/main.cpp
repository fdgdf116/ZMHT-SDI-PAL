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
#include <inttypes.h>
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
#include "stream_lifecycle.h"
#include "a818_error.h"

int rga_test_main();
static volatile sig_atomic_t g_shutdown_requested;
static void sig_handler(int sig)
{
    (void)sig;
    g_shutdown_requested = 1;
}
#if 1
int main(int argc, char *argv[])
{
    int ret = 0; 
    uint64_t open_started = stream_monotonic_us();
    printf("Test Mpp...\n");
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, sig_handler);
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
        goto err_1;
    }
#endif
    ret = data_stream_init();
	if(ret != 0){
		printf("data stream init error ret = %d \n", ret);
		goto err_2;
	} else {
		printf("data stream init ok \n");
	}

    ret = gui_osd_init();
    if(ret != 0){
        printf("a818 osd init error ret = %d \n", ret);
        goto err_4;
    } else {
        printf("a818 osd init ok \n");
    }

    ret = mpp_encode_init();
	if(ret != 0){
		printf("encode frame init error ret = %d \n", ret);
		goto err_5;
	} else {
		printf("encode frame init ok \n");
	}

	ret = pcie_fpga_data_init();
	if(ret != 0){
		printf("pcie data init error ret = %d \n", ret);
		goto err_6;
	} else {
		printf("fpga pcie data init ok \n");
	}

    ret = crop_stream_init();
    if(ret != 0){
        printf("crop stream init error ret = %d \n", ret);
        goto err_7;
    } else {
        printf("crop stream init ok \n");
    }

    ret = axififo_data_init();/* fifo初始化 */
    if(ret != 0){
        printf("axififo init error \n");
        goto err_8;
    }
    printf("init finished!!!\n");
    {
        uint64_t open_finished = stream_monotonic_us();
        SLOGI("[PERF] board open ready: started=%" PRIu64
              " finished=%" PRIu64 " elapsed=%" PRIu64 " us\n",
              open_started, open_finished, open_finished - open_started);
    }

#if TEST
	pcie_fpga_reg_reset();
	run_test();
#endif

    while(!g_shutdown_requested){
        usleep(100*1000);
    }
    {
        uint64_t close_started = stream_monotonic_us();
        int capture_stopped = 1;
        int stop_ret;
        SLOGI("[PERF] board close begin\n");
        for (int chn = MPP_DECODE_NTHREAD_MAX; chn < DATA_CHANNEL_NUM_MAX; chn++) {
            stop_ret = DecodeStop(chn);
            if (stop_ret == A818_STREAM_STOP_TIMEOUT)
                capture_stopped = 0;
        }
        for (int chn = 0; chn < MPP_ENCODE_NTHREAD_MAX; chn++) {
            stop_ret = EncodeStop(chn);
            if (stop_ret != 0)
                capture_stopped = 0;
        }
        if (!capture_stopped) {
            SLOGI("[ERROR] board close aborted: capture workers did not stop; "
                  "resources retained and process will exit\n");
            return A818_STREAM_STOP_TIMEOUT;
        }
        axififo_data_destory();
        int pcie_stopped = pcie_fpga_data_destory() == 0;
        crop_stream_destory();
        mpp_encode_destory();
        gui_osd_destory();
        data_stream_destory();
        if (pcie_stopped)
            fpga_pcie_device_close();
        else
            SLOGI("[ERROR] PCIe workers still active; BAR left mapped\n");
        gpu_lib_destory();
        {
            uint64_t close_finished = stream_monotonic_us();
            SLOGI("[PERF] board close complete: started=%" PRIu64
                  " finished=%" PRIu64 " elapsed=%" PRIu64 " us\n",
                  close_started, close_finished, close_finished - close_started);
        }
        return 0;
    }
err_8:
    crop_stream_destory();
err_7:
	pcie_fpga_data_destory();
err_6:
	mpp_encode_destory();
err_5:
	gui_osd_destory();
err_4:
err_3:
	data_stream_destory();
err_2:
	fpga_pcie_device_close();
err_1:
	gpu_lib_destory();
    return ret;
}
#endif
