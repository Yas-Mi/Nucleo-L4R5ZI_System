/*
 * wave.c
 *
 *  Created on: 2022/12/12
 *      Author: User
 */
#include "defines.h"
#include "kozos.h"
#include "wav.h"
#include "lib.h"

// 状態
#define ST_NOT_INTIALIZED	(0U) // 未初期化
#define ST_INITIALIZED		(1U) // 初期化済み
#define ST_OPEND			(2U) // オープン済み
#define ST_FMT_ANLYSING		(3U) // フォーマット解析中
#define ST_DATA_RECEIVING	(4U) // 音楽データ受信中

// マクロ
#define WAVE_FOMART_ERR			(-1)	// フォーマットエラー
#define WAVE_FOMART_OK			(0)		// フォーマットOK
#define WAVE_FOMART_ANALYSING	(1)		// フォーマット解析中

// WAVEファイルヘッダ情報
typedef union {
	uint8_t data[44];
	struct {
		uint8_t riff_str[4];		// RIFF
		uint8_t size1[4];			// 総ファイルサイズ-8[byte]
		uint8_t wave_str[4];		// WAVE
		uint8_t fmt_str[4];			// fmt
		uint8_t fmt_size[4];		// フォーマットサイズ
		uint8_t fmt_code[2];		// フォーマットコード
		uint8_t ch_num[2];			// チャンネル数 モノラルは1、ステレオは2
		uint8_t sampling_rate[4];	// サンプリングレート
		uint8_t byte_par_sec[4];	// バイト/秒
		uint8_t blk_align[2];		// ブロック境界
		uint8_t bit_par_sample[2];	// ビット/サンプル
		uint8_t data_str[4];		// データ
		uint8_t size2[4];			// 総ファイルサイズ-126[byte]
	} info;
} WAV_HEADER;

// 制御用ブロック
typedef struct {
	uint8_t status;					// 状態
	WAV_RCV_CALLBACK rcv_callback;	// 受信コールバック
	WAV_END_CALLBACK end_callback;	// 終了コールバック
	void *callback_vp;				// コールバックパラメータ
	WAV_HEADER rcv_buf;				// 受信バッファ
	uint32_t rcv_cnt;				// 受信したサイズ
	uint32_t wave_data_size;		// 全データサイズ[byte]
	uint16_t sample_data_size;		// 1データサイズ[byte]
	uint32_t sample_data_cnt;		// 現在受信したwaveのデータサイズ[byte]
	int32_t	sample_data;			// サンプルデータ	(*) とりあえず4byte
} WAV_CTL;
static WAV_CTL wav_ctl;

// データ解析
static int32_t data_analysis(uint8_t data)
{
	WAV_CTL *this = &wav_ctl;
	
	// データ格納
	this->rcv_buf.data[this->rcv_cnt++] = data;
	
	// 4byte受信時
	if (this->rcv_cnt == 4) {
		// RIFF?
		if (memcmp(this->rcv_buf.info.riff_str, "RIFF", 4)) {
			this->rcv_cnt = 0;
			return WAVE_FOMART_ERR;
		}
	// 8byte受信時
	} else if (this->rcv_cnt == 8) {
		// 全データサイズを格納
		this->wave_data_size = ((this->rcv_buf.info.size1[0] << 0) | 
								(this->rcv_buf.info.size1[1] << 8) |
								(this->rcv_buf.info.size1[2] << 16 ) |
								(this->rcv_buf.info.size1[3] << 24 )) + 8 - sizeof(WAV_HEADER);
	// 12byte受信時
	} else if (this->rcv_cnt == 12) {
		// WAVE?
		if (memcmp(this->rcv_buf.info.wave_str, "WAVE", 4)) {
			this->rcv_cnt = 0;
			return WAVE_FOMART_ERR;
		}
	// 16byte受信時
	} else if (this->rcv_cnt == 16) {
		// fmt ?
		if (memcmp(this->rcv_buf.info.fmt_str, "fmt ", 4)) {
			this->rcv_cnt = 0;
			return WAVE_FOMART_ERR;
		}
	// 20byte受信時
	} else if (this->rcv_cnt == 20) {
		// フォーマットサイズ
		// デフォルトは16
	// 22byte受信時
	} else if (this->rcv_cnt == 22) {
		// フォーマットコード
		// 非圧縮のPCMフォーマットは1
	// 24byte受信時
	} else if (this->rcv_cnt == 24) {
		// チャネル数
		// モノラルは1、ステレオは2
	// 28byte受信時
	} else if (this->rcv_cnt == 28) {
		// サンプリングレート
		// 44.1kHzの場合なら44100
	// 32byte受信時
	} else if (this->rcv_cnt == 32) {
		// バイト／秒
		// １秒間の録音に必要なバイト数
	// 34byte受信時
	} else if (this->rcv_cnt == 34) {
		// ブロック境界
		// ステレオ16bitなら、16bit*2 = 32bit = 4byte
	// 36byte受信時
	} else if (this->rcv_cnt == 36) {
		// ビット／サンプル
		// １サンプルに必要なビット数
		this->sample_data_size = ((this->rcv_buf.info.bit_par_sample[1] << 8)|(this->rcv_buf.info.bit_par_sample[0] << 0))/8;
	// 40byte受信時
	} else if (this->rcv_cnt == 40) {
		// data?
		if (memcmp(this->rcv_buf.info.data_str, "data ", 4)) {
			this->rcv_cnt = 0;
			return WAVE_FOMART_ERR;
		}
	} else if (this->rcv_cnt == 44) {
		// データサイズを格納
		// (*) -128しないといけないらしい
		// 最初にもうデータサイズは格納したので改めてここでは何もしない
		return WAVE_FOMART_OK;
	} else {
		;
	}
	return WAVE_FOMART_ANALYSING;
}

// コールバック
static void bt_dev_callback(uint8_t data, void *vp)
{
	WAV_CTL *this = (WAV_CTL*)vp;
	uint8_t byte_pos;
	int32_t ret;
	
	// オープン済み、解析中、データ受信中でなければ何もしない
	if ((this->status != ST_OPEND) &&
		(this->status != ST_FMT_ANLYSING) &&
		(this->status != ST_DATA_RECEIVING)) {
		return;
	}
	
	// データ解析
	if (this->status != ST_DATA_RECEIVING) {
		// 解析
		ret = data_analysis(data);
		// フォーマット解析中？
		if (ret == WAVE_FOMART_ANALYSING) {
			// 状態をフォーマット解析中に更新
			this->status = ST_FMT_ANLYSING;
		// フォーマットエラー？
		} else if (ret == WAVE_FOMART_ERR) {
			// 状態をオープン状態に更新
			this->status = ST_OPEND;
		// フォーマットが正しい？
		} else if (ret == WAVE_FOMART_OK) {
			// 状態をデータ受信中に更新
			this->status = ST_DATA_RECEIVING;
		} else {
			;
		}
		return;
	}
	
	// データを格納
	byte_pos = (this->sample_data_cnt++) % this->sample_data_size;
	this->sample_data |= (data << (byte_pos * 8));
	
	// 1サンプルたまった?
	if (byte_pos == (this->sample_data_size - 1)) {
		// コールバック
		if (this->rcv_callback != NULL) {
			this->rcv_callback(this->sample_data, this->callback_vp);
		}
		// データクリア
		this->sample_data = 0;
	}
	
	// 全データ受信しきった？
	if (this->sample_data_cnt >= this->wave_data_size) {
		// コールバック
		if (this->end_callback != NULL) {
			this->end_callback(this->callback_vp);
		}
		// 内部情報のクリア
		memset(&(this->rcv_buf), 0, sizeof(WAV_HEADER));
		this->rcv_cnt = 0;
		this->wave_data_size = 0;
		this->sample_data_size = 0;
		this->sample_data_cnt = 0;
		// 状態を更新
		this->status = ST_OPEND;
	}
	
	return;
}

// 外部公開関数
// 初期化処理
void wav_init(void)
{
	WAV_CTL *this = &wav_ctl;
	
	// 制御ブロックの初期化
	memset(this, 0, sizeof(WAV_CTL));
	
	// 状態を初期化済みに更新
	this->status = ST_INITIALIZED;
	
	return;
}

// オープン処理
int32_t wav_open(WAV_RCV_CALLBACK rcv_callback, WAV_END_CALLBACK end_callback, void *callback_vp)
{
	WAV_CTL *this = &wav_ctl;
	int32_t ret;
	
	// コールバック登録
	ret = bt_dev_reg_callback(bt_dev_callback, this);
	if (ret != 0) {
		return -1;
	}
	
	// コールバック登録
	this->rcv_callback = rcv_callback;
	this->end_callback = end_callback;
	this->callback_vp = callback_vp;
	
	// 状態をオープン済みに更新
	this->status = ST_OPEND;
	
	return 0;
}
