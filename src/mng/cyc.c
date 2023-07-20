/*
 * ctl_main.c
 *
 *  Created on: 2022/09/22
 *      Author: User
 */
#include "defines.h"
#include "kozos.h"
#include "console.h"
#include "cyc.h"

// 状態
#define ST_NOT_INITIALIZED	(0)		// 未初期
#define ST_INITIALIZED		(1)		// 初期化済み

// マクロ定義
#define	CYC_TASK_PERIOD			(5U)	//!< 周期メッセージを送信するタスクの周期[ms]
#define	QUE_NUM					(1U)	//!< キューの数

// リスト
typedef struct lst {
	struct lst *next;
	CYC_MSG cyc_msg;
	uint32_t period;
	uint32_t remain_period;
} LIST;

// 周期メッセージ管理用のキュー
static struct {
  LIST *head;
  LIST *tail;
} cycmsg_que[QUE_NUM];

// 制御用ブロック
typedef struct {
	uint32_t state;				// 状態
	kz_thread_id_t tsk_id;      // メインタスクのID
} CYC_CTL;
static CYC_CTL cyc_ctl;		// 制御ブロックの実体

/*!
 @brief 周期メッセージを送信するタスク
 @param [in] argc
 @param [in] *argv[]
 */
int cycmsg_main(int argc, char *argv[])
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
	
	if (cycmsg_que[0].tail) {
		cycmsg_que[0].tail->next = new_node;
	} else {
		cycmsg_que[0].head = new_node;
	}
	cycmsg_que[0].tail = new_node;
	
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
 @brief 初期化関数
 @param [in] なし
 @return     なし
*/
void cyc_init(void)
{
	CYC_CTL *this = &cyc_ctl;
	int32_t ret;
	
	// 制御ブロックの初期化
	memset(this, 0, sizeof(CYC_CTL));
	
	// タスクの生成
	this->tsk_id = kz_run(cycmsg_main, "cyc_main",  CYC_PRI, CYC_STACK, 0, NULL);
	
	// 状態の更新
	this->state = ST_INITIALIZED;
}
