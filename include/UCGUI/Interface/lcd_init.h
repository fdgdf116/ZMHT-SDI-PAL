/*
 * lcd_init.h
 *
 *  Created on: 2016-1-6
 *      Author: Ray
 */

#ifndef LCD_INIT_H_
#define LCD_INIT_H_
void InterfaceLCDSetClearColor(int color);
void LCD_PutPixel(int x,int y,int PixelIndex);
int  LCD_GetPixel(int x,int y);
void InterfaceLCDSetClearColor(int color);
void InterfaceStartLCD(void);
void InterfaceShowLCD(unsigned char*tmp);
void InterfaceShowLCDPara(int id);
int Get_Idx(void);

unsigned char *get_gui_addr(void);
void set_gui_addr(unsigned char **buf,int mode);//mode:0 字库本地申请buf  1：外部指针传入buf （默认mode = 0）

#endif /* LCD_INIT_H_ */
