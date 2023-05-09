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
#include "intr.h"

// 状態
#define ST_NOT_INTIALIZED	(0U)	// 未初期化
#define ST_INTIALIZED		(1U)	// 初期化済
#define ST_OPENED			(2U)	// オープン済
#define ST_RUN				(3U)	// 実行中

// マクロ
#define SAI_CLOCK		(4000000U)		// ペリフェラルクロック 4MHz
#define SAI_FIFO_SIZE	(8U)			// FIFOサイズ 8段
#define SAI_BUF_SIZE	(256)			// SAIの送信データサイズ

//SAI ベースレジスタ
#define SAI1_BASE_ADDR	(0x40015400)
#define SAI2_BASE_ADDR	(0x40015800)

// レジスタ設定値
// ACR1
#define ACR1_OSR			(1UL << 26)
#define ACR1_MCKDIV_WIDTH	(5UL)
#define ACR1_MCKDIV_POS		(20UL)
#define ACR1_MCKDIV			(((1UL << ACR1_MCKDIV_WIDTH) - 1UL) << ACR1_MCKDIV_POS)
#define ACR1_NOMCK			(1UL << 19)
#define ACR1_DMAEN			(1UL << 17)
#define ACR1_SAIEN			(1UL << 16)
#define ACR1_OUTDRIV		(1UL << 13)
#define ACR1_MONO			(1UL << 12)
#define ACR1_SYNCEN_WIDTH	(2UL)
#define ACR1_SYNCEN			(((1UL << ACR1_SYNCEN_WIDTH) - 1UL) << 10)
#define ACR1_CKSTR			(1UL << 9)
#define ACR1_LSBFIRST		(1UL << 8)
#define ACR1_DS_WIDTH		(3UL)
#define ACR1_DS_POS			(5)
#define ACR1_DS				(((1UL << ACR1_DS_WIDTH) - 1UL) << ACR1_DS_POS)
#define ACR1_PRTCFG_WIDTH	(2UL)
#define ACR1_PRTCFG			(((1UL << ACR1_PRTCFG_WIDTH) - 1UL) << 2)
#define ACR1_MODE_WIDTH		(2UL)
#define ACR1_MODE			(((1UL << ACR1_MODE_WIDTH) - 1UL) << 0)
// AFRCR
#define AFRCR_FSOFF				(1UL << 18)
#define AFRCR_FSPOL				(1UL << 17)
#define AFRCR_FSDEF				(1UL << 16)
#define AFRCR_FSALL_WIDTH		(7UL)
#define AFRCR_FSALL_POS			(8UL)
#define AFRCR_FSALL				(((1UL << AFRCR_FSALL_WIDTH) - 1UL) << AFRCR_FSALL_POS)
#define AFRCR_FRL_WIDTH			(8)
#define AFRCR_FRL_POS			(0UL)
#define AFRCR_FRL				(((1UL << AFRCR_FRL_WIDTH) - 1UL) << AFRCR_FRL_POS)
// ASLOTR
#define ASLOTR_SLOTEN_WIDTH		(16UL)
#define ASLOTR_SLOTEN_POS		(16UL)
#define ASLOTR_SLOTEN			(((1UL << ASLOTR_SLOTEN_WIDTH) - 1UL) << ASLOTR_SLOTEN_POS)
#define ASLOTR_NBSLOT_WIDTH		(4UL)
#define ASLOTR_NBSLOT_POS		(8UL)
#define ASLOTR_NBSLOT			(((1UL << ASLOTR_NBSLOT_WIDTH) - 1UL) << 8)
#define ASLOTR_SLOTSZ_WIDTH		(2UL)
#define ASLOTR_SLOTSZ_POS		(6UL)
#define ASLOTR_SLOTSZ			(((1UL << ASLOTR_SLOTSZ_WIDTH) - 1UL) << ASLOTR_SLOTSZ_POS)
#define ASLOTR_FBOFF_WIDTH		(5UL)
#define ASLOTR_FBOFF_POS		(0UL)
#define ASLOTR_FBOFF			(((1UL << ASLOTR_FBOFF_WIDTH) - 1UL) << ASLOTR_FBOFF_POS)
// AIM
#define AIM_FRQIE		(1UL << 3)
// ASR
#define ASR_FREQ		(1UL << 3)
// ACLRFR
#define ACLRFR_COVRUDR  (1UL << 0)

// レジスタ設定値
#define AFRCR_FSOFF_NOT_DELAY		(0x00000000)
#define AFRCR_FSOFF_DELAY			(AFRCR_FSOFF)
#define AFRCR_FSPOL_ACTIVE_LOW		(0x00000000)
#define AFRCR_FSPOL_ACTIVE_HIGH		(AFRCR_FSPOL)

// USARTレジスタ定義
struct stm32l4_sai {
	volatile uint32_t gcr;
	volatile uint32_t acr1;
	volatile uint32_t acr2;
	volatile uint32_t afrcr;
	volatile uint32_t aslotr;
	volatile uint32_t aim;
	volatile uint32_t asr;
	volatile uint32_t aclrfr;
	volatile uint32_t adr;
	volatile uint32_t bcr1;
	volatile uint32_t bcr2;
	volatile uint32_t bfrcr;
	volatile uint32_t bslotr;
	volatile uint32_t bim;
	volatile uint32_t bsr;
	volatile uint32_t bclrfr;
	volatile uint32_t bdr;
	volatile uint32_t pdmcr;
	volatile uint32_t pdmdly;
};

/* リングバッファ */
typedef struct {
	uint32_t buf[SAI_BUF_SIZE];		// バッファ
	uint32_t r_idx;					// リードインデックス
	uint32_t w_idx;					// ライトインデックス
} RING_BUF;

// プロトコル情報
typedef struct {
	uint32_t fsoff;		// Frame Synchronizationのアサートタイミング
	uint32_t fspol;		// Frame Synchronizationの極性
} SAI_PROTCOL;

// 割込みハンドラのプロトタイプ
void sai1_handler(void);

// SAIチャネル固有情報
typedef struct {
	volatile struct stm32l4_sai *sai_base_addr;			// ベースレジスタ
	IRQn_Type irq_type;									// 割込みタイプ
	INT_HANDLER handler;								// 割込みハンドラ
	uint32_t	vec_no;									// 割込み番号
	uint32_t	clock_no;								// クロックNo.
} SAI_CFG;

// SAIチャネル固有情報テーブル
static const SAI_CFG sai_cfg[SAI_CH_MAX] =
{
	{(volatile struct stm32l4_usart*)SAI1_BASE_ADDR, SAI1_IRQn, sai1_handler, SAI1_GLOBAL_INTERRUPT_NO, RCC_PERIPHCLK_SAI1},
};
#define get_reg(ch)			(sai_cfg[ch].sai_base_addr)		// レジスタ取得マクロ
#define get_ire_type(ch)	(sai_cfg[ch].irq_type)			// 割込みタイプ取得マクロ
#define get_handler(ch)		(sai_cfg[ch].handler)			// 割り込みハンドラ取得マクロ
#define get_vec_no(ch)		(sai_cfg[ch].vec_no)			// 割り込み番号取得マクロ
#define get_clock_no(ch)	(sai_cfg[ch].clock_no)			// クロック番号取得マクロ

// プロトコル情報テーブル
static const SAI_PROTCOL sai_protcol[SAI_FMT_MAX] =
{
	{AFRCR_FSOFF_DELAY,     AFRCR_FSPOL_ACTIVE_LOW},		// SAI_FMT_I2S
	{AFRCR_FSOFF_NOT_DELAY, AFRCR_FSPOL_ACTIVE_HIGH},		// SAI_FMT_LSB_JUSTIFIED
};

// データ幅変換情報テーブル
static const uint32_t sai_conv_data_width[SAI_DATA_WIDTH_MAX] =
{
	8U,		// SAI_DATA_WIDTH_8
	10U,	// SAI_DATA_WIDTH_10
	16U,	// SAI_DATA_WIDTH_16
	20U,	// SAI_DATA_WIDTH_20
	24U,	// SAI_DATA_WIDTH_24
	32U,	// SAI_DATA_WIDTH_32
};

// データ幅からレジスタ設定値に変換するテーブル
static const uint32_t sai_conv_ds_reg[SAI_DATA_WIDTH_MAX] = 
{
	0x00000002UL,	// SAI_DATA_WIDTH_8
	0x00000003UL,	// SAI_DATA_WIDTH_10
	0x00000004UL,	// SAI_DATA_WIDTH_16
	0x00000005UL,	// SAI_DATA_WIDTH_20
	0x00000006UL,	// SAI_DATA_WIDTH_24
	0x00000007UL,	// SAI_DATA_WIDTH_32
};

// 転送情報
typedef struct {
	SAI_MODE        mode;				// モード
	SAI_DATA_WIDTH  data_width;			// データ幅
	uint8_t         fifo_empty_size;	// 割込み時のFIFOの空き段数
	RING_BUF        s_buf;				// 送信データ
	RING_BUF        r_buf;				// 受信データ (未使用)
} SAI_TX_INFO;

// SAI制御ブロック
typedef struct {
	uint8_t				status;			// 状態
	SAI_TX_INFO			tx_info;		// 転送情報
} SAI_CTL;
static SAI_CTL sai_ctl[SAI_CH_MAX];
#define get_myself(n) (&sai_ctl[(n)])

// 割込み共通ハンドラ
static void sai_common_handler(SAI_CH ch)
{
	volatile struct stm32l4_sai *sai_base_addr;
	SAI_CTL *this;
	SAI_TX_INFO *tx_info;
	RING_BUF *buf_info;
	uint8_t i;
	
	// ベースレジスタのアドレスを取得
	sai_base_addr = get_reg(ch);
	
	// オーバーラン/アンダーランフラグをクリア
	sai_base_addr->aclrfr |= ACLRFR_COVRUDR;
	
	// FIQ割込みが入ってきたら
	if ((sai_base_addr->aim & AIM_FRQIE) && (sai_base_addr->asr & ASR_FREQ)) {
		// 制御ブロック取得
		this = get_myself(ch);
		tx_info = &(this->tx_info);
		buf_info = &(tx_info->s_buf);
		// FIFOの空き数分データを送信
		for (i = 0; i < tx_info->fifo_empty_size; i++) {
			// 送信用リングバッファにデータがある？
			if (buf_info->w_idx != buf_info->r_idx) {
				sai_base_addr->adr = buf_info->buf[buf_info->r_idx];
				// リードインデックスを進める
				buf_info->r_idx = (buf_info->r_idx + 1U) & (SAI_BUF_SIZE - 1U);
			// 送信リングバッファにデータがない場合
			} else {
				// 割込みを無効にする
				sai_base_addr->aim &= ~AIM_FRQIE;
				// 状態を更新
				this->status = ST_OPENED;
				break;
			}
		}
	}
}

// 割込みハンドラ
void sai1_handler(void){
	sai_common_handler(SAI_CH1);
}

// 内部関数
// レジスタ設定関数
// 前提
// SAIは1chに対して、SAI_A、SAI_Bと2つのBlockを持っているが、SAI_Aのみ使用する。
// I2S、LSB-Justified、MSB-Justifiedのみ対応。(SPIDF、PDMは非対応)
static int32_t set_config(SAI_CH ch, SAI_OPEN *par)
{
	volatile struct stm32l4_sai *sai_base_addr;
	SAI_CTL *this;
	SAI_TX_INFO *tx_info;
	uint32_t sai_clock;
	uint32_t mckdiv;
	uint32_t tmp_flame_sync_active_length;
	uint32_t flame_sync_active_length;
	uint32_t flame_length;
	uint32_t fboff = 0UL;
	uint32_t tmp_acr1 = 0UL;
	uint32_t tmp_acr2 = 0UL;
	uint32_t tmp_afrcr = 0UL;
	uint32_t tmp_aslotr = 0UL;
	uint8_t i;
	
	// パラメータチェック
	if ((par->mode >= SAI_MODE_MAX) || (par->fmt >= SAI_FMT_MAX) || 
	    (par->packing >= SAI_PACK_MAX) || (par->width >= SAI_DATA_WIDTH_MAX)) {
		return -1;
	}
	
	// ベースレジスタのアドレスを取得
	sai_base_addr = get_reg(ch);
	
	// 制御ブロック取得
	this = get_myself(ch);
	tx_info = &(this->tx_info);
	
	// クロックの設定
	// SAIのペリフェラルクロックを取得
	sai_clock = HAL_RCCEx_GetPeriphCLKFreq(get_clock_no(ch));
	// Master Clockの設定
    /* Configure Master Clock Divider using the following formula :
       - If NOMCK = 1 : MCLKを使用しない
         MCKDIV[5:0] = SAI_CK_x / (FS * (FRL + 1))
       - If NOMCK = 0 : MCLKを使用する
         MCKDIV[5:0] = SAI_CK_x / (FS * (OSR + 1) * 256) */
	// Master Clockを使用する場合
	if (par->is_mclk_used) {
		// クロックの設定
		// MCLK(Master Clock)  = 256 * サンプリング周波数 (OSR = 0固定)
		// MCLK(Master Clock)  = SAIのクロック / MCKDIV
		// サンプリング周波数  = SAIのクロック / (256 * MCKDIV)
		// SCK(ビットクロック) = MCLK * (FRL + 1) / 256
		// MCKDIV = SAIのクロック / (FS * 256)
		// (*)SAIのクロックが現状12MHzのため、FSは15.625kHz、23.4375kHzの2パターンしかない。
		// (*)したがって、サンプリング周波数は16、24kHzしか対応できていない。
		// MCLKを有効
		tmp_acr1 &= ~ACR1_NOMCK;
		// 分周比を計算
		mckdiv = (uint32_t)((sai_clock/(par->fs * 256)) + 1U) & 0x1F;
		tmp_acr1 |= (mckdiv << ACR1_MCKDIV_POS);
		
		// Flame Lengthの設定
		// Master CLockを使用する場合、Flame Lengthが2の乗数である必要がある
		// このため、データ幅を超える最小の2の乗数を計算
		// 8 < Flame Length < 256
		for (i = 3; i < 8; i++) {
			tmp_flame_sync_active_length = 1UL << i;
			// 2の乗数がデータ幅を超えた？
			if (tmp_flame_sync_active_length >= sai_conv_data_width[par->width]) {
				break;
			}
		}
		// 1ch分の幅を設定
		flame_sync_active_length = tmp_flame_sync_active_length;
		
		// 1ch分の長さとデータ幅が違う場合
		if (flame_sync_active_length != sai_conv_data_width[par->width]) {
			// MSB Firstの場合
			if (par->packing == SAI_PACK_MSB_FIRST) {
				// 1SLOTのデータサイズを設定する
				for (i = 0; i < SAI_DATA_WIDTH_MAX; i++) {
					if (flame_sync_active_length == sai_conv_data_width[i]) {
						break;
					}
				}
				if (i == SAI_DATA_WIDTH_MAX) {
					// ありえないが、一応無限ループさせておく
					while(1) {};
				}
				tmp_acr1 |= (sai_conv_ds_reg[i] << ACR1_DS_POS);
				
				// 1SLOTの送信開始bitを決定する
				fboff = flame_sync_active_length - sai_conv_data_width[par->width];
			} else {
				// LSB Firstの場合は特に何もする必要なし
				
			}
		} else {
			// 1ch分の長さとデータ幅が同じ場合は特に何もする必要なし
			;
		}
	// Master Clockを使用しない場合
	} else {
		// クロックの設定
		// SCK(ビットクロック) = SAIのクロック / MCKDIV
		// サンプリング周波数  = SAIのクロック / (FRL + 1) * MCKDIV
		// MCKDIV = SAIのクロック / (FRL + 1) * サンプリング周波数
		// 8 < Flame Length < 256 (*)Flame length = FRL + 1
		// MCLKを無効
		tmp_acr1 |= ACR1_NOMCK;
		// 1ch分の幅を設定
		flame_sync_active_length = sai_conv_data_width[par->width];
		// 分周比を計算
		mckdiv = (uint32_t)((sai_clock/(par->fs * (flame_sync_active_length * 2))) + 1U) & 0x1F;
	}
	
	// ステレオの場合
	if (par->mode == SAI_MODE_STEREO) {
		// ステレオに設定
		tmp_acr1 &= ~ACR1_MONO;
		tx_info->mode = SAI_MODE_STEREO;
	// モノラルの場合
	} else {
		// モノラルに設定
		tmp_acr1 |= ACR1_MONO;
		tx_info->mode = SAI_MODE_MONAURAL;
	}
	
	// データの詰め方の設定
	// LSB Firstの場合
	if (par->packing == SAI_PACK_LSB_FIRST) {
		tmp_acr1 |= ACR1_LSBFIRST;
	// MSB Firstの場合
	} else {
		tmp_acr1 &= ~ACR1_LSBFIRST;
	}
	
	// FIFOに半分の空き(4)ができたときに割込み、または、DMA要求を出す設定にする
	tmp_acr2 |= 2UL;
	tx_info->fifo_empty_size = SAI_FIFO_SIZE/2;
	
	// Free Protocolを使用
	// とりあえずMaster Transmitのみ対応
	tmp_afrcr |= AFRCR_FSDEF | sai_protcol[par->fmt].fsoff | sai_protcol[par->fmt].fspol;
	
	// Flame Lengthを設定
	flame_length = flame_sync_active_length * 2;
	
	// レジスタ設定値を作成
	tmp_afrcr |= ((flame_sync_active_length - 1U) << AFRCR_FSALL_POS) | ((flame_length - 1U) << AFRCR_FRL_POS);
	
	// データサイズを設定
	tmp_acr1 |= (sai_conv_ds_reg[par->width] << ACR1_DS_POS);
	
	// フォーマットによって1スロットの送信開始bitを設定
	if (par->fmt == SAI_FMT_MSB_JUSTIFIED) {
		if (flame_sync_active_length != sai_conv_data_width[par->width]) {
			// 1SLOTの送信開始bitを決定する
			fboff = flame_sync_active_length - sai_conv_data_width[par->width];
		}
	} else {
		// 右寄せ以外は特に何もする必要なし
		;
	}
	
	// スロットは2つ固定 (*)Rchで1スロット、Lchで1スロット (*)SLOTの数は左右で半々になる
	// (*)MONAURALの場合は、1つ目のスロットのデータが2つ目のデータにコピーされる
	tmp_aslotr |= (3UL << ASLOTR_SLOTEN_POS) | (1UL << ASLOTR_NBSLOT_POS) | (fboff << ASLOTR_FBOFF_POS);
	
	// レジスタへ値を設定
	sai_base_addr->acr1 = tmp_acr1;
	sai_base_addr->acr2 = tmp_acr2;
	sai_base_addr->afrcr = tmp_afrcr;
	sai_base_addr->aslotr = tmp_aslotr;
	
	// 割り込み有効
	HAL_NVIC_SetPriority(SAI1_IRQn, INTERRPUT_PRIORITY_5, 0);
	HAL_NVIC_EnableIRQ(SAI1_IRQn);
	
	return 0;
}

// SAI有効化
static void start_sai(SAI_CH ch)
{
	volatile struct stm32l4_sai *sai_base_addr;
	
	// ベースレジスタのアドレスを取得
	sai_base_addr = get_reg(ch);
	
	// SAIを有効化
	sai_base_addr->acr1 |= ACR1_SAIEN;
}

// 外部公開関数
// SAI初期化関数
void sai_init(void)
{
	uint32_t ch;
	SAI_CTL *this;
	
	for (ch = 0; ch < SAI_CH_MAX; ch++) {
		// 制御ブロック取得
		this = get_myself(ch);
		// 制御ブロック初期化
		memset(this, 0, sizeof(SAI_CTL));
		// 割込みハンドラ登録
		kz_setintr(get_vec_no(ch), get_handler(ch));
		// 状態を更新
		this->status = ST_INTIALIZED;
	}
	
	return;
}

// SAIオープン関数
int32_t sai_open(SAI_CH ch, SAI_OPEN *par)
{
	SAI_CTL *this;
	
	// chが範囲外であれば、エラーを返して終了
	if (ch >= SAI_CH_MAX) {
		return -1;
	}
	
	// parがNULLであれば、エラーを返して終了
	if (par == NULL) {
		return -1;
	}
	
	// 制御ブロック取得
	this = get_myself(ch);
	
	// 初期化未実施であれば、エラーを返して終了
	if (this->status != ST_INTIALIZED) {
		return -1;
	}
	
	// レジスタ設定
	if (set_config(ch, par) != 0) {
		return -1;
	}
	
	// SAI有効化
	start_sai(ch);
	
	// 状態を更新
	this->status = ST_OPENED;
	
	return 0;
}

// SAI送信関数
int32_t sai_send(SAI_CH ch, uint32_t *data, uint32_t size)
{
	volatile struct stm32l4_sai *sai_base_addr;
	SAI_CTL *this;
	RING_BUF *buf_info;
	SAI_TX_INFO *tx_info;
	uint16_t i;
	
	// chが範囲外であれば、エラーを返して終了
	if (ch >= SAI_CH_MAX) {
		return -1;
	}
	
	// parがNULLであれば、エラーを返して終了
	if (data == NULL) {
		return -1;
	}
	
	// 制御ブロック取得
	this = get_myself(ch);
	tx_info = &(this->tx_info);
	
	// オープン未実施であれば、エラーを返して終了
	if (this->status != ST_OPENED) {
		return -1;
	}
	// 割込み禁止
	INTR_DISABLE;
	
	// バッファを取得
	buf_info = &(tx_info->s_buf);
	
	// 送信データを設定
	for (i = 0; i < size; i++) {
		buf_info->buf[buf_info->w_idx] = data[i];
		// ライトインデックスを進める
		buf_info->w_idx = (buf_info->w_idx + 1U) & (SAI_BUF_SIZE - 1U);
	}
	
	// ベースレジスタのアドレスを取得
	sai_base_addr = get_reg(ch);
	
	// 割り込み有効
	sai_base_addr->aim |= AIM_FRQIE;
	
	// 状態を更新
	this->status = ST_RUN;
	
	// 割込み禁止解除
	INTR_ENABLE;
	
	return 0;
}
