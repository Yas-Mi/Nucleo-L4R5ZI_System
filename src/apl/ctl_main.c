/*
 * ctl_main.c
 *
 *  Created on: 2022/09/22
 *      Author: User
 */
#include "defines.h"
#include "kozos.h"
#include "console.h"
#include "bt_dev.h"
#include "btn_dev.h"
#include "lcd_app.h"
#include "ctl_main.h"

// マクロ定義
#define	CYC_TASK_PERIOD			(10U)	//!< 周期メッセージを送信するタスクの周期[ms]
#define	CTL_TASK_EVENT_PERIOD	(100U)	//!< 制御タスクのイベント送信周期[ms]
#define	QUE_NUM					(1U)	//!< キューの数
#define MODE_SELECT_TIME		(1000U)	//!< 起動画面からモードセレクト画面までの時間

// 状態
enum State_Type {
	ST_UNINITIALIZED = 0U,	//!< 初期状態
	ST_START_UP,			//!< 起動状態
	ST_SELECT_MODE,	        //!< モード選択画面
	ST_RUN,					//!< 動作中
	ST_UNDEIFNED,			//!< 未定義
	ST_MAX,					//!< 最大値
};

// メッセージ
enum MSG_Type {
	MSG_INIT = 0U,		//!< 初期化要求メッセージ
	MSG_EVENT,			//!< イベント発生メッセージ
	MSG_MAX,		//!< 最大値
};

// 関数ポインタ
typedef void (*INIT_TSK)(void);

// リスト
typedef struct lst {
	struct lst *next;
	CYC_MSG cyc_msg;
	uint32_t period;
	uint32_t remain_period;
} LIST;

// 状態遷移用定義
typedef struct {
	CTL_FUNC func;
	uint8_t  nxt_state;
}FSM;

// 制御用ブロック
typedef struct {
	kz_thread_id_t tsk_id;      // メインタスクのID
	kz_msgbox_id_t msg_id;      // メッセージID
	uint8_t state;              // 状態
	uint8_t req_chg_sts;		// 状態変更要求フラグ
	uint8_t mode_select_timer;  // 起動画面からモードセレクト画面までの時間をカウントするためのタイマー
} CTL_CTL;

// 周期メッセージ管理用のキュー
static struct {
  LIST *head;
  LIST *tail;
} cycmsg_que[QUE_NUM];

// タスク初期化関数テーブル
static const INIT_TSK init_tsk_func[] =
{
	//US_MSG_init,    // 超音波データ取得タスク初期化関数
	LCD_APP_MSG_init, // LCDアプリタスク初期化関数
};

// プロトタイプ宣言
static void init_apl_tsk(void* info);
static void ctl_disp_start_up(void* info);
static void ctl_disp_mode_select(void* info);
static void ctl_btn(void* info);

// 状態遷移テーブル
static const FSM fsm[ST_MAX][MSG_MAX] = {
	// MSG_INIT                    MSG_EVENT								 MSG_MAX
	{{init_apl_tsk, ST_START_UP}, {NULL, ST_UNDEIFNED},  					{NULL, ST_UNDEIFNED}, },	// ST_UNINITIALIZED
	{{NULL, ST_UNDEIFNED},        {ctl_disp_start_up, ST_SELECT_MODE}, 		{NULL, ST_UNDEIFNED}, },	// ST_START_UP
	{{NULL, ST_UNDEIFNED},        {ctl_disp_mode_select, ST_SELECT_MODE},	{NULL, ST_UNDEIFNED}, },	// ST_SELECT_MODE
	{{NULL, ST_UNDEIFNED},        {NULL, ST_UNDEIFNED},						{NULL, ST_UNDEIFNED}, },	// ST_RUN
};

// 制御ブロックの実体
static CTL_CTL ctl_ctl;

// 内部関数
// 状態変更関数
static void ctl_main_chg_sts(uint8_t req_sts) 
{
	CTL_CTL *this = &ctl_ctl;
	
	// 状態を更新する
	this->state = req_sts;
	this->req_chg_sts = 1U;
}

// 起動画面表示関数
static void ctl_disp_start_up(void* info) 
{
	// LCDアプリに起動画面表示メッセージを送る
	LCD_APP_MSG_start_up();
}

// モードセレクト表示関数
static void ctl_disp_mode_select(void* info) 
{
	CTL_CTL *this = &ctl_ctl;
	
	// 起動画面からモードセレクト画面までの時間経過したときにモードセレクト表示メッセージを送る
	if (this->mode_select_timer > (MODE_SELECT_TIME/CTL_TASK_EVENT_PERIOD)) {
		// 状態を更新する
		ctl_main_chg_sts(ST_RUN);
		// 100msのイベントを削除
		del_cyclic_message(CTL_MSG_event);
		// メッセージを送信
		LCD_APP_MSG_select_mode();
	} else {
		// カウンタを進める
		this->mode_select_timer++;
	}
}

/*!
 @brief 全タスクを初期化する
 @param [in] argc
 @param [in] *argv[]
 */
static void init_apl_tsk(void* info)
{
	uint8_t i;
	uint8_t size;
	uint32_t ret;
	
	// サイズを計算
	size = sizeof(init_tsk_func)/sizeof(init_tsk_func[0]);
	
	// タスクの初期化関数をコール
	for (i = 0; i < size; i++) {
		init_tsk_func[i]();
	}
	
	// 自身のタスクが100ms周期で動作するように設定
	ret = set_cyclic_message(CTL_MSG_event, CTL_TASK_EVENT_PERIOD);
	
	return;
}

/*!
 @brief システムを制御するためのタスク
 @param [in] argc
 @param [in] *argv[]
 */
int ctl_main(int argc, char *argv[])
{
	CTL_CTL *this = &ctl_ctl;
	uint8_t nxt_state;
	uint32_t size;
	CTL_MSG *msg;
	CTL_FUNC func;
	
	// 制御ブロックの初期化
	memset(this, 0, sizeof(CTL_CTL));
	
	// 制御ブロックの設定
	this->tsk_id = kz_getid();
	this->msg_id = MSGBOX_ID_CTL_MAIN;
	this->state  = ST_UNINITIALIZED;
	
	while(1){
		// メッセージ受信
		kz_recv(this->msg_id, &size, &msg);
		
		// 処理/次状態を取得
		func = fsm[this->state][msg->msg_type].func;
		nxt_state = fsm[this->state][msg->msg_type].nxt_state;
		
		// 現状態を更新
		this->state = nxt_state;

		// メッセージを解放
	    kz_kmfree(msg);

		// 処理を実行
		if (func != NULL) {
			func(msg->msg_data);
		}
		
		// 状態遷移
		if ((nxt_state != ST_UNDEIFNED) && (this->req_chg_sts == 0U)) {
			this->state = nxt_state;
		} else if (this->req_chg_sts == 1U) {
			this->req_chg_sts = 0U;
		}
	}
}

/*!
 @brief 周期メッセージを送信するタスク
 @param [in] argc
 @param [in] *argv[]
 */
int ctl_cycmsg_main(int argc, char *argv[])
{
	LIST *node;
	LIST *prev_node;
	
	while(1){
		// TASK_PERIODの期間スリープ
		kz_tsleep(CYC_TASK_PERIOD);

		node = cycmsg_que[0].head;
		prev_node = cycmsg_que[0].head;

		// コールするタイミングの周期メッセージを検索
		while (node != NULL) {
			// 指定された周期以下になっている場合
			if (node->remain_period <= CYC_TASK_PERIOD) {
				// 指定された周期メッセージをコール
				node->cyc_msg();
				// リストから削除
				//prev_node->next = node->next;
				// ノードを削除
				//kz_kmfree(node);
				node->remain_period = node->period;
			// 指定された周期になっていない場合
			} else {
				// 指定された周期からタスクの周期を引く
				node->remain_period -= CYC_TASK_PERIOD;
			}
			// 更新
			prev_node = node;
			node = node->next;
		}
	}
	
	return 0;
}

/*! 
 @brief 周期メッセージを登録する
 @param [in] cyclic_message 周期メッセージ
 @param [in] period         周期
 @return     -1             登録できなかった
 @return      0             登録できた
*/
uint32_t set_cyclic_message(CYC_MSG cyclic_message, uint32_t period)
{
	LIST *new_node;
	LIST *node;
	
	// パラメータチェック
	// 周期メッセージがNULLの場合、エラーを返して終了
	if (cyclic_message == NULL) {
		return -1;
	}
	
	// 新しいノードを作成
	new_node = kz_kmalloc(sizeof(LIST));
	
	// 周期メッセージと周期を設定
	new_node->cyc_msg       = cyclic_message;
	new_node->period        = period;
	new_node->remain_period = period;
	new_node->next          = NULL;
	
	// リストの最後を検索
	node = cycmsg_que[0].head;
	while (node != NULL) {
		node = cycmsg_que[0].head->next;
	}
	
	// リストの最後に、ノードを追加
	node = new_node;
	cycmsg_que[0].head = new_node;
	
	return 0;
}

/*! 
 @brief 周期メッセージを削除
 @param [in] cyclic_message 周期メッセージ
 @param [in] period         周期
 @return     -1             登録できなかった
 @return      0             登録できた
*/
uint32_t del_cyclic_message(CYC_MSG cyclic_message)
{
	LIST **prev_node;
	LIST **node;

	// パラメータチェック
	// 周期メッセージがNULLの場合、エラーを返して終了
	if (cyclic_message == NULL) {
		return -1;
	}

	// 検索
	node = &(cycmsg_que[0].head);
	prev_node = &(cycmsg_que[0].head);
	while (*node != NULL) {
		if ((*node)->cyc_msg == cyclic_message) {
			break;
		}
		prev_node = node;
		node = &(cycmsg_que[0].head->next);
	}

	if (*node == NULL) {
		return -1;
	}

	// リストから削除
	*prev_node = (*node)->next;

	return 0;
}

/*!
 @brief システム制御タスクに初期化メッセージを送信する関数
 @param [in] なし
 @return     なし
*/
void CTL_MSG_init(void)
{
	CTL_CTL *this = &ctl_ctl;
	CTL_MSG *msg;
	
	msg = kz_kmalloc(sizeof(CTL_MSG));
	
	msg->msg_type = MSG_INIT;
	msg->msg_data = NULL;
	
	kz_send(this->msg_id, sizeof(CTL_MSG), msg);
	
	return;
}

/*! 
 @brief システム制御タスクに初期化メッセージを送信する関数
 @param [in] なし
 @return     なし
*/
void CTL_MSG_event(void)
{
	CTL_CTL *this = &ctl_ctl;
	CTL_MSG *msg;

	msg = kz_kmalloc(sizeof(CTL_MSG));
	
	msg->msg_type = MSG_EVENT;
	msg->msg_data = NULL;
	
	kz_send(this->msg_id, sizeof(CTL_MSG), msg);
	
	return;
}
