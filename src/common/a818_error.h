#ifndef _A818_ERROR_H
#define _A818_ERROR_H

#define A818_SUCCESS       (0)

#define A818_PCIE_BAR_OPEN          (1)
#define A818_PCIE_IMPL_INIT         (2) //pcie_impl_init
#define A818_PCIE_REG_MMAP          (3)
#define A818_PCIE_PC_REG_MMAP       (3)
#define A818_PCIE_DATA_MALLOC       (4)
#define A818_PCIE_DATA_RB           (5)
#define A818_PCIE_READ_THREAD       (6)
#define A818_PCIE_ANALYSIS_THREAD   (7)
#define A818_PCIE_SEND_THREAD       (8)

#define A818_GUI_FRAME_MALLOC           (9)
#define A818_GUI_DLOPEN                 (10)
#define A818_GUI_DLSYM                  (11)
#define A818_GUI_LIBRARY                (12)
#define A818_GUI_GET_HDL                (13)
#define A818_GUI_OSD_DATA_RB            (14)
#define A818_GUI_MPP_DATA_RB            (15)
#define A818_GUI_OSD_DATA_BUFFER        (16)
#define A818_GUI_OUT_RGB_DATA_BUFFER    (17)
#define A818_GUI_IN_YUV_DATA_BUFFER     (18)
#define A818_GUI_OUT_YUV_DATA_BUFFER    (19)
#define A818_GUI_MPP_YUV_DATA_BUFFER    (20)

#define A818_DATA_STREAM_MALLOC         (21)
#define A818_DATA_CHN_IMAGE_MALLOC      (22)
#define A818_DATA_RGA_DATA_RB           (23)
#define A818_DATA_STITCHBASE_DATA_RB    (24)
#define A818_DATA_INIT_NO_SIGNAL        (25)
#define A818_DATA_STITCHBASE_THREAD     (26)
#define A818_DATA_LOW_CHN_THREAD        (27)
#define A818_DATA_HIGH_CHN_THREAD       (28)
#define A818_DATA_RATIO_TYPE            (29)
//#define A818_DATA_INIT_RESIZE           (30)
#define A818_DATA_RGA_ADDR_OFFSET       (30)
#define A818_DATA_YUV_DATA              (31)
#define A818_DATA_RGB_DATA              (32)
#define A818_DATA_RESIZE_DATA           (33)
#define A818_DATA_ROTATE_DATA           (34)

#define A818_RGA_VIR_ADDR_MALLOC        (35)
#define A818_RGA_INIT_HANDLE            (36)
#define A818_RGA_INIT_HANDLE_BUFFER     (37)

#define A818_DP_DATA_MALLOC             (38)
#define A818_DP_READ_MODE               (39)
#define A818_DP_DATA_RB                 (40)
#define A818_DP_RGB_DATA                (41)
#define A818_DP_RESIZE_DATA             (42)
#define A818_DP_BGRA_DATA               (43)

#define A818_DRM_DEVICE_INIT            (44)
#define A818_DRM_EVENTE_THREAD          (45)

#define A818_OSD_DP_RB                  (46)
#define A818_OSD_RB                     (47)
#define A818_OSD_GUI_THREAD             (48)
#define A818_OSD_GPU_THREAD             (49)
#define A818_OSD_DP_THREAD              (50)


#define A818_MPI_INIT_ENCODE_INFO           (51)
#define A818_MPI_INIT_ENCODE_CONTEXT        (52)
#define A818_MPI_INIT_ENCODE_CMD            (53)
#define A818_MPI_INIT_DATA_MALLOC           (54)
#define A818_MPI_INIT_ENCODE_RB             (55)
#define A818_MPI_INIT_ENCODE_THREAD         (56)

#define A818_FIFO_DATA_INFO                 (57)
#define A818_FIFO_CONFIG_REG_MMAP           (58)
#define A818_FIFO_DATA_REG_MMAP             (59)
#define A818_FIFO_RECV_THREAD               (60)


#define A818_FIFO_CMD_REINVENT              (61)
#define A818_FIFO_CMD_ERROR                 (62)
#define A818_FIFO_CMD_SUM_ERROR             (63)
#define A818_FIFO_ENABLE_ERROR              (64)

#define A818_CROP_DATA_BUFFER           (65)
#define A818_CROP_DATA_RB               (66)
#define A818_CROP_THREAD                (67)
#define A818_CROP_FRAME_MALLOC          (68)

/* Stop is still in progress. Resources are retained and the command can retry. */
#define A818_STREAM_STOP_TIMEOUT        (74)
#endif
