#include "osd_engine.h"
#include "log/log.h"
#include "GUI.h"
#include "GUI_Init.h"
#include "lcd_init.h"
#include "data_engine.h"
// #include "dp_engine.h"
#include "gpu_interface.h"
#include "pcie_reg_rw.h"
#include "xutils.h"
#include "a818_error.h"
#include "mpi_decode.h"
#include "crop_engine.h"

#define ringbuffer_destory ringbuffer_deinit

static osd_data_stream_t *gs_osd_stream = NULL;
osd_data_stream_t* GetOsdDataStream()
{
  return gs_osd_stream;
}

struct osd_gui_font_info {
  unsigned char fony_size;
  const GUI_FONT GUI_UNI_PTR * pNewFont;
};

#define SET_OSD_GUI_FONT(size, Font) { 	             \
	.fony_size = size,   				                       \
	.pNewFont = &Font,					                      \
}

struct osd_gui_font_info gui_font[OSD_GUI_FONT_FORMAT_MAX][OSD_GUI_FONT_SIZE_MAX] = {
  {
    SET_OSD_GUI_FONT(0, GUI_FontHZ8x8_songti),
    SET_OSD_GUI_FONT(1, GUI_FontHZ16x16_songti),
    SET_OSD_GUI_FONT(2, GUI_FontHZ24x24_songti),
    SET_OSD_GUI_FONT(3, GUI_FontHZ32x32_songti),
    SET_OSD_GUI_FONT(4, GUI_FontHZ40x40_songti),
    SET_OSD_GUI_FONT(5, GUI_FontHZ48x48_songti),
    SET_OSD_GUI_FONT(6, GUI_FontHZ56x56_songti),
    SET_OSD_GUI_FONT(7, GUI_FontHZ64x64_songti),
    SET_OSD_GUI_FONT(8, GUI_FontHZ96x96_songti),
  },

  {
    SET_OSD_GUI_FONT(0, GUI_FontHZ8x8_heiti),
    SET_OSD_GUI_FONT(1, GUI_FontHZ16x16_heiti),
    SET_OSD_GUI_FONT(2, GUI_FontHZ24x24_heiti),
    SET_OSD_GUI_FONT(3, GUI_FontHZ32x32_heiti),
    SET_OSD_GUI_FONT(4, GUI_FontHZ40x40_heiti),
    SET_OSD_GUI_FONT(4, GUI_FontHZ48x48_heiti),
    SET_OSD_GUI_FONT(4, GUI_FontHZ56x56_heiti),
    SET_OSD_GUI_FONT(4, GUI_FontHZ64x64_heiti),
    SET_OSD_GUI_FONT(4, GUI_FontHZ96x96_heiti),
  },

  {
    SET_OSD_GUI_FONT(0, GUI_FontHZ8x8_fangsong),
    SET_OSD_GUI_FONT(1, GUI_FontHZ16x16_fangsong),
    SET_OSD_GUI_FONT(2, GUI_FontHZ24x24_fangsong),
    SET_OSD_GUI_FONT(3, GUI_FontHZ32x32_fangsong),
    SET_OSD_GUI_FONT(4, GUI_FontHZ40x40_fangsong),
    SET_OSD_GUI_FONT(4, GUI_FontHZ48x48_fangsong),
    SET_OSD_GUI_FONT(4, GUI_FontHZ56x56_fangsong),
    SET_OSD_GUI_FONT(4, GUI_FontHZ64x64_fangsong),
    SET_OSD_GUI_FONT(4, GUI_FontHZ96x96_fangsong),
  },

};

static void osd_image_to_queue(unsigned char chn, image_frame_info_t *frame_info)
{
  image_frame_info_t event_param, tmp_param;
  ring_buffer_t *rb = gs_osd_stream->chn_info[chn].osd_rb;
	
	if (ringbuffer_is_full(rb)) {				
    SLOGI("[Debug] %s ringbuffer_is_full", __func__);
	}
	memcpy(&event_param,frame_info,sizeof(image_frame_info_t));
    		
	ringbuffer_put(rb, (uint8_t *)&event_param, sizeof(event_param));
}


void gui_osd_src_image(unsigned char chn)
{
		char buf[128];
		time_t timep;
		struct tm tm_p;
		int position_x;
		int position_y;
		unsigned int offset;
		unsigned int y_start;
		unsigned int chn_width;
		unsigned int chn_height;
		unsigned int chn_fps;

    data_stream_t* data_stream = data_stream_get();
    image_chn_info_t* image_chn_info = data_stream->image_chn_info[chn];
    fpga_chn_image_info_t *chn_image = data_stream->chn_image[chn];

    rga_param_t rga_info = image_chn_info->rga_info;
    osd_image_info_t* osd_info = &image_chn_info->osd_info;

		/*获取通道的宽、高、以及帧率*/
		chn_width = rga_info.out_width;
		chn_height = rga_info.out_height;
    if(chn < MPP_ENCODE_NTHREAD_MAX)
		  chn_fps = data_stream->mpp_encode_info[chn%MPP_ENCODE_NTHREAD_MAX]->fps;
    else
      chn_fps = data_stream->mpp_decode_info[chn%MPP_ENCODE_NTHREAD_MAX]->fps;

		/*获取到对应通道的画布的位置*/
		y_start = rga_info.rga_chn * VIDEO_OSD_HIGHT;

    pthread_mutex_lock(&data_stream->osd_lock);
		/*根据不同的分辨率设置时帧标显示的大小*/
		if(chn_height <= 256){
			GUI_SetFont(&GUI_FontHZ16x16_songti);
			offset = 16;
		} else if(chn_height <= 1024){
			GUI_SetFont(&GUI_FontHZ24x24_songti);
			offset = 24;
		} else if(chn_height <= 2048){
			GUI_SetFont(&GUI_FontHZ32x32_songti);
			offset = 32;
		} else if(chn_height <= 3184){
			GUI_SetFont(&GUI_FontHZ40x40_songti);
			offset = 40;
		} else {
			GUI_SetFont(&GUI_FontHZ64x64_songti);
			offset = 64;
		}

		/*根据叠加的位置获取到叠加的起始点坐标*/
		switch(osd_info->time_position){
			case 0:
				position_x = offset;
				position_y = y_start + offset;
				break;
			case 1:
				position_x = chn_width - (offset*13);
				position_y = y_start + offset;
				break;
			case 2:
				position_x = offset;
				position_y = y_start + chn_height - (offset*4);
				break;
			case 3:
				position_x = chn_width - (offset*13);
				position_y = y_start + chn_height - (offset*4);
				break;
			default:
				position_x = 0;
				position_y = y_start;
				break;

		}

		GUI_SetColor(osd_info->font_color);

		/*进行叠加时针标*/
		GUI_SetPenSize(2);
		memset(buf, 0x0, sizeof(buf));
		sprintf(buf,"chn%d_%dx%d@%d", (rga_info.rga_chn), \
			chn_width, chn_height , chn_fps);
		GUI_DispStringAt(buf, position_x, position_y);

		memset(buf, 0x0, sizeof(buf));
		sprintf(buf, "ID:%d", chn_image->frame_num);
		GUI_DispStringAt(buf, position_x, position_y+offset);
		chn_image->frame_num +=1;

		memset(buf, 0x0, sizeof(buf));
		time(&timep);
		localtime_r(&timep, &tm_p);
		sprintf(buf, "time:%d-%d-%d %d:%d:%d", (1900 + tm_p.tm_year), ( 1 + tm_p.tm_mon), tm_p.tm_mday, \
			tm_p.tm_hour, tm_p.tm_min, tm_p.tm_sec);
		GUI_DispStringAt(buf, position_x, position_y + (offset*2));
		GUI_SetPenSize(1);
    pthread_mutex_unlock(&data_stream->osd_lock);
}


void draw_wujiaoxing(int x, int y, int r)
{
   GUI_POINT aPoints[10];
   float pi = 3.1415926;

    //左上点
    aPoints[0].x = x - (short)(r * sin(2*pi/5));
    aPoints[0].y = y - (short)(r * cos(2*pi/5));

    aPoints[1].x = x - (short)(r * cos(2*pi/5) * tan(pi/5));
    aPoints[1].y = y - (short)(r * cos(2*pi/5));

    //顶点
    aPoints[2].x  = x;
    aPoints[2].y = y-r;

    aPoints[3].x = x + (short)(r * cos(2*pi/5) * tan(pi/5));
    aPoints[3].y = y - (short)(r * cos(2*pi/5));

    //右上点
    aPoints[4].x  = x + (short)(r * sin(2 * pi/5));
    aPoints[4].y =  y - (short)(r * cos(2 * pi/5));

    aPoints[5].x = x + (short)(r * cos(2*pi/5) * acos(pi/5) * sin(2 * pi/5));
    aPoints[5].y = y + (short)(r * cos(2*pi/5) * acos(pi/5) * cos(2 * pi/5));

    //右下点
    aPoints[6].x  = x + (short)(r * sin(pi/5));
    aPoints[6].y = y + (short)(r * cos(pi/5));

    aPoints[7].x  = x ;
    aPoints[7].y = y + (short)(r * cos(2*pi/5)* acos(pi/5) );

  //左下点
    aPoints[8].x  = x - (short)(r * sin(pi/5));
    aPoints[8].y = y + (short)(r * cos(pi/5));
  
    aPoints[9].x = x - (short)(r * cos(2*pi/5)* acos(pi/5) * sin(2 * pi/5));
    aPoints[9].y = y + (short)(r * cos(2*pi/5) * acos(pi/5) * cos(2 * pi/5));

  GUI_FillPolygon(aPoints, 10, 0, 0);
  
}



void draw_sanjiao(int x, int y, int r)
{
    GUI_POINT aPoints[3];
    aPoints[0].x = x;
    aPoints[0].y = y-r;

    aPoints[1].x = x-r;
    aPoints[1].y = y+r;

    aPoints[2].x = x+r;
    aPoints[2].y = y+r;

    GUI_FillPolygon(aPoints, 3, 0, 0);
  
}


void gui_osd_draw_character(unsigned int x, unsigned int y, unsigned int r, unsigned char type)
{
    switch(type)
    {
      case 0:
        draw_wujiaoxing(x, y, r);
        break;
      case 1:
        draw_sanjiao(x,y,r);
        break;
    }
}

void gui_osd_stitchbase_image(unsigned char chn)
{
  unsigned int x, y, w, h,offset;
  unsigned char font_size;
  unsigned char num;
  string_info_t * info;
  unsigned char c_data;
  unsigned char s_data[3];
  unsigned int y_start;

  data_stream_t *data_stream = data_stream_get();
  fpga_chn_image_info_t* chn_image = data_stream->chn_image[chn];
  image_chn_info_t* image_chn_info = data_stream->image_chn_info[chn];
  osd_image_info_t * osd_info = &image_chn_info->osd_info;
  rga_param_t * rga_info = &image_chn_info->rga_info;
  y_start = rga_info->rga_chn * VIDEO_OSD_HIGHT;

  x = osd_info->x;
  y = osd_info->y + y_start;
  w = osd_info->x_off;
  h = osd_info->y_off;

  pthread_mutex_lock(&data_stream->osd_lock);
  // GUI_Clear();
  GUI_SetFont(gui_font[osd_info->font_format][osd_info->font_size].pNewFont);
  GUI_SetColor(osd_info->font_color);
  GUI_SetPenSize(osd_info->line_width);
  if(((osd_info->osd_type >> Shape_Osd) & 0x01))
  {
    /* 画图形 */
    switch(osd_info->shape){
      case STITCHBASE_OSD_ACROSS_SHAPE:
        GUI_DrawLine(x-w-1, y-1, x+w-1, y-1);
        break;
      case STITCHBASE_OSD_VERTICAL_SHAPE:
        GUI_DrawLine(x-1, y-h-1, x-1, y+h-1);
        break;
      case STITCHBASE_OSD_CROSS_SHAPE:
        GUI_DrawLine(x-w-1, y-1, x+w-1, y-1);
        GUI_DrawLine(x-1, y-h-1, x-1, y+h-1);
        break;
      case STITCHBASE_OSD_BLOCK_SHAPE:
        GUI_DrawLine(x-w-1, y-h-1, x+w-1, y-h-1);
        GUI_DrawLine(x-w-1, y+h-1, x+w-1, y+h-1);
        GUI_DrawLine(x-w-1, y-h-1, x-w-1, y+h-1);
        GUI_DrawLine(x+w-1, y-h-1, x+w-1, y+h-1);
        break;
      case STITCHBASE_OSD_ROUND_SHAPE:
        GUI_FillEllipse(x,y, w, h);
        GUI_SetColor(GUI_BLACK);
        GUI_FillEllipse(x,y, w-osd_info->line_width, h-osd_info->line_width);
        break;
      default:
        break;
    }
  }
  GUI_SetColor(osd_info->font_color);
  if((osd_info->data_size) && ((osd_info->osd_type >> Char_Osd) & 0x01)){
		GUI_DispStringAt(osd_info->data, osd_info->string_x, osd_info->string_y + y_start);
  }
  GUI_SetPenSize(1);
  pthread_mutex_unlock(&data_stream->osd_lock);
}

void chage_src_to_new(unsigned char *src, unsigned char *dec, int new_w, int new_h)
{
	int i = 0;
  if(new_w > VIDEO_OSD_WIDTH)
  {
    new_w = VIDEO_OSD_WIDTH;
  }
  if(new_h > VIDEO_OSD_HIGHT)
  {
    new_h = VIDEO_OSD_HIGHT;
  }
  // printf("[debug] %s %d new_w:%d, new_h:%d VIDEO_OSD_WIDTH:%d VIDEO_OSD_HIGHT:%d\n", __func__, __LINE__, new_w, new_h, VIDEO_OSD_WIDTH, VIDEO_OSD_HIGHT);
	for(i = 0; i < new_h; i++) {
    memcpy(dec + (i*new_w*2), src + (i *VIDEO_OSD_WIDTH*2), new_w*2);
    memset(src + (i *VIDEO_OSD_WIDTH*2), 0x0, new_w*2);
	}

	return ;
}

static void* image_over_thread(void* parameter)
{
  unsigned char	osd_chn = *((unsigned char*)parameter);
	struct timeval time;
  
	GPU_Frame_st* gpu_frame_stream = gpu_data_stream_get();
	gpu_image_info_t *gpu_image = &gpu_frame_stream->gpu_image[osd_chn];
	data_stream_t *data_stream = data_stream_get();
	Crop_Frame_st* crop_stream = crop_data_stream_get();
  fpga_chn_image_info_t* chn_image = data_stream->chn_image[osd_chn];
  image_chn_info_t* image_chn_info = data_stream->image_chn_info[osd_chn];
  osd_image_info_t* osd_info = &image_chn_info->osd_info;
  rga_param_t rga_info = image_chn_info->rga_info;

	image_frame_info_t frame_info = {0};
	image_frame_info_t osd_frame_info = {0};
	image_frame_info_t event_param = {0};
	void *out_gui_osd_addr = NULL;

  ring_buffer_t* image_over_rb = crop_stream->crop_data_rb[osd_chn];;
	ring_buffer_t* osd_data_rb = gpu_frame_stream->osd_data_rb[osd_chn];
	uint8_t* in_rgb_addr = NULL;
	uint8_t* out_yuv_addr = NULL;
	unsigned int width = image_chn_info->rga_info.osd_width;
	unsigned int height = image_chn_info->rga_info.osd_height;

  printf("chn:[%d] %s start\n", osd_chn, __func__);
  struct timeval start, end;
  long usec1;
	while(!gs_osd_stream->chn_info[osd_chn].over_flag){

		while(ringbuffer_is_empty(osd_data_rb)){
			time.tv_sec=0;
			time.tv_usec=(1000);
			select(0,NULL,NULL,NULL,&time);
			if(gs_osd_stream->chn_info[osd_chn].over_flag){
				goto stop;
			}
		}
    while (ringbuffer_is_full(image_over_rb))
    {
      // printf("image_over_rb chn:%d is_full\n", osd_chn);
			time.tv_sec=0;
			time.tv_usec=(1000);
			select(0,NULL,NULL,NULL,&time);
			if(gs_osd_stream->chn_info[osd_chn].over_flag){
				goto stop;
			}
    }
		ringbuffer_get(osd_data_rb, (uint8_t *)&frame_info, sizeof(image_frame_info_t));

		if(gpu_image->osd_rb_offset >= gpu_image->data_frame_max+1){
				gpu_image->osd_rb_offset = 0;
		}
		/*进行图像的叠加*/
		in_rgb_addr = gpu_image->in_rgb_buffer[frame_info.frame_offset].vir_addr;
    out_gui_osd_addr =  gpu_image->gui_osd_buffer[frame_info.frame_offset%(DATA_STREAM_FRAME_MAX + 1)];
		out_yuv_addr = gpu_image->out_yuv_buffer[gpu_image->osd_rb_offset];
#if OSD_DEBUG
    gettimeofday(&start, NULL);
#endif
    if(rga_info.out_color == 1)
    {
      if(osd_info->osd_type)
      {
        gpu_cooperate_overlap_osd(out_gui_osd_addr, in_rgb_addr, out_yuv_addr, width, height);
      }
      else
      {
        memcpy(out_yuv_addr, in_rgb_addr, width * height * 3);
      }
      event_param.frame_size = width*height*3;
    }
    else if(rga_info.out_color == 2)
    {
      if(osd_info->osd_type)
      {
        gpu_cooperate_encode_yuyv(out_gui_osd_addr, in_rgb_addr, out_yuv_addr, width, height);
      }
      else
      {
        gpu_cooperate_encode_yuyv(0, in_rgb_addr, out_yuv_addr, width, height);
      }
      event_param.frame_size = width*height*2;
    }
    else if(rga_info.out_color == 3)
    {
      if(osd_info->osd_type)
      {
        gpu_cooperate_encode_nv12(out_gui_osd_addr, in_rgb_addr, out_yuv_addr, width, height);
      }
      else
      {
        gpu_cooperate_encode_nv12(0, in_rgb_addr, out_yuv_addr, width, height);
      }
      event_param.frame_size = width*height*3/2;
    }
#if OSD_DEBUG
    gettimeofday(&end, NULL);
    usec1 = 1000000 * (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec);
    printf("gpu_cooperate .... cost time %ld us\n", usec1);
#endif
    // if(rga_info.out_color == 1)
    // {
    //   save_image_data(out_yuv_addr,width*height*3,"/tmp/osd_buf.raw");
    // }
    // else if (rga_info.out_color == 2)
    // {
    //   save_image_data(out_yuv_addr,width*height*2,"/tmp/osd_buf.raw");
    // }
    // else if (rga_info.out_color == 3)
    // {
    //   save_image_data(in_rgb_addr,width*height*3,"/tmp/in_rgb_addr.raw");
    //   save_image_data(out_yuv_addr,width*height*3/2,"/tmp/osd_buf.raw");
    // }
		/*放到编码的ring buffer中，后续进行裁剪*/
    if(!ringbuffer_is_full(image_over_rb)){
      event_param.frame_offset = gpu_image->osd_rb_offset;
      event_param.chn = osd_chn;
      ringbuffer_put(image_over_rb, (uint8_t *)&event_param, sizeof(event_param));
      gpu_image->osd_rb_offset += 1;
    }
    else
    {
      SLOGI("[Debug] %s ringbuffer_is_full\n", __func__);
    }
	}
stop:
  printf("chn:[%d] %s stop\n", osd_chn, __func__);
  pthread_exit(NULL);
}

static void* gui_paint_on_canvas_thread(void* parameter)
{
  unsigned char	over_chn = *((unsigned char*)parameter);
	SLOGI("[debug] %s %d chn:%d \n", __func__, __LINE__, over_chn);
	struct timeval time;
  
	GPU_Frame_st* gpu_frame_stream = gpu_data_stream_get();
	gpu_image_info_t *gpu_image = &gpu_frame_stream->gpu_image[over_chn];
	data_stream_t *data_stream = data_stream_get();
  fpga_chn_image_info_t* chn_image = data_stream->chn_image[over_chn];
  image_chn_info_t* image_chn_info = data_stream->image_chn_info[over_chn];
  osd_image_info_t* osd_info = &image_chn_info->osd_info;
  rga_param_t* rga_info = &image_chn_info->rga_info;

	image_frame_info_t frame_info = {0};
	image_frame_info_t event_param = {0};

  ring_buffer_t* osd_data_rb = gpu_frame_stream->osd_data_rb[over_chn];
	ring_buffer_t* resize_data_rb = chn_image->resize_data_rb;

	unsigned int over_width = rga_info->osd_width;
	unsigned int over_height = rga_info->osd_height;
  unsigned char* gui_osd_data = gs_osd_stream->gs_osd_data + (over_chn * (VIDEO_OSD_WIDTH*VIDEO_OSD_HIGHT*2));
  unsigned char* image_over_data = NULL;
  void *out_gui_osd_addr = NULL;

  printf("chn:[%d] %s start\n", over_chn, __func__);
  struct timeval start, end;
  long usec1;
	while(!gs_osd_stream->chn_info[over_chn].osd_flag){
		while(ringbuffer_is_empty(resize_data_rb)){
			time.tv_sec=0;
			time.tv_usec=(1000);
			select(0,NULL,NULL,NULL,&time);
			if(gs_osd_stream->chn_info[over_chn].osd_flag){
				goto stop;
			}
		}
    while(ringbuffer_is_full(osd_data_rb)){
      // printf("osd_data_rb chn:%d is_full\n", over_chn);
			time.tv_sec=0;
			time.tv_usec=(1000);
			select(0,NULL,NULL,NULL,&time);
			if(gs_osd_stream->chn_info[over_chn].osd_flag){
				goto stop;
			}
		}
		ringbuffer_get(resize_data_rb, (uint8_t *)&frame_info, sizeof(image_frame_info_t));

		/*
			判断编码图是不是需要叠加时针标，如果不叠加的话，直接编码输出，这样会减少耗时
		*/
    image_over_data = gpu_image->in_rgb_buffer[frame_info.frame_offset].vir_addr;
		if( osd_info->osd_type){
      if((osd_info->osd_type >> Image_Over) & 0x01)
      {
          for(int i = 0; i < image_chn_info->rga_info.image_y0; i++){
              memcpy(image_over_data + osd_info->image_y0*over_width*3 + osd_info->image_x0*3 + i * over_width * 3, gpu_image->over_image + image_chn_info->rga_info.image_w0*i*3, image_chn_info->rga_info.image_w0*3);
          }
      }
      if(((osd_info->osd_type >> Char_Osd) & 0x01) || ((osd_info->osd_type >> Shape_Osd) & 0x01))
      {
        gui_osd_stitchbase_image(over_chn);
      }
      if((osd_info->osd_type >> Time_Osd) & 0x01)
      {
        /*在画布上画时针标*/
        gui_osd_src_image(over_chn);
      }
      out_gui_osd_addr = gpu_image->gui_osd_buffer[frame_info.frame_offset%(DATA_STREAM_FRAME_MAX + 1)];
      /*裁剪出需要的画布*/
      chage_src_to_new(gui_osd_data, out_gui_osd_addr, over_width, over_height);
      // save_image_data(image_over_data, over_width * over_height * 3, "/tmp/image_over.raw");
      /*放到编码的ring buffer中，后续进行编码输出*/
      if(!ringbuffer_is_full(osd_data_rb)){
        event_param.frame_offset = frame_info.frame_offset;
        event_param.frame_size = over_width * over_height * 2;
        event_param.chn = over_chn;
        ringbuffer_put(osd_data_rb, (uint8_t *)&event_param, sizeof(event_param));
      }
      else
      {
        SLOGI("[Debug] %s ringbuffer_is_full\n", __func__);
      }
    }
  }
stop:
  printf("chn:[%d] %s stop\n", over_chn, __func__);
  return NULL;
}

int gui_osd_overlapped_chn_init(unsigned char chn)
{
  int ret;
  data_stream_t *data_stream = data_stream_get();
  fpga_chn_image_info_t* chn_image = data_stream->chn_image[chn];
  gs_osd_stream->chn_info[chn].chn = chn;
  chn_image->frame_num = 0;
  gs_osd_stream->chn_info[chn].over_flag = 0;
  gs_osd_stream->chn_info[chn].osd_flag = 0;

  ret = pthread_create(&gs_osd_stream->chn_info[chn].over_tid, NULL, &image_over_thread, &gs_osd_stream->chn_info[chn].chn);
  if (ret != 0) {
      printf("[ERROR] Create image_over_thread failed %d\n",__func__,ret);
      return A818_OSD_GPU_THREAD;
  }
  ret = pthread_create(&gs_osd_stream->chn_info[chn].osd_tid, NULL, &gui_paint_on_canvas_thread, &gs_osd_stream->chn_info[chn].chn);
  if (ret != 0) {
      printf("[ERROR] Create gui_paint_on_canvas_thread failed %d\n",__func__,ret);
      return A818_OSD_GPU_THREAD;
  }
  SLOGI("[Debug] %s chn:%d \n", __func__, chn);
  return 0;
}

void gpu_osd_chn_image_destory(unsigned char chn)
{
    int index;
    GPU_Frame_st* frame_stream = gpu_data_stream_get();
    gpu_image_info_t *gpu_image = &frame_stream->gpu_image[chn];

    for(index = 0; index < gpu_image->data_frame_max + 1; index++)
    {
      if(gpu_image->gui_osd_buffer[index]){
          free(gpu_image->gui_osd_buffer[index]);
          gpu_image->gui_osd_buffer[index] = NULL;
      }
    }

    for(index = 0; index < 2*(gpu_image->data_frame_max + 1); index++){
      if(gpu_image->in_rgb_buffer[index].vir_addr){
        rga_release_frame_image2(&gpu_image->in_rgb_buffer[index]);
        gpu_image->in_rgb_buffer[index].vir_addr = NULL;
      }
    }

    if(gpu_image->over_image){
      free(gpu_image->over_image);
      gpu_image->over_image = NULL;
    }

    if(gpu_image->rga_vir_addr)
    {
      free(gpu_image->rga_vir_addr);
      gpu_image->rga_vir_addr = NULL;
      gpu_image->rga_addr_offset_max = 0;
      gpu_image->rga_addr_offset = 0;
    }
    SLOGI("[DEBUG] %s chn:%d end \n", __func__, chn);
    return ;
}

/* 
osd相关资源的初始化，GPU进行字符叠加相关的资源的申请
*/
int gpu_osd_chn_image_init(unsigned char chn)
{
    int ret, i;
  	unsigned int buffer_len;
    unsigned int ret_val = 0;
    int index = 0;
    data_stream_t* data_stream = data_stream_get();
    image_chn_info_t* image_chn_info = data_stream->image_chn_info[chn];
    rga_param_t rga_info = image_chn_info->rga_info;
    osd_image_info_t osd_info = image_chn_info->osd_info;

    GPU_Frame_st* frame_stream = gpu_data_stream_get();
    gpu_image_info_t *gpu_image = &frame_stream->gpu_image[chn];
    memset(gpu_image, 0x0, sizeof(gpu_image_info_t));
    gpu_image->osd_rb_offset = 0;
    gpu_image->crop_rb_offset = 0;

    u_int32_t osd_width = rga_info.osd_width;
    u_int32_t osd_height = rga_info.osd_height;

    gpu_image->data_frame_max = 0;
    // if((osd_width >= VIDEO_FRAME_WIDTH_4k) && (osd_height >= VIDEO_FRAME_HEIGHT_4k)){
    //     gpu_image->data_frame_max = DATA_4K_STREAM_FRAME_MAX;
    // } else {
        gpu_image->data_frame_max = DATA_STREAM_FRAME_MAX;
    // }
    /**************************************************************/

    gpu_image->rga_addr_offset_max = (4*(gpu_image->data_frame_max + 1)*3*osd_width* osd_height + 4096);
    gpu_image->rga_addr_offset = 0;
    gpu_image->rga_vir_addr = (unsigned char*)malloc(gpu_image->rga_addr_offset_max);
    if(!gpu_image->rga_vir_addr){
        printf("chn:%d rga_vir_addr malloc error \n",chn);
        ret_val = A818_DATA_CHN_IMAGE_MALLOC;
        goto err_4;
    }

  
    gpu_image->over_image = (uint8_t*)aligned_alloc(4096, rga_info.image_w0*rga_info.image_y0*3);
    if(gpu_image->over_image == NULL){
        printf("[ERROR] osd gpu buffer(%d) acquire error \n", index);
        ret = A818_GUI_OSD_DATA_BUFFER;
        goto err_3;
    }

    for(index = 0; index < (2*(gpu_image->data_frame_max + 1)); index++)
    {
      buffer_len = get_rga_vir_buffer_len(RK_FORMAT_RGB_888, osd_width,osd_height);
      ret = rga_comm_create_image2(&gpu_image->in_rgb_buffer[index],gpu_image->rga_vir_addr +  gpu_image->rga_addr_offset,RK_FORMAT_RGB_888, \
          rga_info.resize_width,rga_info.resize_height, osd_width, osd_height);
      if(ret != 0){
          printf("[ERROR] int rga rgb888_data[%d] error \n", index);
          ret_val = A818_DATA_RGB_DATA;
          ret_val |= (ret << 8);
          goto err_1;
      }
      gpu_image->rga_addr_offset += buffer_len;
      if(gpu_image->rga_addr_offset >= gpu_image->rga_addr_offset_max){
          SLOGI("[ERROR] int rga rgb888_data[%d] buffer_len:%d offset_max:%d rga_addr_offset error \n", index, buffer_len, gpu_image->rga_addr_offset_max);
          ret_val = A818_DATA_RGA_ADDR_OFFSET;
          goto err_1;
      }
    }

    for(index = 0; index < gpu_image->data_frame_max + 1; index++)
    {
        gpu_image->gui_osd_buffer[index] = (uint8_t*)aligned_alloc(4096, osd_width * osd_height*2);
        if(gpu_image->gui_osd_buffer[index] == NULL){
            printf("[ERROR] osd gpu buffer(%d) acquire error \n", index);
            ret = A818_GUI_OSD_DATA_BUFFER;
            goto err_2;
        }

        gpu_image->out_yuv_buffer[index] = (uint8_t*)aligned_alloc(4096, osd_width * osd_height * 2);//rga_info.pixel_byte);
        if(gpu_image->out_yuv_buffer[index] == NULL){
            printf("[ERROR] osd gpu buffer(%d) acquire error \n", index);
            ret = A818_GUI_OSD_DATA_BUFFER;
            goto err_2;
        }
    }
    // gpu_image->frame_size = rga_info.src_width * rga_info.src_height *2;
    // gpu_image->rb_offset = 0;
    // gpu_image->data_offset = 0;

	ret = gui_osd_overlapped_chn_init(chn);
	if(ret != 0){
		SLOGI("gui_osd_overlapped_chn_init error \n");
		goto err_2;
	}
  return A818_SUCCESS;
err_2:
    for(index = 0; index < gpu_image->data_frame_max + 1; index++)
    {
        if(gpu_image->out_yuv_buffer[index]){
            free(gpu_image->out_yuv_buffer[index]);
            gpu_image->out_yuv_buffer[index] = NULL;
        }

        if(gpu_image->gui_osd_buffer[index]){
            free(gpu_image->gui_osd_buffer[index]);
            gpu_image->gui_osd_buffer[index] = NULL;
        }
    }
err_1:
    for(index = 0; index < 2*(gpu_image->data_frame_max + 1); index++){
      if(gpu_image->in_rgb_buffer[index].vir_addr){
        rga_release_frame_image2(&gpu_image->in_rgb_buffer[index]);
        gpu_image->in_rgb_buffer[index].vir_addr = NULL;
      }
    }
err_3:
    if(gpu_image->over_image){
        free(gpu_image->over_image);
        gpu_image->over_image = NULL;
    }
err_4:
    if(gpu_image->rga_vir_addr)
    {
      free(gpu_image->rga_vir_addr);
      gpu_image->rga_vir_addr = NULL;
      gpu_image->rga_addr_offset_max = 0;
      gpu_image->rga_addr_offset = 0;
    }
    return ret;
}

int osd_ringbuffer_init(void)
{
  int ret;
  int chn;
  for(chn = 0; chn < DATA_CHANNEL_NUM_MAX;chn++){
    gs_osd_stream->chn_info[chn].osd_rb = ringbuffer_create(RINGBUFFER_FRAME_MAX*sizeof(image_frame_info_t),sizeof(image_frame_info_t));
    if (!gs_osd_stream->chn_info[chn].osd_rb) {
        printf("[ERROR] create osd rb buffer failed\n");
        ret = A818_OSD_RB;
        goto err_1;
    }
  }

  return A818_SUCCESS;

err_1:
  for(chn = 0; chn < DATA_CHANNEL_NUM_MAX; chn++){
    if(gs_osd_stream->chn_info[chn].osd_rb){
      ringbuffer_destory(gs_osd_stream->chn_info[chn].osd_rb);
    }
  }
  return ret;
}

void gui_osd_overlapped_chn_destory(unsigned char chn)
{
  struct timeval time_val;
	GPU_Frame_st* gpu_frame_stream = gpu_data_stream_get();
	gpu_image_info_t *gpu_image = &gpu_frame_stream->gpu_image[chn];
	data_stream_t *data_stream = data_stream_get();
  fpga_chn_image_info_t* chn_image = data_stream->chn_image[chn];
  ring_buffer_t* osd_data_rb = gpu_frame_stream->osd_data_rb[chn];
	ring_buffer_t* resize_data_rb = chn_image->resize_data_rb;
  image_frame_info_t frame_info = {0};

  while(!ringbuffer_is_empty(resize_data_rb)){
    ringbuffer_get(resize_data_rb, (uint8_t *)&frame_info, sizeof(image_frame_info_t));
    // printf("InPutRingBuffer is_empty\n");
    time_val.tv_sec=0;
    time_val.tv_usec=(1000);
    select(0,NULL,NULL,NULL,&time_val);
  }
  while(!ringbuffer_is_empty(osd_data_rb)){
    ringbuffer_get(osd_data_rb, (uint8_t *)&frame_info, sizeof(image_frame_info_t));
    // printf("InPutRingBuffer is_empty\n");
    time_val.tv_sec=0;
    time_val.tv_usec=(1000);
    select(0,NULL,NULL,NULL,&time_val);
  }
  if(!gs_osd_stream->chn_info[chn].osd_flag){
    //pthread_cancel(gs_osd_stream->chn_info[chn].osd_tid);
    gs_osd_stream->chn_info[chn].osd_flag = 1;
    xt_nanosleep(0, 1000*1000);
  }
  if(!gs_osd_stream->chn_info[chn].over_flag){
    //pthread_cancel(gs_osd_stream->chn_info[chn].osd_tid);
    gs_osd_stream->chn_info[chn].over_flag = 1;
    xt_nanosleep(0, 1000*1000);
  }
}

void osd_ringbuffer_destory(void)
{
  int chn;
  for(chn = 0; chn < DATA_CHANNEL_NUM_MAX;chn++){
    if(gs_osd_stream->chn_info[chn].osd_rb){
      ringbuffer_destory(gs_osd_stream->chn_info[chn].osd_rb);
    }
  }
}


void gui_osd_ringbuffer_is_empty(int chn)
{
  image_frame_info_t frame_info = {0};
  while(!ringbuffer_is_empty(gs_osd_stream->chn_info[chn].osd_rb)){
    xt_nanosleep(0, 100); 
  }
}

void ucgui_lib_init(unsigned char **pic_buf)
{

	set_gui_addr(pic_buf,1);
	UcguiInitPara();
	GUI_Init();
 
	GUI_Clear();
  GUI_SetFont(&GUI_FontHZ32x32_heiti);
  GUI_SetColor(GUI_GREEN);
	GUI_SetTextMode(GUI_TEXTMODE_TRANS);
}


int gui_osd_init(void)
{
  int ret;
  gs_osd_stream = (osd_data_stream_t *)malloc(sizeof(osd_data_stream_t));
    if(gs_osd_stream == NULL){
    printf("[err] osd_data_stream_t buffer malloc error \n");
    return 111;
  }
  memset(gs_osd_stream, 0x0, sizeof(osd_data_stream_t));

  gs_osd_stream->gs_osd_data = (unsigned char *)malloc(VIDEO_OSD_WIDTH * VIDEO_OSD_HIGHT *2 * DATA_CHANNEL_NUM_MAX);
  if(gs_osd_stream->gs_osd_data == NULL){
    printf("[err] ucgui lib osd buffer malloc error \n");
    return 222;
  }

  memset(gs_osd_stream->gs_osd_data, 0x0, (VIDEO_OSD_WIDTH * VIDEO_OSD_HIGHT *2 * DATA_CHANNEL_NUM_MAX));
  ucgui_lib_init(&gs_osd_stream->gs_osd_data);

	return A818_SUCCESS;
}

void gui_osd_destory(void)
{
  free(gs_osd_stream->gs_osd_data);
  return ;
}