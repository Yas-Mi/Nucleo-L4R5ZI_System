/*
 * octspi.h
 *
 *  Created on: 2022/08/19
 *      Author: User
 */
#if 0
#ifndef DRV_OCTSPI_H_
#define DRV_OCTSPI_H_

#define OCTSPI1 (0)
#define OCTSPI2 (1)
#define OCTSPI_NUM (2)

/* functional_mode */
#define REGULAR_COMMAND_PROTOCOL (0x00000000)
#define HYPERBUS_PROTOCOL        (0x00000000)
/* operating_mode */
#define INDIRECT_MODE_READ (0x10000000)
#define INDIRECT_MODE_WRITE (0x00000000)
#define AUTMATIC_STATUSPOLLING_MODE (0x20000000)
#define MEMORYMAPPDE_MODE (0x30000000)

/* interface */
#define SINGLE_SPI_MODE (1)
#define DUAL_SPI_MODE   (2)
#define QUAD_SPI_MODE   (3)
#define OCTAL_SPI_MODE  (4)
/* address_size */
#define ADDRES_SIZE_8  (0)
#define ADDRES_SIZE_16 (1)
#define ADDRES_SIZE_24 (2)
#define ADDRES_SIZE_32 (3)

typedef struct{
	uint32_t ch;
	uint32_t functional_mode;
	uint32_t operating_mode;
	uint32_t interface;
	uint32_t bps;
	uint32_t devsize;
	uint32_t chip_select_high_time;
	uint32_t delay_block_bypass;
	uint32_t clock_mode;
	uint32_t sample_shift_en;
}OCTSPI_OPEN_CFG;

typedef struct{
	uint32_t instruction;
	uint32_t instruction_size;
	uint8_t  instruction_en;
	uint32_t data_size;
	uint8_t  data_en;
	uint32_t address;
	uint32_t address_size;
	uint8_t  address_en;
	uint32_t alternate;
	uint32_t alternate_size;
	uint8_t  alternate_en;
	uint32_t dummy_cycle;
	uint8_t  dummy_cycle_en;
	uint8_t  *p_data;
}OCTSPI_COMMAND;

typedef void (*OCTSPI_RECV_CALLBACK)(uint32_t ch, uint8_t *data, uint8_t size);
typedef void (*OCTSPI_SEND_CALLBACK)(uint32_t ch);

void octspi1_open(OCTSPI_OPEN_CFG *cfg);

#endif /* DRV_OCTSPI_H_ */
#endif
