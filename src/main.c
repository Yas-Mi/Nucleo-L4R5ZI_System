/*
******************************************************************************
File:     main.c
Info:     Generated by Atollic TrueSTUDIO(R) 9.3.0   2022-08-12

The MIT License (MIT)
Copyright (c) 2019 STMicroelectronics

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

******************************************************************************
*/

/* Includes */
#include <console.h>
#include <string.h>
#include "stm32l4xx.h"
#include "defines.h"
#include "kozos.h"
#include "interrupt.h"
#include "pin_function.h"
#include "clock.h"
// ドライバ
#include "octspi.h"
#include "sai.h"
#include "i2c_wrapper.h"
#include "dma.h"
#include "spi.h"
#include "tim.h"
// デバイス
#include "w25q20ew_ctrl.h"
#include "gysfdmaxb.h"
#include "bt_dev.h"
#include "pcm3060.h"
#include "lcd_dev.h"
#include "MSP2807.h"

// マネージャ
#include "wav.h"
#include "cyc.h"
#include "flash_mng.h"
// アプリ
#include "console.h"
#include "sound_app.h"
#include "lcd_app.h"
/* Private macro */
/* Private variables */
/* Private function prototypes */
/* Private functions */

/**
**===========================================================================
**
**  Abstract: main program
**
**===================
**===========================================================================
*/
#if 1
static uint16_t test_data[MSP2807_DISPLAY_WIDTH*MSP2807_DISPLAY_HEIGHT];
// テスト用タスク 
static int test_tsk1(int argc, char *argv[])
{	
	uint16_t cnt = 0;
	uint32_t i;
	msp2807_open();
	while(1) {
		for (i = 0; i < MSP2807_DISPLAY_WIDTH*MSP2807_DISPLAY_HEIGHT; i++) {
			test_data[i] = cnt;
		}
		console_str_send("writewrite\n");
		msp2807_write(test_data);
		cnt++;
		kz_tsleep(1000);
	}
	
	return 0;
}
#endif

/* システム・タスクとユーザ・タスクの起動 */
static int start_threads(int argc, char *argv[])
{
	// ペリフェラルの初期化
	usart_init();
	i2c_wrapper_init();
	sai_init();
//	tim_init();
	dma_init();
	octospi_init();
	spi_init();
	
	// デバイスの初期化
	bt_dev_init();
	msp2807_init();
	//pcm3060_init();
	//LCD_dev_init();
	//gysfdmaxb_init();
	
	// マネージャの初期化
	wav_init();
	cyc_init();
	flash_mng_init();
	
	// アプリの初期化
	console_init();
	//sound_app_init();
	//lcd_apl_init();
	
	// コマンドの設定
	//bt_dev_set_cmd();
	//sound_app_set_cmd();
	//pcm3060_set_cmd();
	w25q20ew_set_cmd();
	//gysfdmaxb_set_cmd();
	flash_mng_set_cmd();
	
	// テスト
	flash_mng_open(FLASH_MNG_KIND_W25Q20EW);
	
	// タスクの起動
	// デバイス
	//kz_run(BTN_dev_main, "BTN_dev_main",  2, 0x1000, 0, NULL);
	// アプリ
	//kz_run(console_main, "console",  3, 0x1000, 0, NULL);
	//kz_run(LCD_app_main, "LCD_app_main",  3, 0x1000, 0, NULL);
	//kz_run(ctl_main, "ctl_main",  3, 0x1000, 0, NULL);
	//kz_run(ctl_cycmsg_main, "ctl_cycmsg",  3, 0x1000, 0, NULL);
	//kz_run(US_main, "US_main",  8, 0x1000, 0, NULL);
	//kz_run(BT_main, "BT_main",  8, 0x1000, 0, NULL);
	//kz_run(BT_mng_connect_sts, "BT_mng_connect_sts",  8, 0x1000, 0, NULL);
	//kz_run(BT_dev_main, "bt_dev",  8, 0x1000, 0, NULL);
	//kz_run(bluetoothdrv_main, "blue_tooth",  8, 0x200, 0, NULL);
	//kz_run(flash_main, "flash",  2, 0x200, 0, NULL);
	//kz_run(BTN_dev_main, "BTN_dev_main",  3, 0x1000, 0, NULL);
	kz_run(test_tsk1, "test_tsk1",  3, 0x1000, 0, NULL);
	
	/* 優先順位を下げて，アイドルスレッドに移行する */
	kz_chpri(15); 
	
	// システム制御タスクの初期化
	//CTL_MSG_init();
	
	//INTR_ENABLE; /* 割込み有効にする */
 	while (1) {
		//TASK_IDLE; /* 省電力モードに移行 */
	}
	
	return 0;
}

int main(void)
{	
	// ペリフェラルのクロックを有効化
	periferal_clock_init();
	// ピンファンクションの設定
	pin_function_init();
	
	/* OSの動作開始 */
	kz_start(start_threads, "idle", 0, 0x1000, 0, NULL);
	
	/* ここには戻ってこない */
	
	return 0;
}
