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
#include "net_engine.h"
#include "osd_engine.h"
#include "rga_engine.h"
#include "my_tree.h"
#include "log.h"

int rga_test_main();
static void sig_handler(int sig)
{
    for(int chn = 0; chn < DATA_CHANNEL_NUM_MAX; chn++){
        DecodeStop(chn);
        rga_pthread_chn_destory(chn);
        data_stream_chn_data_destory(chn);
        gui_osd_overlapped_chn_destory(chn);
        gpu_osd_chn_image_destory(chn);
        writePL_chn_destory(chn);
    }
    MpiDecDestroy();
    data_stream_destory();
    gui_osd_destory();
    net_close();
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

    rga_test_main();

    while(1){
        sleep(1);
    }
    return 0;
}
#endif