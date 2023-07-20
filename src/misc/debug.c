#include "stm32l4xx_hal_cortex.h"
#include "syscall.h"
#include "intr.h"
#include "debug.h"

#define DBG_INT_HIST_NUM	(16)	// 最新の割り込みの順番を覚えておく個数
#define DBG_TSK_HIST_NUM	(16)	// 最新のタスクの順番を覚えておく個数

#define DBG_PC_OFFSET		(15)
#define DBG_LR_OFFSET		(14)

// 割込み情報
typedef struct {
	uint32_t			time;
	softvec_type_t		int_type;
	kz_syscall_type_t	svc_type;
	uint32_t			pc;
	uint32_t			lr;
	uint32_t			icsr;
} DBG_INT_HIST;
typedef struct {
	uint32_t		int_cnt[SOFTVEC_TYPE_NUM];		// 割込みの回数
	uint32_t		svc_type[KZ_SYSCALL_TYPE_MAX];	// サービスコールの回数
	uint32_t		all_cnt;						// 全体の割り込み回数
	DBG_INT_HIST	int_hist[DBG_INT_HIST_NUM];		// 最新の割込み
	uint8_t			last_hist_idx;					// 最新の割込みインデックス
} DBG_INT_INFO;
static DBG_INT_INFO dbg_int_info;

// タスク情報(どのタスクで割込みに入ったか)
typedef struct {
	uint32_t			time;
	char				*tsk_name;
	uint32_t			pc;
	uint32_t			lr;
	uint32_t			icsr;
} DBG_TSK_HIST;
typedef struct {
	uint32_t				all_cnt;								// 全体のタスク起床回数
	DBG_TSK_HIST			tsk_hist[DBG_TSK_HIST_NUM];				// 最新のタスク
	uint8_t					last_hist_idx;							// 最新のタスクインデックス
} DBG_TSK_INFO;
static DBG_TSK_INFO dbg_tsk_info;

// タスク情報(どのタスクをディスパッチしたか)
static DBG_TSK_INFO dbg_dispatch_tsk_info;

// 初期化
void dbg_init(void)
{
	DBG_INT_INFO *int_info = &dbg_int_info;
	DBG_INT_INFO *tsk_info = &dbg_tsk_info;
	
	memset(int_info, 0, sizeof(DBG_INT_INFO));
	memset(tsk_info, 0, sizeof(DBG_TSK_INFO));
}

// 割込み
void dbg_save_int_info(softvec_type_t type, uint32_t svc_type, uint32_t sp)
{
	DBG_INT_INFO *int_info = &dbg_int_info;
	uint32_t *tmp_sp;
	
	tmp_sp = (uint32_t*)sp;
	
	// 最新の割込み
	int_info->last_hist_idx = int_info->all_cnt & (DBG_INT_HIST_NUM - 1);
	int_info->int_hist[int_info->last_hist_idx].int_type = type;
	int_info->int_hist[int_info->last_hist_idx].pc = *(tmp_sp + DBG_PC_OFFSET);
	int_info->int_hist[int_info->last_hist_idx].lr = *(tmp_sp + DBG_LR_OFFSET);
	if ((type == SVCall_INTERRUPT_NO) || (type == PENDSV_INTERRUPT_NO)) {
		int_info->int_hist[int_info->last_hist_idx].svc_type = svc_type;
	} else {
		int_info->int_hist[int_info->last_hist_idx].svc_type = KZ_SYSCALL_TYPE_MAX;
	}
	int_info->int_hist[int_info->last_hist_idx].icsr = SCB->ICSR;
	int_info->int_hist[int_info->last_hist_idx].time = kz_get_time_s()*1000 + kz_get_time_ms();
	
	
	// すべての割り込み回数をカウント
	int_info->all_cnt++;
	
	// 割り込み回数をカウント
	int_info->int_cnt[type]++;
	
	// SVC or PendSV？
	if ((type == SVCall_INTERRUPT_NO) || (type == PENDSV_INTERRUPT_NO)) {
		int_info->svc_type[svc_type]++;
	}
}

// タスク
void dbg_save_tsk_info(char *tsk_name)
{
	DBG_TSK_INFO *tsk_info = &dbg_tsk_info;
	
	// 最新のタスク
	tsk_info->last_hist_idx = tsk_info->all_cnt & (DBG_TSK_HIST_NUM - 1);
	tsk_info->tsk_hist[tsk_info->last_hist_idx].tsk_name = tsk_name;
	tsk_info->tsk_hist[tsk_info->last_hist_idx].icsr = SCB->ICSR;
	tsk_info->tsk_hist[tsk_info->last_hist_idx].time = kz_get_time_s()*1000 + kz_get_time_ms();
	
	tsk_info->all_cnt++;
}

// タスク
void dbg_save_dispatch_tsk_info(char *tsk_name, uint32_t sp)
{
	DBG_TSK_INFO *tsk_info = &dbg_dispatch_tsk_info;
	uint32_t *tmp_sp;
	
	tmp_sp = (uint32_t*)sp;
	
	// 最新のタスク
	tsk_info->last_hist_idx = tsk_info->all_cnt & (DBG_TSK_HIST_NUM - 1);
	tsk_info->tsk_hist[tsk_info->last_hist_idx].tsk_name = tsk_name;
	tsk_info->tsk_hist[tsk_info->last_hist_idx].icsr = SCB->ICSR;
	tsk_info->tsk_hist[tsk_info->last_hist_idx].time = kz_get_time_s()*1000 + kz_get_time_ms();
	tsk_info->tsk_hist[tsk_info->last_hist_idx].pc = *(tmp_sp + DBG_PC_OFFSET);
	tsk_info->tsk_hist[tsk_info->last_hist_idx].lr = *(tmp_sp + DBG_LR_OFFSET);
	
	tsk_info->all_cnt++;
}
