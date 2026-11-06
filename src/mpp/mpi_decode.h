#ifndef MPI_DECODE_H
#define MPI_DECODE_H

#include "rk_mpi.h"
#include "mpp_mem.h"
#include "mpp_env.h"
#include "mpp_time.h"
#include "mpp_common.h"
#include "vivado_common.h"
#include "pcie_impl.h"
#include "rga_interface.h"
#include "ringbuffer.h"

#define MPI_DEC_STREAM_SIZE         (SZ_4K)
#define H264_FILE_NAME   "001.h264"

// #define H264_FILE_NAME   "/userdata/sda1/001.h264"

#define OUTPUT_FILE_NAME "/userdata/sda1/output.yuv"
typedef struct 
{
    MppCtx          ctx;
    MppApi          *mpi;
    RK_U32          eos;
    char            *buf;
 
    MppBufferGroup  frm_grp;
    MppPacket       packet;
    size_t          packet_size;
    MppFrame        frame;

    int             chn;
    FILE            *fp_input;
    FILE            *fp_output;
    RK_S32          frame_count;
    RK_S32          fps;
    RK_S32          frame_num;
    size_t          max_usage;
} MpiDecLoopData;

int mpi_dec_chn_init(int chn);
void DumpYUYVtoFile(MppFrame frame,int chn);
static int decode_simple(MpiDecLoopData *data);

int MpiDecodeDestroy(int chn);
void SetOutputFilename(unsigned char *name);
void SetInputFilename(unsigned char *name);
void BindChnFilename(int chn,unsigned char *name,unsigned char *out_name);
#endif
