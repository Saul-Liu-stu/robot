/*
 * LCD_Test.h
 *
 *  Created on: 2026年7月12日
 *      Author: 31288
 */

#ifndef LCD_TEST_H_
#define LCD_TEST_H_

#include "lcd_spi_154.h"
// LCD测试函数，函数定义在底部
void 	LCD_Test_Clear(void);			// 清屏测试
void 	LCD_Test_Text(void);			   // 文本测试
void 	LCD_Test_Variable (void);	   // 变量显示，包括整数和小数
void 	LCD_Test_Color(void);			// 矩形填充测试
void 	LCD_Test_Grahic(void);		   // 2D图形绘制
void 	LCD_Test_Image(void);			// 图片显示
void    LCD_Test_Direction(void);	   // 更换显示方向
void 	LCD_Test_ColorfulImage(void);	//显示彩色图片

#endif /* LCD_TEST_H_ */
