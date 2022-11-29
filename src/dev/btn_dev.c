/*
 * btn_dev.c
 *
 *  Created on: 2022/10/29
 *      Author: User
 */
#include "defines.h"
#include "kozos.h"
#include "consdrv.h"
#include "stm32l4xx_hal_gpio.h"
#include "btn_dev.h"
#include "lib.h"

// ボタン状態
#define PUSHED			(0U)	// ボタンが離された
#define RELEASED		(1U)	// ボタンが押された
#define SHORT_PUSHED	(2U)	// 短押し
#define LONG_PUSHED		(3U)	// 長押し

// ボタンの種類
#define BTN_UP     (0U)		// 上ボタン
#define BTN_DOWN   (1U)		// 下ボタン
#define BTN_BACK   (2U)		// 戻るボタン
#define BTN_SELECT (3U)		// 決定ボタン
#define BTN_MAX    (4U)		// 最大値

// 短押し/長押し時間
#define BTN_SHORT_PUSHED (1500U)	// 短押し時間 (*)ボタンを押してから話すまでの時間が1000ms以下の場合、短押しと判定
#define BTN_LONG_PUSHED  (2500U)	// 長押し時間 (*)ボタンを押してから押した状態が2000ms以上の場合、長押しと判定

// ボタン状態を確認する周期
#define BTN_CHECK_PERIOD (100U)		// 100ms周期でボタン状態をチェック

// ボタンの状態管理ブロック
typedef struct {
	uint8_t  status; 	// ボタンの状態
	uint32_t counter;	// ボタンが押された時間[ms] (= counter * BTN_CHECK_PERIOD)
	BTN_CALLBACK cb;	// コールバック関数
} BTN_STS;

// 制御用ブロック
typedef struct {
	kz_thread_id_t tsk_id;		// メインタスクのID
	BTN_STS info[BTN_MAX];		// 各ボタンの情報
} BTN_CTL;
static BTN_CTL btn_ctl;

// 内部関数
// ボタン初期化関数
static void btn_dev_init(void)
{
	BTN_CTL *this = &btn_ctl;
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	uint32_t ret;
	uint8_t i;
	
	// 制御ブロックの初期化
	memset(this, 0, sizeof(this));
	
	// ボタンの状態をRELEASEDに設定
	for (i = 0; i < BTN_MAX; i++) {
		this->info[i].status = RELEASED;
	}
	
	// 自タスクのIDを制御ブロックに設定
	this->tsk_id = kz_getid();
	
	// クロック設定
	HAL_PWREx_EnableVddIO2();
  __HAL_RCC_GPIOE_CLK_ENABLE();
	
	// ピン設定
	/*Configure GPIO pins : PE2 PE3 PE0 PE1 */
	GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_0|GPIO_PIN_1;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
	
	// 割込み許可
	HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(EXTI0_IRQn);
	HAL_NVIC_SetPriority(EXTI1_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(EXTI1_IRQn);
	HAL_NVIC_SetPriority(EXTI2_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(EXTI2_IRQn);
	HAL_NVIC_SetPriority(EXTI3_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(EXTI3_IRQn);
	
	return;
}

// ボタンの状態を確認する関数
static void btn_dev_check(void)
{
	uint8_t i;
	BTN_CTL *this = &btn_ctl;
	BTN_STS *info;
	uint32_t pushed_time;
	
	// ボタンの状態をチェック
	for (i = 0; i < BTN_MAX; i++) {
		// ボタンの情報を取得
		info = &(this->info[i]);
		// ボタンが押されているとき
		if (info->status == PUSHED) {
			// カウンタを進める
			info->counter++;
			// カウンタから時間[ms]に変換
			pushed_time = info->counter * BTN_CHECK_PERIOD;
			// 長押し判定
			if(pushed_time > BTN_LONG_PUSHED) {
				// 状態を更新
				info->status = LONG_PUSHED;
				// コールバック実行
				if (info->cb) {
					info->cb(BTN_LONG_PUSHED);
				}
			} else {
				; // ボタン押し込み中
			}
		// ボタンが離されているとき
		} else if (info->status == RELEASED) {
			// そもそもボタンが押されていた？
			if (info->counter > 0) {
				// カウンタから時間[ms]に変換
				pushed_time = info->counter * BTN_CHECK_PERIOD;
				// カウンタをクリア
				info->counter = 0;
				// 短押し判定
				if (pushed_time < BTN_SHORT_PUSHED) {
					// 状態を更新
					info->status = SHORT_PUSHED;
					// コールバック実行
					if (info->cb) {
						info->cb(BTN_SHORT_PUSHED);
					}
				} else {
					;
				}
			}
		} else {
			;
		}
	}
	
	return;
}

// 外部関数
// 上ボタン割込み関数
void btn_up_interrupt(void)
{
	BTN_CTL *this = &btn_ctl;
	BTN_STS *info = &(this->info[BTN_UP]);
	uint8_t sts;
	
	// 割込み要因のクリア
	__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_0);
	
	// ボタンの状態を取得
	sts = HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_0);
	
	// ボタンが押された
	if (sts == PUSHED) {
		// 状態の更新
		info->status = PUSHED;
	// ボタンが離された
	} else {
		// 状態の更新
		info->status = RELEASED;
	}
}

// 下ボタン割込み関数
void btn_down_interrupt(void)
{
	BTN_CTL *this = &btn_ctl;
	BTN_STS *info = &(this->info[BTN_DOWN]);
	uint8_t sts;
	
	// 割込み要因のクリア
	__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_1);
	
	// ボタンの状態を取得
	sts = HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_1);
	
	// ボタンが押された
	if (sts == PUSHED) {
		// 状態の更新
		info->status = PUSHED;
	// ボタンが離された
	} else {
		// 状態の更新
		info->status = RELEASED;
	}
}

// 戻るボタン割込み関数
void btn_back_interrupt(void)
{
	BTN_CTL *this = &btn_ctl;
	BTN_STS *info = &(this->info[BTN_BACK]);
	uint8_t sts;
	
	// 割込み要因のクリア
	__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_2);
	
	// ボタンの状態を取得
	sts = HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_2);
	
	// ボタンが押された
	if (sts == PUSHED) {
		// 状態の更新
		info->status = PUSHED;
	// ボタンが離された
	} else {
		// 状態の更新
		info->status = RELEASED;
	}
}

// 決定ボタン割込み関数
void btn_select_interrupt(void)
{
	BTN_CTL *this = &btn_ctl;
	BTN_STS *info = &(this->info[BTN_SELECT]);
	uint8_t sts;
	
	// 割込み要因のクリア
	__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_3);
	
	// ボタンの状態を取得
	sts = HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_3);
	
	// ボタンが押された
	if (sts == PUSHED) {
		// 状態の更新
		info->status = PUSHED;
	// ボタンが離された
	} else {
		// 状態の更新
		info->status = RELEASED;
	}
}

// 上ボタンコールバック登録関数
int BTN_dev_reg_callback_up(BTN_CALLBACK cb)
{
	BTN_CTL *this = &btn_ctl;
	BTN_STS *info = &(this->info[BTN_UP]);
	
	// コールバックがNULLの場合エラーを返す
	if (cb == NULL) {
		return -1;
	}
	
	// コールバックを登録
	info->cb = cb;
	
	return 0;
}

// 下ボタンコールバック登録関数
int BTN_dev_reg_callback_down(BTN_CALLBACK cb)
{
	BTN_CTL *this = &btn_ctl;
	BTN_STS *info = &(this->info[BTN_DOWN]);
	
	// コールバックがNULLの場合エラーを返す
	if (cb == NULL) {
		return -1;
	}
	
	// コールバックを登録
	info->cb = cb;
	
	return 0;
}

// 戻るボタンコールバック登録関数
int BTN_dev_reg_callback_back(BTN_CALLBACK cb)
{
	BTN_CTL *this = &btn_ctl;
	BTN_STS *info = &(this->info[BTN_BACK]);
	
	// コールバックがNULLの場合エラーを返す
	if (cb == NULL) {
		return -1;
	}
	
	// コールバックを登録
	info->cb = cb;
	
	return 0;
}

// 決定ボタンコールバック登録関数
int BTN_dev_reg_callback_select(BTN_CALLBACK cb)
{
	BTN_CTL *this = &btn_ctl;
	BTN_STS *info = &(this->info[BTN_SELECT]);
	
	// コールバックがNULLの場合エラーを返す
	if (cb == NULL) {
		return -1;
	}
	
	// コールバックを登録
	info->cb = cb;
	
	return 0;
}

int BTN_dev_main(int argc, char *argv[])
{
	// 初期化
	btn_dev_init();
	
	while(1){
		// 200ms SLEEP
		kz_tsleep(BTN_CHECK_PERIOD);
		
		// ボタンの状態をチェック
		btn_dev_check();
	}

	return 0;
	
}

