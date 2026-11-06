/*
 * osd_show_menu.h
 *
 *  Created on: Jun 29, 2022
 *      Author: xh
 */

#ifndef OSD_ENGINE_H_
#define OSD_ENGINE_H_

#ifdef __cplusplus
extern "C" {
#endif
#include <pthread.h>

#include "vivado_common.h"
#include "ringbuffer.h"

#define OSD_GUI_FONT_FORMAT_MAX (3)
#define OSD_GUI_FONT_SIZE_MAX (9)

struct osd_chn_info_t{
  unsigned char chn;
  pthread_t osd_tid;
  pthread_t over_tid;
  unsigned char osd_flag;
  unsigned char over_flag;
  unsigned char osd_created;
  unsigned char over_created;
  ring_buffer_t *osd_rb;
};
typedef struct {
  pthread_t gui_tid;
  unsigned char* gs_osd_data; /* OSD数据 */
  struct osd_chn_info_t chn_info[DATA_CHANNEL_NUM_MAX];
}osd_data_stream_t;

int gui_osd_init(void);
void gui_osd_src_image(unsigned char chn);
void gui_osd_stitchbase_image(unsigned char chn);
void chage_src_to_new(unsigned char *src, unsigned char *dec, int new_w, int new_h);
void gui_osd_destory(void);
osd_data_stream_t* GetOsdDataStream();
void gui_osd_ringbuffer_is_empty(int chn);
int gui_osd_overlapped_chn_init(unsigned char chn);
int gui_osd_overlapped_chn_destory(unsigned char chn);
int gpu_osd_chn_image_init(unsigned char chn);
void gpu_osd_chn_image_destory(unsigned char chn);
#ifdef __cplusplus
}
#endif

#endif /* PRO_SYSTEM_SJ_20_OSD_SHOW_MENU_H_ */
