#ifndef PIN_FUNCTION_H_
#define PIN_FUNCTION_H_

typedef struct {
	GPIO_TypeDef	 	*pin_group;
	GPIO_InitTypeDef	pin_cfg;
	GPIO_PinState		pin_state;
} PIN_FUNC_INFO;

// 公開関数
void pin_function_init(void);

#endif /* PIN_FUNCTION_H_ */
