/* Includes ------------------------------------------------------------------*/
#include "string.h"
#include "defines.h"
#include "kozos.h"
#include "stm32l4xx_hal_rcc.h"
#include "stm32l4xx_hal_gpio.h"
#include "stm32l4xx_hal_pwr_ex.h"
#include "stm32l4xx_hal_cortex.h"
#include "stm32l4xx.h"
#include "sai.h"
//#include "pcm3060.h"

// 状態
#define ST_NOT_INTIALIZED	(0U)	// 未初期化
#define ST_INTIALIZED		(1U)	// 初期化済
#define ST_OPENED			(2U)	// オープン済
#define ST_RUN				(3U)	// 実行中

// SAI使用チャネル
#define USE_SAI_CH			SAI_CH1

// 音声パラメータ
#define SAMPLING_FREQUENCY	(16000U)
#define MASTER_CLOCK		(SAMPLING_FREQUENCY * 256)

// SAI制御ブロック
typedef struct {
	uint8_t				status	;			// 状態
} SAI_MNG_CTL;
static SAI_MNG_CTL sai_mng_ctl;

// SAIオープンパラメータ
static const SAI_OPEN sai_open_par = 
{
	SAI_MODE_MONAURAL,	// モノラル
	SAI_FMT_MSB_JUSTIFIED,		// I2S
	SAI_PACK_MSB_FIRST, // MSB First
	SAI_DATA_WIDTH_24,	// 16bit
	SAMPLING_FREQUENCY,	// サンプリング周波数:16kHz
	TRUE,				// MCLKを使用
};

// 外部公開関数
// sai_mng初期化関数
void sai_mng_init(void)
{
	uint32_t ch;
	SAI_MNG_CTL *this;
	
	// 制御ブロック取得
	this = &sai_mng_ctl;
	// 制御ブロック初期化
	memset(this, 0, sizeof(SAI_MNG_CTL));
	// 状態を更新
	this->status = ST_INTIALIZED;
	
	return;
}

// sai_mngオープン関数
void sai_mng_open(void)
{
	SAI_MNG_CTL *this;
	
	// 制御ブロック取得
	this = &sai_mng_ctl;
	
	// saiをオープン
	sai_open(USE_SAI_CH, &sai_open_par);
	
	// 状態を更新
	this->status = ST_OPENED;
	
	return;
}

int32_t sai_mng_send(void)
{
	SAI_MNG_CTL *this;
	const int32_t data[8] = {0xAAAAAAAAU, 0xFFFFFFFFU, 0xAAAAAAAAU, 0xFFFFFFFFU, 0xAAAAAAAAU, 0xFFFFFFFFU, 0xAAAAAAAAU, 0xFFFFFFFFU};
	
	// 制御ブロック取得
	this = &sai_mng_ctl;
	
	sai_send(USE_SAI_CH, data, 8);
}