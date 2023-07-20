extern void dbg_init(void);
extern void dbg_save_int_info(softvec_type_t type, uint32_t svc_type, uint32_t sp);
extern void dbg_save_tsk_info(char *tsk_name);
extern void dbg_save_dispatch_tsk_info(char *tsk_name, uint32_t sp);

