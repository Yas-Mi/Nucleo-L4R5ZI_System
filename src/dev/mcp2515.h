/*
 * mcp2515.h
 *
 *  Created on: 2024/2/12
 *      Author: User
 */

#ifndef DEV_MCP2515_H_
#define DEV_MCP2515_H_

// コールバック関数定義
typedef void (*PCM3060_CALLBACK)(void *vp);

extern void mcp2515_dev_init(void);
extern int32_t mcp2515_dev_open(void);
extern void mcp2515_set_cmd(void);
#endif /* DEV_MCP2515_H_ */