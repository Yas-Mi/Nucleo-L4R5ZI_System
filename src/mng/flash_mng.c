/*
 * flash_mng.c
 *
 *  Created on: 2023/10/18
 *      Author: User
 */
#include "defines.h"
#include "kozos.h"
#include "console.h"
#include "intr.h"

// 状態
#define ST_UNINITIALIZED	(0)		// 未初期化
#define ST_IDLE				(1)		// アイドル
#define ST_RUNNING			(2)		// 実行中
#define ST_MAX				(3)

// マクロ


// 制御用ブロック
typedef struct {
	uint32_t			state;					// 状態
	kz_thread_id_t		tsk_id;					// タスクID
} FLASH_MNG_CTL;
static FLASH_MNG_CTL fls_mng_ctl[FLASH_MNG_KIND_MAX];

// フラッシュ情報
typedef int32_t (*FLASH_MNG_INIT)(void);
typedef int32_t (*FLASH_MNG_OPEN)(void);
typedef int32_t (*FLASH_MNG_WRITE_ENABLE)(void);
typedef int32_t (*FLASH_MNG_WRITE)(uint32_t addr, uint8_t *data, uint8_t size);
typedef int32_t (*FLASH_MNG_ERASE)(uint32_t addr);
typedef int32_t (*FLASH_MNG_READ)(uint32_t addr, uint8_t *data, uint8_t size);
typedef int32_t (*FLASH_MNG_WRITE_DISABLE)(USART_CH ch, void *vp);
typedef struct {
	
	FLASH_MNG_WRITE_ENABLE	write_enable;	// ライトイネーブル
	FLASH_MNG_WRITE			write;			// ライト
	FLASH_MNG_ERASE			erase;			// イレース
	FLASH_MNG_READ			read;			// リード
	FLASH_MNG_WRITE_DISABLE	write_disable;	// ライトディセーブル
} FLASH_MNG_CFG

// フラッシュ情報テーブル
static const FLASH_MNG_CFG fls_mng_cfg[FLASH_MNG_KIND_MAX] =
{
	// FLASH_MNG_KIND_W25Q20EW
	{
		w25q20ew_write_enable,
		w25q20ew_write,
		w25q20ew_erase,
		w25q20ew_read,
		w25q20ew_write_disable,
	},
};

// コールバック
static void w25q20ew_callback(void *vp)
{
	FLASH_MNG_CTL *this = (FLASH_MNG_CTL*)vp;
}

// BlueTooth初期化関数
int32_t flash_mng_init(void)
{
	FLASH_MNG_CTL *this = &fls_mng_ctl;
	int32_t ret;
	
	// 制御ブロックの初期化
	memset(this, 0, sizeof(FLASH_MNG_CTL));
	
	// デバイス初期化
	ret = w25q20ew_init();
	if (ret != 0) {
		return -1;
	}
	
	// デバイスオープン
	ret = w25q20ew_open(w25q20ew_callback, this);
	if (ret != 0) {
		return -1;
	}
	
	// 状態の更新
	this->state = ST_IDLE;
	
	return 0;
}

// 書き込み
int32_t flash_mng_write(uint32_t kind, uint32_t addr, uint8_t *data, uint32_t size)
{
	FLASH_MNG_CTL *this = &fls_mng_ctl[kind];
	int32_t ret;
	
	// NULLの場合はエラーを返して終了
	if (data == NULL) {
		return -1;
	}
	
	// 割込み禁止 (*) 複数のタスクから呼ばれることを想定している
	INTR_DISABLE;
	// 未初期化の場合はエラーを返して終了
	if (this->state != ST_IDLE) {
		// 割込み禁止解除
		INTR_ENABLE;
		return -1;
	}
	// 状態を更新
	this->state = ST_RUNNING;
	// 割込み禁止解除
	INTR_ENABLE;
	
	// ライトイネーブル
	ret = w25q20ew_write_enable();
	if (ret != 0) {
		return -1;
	}
	
	// 完了まで寝る
	this->tsk_id = kz_getid();
	kz_sleep();
	
#if 0
	// 書き込み
	w25q20ew_send();
	
	// 完了まで寝る
	this->tsk_id = kz_getid();
	kz_sleep();
	
	// ライトディセーブル
	ret = w25q20ew_write_enable();
	
	// 完了まで寝る
	this->tsk_id = kz_getid();
	kz_sleep();
#endif
	
	// 状態を更新
	this->state = ST_IDLE;
	
	return 0;
}

// BlueTooth送信通知関数
int32_t flash_mng_read(uint32_t addr, uint8_t *data, uint32_t size)
{
	FLASH_MNG_CTL *this = &fls_mng_ctl;
	
}

// コマンド設定関数
void flash_mng_set_cmd(void)
{
	
}
