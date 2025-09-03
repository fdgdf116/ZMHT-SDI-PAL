/*
*********************************************************************************************************
*                                                uC/GUI
*                        Universal graphic software for embedded applications
*
*                       (c) Copyright 2002, Micrium Inc., Weston, FL
*                       (c) Copyright 2002, SEGGER Microcontroller Systeme GmbH
*
*              µC/GUI is protected by international copyright laws. Knowledge of the
*              source code may not be used to write a similar product. This file may
*              only be used in accordance with a license and should not be redistributed
*              in any way. We appreciate your understanding and fairness.
*
----------------------------------------------------------------------
File        : GUITouch.Conf.h
Purpose     : Configures touch screen module
----------------------------------------------------------------------
*/


#ifndef GUITOUCH_CONF_H
#define GUITOUCH_CONF_H
#include "ucguiCfg.h"
#define GUI_TOUCH_AD_LEFT 	 160
#define GUI_TOUCH_AD_RIGHT 	 3900
#define GUI_TOUCH_AD_TOP 	 3820//3715
#define GUI_TOUCH_AD_BOTTOM  310
//#define GUI_TOUCH_AD_TOP 	 230//3715
//#define GUI_TOUCH_AD_BOTTOM  4000

//#define GUI_TOUCH_AD_LEFT  35   
//#define GUI_TOUCH_AD_RIGHT  980    
#if defined(ROTATENINETY)

#define GUI_TOUCH_SWAP_XY    1
#define GUI_TOUCH_MIRROR_X   0
#define GUI_TOUCH_MIRROR_Y   1

#elif defined(SCREENREVERSAL)

#define GUI_TOUCH_SWAP_XY    0
#define GUI_TOUCH_MIRROR_X   1
#define GUI_TOUCH_MIRROR_Y   1

#else

#define GUI_TOUCH_SWAP_XY    0
#define GUI_TOUCH_MIRROR_X   0
#define GUI_TOUCH_MIRROR_Y   0

#endif


#endif /* GUITOUCH_CONF_H */
