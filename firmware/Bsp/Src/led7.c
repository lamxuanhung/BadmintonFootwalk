/*
 * LED7.c
 *
 *  Created on: Aug 11, 2019
 *      Author: XuanHung
 *  This LED7 function use with TM1637
 */

#include "../Inc/led7.h"

LED7_Desc_Block_t LED7_Desc_cs;

//
//void LED7_PrintNumber(LED7_Desc_Block_t* p_cb,uint16_t number)
//{
//	if (number > 999)
//	{
//		p_cb->TM1637_cs->databuffer[4] =  p_cb->DefSegment[number%10];
//		p_cb->TM1637_cs->databuffer[3] =  p_cb->DefSegment[(number/10)%10];
//		p_cb->TM1637_cs->databuffer[2] =  p_cb->DefSegment[(number/100)%10];
//		p_cb->TM1637_cs->databuffer[1] =  p_cb->DefSegment[(number/1000)%10];
//	}
//	else if (number > 99)
//	{
//		p_cb->TM1637_cs->databuffer[4] =  p_cb->DefSegment[number%10];
//		p_cb->TM1637_cs->databuffer[3] =  p_cb->DefSegment[(number/10)%10];
//		p_cb->TM1637_cs->databuffer[2] =  p_cb->DefSegment[(number/100)%10];
//		p_cb->TM1637_cs->databuffer[1] =  p_cb->DefSegment[16];
//	}
//	else if (number > 9)
//	{
//		p_cb->TM1637_cs->databuffer[4] =  p_cb->DefSegment[number%10];
//		p_cb->TM1637_cs->databuffer[3] =  p_cb->DefSegment[(number/10)%10];
//		p_cb->TM1637_cs->databuffer[2] =  p_cb->DefSegment[16];
//		p_cb->TM1637_cs->databuffer[1] =  p_cb->DefSegment[16];
//	}
//	else
//	{
//		p_cb->TM1637_cs->databuffer[4] =  p_cb->DefSegment[number%10];
//		p_cb->TM1637_cs->databuffer[3] =  p_cb->DefSegment[16];
//		p_cb->TM1637_cs->databuffer[2] =  p_cb->DefSegment[16];
//		p_cb->TM1637_cs->databuffer[1] =  p_cb->DefSegment[16];
//	}
//}
//
//void LED7_Display(uint16_t number)
//{
//	LED7_PrintNumber(&LED7_Desc_cs,number);
//}
//
//void LED7_PrintSpeed(LED7_Desc_Block_t* p_cb,uint16_t speed)
//{
//	if (speed > 99)
//	{
//		p_cb->TM1637_cs->databuffer[4] =  p_cb->DefSegment[speed%10];
//		p_cb->TM1637_cs->databuffer[3] =  p_cb->DefSegment[(speed/10)%10]| 0x80;
//		p_cb->TM1637_cs->databuffer[2] =  p_cb->DefSegment[(speed/100)%10];
//		p_cb->TM1637_cs->databuffer[1] =  p_cb->DefSegment[16];
//	}
//	else {
//		p_cb->TM1637_cs->databuffer[4] =  p_cb->DefSegment[speed%10];
//		p_cb->TM1637_cs->databuffer[3] =  p_cb->DefSegment[(speed/10)%10] | 0x80;
//		p_cb->TM1637_cs->databuffer[2] =  p_cb->DefSegment[16] ;
//		p_cb->TM1637_cs->databuffer[1] =  p_cb->DefSegment[16];
//	}
//}
///* Training speed from 1s to 10s, with stepping 0.5*/
//void LED7_DisplaySpeed(uint8_t speed)
//{
//	LED7_PrintSpeed(&LED7_Desc_cs,speed);
//
//}
//void LED7_Ini()
//{
//	LED7_Desc_cs.TM1637_cs = &TM1637;
//	LED7_Desc_cs.DefSegment = SegmentMap;
//	TM1637_Ini(&LED7_Desc_cs.TM1637_cs);
//	LED7_PrintNumber(&LED7_Desc_cs,0);
//}
//


