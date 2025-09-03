
/*
 * Important:
 *
 * This file using "UTF-8 65001 with BOM" coding.
 * Choose editor coding if you could not see words below.
 *
 * “防控烦人的乱码，编辑器使用上述编码才看得到这句话！”
 * “这句就是故意给编辑器看的，它一般会识别头部，勿删。”
 */

#ifndef HETERO_INTERFACE_202104151110_H
#define HETERO_INTERFACE_202104151110_H

#include <math.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#define restrict                                __restrict__
#endif

#define HETERO_INTERFACE_VER                    0xAC20240109100945

#define C_PI                                    3.1415926535897932384626433832795f

/*
 * 和 FFTW 兼容的复数格式
 */
typedef struct
{
    float r;
    float i;
}
cxf32;

/*
 * 函数执行位标识
 */
typedef enum
{
    e_exc_none      = 0,
    e_exc_queue     = (1 << 0),
    e_exc_profile   = (1 << 1),
    e_exc_bypass    = (1 << 2),
}
hfc_flag_et;

/*
 * 数据格式转变标识
 */
typedef enum
{
    e_dtr_uc_uc,
    e_dtr_uc_hf,
    e_dtr_uc_sf
}
date_from_to_et;

/*
 * 透视变换四边形顶点定义
 */
typedef struct
{
    float v0x, v0y;     // 左上点
    float v1x, v1y;     // 右上点
    float v2x, v2y;     // 左下点
    float v3x, v3y;     // 右下点
}
quad_cord_st;

/*
 * 透视变换矩阵定义
 */
typedef struct
{
    double a00, a01, a02;
    double a10, a11, a12;
    double a20, a21, a22;
}
trans_mat_st;

/*
 * 由 u16 定义的区域
 *  (x1, y1) 为左上角
 *  (x2, y2) 为右下角
 * 注意这和一般的长宽表示法不相同
 */
typedef struct
{
    uint16_t x1, y1;
    uint16_t x2, y2;
}
area_u16_st;

typedef enum {
    e_image_type_input,             // 图像输入
    e_image_type_output,            // 图像输出
    e_image_type_general,           // 不同函数中可分别作为输入和输出
    e_image_type_no_access,         // 供异构算法内部缓存之用
    e_image_type_max
}
image_type_et;

typedef enum {
    e_buffer_type_input,            // 数据输入
    e_buffer_type_output,           // 数据输出
    e_buffer_type_general,          // 数据输入输出通用
    e_buffer_type_no_access,        // 供异构算法内部缓存之用
    e_buffer_type_max
}
buffer_type_et;

typedef enum {
    e_mapping_read,                 // GPU -> CPU
    e_mapping_write,                // CPU -> GPU
    e_mapping_max
}
memory_map_et;

/*
 * 图像通道组织
 */
typedef enum {
    e_pix_chn_1,
    e_pix_chn_2,
    e_pix_chn_3,
    e_pix_chn_4,
    e_pix_chn_max
}
pixel_channel_et;

/*
 * 图像数据格式
 */
typedef enum {
    e_pix_data_norm_u08,
    e_pix_data_norm_u16,
    e_pix_data_norm_s08,
    e_pix_data_norm_s16,
    e_pix_data_gene_u08,
    e_pix_data_gene_u16,
    e_pix_data_gene_u32,
    e_pix_data_gene_s08,
    e_pix_data_gene_s16,
    e_pix_data_gene_s32,
    e_pix_data_f16,
    e_pix_data_f32,
    e_pix_data_max
}
pixel_data_et;

/*
 * YUV 数据类型
 */
typedef enum {
    e_yuv_nv12, // bit_0000, 420sp
    e_yuv_nv21, // bit_0001, 420sp
    e_yuv_yv12, // bit_0010, 420p
    e_yuv_yu12, // bit_0011, 420p
    e_yuv_vyuy, // bit_0100, 422
    e_yuv_uyvy, // bit_0101, 422
    e_yuv_yvyu, // bit_0110, 422
    e_yuv_yuyv, // bit_0111, 422
    e_yuv_nv16, // bit_1000, 422sp
    e_yuv_nv61, // bit_1001, 422sp
    e_yuv_nv24, // bit_1010, 444sp, coop decode only
    e_yuv_nv42, // bit_1011, 444sp, coop decode only
    e_yuv_max
}
e_yuv_format_et;

/*
 * 返回值编码意义对照表，取下列整数的负值
 */
typedef enum {
    /* 正常 */
    e_ret_ok,
    /* 输入含有空指针 */
    e_ret_invalid_pointer,
    /* 参数不合法 */
    e_ret_invalid_para,
    /* 物理内存空间不足 */
    e_ret_insufficient_physical_ram,
    /* 可分配内存空间不足 */
    e_ret_out_of_allocable_ram,
    /* 异构计算失败 */
    e_ret_hetero,
    /* 该编译版本不支持 */
    e_ret_not_support,
    /* 未知故障 */
    e_ret_unknown_error
}
e_return_code_et;

typedef int32_t (*hetero_so_puts_ft)(const char* out_str);

/*
 * 功能：
 *   创建异构缓冲
 *
 * 返回值：
 *   返回异构缓冲句柄
 */
void*
buffer_acquire(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 缓冲用途 */
    buffer_type_et buftp,
    /* 缓冲总字节大小 */
    uint64_t byte_size
);

/*
 * 功能：
 *   创建导出缓冲，具备可直接访问的 CPU 地址，
 *   比异构缓冲速度慢，但不需要经过 MAP 过程。
 *   仅适用于数据直接输入和输出 GPU 的环节，
 *   不应该用于 GPU 内部计算环节中。
 *
 * 返回值：
 *   返回异构缓冲句柄
 */
void*
buffer_acquire_virtual(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 缓冲用途，只能是输入和输出中的一种 */
    buffer_type_et buftp,
    /* 缓冲总字节大小 */
    uint64_t byte_size,
    /* 返回 CPU 虚拟地址的指针 */
    uint8_t** buf_va
);

/*
 * 功能：
 *   复制数据到异构缓冲。
 * 
 * 返回值：
 *   （将）实际复制字节数，参数错误时为负数。
 */
int64_t
buffer_copy_from(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 输入，异构缓冲句柄 */
    void* buf_obj,
    /* 输入数据缓冲 */
    const void* restrict vm_addr,
    /* 数据字节容量 */
    uint64_t size,
    /* 执行位掩码 */
    hfc_flag_et flag
);

/*
 * 功能：
 *   从异构缓冲复制数据。
 * 
 * 返回值：
 *   （将）实际复制字节数，参数错误时为负数。
 */
int64_t
buffer_copy_to(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 输入，异构缓冲句柄 */
    const void* buf_obj,
    /* 输出数据缓冲 */
    void* restrict vm_addr,
    /* 数据字节容量 */
    uint64_t size,
    /* 执行位掩码 */
    hfc_flag_et flag
);

/*
 * 功能：
 *   将满足对齐要求虚拟地址上缓冲导入为异构计算可访问的对象，
 *   速度较原生异构缓冲区慢，但无需 map/unmap 过程。
 *
 * 返回值：
 *   失败时返回0，否则返回异构缓冲句柄。
 */
void*
buffer_import_virtual(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 缓冲用途 */
    buffer_type_et buftp,
    /* 缓冲字节大小 */
    uint64_t buffer_size,
    /* 拟导入的虚拟地址 */
    void* restrict aligned_buf_va
);

/*
 * 功能：
 *   映射异构缓冲。若映射读，则该函数返回前对端会确保完成输出同步。
 *
 * 返回值：
 *   映射出的虚拟地址，每次调用返回的虚拟地址 *不一定* 相同。
 */
void*
buffer_map(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 输入，异构缓冲句柄 */
    void* buffer_obj,
    /* 映射功能，读或写 */
    memory_map_et mapping_type
);

/*
 * 功能：
 *   回收异构缓冲
 */
void
buffer_release(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 输入，异构图像句柄 */
    void* buffer_obj
);

/*
 * 功能：
 *   异构缓冲矩形区间的数据复制。
 * 注意：
 *   所复制的矩形区域宽度和起始点横坐标
 *   必须满足 16 字节对齐的要求。
 *
 * 返回值：
 *   0 表示成功，负数表示失败。
 */
int32_t
buffer_to_buffer_rect_copy(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 输入，异构缓冲句柄 */
    const void* buf_obj_i,
    /* 输出，异构缓冲句柄 */
    void* buf_obj_o,
    /* 源缓冲，宽 U16_2，区间左上点，横坐标 U16_1，纵坐标 U16_0 */
    uint64_t src_wxy,
    /* 目标缓冲，宽 U16_2，区间左上点，横坐标 U16_1，纵坐标 U16_0 */
    uint64_t dst_wxy,
    /* 矩形，宽 U16_1，高 U16_0 */
    uint64_t rect_wh,
    /* 执行位掩码 */
    hfc_flag_et flags
);

/*
 * 功能：
 *   撤销异构缓冲映射。若映射写，则该函数返回前对端会确保已完成输入同步。
 */
void
buffer_unmap(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 输入，异构缓冲句柄 */
    void* buffer_obj,
    /* 输入，映射出的虚拟地址 */
    void* mapped
);

/*
 * 功能：
 *   耗时结束计量。
 *
 * 返回值：
 *   push 与匹配的 pop 区间的毫秒耗时。
 */
float
clock_stack_pop(
    /* 输入，异构环境句柄 */
    void* hetero_hdr
);

/*
 * 功能：
 *   耗时启动计量。push 与 pop 成对使用，返回该区间的耗时，单位毫秒。
 *   可多层嵌套，如
 *       push( ) ... push( ) ... pop( ) ... pop( )
 *   可返回两层各自的耗时。
 */
void
clock_stack_push(
    /* 输入，异构环境句柄 */
    void* hetero_hdr
);

/*
 * 功能：
 *   关联可用的异构计算装置，比如 GPU。初始化异构计算环境，
 *   所有的计算函数都需要在特定的异构环境中才能执行。
 *
 * 返回值：
 *   异构计算环境句柄
 */
void *
compute_acquire(
    /* 输入，调试输出函数回调指针，若为 NULL 则使用库默认输出 */
    hetero_so_puts_ft so_puts
);

/*
 * 功能：
 *   复制异构计算环境，以此方式可开启多路计算模式，
 *   各路分享相同设备，降低设备等待数据时间。
 *
 * 返回值：
 *   异构计算环境句柄
 */
void *
compute_duplicate(
    /* 输入，异构计算环境句柄 */
    const void* hetero_hdr
);

/*
 * 功能：
 *   清理异构计算环境。
 */
void
compute_release(
    /* 异构计算环境句柄 */
    void* hetero_hdr
);

/*
 * 功能：
 *   按伪彩表将 GRAY 格式数据转 RGB24 格式数据。
 *
 * 返回值：
 *   0 表示成功，负数表示失败。
 */
int64_t
convert_gray_to_rgb(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 输入，异构数据句柄，连续的 GRAY 数据 */
    const void* buf_obj_i,
    /* 输入，异构数据句柄，连续的 RGB 伪彩表，可以为 NULL 忽略 */
    const void* buf_obj_r,
    /* 输出，异构数据句柄，输出数据 */
    void* buf_obj_o,
    /* 图像像素宽，必须是 8 的倍数 */
    uint32_t w,
    /* 图像像素高 */
    uint32_t h,
    /* 执行位掩码 */
    hfc_flag_et flags
);

typedef enum
{
    e_bayer_rggb,
    e_bayer_gbrg,
    e_bayer_bggr,
    e_bayer_grbg,
    e_bayer_max
}
bayer_type_et;

/*
 * 功能：
 *   RGB24 格式数据转 BAYER 格式数据。
 *
 * 返回值：
 *   0 表示成功，负数表示失败。
 */
int64_t
convert_rgb_to_bayer(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 输入，异构数据句柄，存放 RGB24 数据 */
    const void* buf_obj_i,
    /* 输出，异构数据句柄，存放 BAYER 数据 */
    void* buf_obj_o,
    /* 图像像素宽，必须是 8 的倍数 */
    uint32_t w,
    /* 图像像素高 */
    uint32_t h,
    /* 配置 BAYER 格式 */
    bayer_type_et format,
    /* 执行位掩码 */
    hfc_flag_et flags
);

/*
 * 功能：
 *   RGB24 格式数据转 GRAY 格式数据。
 *
 * 返回值：
 *   0 表示成功，负数表示失败。
 */
int64_t
convert_rgb_to_gray(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 输入，异构数据句柄，存放 RGB24 数据 */
    const void* buf_obj_i,
    /* 输出，异构数据句柄，像素类型为 UCHAR */
    void* buf_obj_o,
    /* 图像像素宽，必须是 8 的倍数 */
    uint32_t w,
    /* 图像像素高 */
    uint32_t h,
    /* 执行位掩码 */
    hfc_flag_et flags
);

/*
 * 功能：
 *   RGB24 格式数据转 BT601_LIMITED_YUV 格式数据
 *   注意该接口不支持 BT709 和 FULL_YUV 格式
 *   如果输出是 422 格式，则输出缓冲大小为 (W * H * 2)；
 *   若是 420 格式，则输处缓冲大小为 (W * H * 3 / 2)。
 *
 * 返回值：
 *   0 表示成功，负数表示失败。
 */
int64_t
convert_rgb_to_yuv(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 输入，异构数据句柄，像素类型为 RGB24 彩色 */
    const void* buf_obj_i,
    /* 输出，异构数据句柄，存放 YUV 数据 */
    void* buf_obj_o,
    /* 图像像素宽度，必须是 8 的整数倍 */
    uint32_t w,
    /* 图像像素高度, 必须是 2 的整数倍 */
    uint32_t h,
    /* 输出数据的格式 */
    e_yuv_format_et format,
    /* 执行位掩码 */
    hfc_flag_et flags
);

/*
 * 功能：
 *   BT601_LIMITED_YUV 格式数据转 RGB24 格式数据，附灰度输出（可选）。
 *   注意该接口不支持 BT709 和 FULL_YUV 格式
 *   如果输入是 422 格式，则输入缓冲大小为 (W * H * 2)；
 *   若是 420 格式，则输入缓冲大小为 (W * H * 3 / 2)。
 *   注意，灰和 YUV 的 Y 分量并不是同一种数据，两者存在
 *   线性转换关系，并不等价，只是相似。
 *
 * 返回值：
 *   0 表示成功，负数表示失败。
 */
int32_t
convert_yuv_to_rgb(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 输入，异构数据句柄，存放 YUV 数据 */
    const void* buf_obj_i,
    /* 输出，可为 0 忽略，像素类型为 UCHAR 灰度 */
    void* buf_obj_g,
    /* 输出，异构数据句柄，像素类型为 RGB24 彩色 */
    void* buf_obj_c,
    /* 图像像素宽度，必须是 8 的整数倍 */
    uint32_t w,
    /* 图像像素高度，必须是 2 的整数倍 */
    uint32_t h,
    /* 输入数据的具体格式 */
    e_yuv_format_et format,
    /* 执行位掩码 */
    hfc_flag_et flags
);

/*
 * 功能：
 *   搬移 RGB24 数据并同时支持以下特性
 *     1）截取区域
 *     2）翻转 RGB 和 BGR
 *     3）画面 180 度翻转
 *     4）可选灰度输出
 *     5）半插值缩小至 1/4 画面
 *
 * 返回值：
 *   0 表示成功，负数表示失败。
 */
int64_t
cooperate_copy_rgb24(
    /* 图像像素宽，若不启用 rect 则必须满足 32 整除 */
    uint64_t w,
    /* 图像像素高 */
    uint64_t h,
    /*
     * 设定复制区域，当为 0 时忽略该参数，否则
     *   BIT 48-63 -> 左上点横坐标，若满足整除 16 则效率最高
     *   BIT 32-47 -> 左上点纵坐标
     *   BIT 16-31 -> 矩形宽度，必须整除 32
     *   BIT 00-15 -> 矩形高度
     * 该矩形区域必须在原画面范围内
     */
    uint64_t rect,
    /* 128 位对齐的 CPU 地址，输入 RGB24 */
    const uint8_t* restrict src,
    /* 128 位对齐的 CPU 地址，合成 GRAY，可为零忽略 */
    uint8_t* restrict dst_g,
    /* 128 位对齐的 CPU 地址，输出 RGB24，可为零忽略 */
    uint8_t* restrict dst_c,
    /*
       BIT2:   0 -> 无操作       1 -> 图像半插值缩小到 1/4
       BIT1:   0 -> 无操作       1 -> BGR 转 RGB
       BIT0:   0 -> 无操作       1 -> 旋转 180 度
     */
    uint64_t flag
);

/*
 * 功能：
 *   解码 YUV 数据并计算 GRAY / RGB24 数据，可一并完成
 *     1）图像裁剪
 *     2）图像旋转 180 度
 *     3）半插值缩小至 1/4 画面
 *
 * 返回值：
 *   0 表示成功，负数表示失败。
 */
int64_t
cooperate_decode_yuv(
    /* 图像像素宽，若不启用 rect 则必须满足 32 整除 */
    uint64_t w,
    /* 图像像素高, 若不启用 rect 则必须是偶数 */
    uint64_t h,
    /*
     * 设定转换区域（原图坐标），当为 0 时忽略该参数，否则
     *   BIT 48-63 -> 左上点横坐标，必须是偶数，若满足整除 16 则效率最高
     *   BIT 32-47 -> 左上点纵坐标，必须是偶数
     *   BIT 16-31 -> 矩形宽度，必须整除 32
     *   BIT 00-15 -> 矩形高度，必须是偶数
     * 该矩形区域必须在原画面范围内
     */
    uint64_t rect,
    /* YUV 数据的格式 */
    e_yuv_format_et format,
    /* 128 位对齐的 CPU 地址，输入 YUV */
    const uint8_t* restrict src,
    /* 128 位对齐的 CPU 地址，输出 GRAY，可为零忽略 */
    uint8_t* restrict dst_g,
    /* 128 位对齐的 CPU 地址，输出 RGB24，可为零忽略 */
    uint8_t* restrict dst_c,
    /*
       BIT3:   0 -> 无操作       1 -> 图像半插值缩小到 1/4
       BIT2:   0 -> BT601        1 -> BT709
       BIT1:   0 -> LIMITED_YUV  1 -> FULL_YUV
       BIT0:   0 -> 无操作       1 -> 旋转 180 度
     */
    uint64_t flag
);

/*
 * 功能：
 *   合成 OSD 图像（可选），并进行 YUV 编码。
 *   字符层格式为 RGB565 且透明色为 0
 *
 * 返回值：
 *   0 表示成功，负数表示失败。
 */
int64_t
cooperate_encode_yuv(
    /* 图像像素宽，必须满足 32 整除 */
    uint64_t w,
    /* 图像像素高, 必须是偶数 */
    uint64_t h,
    /* YUV 数据的格式 */
    e_yuv_format_et format,
    /* 128 位对齐的 CPU 地址，输入字符层，RGB565，为零则忽略 OSD 合成 */
    const uint16_t* restrict osd,
    /* 128 位对齐的 CPU 地址，输入图像层，RGB24 */
    const uint8_t* restrict src,
    /* 128 位对齐的 CPU 地址，输出 YUV */
    uint8_t* restrict yuv,
    /*
       BIT2:   0 -> BT601        1 -> BT709
       BIT1:   0 -> LIMITED_YUV  1 -> FULL_YUV
       BIT0:   (RESERVED)
     */
    uint64_t flag
);

/*
 * 功能：
 *   合成 OSD 图像，可选 RGB 或 BGR 输出。
 *   字符层格式为 RGB565 且透明色为 0
 *
 * 返回值：
 *   0 表示成功，负数表示失败。
 */
int64_t
cooperate_overlap_osd(
    /* 图像像素宽，必须满足 16 整除 */
    uint64_t w,
    /* 图像像素高 */
    uint64_t h,
    /* 128 位对齐的 CPU 地址，输入字符层，RGB565 */
    const uint16_t* restrict osd,
    /* 128 位对齐的 CPU 地址，输入图像层，RGB24 */
    const uint8_t* restrict src,
    /* 128 位对齐的 CPU 地址，输出合成图，RGB24 */
    uint8_t* restrict dst,
    /*
       BIT0:   0 -> DEFAUT_RGB   1 -> EXCHANGE_BGR
     */
    uint64_t flag
);

/*
 * 功能：
 *   转换大小端。
 *
 * 返回值：
 *   0 表示成功，负数表示失败。
 */
int64_t
cooperate_swap_ending(
    /* 数据字节长度，必须整除 64 */
    uint64_t data_bytes_len,
    /* 128 位对齐的 CPU 地址，输入数据 */
    const uint8_t* restrict src,
    /* 128 位对齐的 CPU 地址，输出数据 */
    uint8_t* restrict dst,
    /* FLAG:
       0 -> 16  位数据
       1 -> 32  位数据
       2 -> 64  位数据 */
    uint64_t flag
);

/*
 * 功能：
 *   暗通道极值卷积。
 *
 * 返回值：
 *   0 表示成功，负数表示失败。
 */
int32_t
dark_filter_with_image(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 输入，异构图像句柄，需要 e_pix_data_gene_u08 数据类型，反置暗图 */
    const void* img_obj_i,
    /* 输出，暗通道卷积后的异构图像句柄，需要 e_pix_data_gene_u08 数据类型 */
    void* img_obj_o,
    /* 卷积半轴长度 */
    uint32_t range,
    /* 执行位掩码 */
    hfc_flag_et flag
);

/*
 * 功能：
 *   回收 enhance 计算句柄
 */
void
enhance_finish(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 输入，enhance 计算句柄 */
    void* enhance_hdr
);

/*
 * 功能：
 *   图像去雾增强灰度版，直接读取 CPU 内存数据
 *
 * 返回值：
 *   0 表示成功，负数表示失败。
 */
int32_t
enhance_gray(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 输入，去雾增强句柄 */
    void* enhance_hdr,
    /* 输入图像缓冲 */
    const uint8_t* restrict input,
    /* 输出，异构数据对象 */
    void* buf_out,
    /* 增强阈值，范围 [4.0, 10.0] */
    float clip_limit,
    /* 执行位掩码 */
    hfc_flag_et flags
);

/*
 * 功能：
 *   初始化 enhance 计算。
 *
 * 返回值：
 *   enhance 计算句柄。
 */
void *
enhance_prepare(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 像素宽，必须是 64 整数倍 */
    uint32_t w,
    /* 像素高，必须是 4 整数倍 */
    uint32_t h,
    /* 输入数据类型，0 为 GRAY 1 为 RGB */
    uint32_t is_rgb
);

/*
 * 功能：
 *   图像去雾增强 RGB24 版，直接读取 CPU 内存数据，并可选灰度增强输出
 *
 * 返回值：
 *   0 表示成功，负数表示失败。
 */
int32_t
enhance_rgb(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 输入，去雾增强句柄 */
    void* enhance_hdr,
    /* 输入图像缓冲，RGB24 格式 */
    const uint8_t* restrict input_rgb,
    /* 输出，异构数据对象，RGB24 格式 */
    void* buf_c,
    /* 输出，异构数据对象，GRAY 格式，可为 NULL 忽略 */
    void* buf_g,
    /* 增强阈值，范围 [4.0, 10.0] */
    float clip_limit,
    /* 执行位掩码 */
    hfc_flag_et flags
);

/*
 * 功能：
 *   GRAY 图像黑边填充中心放大并转伪彩至 RGB 图，目标图长宽尺寸需要大于原图两倍。
 *   采用 1:4 像素放大填充，非插值计算。
 *
 * 返回值：
 *   0 表示成功，负数表示失败。
 */
int32_t
extent_quad_gray_to_rgb(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 输入，异构数据对象，GRAY 图 */
    const void* buf_obj_i,
    /* 输入，异构数据对象，RGB 伪彩表，可为 NULL 忽略 */
    const void* buf_obj_r,
    /* 输出，异构数据对象，RGB24 图 */
    void* buf_obj_o,
    /* 原图像素宽，必须是 8 整数倍 */
    uint32_t sw,
    /* 原图像素高 */
    uint32_t sh,
    /* 目标像素宽，必须是 32 整数倍 */
    uint32_t dw,
    /* 目标像素高 */
    uint32_t dh,
    /* 执行位掩码 */
    hfc_flag_et flags
);

/*
 * 功能：
 *   RGB 图像黑边填充中心放大至 RGB 图，目标图长宽尺寸需要大于原图两倍。
 *   采用 1:4 像素放大填充，非插值计算。
 *
 * 返回值：
 *   0 表示成功，负数表示失败。
 */
int32_t
extent_quad_rgb_to_rgb(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 输入，异构数据对象，RGB24 图 */
    const void* buf_obj_i,
    /* 输出，异构数据对象，RGB24 图 */
    void* buf_obj_o,
    /* 原图像素宽，必须是 8 整数倍 */
    uint32_t sw,
    /* 原图像素高 */
    uint32_t sh,
    /* 目标像素宽，必须是 32 整数倍 */
    uint32_t dw,
    /* 目标像素高 */
    uint32_t dh,
    /* 执行位掩码 */
    hfc_flag_et flags
);

/*
 * 功能：
 *   GRAY 图像黑边填充中心扩展并转伪彩至 RGB 图，目标图长宽尺寸需要大于原图。
 *
 * 返回值：
 *   0 表示成功，负数表示失败。
 */
int32_t
extent_self_gray_to_rgb(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 输入，异构数据对象，GRAY 图 */
    const void* buf_obj_i,
    /* 输入，异构数据对象，RGB 伪彩表，可为 NULL 忽略 */
    const void* buf_obj_r,
    /* 输出，异构数据对象，RGB24 图 */
    void* buf_obj_o,
    /* 原图像素宽，必须是 16 整数倍 */
    uint32_t sw,
    /* 原图像素高 */
    uint32_t sh,
    /* 目标像素宽，必须是 32 整数倍 */
    uint32_t dw,
    /* 目标像素高 */
    uint32_t dh,
    /* 执行位掩码 */
    hfc_flag_et flags
);

/*
 * 功能：
 *   RGB 图像黑边填充中心扩展至 RGB 图，目标图长宽尺寸需要大于原图。
 *
 * 返回值：
 *   0 表示成功，负数表示失败。
 */
int32_t
extent_self_rgb_to_rgb(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 输入，异构数据对象，RGB24 图 */
    const void* buf_obj_i,
    /* 输出，异构数据对象，RGB24 图 */
    void* buf_obj_o,
    /* 原图像素宽，必须是 16 整数倍 */
    uint32_t sw,
    /* 原图像素高 */
    uint32_t sh,
    /* 目标像素宽，必须是 32 整数倍 */
    uint32_t dw,
    /* 目标像素高 */
    uint32_t dh,
    /* 执行位掩码 */
    hfc_flag_et flags
);

/*
 * 功能：
 *   一维 FFT 逆计算。
 *
 * 返回值：
 *   0 表示成功，负数表示失败。
 */
int32_t
fft_1d_backword(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 输入，FFT 计算句柄 */
    void* fft_hdr,
    /* 输入，数据句柄 */
    const void* buf_obj_i,
    /* 输出，数据句柄 */
    void* buf_obj_o,
    /* 单行数据的数据个数 */
    uint64_t size,
    /* 数据行数 */
    uint64_t lines,
    /* 执行位掩码 */
    hfc_flag_et flags
);

/*
 * 功能：
 *   一维 FFT 正计算。
 *
 * 返回值：
 *   0 表示成功，负数表示失败。
 */
int32_t
fft_1d_forword(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 输入，FFT 计算句柄 */
    void* fft_hdr,
    /* 输入，数据句柄 */
    const void* buf_obj_i,
    /* 输出，数据句柄 */
    void* buf_obj_o,
    /* 单行数据的数据个数 */
    uint64_t size,
    /* 数据行数 */
    uint64_t lines,
    /* 执行位掩码 */
    hfc_flag_et flags
);

/*
 * 功能：
 *   回收 FFT 计算句柄
 */
void
fft_finish(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 输入，FFT 计算句柄 */
    void* fft_hdr
);

/*
 * 功能：
 *   初始化 FFT 计算。
 *
 * 返回值：
 *   FFT 计算句柄，在配置不变的条件下可反复调用该算法。
 */
void*
fft_prepare(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 一维 FFT 计算的最大尺寸，必须是 2^N */
    uint64_t max_size
);

/*
 * 功能：
 *   计算指定格式的像素字节宽度。
 *
 * 返回值：
 *   计算所需的像素字节宽度
 */
uint32_t
get_pixel_width(
    /* 通道类型 */
    pixel_channel_et chn, 
    /* 数据格式 */
    pixel_data_et fmt
);

/*
 * 功能：
 *   创建异构图像。图像对象是一种复杂组织的数据，区别于普通的缓冲区，
 *   专门用于像素浮点计算，比如含有插值、变形等要求的高精度计算。
 * 
 * 返回值：
 *   异构图像句柄
 */
void *
image_acquire(
    /* 输入，异构环境句柄 */
    void* hetero_hdr, 
    /* 图像用途 */
    image_type_et imgtp, 
    /* 像素宽度 */
    uint32_t img_w, 
    /* 像素高度 */
    uint32_t img_h, 
    /* 像素格式 */
    pixel_channel_et pix_fmt, 
    /* 数据格式 */
    pixel_data_et dat_c
);

/*
 * 功能：
 *   从已创建的异构缓冲对象中创建异构图像，两者共用同一片内存区间，
 *   可同时使用两者的操作函数，从而避免数据搬移。
 *   该尺寸的图像对象的行填充必须为 0 否则会出现错误。
 *
 * 返回值：
 *   异构图像句柄
 */
void *
image_build_from_buffer(
    /* 输入，异构环境句柄 */
    void* hetero_hdr, 
    /* 输入，异构缓冲句柄 */
    void* buf_obj, 
    /* 图像用途 */
    image_type_et imgtp, 
    /* 像素宽度 */
    uint32_t img_w, 
    /* 像素高度 */
    uint32_t img_h, 
    /* 像素格式 */
    pixel_channel_et pix_fmt, 
    /* 数据格式 */
    pixel_data_et dat_c
);

/*
 * 功能：
 *   复制数据到异构图像。
 *
 * 返回值：
 *   （将）实际复制字节数，参数错误时为负数。
 */
int64_t
image_copy_from(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 输入，异构图像句柄 */
    void* img_obj,
    /* 输入数据缓冲 */
    const void* restrict vm_addr,
    /* 缓冲去字节容量 */
    uint64_t buf_size,
    /* 输入缓冲区每行字节数，用于行填充区的处理，可以为 0 取像素字节数与行宽之积 */
    uint64_t row_pitch,
    /* 执行位掩码 */
    hfc_flag_et flag
);

/*
 * 功能：
 *   从异构图像复制数据。
 *
 * 返回值：
 *   （将）实际复制字节数，参数错误时为负数。
 */
int64_t
image_copy_to(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 输入，异构图像句柄 */
    const void* img_obj,
    /* 输出数据缓冲 */
    void* restrict vm_addr,
    /* 缓冲去字节容量 */
    uint64_t buf_size,
    /* 输出缓冲区每行字节数，用于行填充区的处理，可以为 0 取像素字节数与行宽之积 */
    uint64_t row_pitch,
    /* 执行位掩码 */
    hfc_flag_et flag
);

/*
 * 功能：
 *   映射异构图像。若映射读，则函数返回前对端会确保完成输出同步。
 * 
 * 返回值：
 *   映射出的虚拟地址，每次调用返回的虚拟地址 *不一定* 相同。
 */
void *
image_map(
    /* 输入，异构环境句柄 */
    void *hetero_hdr,
    /* 输入，异构图像句柄 */
    void *image_obj,
    /* 映射功能 */
    memory_map_et mapping_type,
    /* 输出，为 0 表示映射出的空间无间隙，否则其数值表示实际行宽字节数 */
    uint32_t *pitch
);

/*
 * 功能：
 *   回收异构图像
 */
void
image_release(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 输入，异构图像句柄 */
    void* image_obj
);

/*
 * 功能：
 *   撤销异构图像映射。若映射写，则函数返回前对端会确保完成输入同步。
 */
void
image_unmap(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 异构图像句柄 */
    void* image_obj,
    /* 输入，映射出的虚拟地址 */
    void* mapped
);

/*
 * 功能：
 *   将 RGB565 的字符层数据压缩成 BIT 格式，供灰度 OSD 函数使用。
 *
 * 返回值：
 *   0 表示成功，负数表示失败。
 */
int32_t
overlapped_osd_compress(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 输入，CPU 缓冲区，字符层，RGB565 格式 */
    const uint16_t* restrict buf_i,
    /* 输出，异构数据对象，一个 BIT 表示一个像素，容量应该是 (w * h / 8) 字节 */
    void* buf_obj_o,
    /* 字符层（不是画面层）图像像素宽，必须是 64 的倍数 */
    uint32_t w,
    /* 字符层（不是画面层）图像像素高 */
    uint32_t h,
    /* 透明色，buf_obj_i 中为该数值的像素将变为透明 */
    uint32_t pat
);

/*
 * 功能：
 *   将相同像素尺寸的字符层和图像层（BUF格式）进行叠加，灰度模式。
 *
 * 返回值：
 *   0 表示成功，负数表示失败。
 */
int32_t
overlapped_osd_gray(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 输入，异构数据对象，字符层，每像素使用一个BIT，0为透明，1为着色 */
    const void* buf_obj_i,
    /* 输出，异构数据对象，背景图像 */
    void* buf_obj_o,
    /* 字符层（不是画面层）图像像素宽，必须是 64 的倍数 */
    uint32_t w,
    /* 字符层（不是画面层）图像像素高 */
    uint32_t h,
    /*
     * 组合参数：
     * bit 0 - 7    着色灰度值，1 到 255
     * bit 8        四倍扩展模式
     * bit 9 - 31   必须为 0
     */
    uint32_t val,
    /* 执行位掩码 */
    hfc_flag_et flag
);

/*
 * 功能：
 *   将相同像素尺寸的字符层和图像层（BUF格式）进行叠加，彩色模式。
 *
 * 返回值：
 *   0 表示成功，负数表示失败。
 */
int32_t
overlapped_osd_rgb(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 输入，异构数据对象，作为字符，使用 RGB565 格式 */
    const void* buf_obj_i,
    /* 输出，异构数据对象，作为背景 */
    void* buf_obj_o,
    /* 字符层（不是画面层）图像像素宽，必须是 8 的倍数 */
    uint32_t w,
    /* 字符层（不是画面层）图像像素高 */
    uint32_t h,
    /*
     * 组合参数：
     * bit 0 - 15   透明色，buf_obj_i 中为该数值的像素将变为透明
     * bit 16       四倍扩展模式
     * bit 17 - 31  必须为 0
     */
    uint32_t para,
    /* 执行位掩码 */
    hfc_flag_et flag
);

/*
 * 功能：
 *   将相同像素尺寸的字符层和图像层（BUF格式）进行半透明叠加，彩色模式。
 *   OSD 层为 0 的像素将是完全透明的，此外的像素完全不透明。
 *
 * 返回值：
 *   0 表示成功，负数表示失败。
 */
int32_t
overlapped_osd_rgb_alpha(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 输入，异构数据对象，作为字符，使用 RGB565 格式 */
    const void* buf_obj_i,
    /* 输出，异构数据对象，作为背景 */
    void* buf_obj_o,
    /* 字符层（不是画面层）图像像素宽，必须是 8 的倍数 */
    uint32_t w,
    /* 字符层（不是画面层）图像像素高 */
    uint32_t h,
    /*
     * 组合参数：
     * bit 0 - 15   半透明色，buf_obj_i 中为该数值的像素将变为透明
     * bit 16 - 23  混合比率，有效范围 0x1 至 0xfe
     * bit 24 - 31  必须为 0
     */
    uint32_t para,
    /* 执行位掩码 */
    hfc_flag_et flag
);

/*
 * 功能：
 *   通过转换矩阵完成透视变换，并按比例进行中心膨胀。
 *
 * 返回值：
 *   0 表示成功，负数表示失败。
 */
int32_t
perstrans_with_expand(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 输入，透视算法输入的异构图像句柄，需要 e_pix_data_norm_u08 数据类型 */
    void* img_obj_i,
    /* 输入，透视算法输出的异构图像句柄，需要 e_pix_data_norm_u08 数据类型 */
    void* img_obj_o,
    /* 输入，3x3 变换矩阵，和 OPENCV 相同不需要求逆 */
    const trans_mat_st* transmat,
    /* 中心膨胀系数，用于稳像去黑边 */
    double expand_rate,
    /* 由 4 个 ushort 组成 64 位的输出坐标限定，超出部分为黑色，
       不足部分自动沿边沿延补，该参数设为 0 表示忽略。
         限定输出框左上 X 坐标: bit00 - bit15
         限定输出框左上 Y 坐标: bit16 - bit31
         限定输出框宽带 W 像素: bit32 - bit47
         限定输出框高度 H 像素: bit48 - bit63 */
    uint64_t limited_rect,
    /* 执行位掩码 */
    hfc_flag_et flags
);

/*
 * 功能：
 *   通过四边形坐标完成透视变换。
 * 
 * 返回值：
 *   0 表示成功，负数表示失败。
 */
int32_t
perstrans_with_quadcord(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 输入，透视算法输入的异构图像句柄，需要 e_pix_data_norm_u08 数据类型 */
    void* img_obj_i,
    /* 输入，透视算法输出的异构图像句柄，需要 e_pix_data_norm_u08 数据类型 */
    void* img_obj_o,
    /* 输入，原始四边形，没有边沿限制，坐标可以是任意点，比如负数或超过图像宽度与高度 */
    const quad_cord_st* from,
    /* 输入，目标四边形，没有边沿限制，坐标可以是任意点，比如负数或超过图像宽度与高度 */
    const quad_cord_st* to,
    /* 叠层覆盖模式，用于消旋类需求，非零值开启 */
    uint32_t overwrite_mode,
    /* 由 4 个 ushort 组成 64 位的输出坐标限定，超出部分为黑色，
       不足部分自动沿边沿延补，该参数设为 0 表示忽略,
       并且和 overwrite 模式不能同时生效：
         限定输出框左上 X 坐标: bit00 - bit15
         限定输出框左上 Y 坐标: bit16 - bit31
         限定输出框宽带 W 像素: bit32 - bit47
         限定输出框高度 H 像素: bit48 - bit63 */
    uint64_t limited_rect,
    /* 执行位掩码 */
    hfc_flag_et flags
);

/*
 * 功能：
 *   通过转换矩阵完成透视变换。
 *
 * 返回值：
 *   0 表示成功，负数表示失败。
 */
int32_t
perstrans_with_transmat(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 输入，透视算法输入的异构图像句柄，需要 e_pix_data_norm_u08 数据类型 */
    void* img_obj_i,
    /* 输入，透视算法输出的异构图像句柄，需要 e_pix_data_norm_u08 数据类型 */
    void* img_obj_o,
    /* 输入，3x3 变换矩阵，和 OPENCV 相同不需要求逆 */
    const trans_mat_st* transmat,
    /* 叠层覆盖模式，用于消旋类需求，非零值开启 */
    uint32_t overwrite_mode,
    /* 由 4 个 ushort 组成 64 位的输出坐标限定，超出部分为黑色，
       不足部分自动沿边沿延补，该参数设为 0 表示忽略，
       并且和 overwrite 模式不能同时生效：
         限定输出框左上 X 坐标: bit00 - bit15
         限定输出框左上 Y 坐标: bit16 - bit31
         限定输出框宽带 W 像素: bit32 - bit47
         限定输出框高度 H 像素: bit48 - bit63 */
    uint64_t limited_rect,
    /* 执行位掩码 */
    hfc_flag_et flags
);

/*
 * 功能：
 *   将输入图像进行 1/2 下采样，输出 1/4 图，
 *   采用批量像素均值计算，较 resize 更快。
 *
 * 返回值：
 *   0 表示成功，负数表示失败。
 */
int32_t
prescale_2(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 输入，异构数据对象，数据格式由 format 参数确定 */
    const void* buf_obj_i,
    /* 输出，异构数据对象，数据格式由 format 参数确定 */
    void* buf_obj_o,
    /* 原图像素宽，必须是 8 整数倍 */
    uint64_t w,
    /* 原图像素高，必须是 2 整数倍 */
    uint64_t h,
    /* 源图和目标图数据格式 */
    date_from_to_et format,
    /* 执行位掩码 */
    hfc_flag_et flags
);

/*
 * 功能：
 *   将输入图像水平方向进行 1/2 下采样，输出 1/2 图，
 *   采用批量像素均值计算，较 resize 更快。
 *
 * 返回值：
 *   0 表示成功，负数表示失败。
 */
int32_t
prescale_2_horizental(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 输入，异构数据对象，数据格式由 format 参数确定 */
    const void* buf_obj_i,
    /* 输出，异构数据对象，数据格式由 format 参数确定 */
    void* buf_obj_o,
    /* 原图像素宽，必须是 16 整数倍 */
    uint64_t w,
    /* 原图像素高 */
    uint64_t h,
    /* 源图和目标图数据格式 */
    date_from_to_et format,
    /* 执行位掩码 */
    hfc_flag_et flags
);

/*
 * 功能：
 *   截取区域并缩放图像。
 *
 * 返回值：
 *   0 表示成功，负数表示失败。
 */
int32_t
resize_from_extracted(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 输入，原始尺寸的异构图像句柄，需要 e_pix_chn_3/4 通道类型和 e_pix_data_norm_u08 数据类型 */
    const void* img_obj_i,
    /* 输出，目标尺寸的异构数据句柄，映射后为连续缓冲区，像素类型为 RGB888 */
    void* buf_obj_rgba_o,
    /* 截取框左上角 X 坐标，可为负数表示在图像外截取黑边 */
    float ex,
    /* 截取框左上角 Y 坐标，可为负数表示在图像外截取黑边 */
    float ey,
    /* 截取框像素宽，超出图像区域采用黑边填充 */
    float ew,
    /* 截取框像素高，超出图像区域采用黑边填充 */
    float eh,
    /* 缩放后的像素宽 */
    uint32_t dw,
    /* 缩放后的像素高 */
    uint32_t dh,
    /* 执行位掩码 */
    hfc_flag_et flags
);

/*
 * 功能：
 *   缩放图像 A 并填充至图像 B 指定位置。
 *
 * 返回值：
 *   0 表示成功，负数表示失败。
 */
int32_t
resize_then_draw_to(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 输入，被缩放的异构图像句柄，需要 e_pix_chn_3/4 通道类型和 e_pix_data_norm_u08 数据类型 */
    const void* img_obj_i,
    /* 输出，被填充的异构数据句柄 */
    void* img_obj_o,
    /* 填充区域左上角 X 坐标 */
    uint32_t dx,
    /* 填充区域左上角 Y 坐标 */
    uint32_t dy,
    /* 填充区域的像素宽 */
    uint32_t dw,
    /* 填充区域的像素高 */
    uint32_t dh,
    /* 执行位掩码 */
    hfc_flag_et flags
);

/*
 * 功能：
 *   缩放图像，反置暗通道专用。
 *
 * 返回值：
 *   0 表示成功，负数表示失败。
 */
int32_t
resize_with_image_dark(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 输入，原始尺寸的异构图像句柄，需要 e_pix_chn_3/4 通道类型和 e_pix_data_norm_u08 数据类型 */
    const void* img_obj_i,
    /* 输出，目标尺寸的异构图像句柄，需要 e_pix_data_gene_u08 数据类型，反置暗图 */
    void* img_obj_dark_o,
    /* 输出，目标尺寸的异构图像句柄，需要 e_pix_data_norm_u08 数据类型，灰图，需要与 image_obj_dark_o 相同图像尺寸 */
    void* img_obj_gray_o,
    /* 执行位掩码 */
    hfc_flag_et flags
);

/*
 * 功能：
 *   进行 Shi-Tomas 特征点计算。
 *
 * 返回值：
 *   从 point_xy 中返回的坐标数，负数表示失败。
 */
int32_t
shitomas_corners(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 输入，Shi-Tomas 计算对象句柄 */
    void* shitomas_hdr,
    /* 输入，异构数据对象，单字节灰图 */
    const void* buf_obj_i,
    /* 输出，返回有效顶点的坐标，Y 为高 16 位，X 为低 16 位 */
    uint32_t* point_xy,
    /* 排除区域列表，可用于筛掉输入中的背景字符和目标框等干扰，可为 0 */
    const area_u16_st* except_list,
    /* 上述列表项数 */
    uint32_t except_num
);

/*
 * 功能：
 *   用于 Shi-Tomas 算法的清理工作。
 */
void
shitomas_finish(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 输入，Shi-Tomas 算法句柄 */
    void* shitomas_hdr
);

/*
 * 功能：
 *   用于 Shi-Tomas 算法的准备工作。
 *
 * 返回值：
 *   Shi-Tomas 计算句柄，在配置不变的条件下可反复调用该算法。
 */
void*
shitomas_prepare(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 图像像素宽 */
    uint32_t w,
    /* 图像像素高 */
    uint32_t h,
    /* 图像有效区域划定，可用于排除边框等无效区域，为 0 使用默认值 */
    const area_u16_st* enabled_area,
    /* 和计算相关的调优参数，为 0 使用默认值 */
    uint32_t hint,
    /* 最大顶点数 */
    uint32_t max_corners,
    /* 质量参数，需满足 0 < quality_level < 1 条件 */
    float quality_level,
    /* 顶点最小间距 */
    float min_distance
);

/*
 * 功能：
 *   用于简化版 LK 光流算法的清理工作。
 */
void
simp_optical_finish(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 输入，Simp-Optical 算法句柄 */
    void* optical_hdr
);

/*
 * 功能：
 *   用于简化版 LK 光流算法的准备工作。
 *
 * 返回值：
 *   Simp-Optical 计算句柄，在配置不变的条件下可反复调用该算法。
 */
void *
simp_optical_prepare(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 图像数据像素宽 */
    uint32_t w,
    /* 图像数据像素高 */
    uint32_t h,
    /* 图像裁剪像素宽，按此数值居中裁掉左右边沿 */
    uint32_t crop_w,
    /* 图像裁剪像素高，按此数值居中裁掉上下边沿 */
    uint32_t crop_h,
    /* 金字塔层数 */
    uint32_t level,
    /* 每层迭代数 */
    uint32_t iterate,
    /* 最大角点数量，用于确定内部数组长度，需要二幂对齐 */
    uint32_t max_points
);

/*
 * 功能：
 *   将前一帧“退壳”，把当前帧刷新为前一帧，手动执
 *   行的原因是可以在该操作前先出光流的计算结果。
 * 返回值：
 *   0 表示成功，负数表示失败。
 */
int32_t
simp_optical_reload(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 输入，Simp-Optical 算法句柄 */
    void* optical_hdr,
    /* 执行位掩码 */
    hfc_flag_et flags
);

/*
 * 功能：
 *   重置简化版 LK 光流算法的内部缓存，用于开始新画面片段。
 */
void
simp_optical_reset(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 输入，Simp-Optical 算法句柄 */
    void* optical_hdr
);

/*
 * 功能：
 *   通过简化版 LK 光流算法计算角点位置迁移。
 *
 * 返回值：
 *   0 表示成功，负数表示失败。
 */
int32_t
simp_optical_shoot(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 输入，Simp-Optical 算法句柄 */
    void* optical_hdr,
    /* 输入，异构数据对象，单字节灰图 */
    const void* buf_obj,
    /* 输入角点列表，低 16 位为 X 坐标，高 16 位为 Y 坐标 */
    uint32_t* list_in,
    /* 输出角点列表，坐标变化按 X0 Y0 X1 Y1 …… Xn Yn 排列 */
    float* list_out,
    /* 角点数 */
    uint64_t num
);

/*
 * 功能：
 *   用于 Wavelet-Fuse 算法的清理工作。
 */
void
wavelet_finish(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 输入，Wavelet-Fuse 算法句柄 */
    void* wavelet_hdr
);

/*
 * 功能：
 *   按坐标偏移量对大图和小图进行小波融合。
 *   需满足的条件是小图在大图边界范围内，否则不产生效果并返回错误码。
 * 
 * 返回值：
 *   0 表示成功，负数表示失败。
 */
int32_t
wavelet_fuse(
    /* 输入，异构环境句柄 */
    void* hetero_hdr,
    /* 输入，Wavelet-Fuse 计算对象句柄 */
    void* wavelet_hdr,
    /* 小图缓冲区对象 */
    const void* buf_obj_lt,
    /* 大图缓冲区对象，将被融合数据直接覆盖 */
    void* buf_obj_bg,
    /* 小图左上角在大图上的 X 坐标偏移 */
    uint32_t off_x,
    /* 小图左上角在大图上的 Y 坐标偏移 */
    uint32_t off_y,
    /* 混合比例，支持 65 段，取值范围 0 <= ratio <= 64 */
    uint32_t ratio,
    /* 执行位掩码 */
    hfc_flag_et flags
);

/*
 * 功能：
 *   专用于 Wavelet-Fuse 算法的准备工作。
 *
 * 返回值：
 *   Wavelet-Fuse 计算句柄，在配置不变的条件下可反复调用该算法。
 */
void *
wavelet_prepare(
    /* 输入，异构环境句柄 */
    void *hetero_hdr,
    /* 小图宽高 */
    uint32_t w_lt,
    uint32_t h_lt,
    /* 大图宽高 */
    uint32_t w_bg,
    uint32_t h_bg
);

/* 定义集体访问指针 */
typedef void *      (BUFFER_ACQUIRE)(void*, buffer_type_et, uint64_t);
typedef void *      (BUFFER_ACQUIRE_VIRTUAL)(void*, buffer_type_et, uint64_t, uint8_t**);
typedef int64_t     (BUFFER_COPY_FROM)(void*, void*, const void* restrict, uint64_t, hfc_flag_et);
typedef int64_t     (BUFFER_COPY_TO)(void*, const void*, void* restrict, uint64_t, hfc_flag_et);
typedef void *      (BUFFER_IMPORT_VIRTUAL)(void*, buffer_type_et, uint64_t, void* restrict);
typedef void *      (BUFFER_MAP)(void*, void*, memory_map_et);
typedef void        (BUFFER_RELEASE)(void*, void*);
typedef int32_t     (BUFFER_TO_BUFFER_RECT_COPY)(void*, const void*, void*, uint64_t, uint64_t, uint64_t, hfc_flag_et);
typedef void        (BUFFER_UNMAP)(void*, void*, void*);
typedef float       (CLOCK_STACK_POP)(void*);
typedef void        (CLOCK_STACK_PUSH)(void*);
typedef void*       (COMPUTE_ACQUIRE)(hetero_so_puts_ft);
typedef void*       (COMPUTE_DUPLICATE)(const void*);
typedef void        (COMPUTE_RELEASE)(void*);
typedef int64_t     (CONVERT_GRAY_TO_RGB)(void*, const void*, const void*, void*, uint32_t, uint32_t, hfc_flag_et);
typedef int64_t     (CONVERT_RGB_TO_BAYER)(void*, const void*, void*, uint32_t, uint32_t, bayer_type_et, hfc_flag_et);
typedef int64_t     (CONVERT_RGB_TO_GRAY)(void*, const void*, void*, uint32_t, uint32_t, hfc_flag_et);
typedef int64_t     (CONVERT_RGB_TO_YUV)(void*, const void*, void*, uint32_t, uint32_t, e_yuv_format_et, hfc_flag_et);
typedef int64_t     (CONVERT_YUV_TO_GRAY_RGB)(void*, const void*, void*, void*, uint32_t, uint32_t, e_yuv_format_et, hfc_flag_et);
typedef int64_t     (COOPERATE_COPY_RGB)(uint64_t, uint64_t, uint64_t, const uint8_t* restrict, uint8_t* restrict, uint8_t* restrict, uint64_t);
typedef int64_t     (COOPERATE_DECODE_YUV)(uint64_t, uint64_t, uint64_t, e_yuv_format_et, const uint8_t* restrict, uint8_t* restrict, uint8_t* restrict, uint64_t);
typedef int64_t     (COOPERATE_ENCODE_YUV)(uint64_t, uint64_t, e_yuv_format_et, const uint16_t* restrict, const uint8_t* restrict, uint8_t* restrict, uint64_t);
typedef int64_t     (COOPERATE_OVERLAP_OSD)(uint64_t, uint64_t, const uint16_t* restrict, const uint8_t* restrict, uint8_t* restrict, uint64_t);
typedef int64_t     (COOPERATE_SWAP_ENDING)(uint64_t, const uint8_t* restrict, uint8_t* restrict, uint64_t);
typedef int32_t     (DARK_FILTER_WITH_IMAGE)(void *, const void *, void *, uint32_t, hfc_flag_et);
typedef void        (ENHANCE_FINISH)(void*, void*);
typedef int32_t     (ENHANCE_GRAY)(void*, void*, const uint8_t* restrict, void*, float, hfc_flag_et);
typedef void*       (ENHANCE_PREPARE)(void*, uint32_t, uint32_t, uint32_t);
typedef int32_t     (ENHANCE_RGB)(void*, void*, const uint8_t* restrict, void*, void*, float, hfc_flag_et);
typedef int32_t     (EXTENT_GRAY_TO_RGB)(void*, const void*, const void*, void*, uint32_t, uint32_t, uint32_t, uint32_t, hfc_flag_et);
typedef int32_t     (EXTENT_RGB_TO_RGB)(void*, const void*, void*, uint32_t, uint32_t, uint32_t, uint32_t, hfc_flag_et);
typedef int32_t     (FFT_1D_CAL)(void *, void *, const void *, void *, uint64_t, uint64_t, hfc_flag_et);
typedef void        (FFT_FINISH)(void *, void *);
typedef void *      (FFT_PREPARE)(void *, uint64_t);
typedef uint32_t    (GET_PIXEL_WIDTH)(pixel_channel_et, pixel_data_et);
typedef void *      (IMAGE_ACQUIRE)(void *, image_type_et, uint32_t, uint32_t, pixel_channel_et, pixel_data_et);
typedef void *      (IMAGE_BUILD_FROM_BUFFER)(void *, void *, image_type_et, uint32_t, uint32_t, pixel_channel_et, pixel_data_et);
typedef int64_t     (IMAGE_COPY_FROM)(void*, void*, const void* restrict, uint64_t, uint64_t, hfc_flag_et);
typedef int64_t     (IMAGE_COPY_TO)(void*, const void*, void* restrict, uint64_t, uint64_t, hfc_flag_et);
typedef void *      (IMAGE_MAP)(void*, void*, memory_map_et, uint32_t*);
typedef void        (IMAGE_RELEASE)(void *, void *);
typedef void        (IMAGE_UNMAP)(void *, void *, void *);
typedef int32_t     (OVERLAPPED_OSD_COMPRESS)(void*, const uint16_t* restrict, void*, uint32_t, uint32_t, uint32_t);
typedef int32_t     (OVERLAPPED_OSD_FUNC)(void*, const void*, void*, uint32_t, uint32_t, uint32_t, hfc_flag_et);
typedef int32_t     (PERSTRANS_WITH_EXPAND)(void*, void*, void*, const trans_mat_st*, double, uint64_t, hfc_flag_et);
typedef int32_t     (PERSTRANS_WITH_QUADCORD)(void*, void*, void*, const quad_cord_st*, const quad_cord_st*, uint32_t, uint64_t, hfc_flag_et);
typedef int32_t     (PERSTRANS_WITH_TRANSMAT)(void*, void*, void*, const trans_mat_st*, uint32_t, uint64_t, hfc_flag_et);
typedef int32_t     (PRESCALE_2)(void*, const void*, void*, uint64_t, uint64_t, date_from_to_et, hfc_flag_et);
typedef int32_t     (RESIZE_FROM_EXTRACTED)(void *, const void *, void *, float, float, float, float, uint32_t, uint32_t, hfc_flag_et);
typedef int32_t     (RESIZE_THEN_DRAW_TO)(void *, const void *, void *, uint32_t, uint32_t, uint32_t, uint32_t, hfc_flag_et);
typedef int32_t     (RESIZE_WITH_IMAGE_DARK)(void *, const void *, void *, void *, hfc_flag_et);
typedef int32_t     (SHITOMAS_CORNERS)(void *, void *, const void *, uint32_t *, const area_u16_st *, uint32_t);
typedef void        (SHITOMAS_FINISH)(void *, void *);
typedef void *      (SHITOMAS_PREPARE)(void *, uint32_t, uint32_t, const area_u16_st *, uint32_t, uint32_t, float, float);
typedef void        (SIMP_OPTICAL_FINISH)(void*, void*);
typedef void *      (SIMP_OPTICAL_PREPARE)(void*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
typedef int32_t     (SIMP_OPTICAL_RELOAD)(void*, void*, hfc_flag_et);
typedef void        (SIMP_OPTICAL_RESET)(void*, void*);
typedef int32_t     (SIMP_OPTICAL_SHOOT)(void*, void*, const void*, uint32_t*, float*, uint64_t);
typedef void        (WAVELET_FINISH)(void *, void *);
typedef int32_t     (WAVELET_FUSE)(void *, void *, const void *, void *, uint32_t, uint32_t, uint32_t, hfc_flag_et);
typedef void *      (WAVELET_PREPARE)(void *, uint32_t, uint32_t, uint32_t, uint32_t);

typedef struct
{
    // 非排序固定部分
    uint64_t                                library_version;
    void *                                  reserved_pointer[7];
    // 排序列表部分
    BUFFER_ACQUIRE *                        buffer_acquire;
    BUFFER_ACQUIRE_VIRTUAL *                buffer_acquire_virtual;
    BUFFER_COPY_FROM *                      buffer_copy_from;
    BUFFER_COPY_TO *                        buffer_copy_to;
    BUFFER_IMPORT_VIRTUAL *                 buffer_import_virtual;
    BUFFER_MAP *                            buffer_map;
    BUFFER_RELEASE *                        buffer_release;
    BUFFER_TO_BUFFER_RECT_COPY *            buffer_to_buffer_rect_copy;
    BUFFER_UNMAP *                          buffer_unmap;
    CLOCK_STACK_POP *                       clock_stack_pop;
    CLOCK_STACK_PUSH *                      clock_stack_push;
    COMPUTE_ACQUIRE *                       compute_acquire;
    COMPUTE_DUPLICATE *                     compute_duplicate;
    COMPUTE_RELEASE *                       compute_release;
    CONVERT_GRAY_TO_RGB *                   convert_gray_to_rgb;
    CONVERT_RGB_TO_BAYER *                  convert_rgb_to_bayer;
    CONVERT_RGB_TO_GRAY *                   convert_rgb_to_gray;
    CONVERT_RGB_TO_YUV *                    convert_rgb_to_yuv;
    CONVERT_YUV_TO_GRAY_RGB *               convert_yuv_to_rgb;
    COOPERATE_COPY_RGB *                    cooperate_copy_rgb24;
    COOPERATE_DECODE_YUV *                  cooperate_decode_yuv;
    COOPERATE_ENCODE_YUV *                  cooperate_encode_yuv;
    COOPERATE_OVERLAP_OSD *                 cooperate_overlap_osd;
    COOPERATE_SWAP_ENDING *                 cooperate_swap_ending;
    DARK_FILTER_WITH_IMAGE *                dark_filter_with_image;
    ENHANCE_FINISH *                        enhance_finish;
    ENHANCE_GRAY *                          enhance_gray;
    ENHANCE_PREPARE *                       enhance_prepare;
    ENHANCE_RGB *                           enhance_rgb;
    EXTENT_GRAY_TO_RGB *                    extent_quad_gray_to_rgb;
    EXTENT_RGB_TO_RGB *                     extent_quad_rgb_to_rgb;
    EXTENT_GRAY_TO_RGB *                    extent_self_gray_to_rgb;
    EXTENT_RGB_TO_RGB *                     extent_self_rgb_to_rgb;
    FFT_1D_CAL *                            fft_1d_backword;
    FFT_1D_CAL *                            fft_1d_forword;
    FFT_FINISH *                            fft_finish;
    FFT_PREPARE *                           fft_prepare;
    GET_PIXEL_WIDTH *                       get_pixel_width;
    IMAGE_ACQUIRE *                         image_acquire;
    IMAGE_BUILD_FROM_BUFFER *               image_build_from_buffer;
    IMAGE_COPY_FROM *                       image_copy_from;
    IMAGE_COPY_TO *                         image_copy_to;
    IMAGE_MAP *                             image_map;
    IMAGE_RELEASE *                         image_release;
    IMAGE_UNMAP *                           image_unmap;
    OVERLAPPED_OSD_COMPRESS *               overlapped_osd_compress;
    OVERLAPPED_OSD_FUNC *                   overlapped_osd_gray;
    OVERLAPPED_OSD_FUNC *                   overlapped_osd_rgb;
    OVERLAPPED_OSD_FUNC *                   overlapped_osd_rgb_alpha;
    PERSTRANS_WITH_EXPAND *                 perstrans_with_expand;
    PERSTRANS_WITH_QUADCORD *               perstrans_with_quadcord;
    PERSTRANS_WITH_TRANSMAT *               perstrans_with_transmat;
    PRESCALE_2 *                            prescale_2;
    PRESCALE_2 *                            prescale_2_horizental;
    RESIZE_FROM_EXTRACTED *                 resize_from_extracted;
    RESIZE_THEN_DRAW_TO *                   resize_then_draw_to;
    RESIZE_WITH_IMAGE_DARK *                resize_with_image_dark;
    SHITOMAS_CORNERS *                      shitomas_corners;
    SHITOMAS_FINISH *                       shitomas_finish;
    SHITOMAS_PREPARE *                      shitomas_prepare;
    SIMP_OPTICAL_FINISH *                   simp_optical_finish;
    SIMP_OPTICAL_PREPARE *                  simp_optical_prepare;
    SIMP_OPTICAL_RELOAD *                   simp_optical_reload;
    SIMP_OPTICAL_RESET *                    simp_optical_reset;
    SIMP_OPTICAL_SHOOT *                    simp_optical_shoot;
    WAVELET_FINISH *                        wavelet_finish;
    WAVELET_FUSE *                          wavelet_fuse;
    WAVELET_PREPARE *                       wavelet_prepare;
}
hetero_world_st;

/*
 * 功能：
 *   用于初始化时获取所有库函数。
 *
 * 返回值：
 *   0 表示成功，-1 表示版本不匹配。
 */
int32_t
path_to_hetero(
    /* 输入，库函数访问指针集 */
    hetero_world_st* phw
);

/* 定义显式加载时的动态库导出函数指针 */
typedef int32_t (PATH_TO_HETERO)(hetero_world_st *);

#ifdef __cplusplus
}
#endif

#endif // HETERO_INTERFACE_202104151110_H
