cmake_minimum_required(VERSION 3.5.1)

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER aarch64-linux-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-g++)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# 项目信息
project(sdi-pal_pro)

# 设置可执行文件目标
set(TARGETS "sdi-pal_pro")

set(CMAKE_CXX_STANDARD 14)

set(CMAKE_CXX_FLAGS "-O0 -g -std=c++11 -Wno-error")

add_definitions(-D_GNU_SOURCE -DLINUX)

include_directories(
	${CMAKE_CURRENT_LIST_DIR}/include/
	${CMAKE_CURRENT_LIST_DIR}/include/opencv4/

	${CMAKE_CURRENT_LIST_DIR}/include/UCGUI/
	${CMAKE_CURRENT_LIST_DIR}/include/UCGUI/Interface/
	${CMAKE_CURRENT_LIST_DIR}/include/UCGUI/ucGUI/
	${CMAKE_CURRENT_LIST_DIR}/include/UCGUI/ucGUI/Config/
	${CMAKE_CURRENT_LIST_DIR}/include/UCGUI/ucGUI/GUI/Core/
	${CMAKE_CURRENT_LIST_DIR}/include/UCGUI/ucGUI/GUI/MultiLayer/
	${CMAKE_CURRENT_LIST_DIR}/include/UCGUI/ucGUI/GUI/Widget/
	${CMAKE_CURRENT_LIST_DIR}/include/UCGUI/ucGUI/GUI/WM/

	${CMAKE_CURRENT_LIST_DIR}/src/gpu/
	${CMAKE_CURRENT_LIST_DIR}/include/rga/
	${CMAKE_CURRENT_LIST_DIR}/include/mpp/
	${CMAKE_CURRENT_LIST_DIR}/src/common/
	${CMAKE_CURRENT_LIST_DIR}/src/system/
	${CMAKE_CURRENT_LIST_DIR}/src/
	${CMAKE_CURRENT_LIST_DIR}/src/log/
	${CMAKE_CURRENT_LIST_DIR}/src/rga/
	${CMAKE_CURRENT_LIST_DIR}/src/mpp/
	${CMAKE_CURRENT_LIST_DIR}/src/mpp/utils/
	${CMAKE_CURRENT_LIST_DIR}/src/mpp/osal/inc	
	${CMAKE_CURRENT_LIST_DIR}/src/mpp/base/
	${CMAKE_CURRENT_LIST_DIR}/src/pcie/
	${CMAKE_CURRENT_LIST_DIR}/src/fifo/

	${CMAKE_CURRENT_LIST_DIR}/ffmpeg_include/
	${CMAKE_CURRENT_LIST_DIR}/ffmpeg_include/libavcodec
	${CMAKE_CURRENT_LIST_DIR}/ffmpeg_include/libavdevice
	${CMAKE_CURRENT_LIST_DIR}/ffmpeg_include/libavfilter
	${CMAKE_CURRENT_LIST_DIR}/ffmpeg_include/libavformat
	${CMAKE_CURRENT_LIST_DIR}/ffmpeg_include/libavutil
	${CMAKE_CURRENT_LIST_DIR}/ffmpeg_include/libpng16
	${CMAKE_CURRENT_LIST_DIR}/ffmpeg_include/libpostproc
	${CMAKE_CURRENT_LIST_DIR}/ffmpeg_include/libswresample
	${CMAKE_CURRENT_LIST_DIR}/ffmpeg_include/libswscale
	${CMAKE_CURRENT_LIST_DIR}/ffmpeg_include/libswscale
	${CMAKE_CURRENT_LIST_DIR}/ffmpeg_include/libyasm
)

link_directories(	
	${CMAKE_CURRENT_LIST_DIR}/lib/
	${CMAKE_CURRENT_LIST_DIR}/lib/rockchip/
	${CMAKE_CURRENT_LIST_DIR}/lib/opencv/
	${CMAKE_CURRENT_LIST_DIR}/include/UCGUI/
	${CMAKE_CURRENT_LIST_DIR}/lib/drm/
)
# 查找src目录下的所有源文件
# 并将名称保存到 DIR_SRCS 变量
aux_source_directory(./src DIR_SRCS)
aux_source_directory(./src/log DIR_SRCS_LOG)
aux_source_directory(./src/system DIR_SRCS_SYSTEM)
aux_source_directory(./src/rga DIR_SRCS_RGA)
aux_source_directory(./src/mpp DIR_SRCS_MPP)
aux_source_directory(./src/mpp/utils DIR_SRCS_MPP_UTILS)
aux_source_directory(./src/common DIR_SRCS_COMMON)
aux_source_directory(./src/pcie DIR_SRCS_PCIE)
aux_source_directory(./src/fifo DIR_SRCS_FIFO)
aux_source_directory(./src/gpu DIR_SRCS_GPU)

# 指定生成目标
add_executable(${TARGETS} ${DIR_SRCS} ${DIR_SRCS_LOG} ${DIR_SRCS_SYSTEM} ${DIR_SRCS_RGA} ${DIR_SRCS_MPP} ${DIR_SRCS_MPP_UTILS} ${DIR_SRCS_COMMON} ${DIR_SRCS_PCIE} ${DIR_SRCS_FIFO} ${DIR_SRCS_GPU})

target_link_libraries(${TARGETS}      
		opencv_core
		opencv_calib3d
		opencv_imgproc
		opencv_highgui
		opencv_photo
		opencv_imgcodecs
		opencv_videoio
		opencv_dnn
		opencv_features2d
		opencv_flann
		opencv_ml
		opencv_objdetect
		opencv_video     		

		avformat
		avcodec
		avutil
		swresample
		x264
		xvidcore

		drm
		rockchip_mpp
		hetero_rk3588
		ucgui_rk3588

		pthread
		m
		rga
		rt
		dl		
		mpp
)
