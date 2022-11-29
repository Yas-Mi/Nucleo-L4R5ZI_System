/*
 * octspi.c
 *
 *  Created on: 2022/08/19
 *      Author: User
 */
/* Includes ------------------------------------------------------------------*/
#include "defines.h"
#include "kozos.h"
#include "stm32l4xx_hal_rcc.h"
#include "stm32l4xx_hal_gpio.h"
#include "stm32l4xx_hal_pwr_ex.h"
#include "stm32l4xx_hal_cortex.h"
#include "stm32l4xx.h"
#include "octspi.h"
#include "buf.h"

/* 共通マクロ */
#define BUF_SIZE (256)

/* OCTSPI ベースレジスタ */
#define OCTSPI1_BASE_ADDR (0xA0001000)
#define OCTSPI2_BASE_ADDR (0xA0001400)
#define OCTSPIM_BASE_ADDR (0x50061C00)

/* レジスタ マスク */
#define TCR_DCYC_MASK  (0x0000001F)
#define CR_FMODE_MASK  (0x30000000)
#define CR_FTHRES_MASK (0x00001F00)
#define CR_FSEL_MASK   (0x00000080)
#define DCR1_MTYPE_MASK (0x07000000)
#define DCR1_DEVSIZE_MASK (0x001F0000)
#define DCR1_PLESCALER_MASK (0x00000008)
#define CR_FSEL_MASK   (0x00000080)
#define CR_FSEL_MASK   (0x00000080)
#define CCR_DMODE_MASK    (0x07000000)
#define CCR_ABMODE_MASK   (0x00070000)
#define CCR_ADMODE_MASK   (0x00000700)
#define CCR_IMODE_MASK    (0x00000007)

/* レジスタ設定値 */
#define CR_FTIE_ENABLE (0x00040000)
#define CR_TCIE_ENABLE (0x00020000)
#define CR_TEIE_ENABLE (0x00010000)
#define CR_EN_ENABLE   (0x00000001)
#define DCR1_MTYPE_STANDARDMODE (0x02000000)
#define TCR_SSHIFT_ENABLE (0x40000000)

/* 状態 */
#define IDLE (0)
#define RX   (1)
#define TX   (2)

/* USARTレジスタ情報 */
struct stm32l4_octspi {
  volatile uint32_t cr;           /* ofs:0x0000 */
  volatile uint32_t reserved1;    /* ofs:0x0004 */
  volatile uint32_t dcr1;         /* ofs:0x0008 */
  volatile uint32_t dcr2;         /* ofs:0x000C */
  volatile uint32_t dcr3;         /* ofs:0x0010 */
  volatile uint32_t dcr4;         /* ofs:0x0014 */
  volatile uint32_t reserved2;    /* ofs:0x0018 */
  volatile uint32_t reserved3;    /* ofs:0x001C */
  volatile uint32_t sr;           /* ofs:0x0020 */
  volatile uint32_t fcr;          /* ofs:0x0024 */
  volatile uint32_t reserved3;    /* ofs:0x0028 */
  volatile uint32_t reserved4;    /* ofs:0x002C */
  volatile uint32_t reserved5;    /* ofs:0x0030 */
  volatile uint32_t reserved6;    /* ofs:0x0034 */
  volatile uint32_t reserved7;    /* ofs:0x0038 */
  volatile uint32_t reserved8;    /* ofs:0x003C */
  volatile uint32_t dlr      ;    /* ofs:0x0040 */
  volatile uint32_t reserved10;   /* ofs:0x0044 */
  volatile uint32_t ar;           /* ofs:0x0048 */
  volatile uint32_t reserved11;   /* ofs:0x004C */
  volatile uint32_t dr;           /* ofs:0x0050 */
  volatile uint32_t reserved12;   /* ofs:0x0054 */
  volatile uint32_t reserved13;   /* ofs:0x0058 */
  volatile uint32_t reserved13;   /* ofs:0x005C */
  volatile uint32_t reserved14;   /* ofs:0x0060 */
  volatile uint32_t reserved15;   /* ofs:0x0064 */
  volatile uint32_t reserved16;   /* ofs:0x0068 */
  volatile uint32_t reserved17;   /* ofs:0x006C */
  volatile uint32_t reserved18;   /* ofs:0x0070 */
  volatile uint32_t reserved19;   /* ofs:0x0074 */
  volatile uint32_t reserved20;   /* ofs:0x0078 */
  volatile uint32_t psmkr;        /* ofs:0x0080 */
  volatile uint32_t reserved21;   /* ofs:0x0084 */
  volatile uint32_t psmar;        /* ofs:0x0088 */
  volatile uint32_t reserved22;   /* ofs:0x008C */
  volatile uint32_t pir;          /* ofs:0x0090 */
  volatile uint32_t reserved23;   /* ofs:0x0094 */
  volatile uint32_t reserved24;   /* ofs:0x0098 */
  volatile uint32_t reserved25;   /* ofs:0x009C */
  volatile uint32_t reserved26;   /* ofs:0x00A0 */
  volatile uint32_t reserved27;   /* ofs:0x00A4 */
  volatile uint32_t reserved28;   /* ofs:0x00A8 */
  volatile uint32_t reserved29;   /* ofs:0x00AC */
  volatile uint32_t reserved30;   /* ofs:0x00B0 */
  volatile uint32_t reserved31;   /* ofs:0x00B4 */
  volatile uint32_t reserved32;   /* ofs:0x00B8 */
  volatile uint32_t reserved33;   /* ofs:0x00BC */
  volatile uint32_t reserved34;   /* ofs:0x00C0 */
  volatile uint32_t reserved35;   /* ofs:0x00C4 */
  volatile uint32_t reserved36;   /* ofs:0x00C8 */
  volatile uint32_t reserved37;   /* ofs:0x00CC */
  volatile uint32_t reserved38;   /* ofs:0x00D0 */
  volatile uint32_t reserved39;   /* ofs:0x00D4 */
  volatile uint32_t reserved40;   /* ofs:0x00D8 */
  volatile uint32_t reserved41;   /* ofs:0x00DC */
  volatile uint32_t reserved42;   /* ofs:0x00E0 */
  volatile uint32_t reserved43;   /* ofs:0x00E4 */
  volatile uint32_t reserved44;   /* ofs:0x00E8 */
  volatile uint32_t reserved45;   /* ofs:0x00EC */
  volatile uint32_t reserved46;   /* ofs:0x00F0 */
  volatile uint32_t reserved47;   /* ofs:0x00F4 */
  volatile uint32_t reserved48;   /* ofs:0x00F8 */
  volatile uint32_t reserved49;   /* ofs:0x00FC */
  volatile uint32_t ccr;          /* ofs:0x0100 */
  volatile uint32_t reserved50;   /* ofs:0x0104 */
  volatile uint32_t tcr;          /* ofs:0x0108 */
  volatile uint32_t reserved51;   /* ofs:0x010C */
  volatile uint32_t ir;           /* ofs:0x0110 */
  volatile uint32_t reserved52;   /* ofs:0x0114 */
  volatile uint32_t reserved53;   /* ofs:0x0118 */
  volatile uint32_t reserved54;   /* ofs:0x011C */
  volatile uint32_t abr;          /* ofs:0x0120 */
  volatile uint32_t reserved55;   /* ofs:0x0124 */
  volatile uint32_t reserved56;   /* ofs:0x0128 */
  volatile uint32_t reserved57;   /* ofs:0x012C */
  volatile uint32_t lptr;         /* ofs:0x0130 */
  volatile uint32_t reserved58;   /* ofs:0x0134 */
  volatile uint32_t reserved59;   /* ofs:0x0138 */
  volatile uint32_t reserved60;   /* ofs:0x013C */
  volatile uint32_t wpcce;        /* ofs:0x0140 */
  volatile uint32_t reserved61;   /* ofs:0x0144 */
  volatile uint32_t wptcr;        /* ofs:0x0148 */
  volatile uint32_t reserved62;   /* ofs:0x014C */
  volatile uint32_t wpir;         /* ofs:0x0150 */
  volatile uint32_t reserved63;   /* ofs:0x0154 */
  volatile uint32_t reserved64;   /* ofs:0x0158 */
  volatile uint32_t reserved65;   /* ofs:0x015C */
  volatile uint32_t wpabr;        /* ofs:0x0160 */
  volatile uint32_t reserved66;   /* ofs:0x0164 */
  volatile uint32_t reserved67;   /* ofs:0x0168 */
  volatile uint32_t reserved68;   /* ofs:0x016C */
  volatile uint32_t reserved69;   /* ofs:0x0170 */
  volatile uint32_t reserved70;   /* ofs:0x0174 */
  volatile uint32_t reserved71;   /* ofs:0x0178 */
  volatile uint32_t reserved72;   /* ofs:0x017C */
  volatile uint32_t wccr;         /* ofs:0x0180 */
  volatile uint32_t reserved73;   /* ofs:0x0184 */
  volatile uint32_t wtcr;         /* ofs:0x0188 */
  volatile uint32_t reserved74;   /* ofs:0x018C */
  volatile uint32_t wir;          /* ofs:0x0190 */
  volatile uint32_t reserved75;   /* ofs:0x0194 */
  volatile uint32_t reserved76;   /* ofs:0x0198 */
  volatile uint32_t reserved77;   /* ofs:0x019C */
  volatile uint32_t wabr;         /* ofs:0x01A0 */
  volatile uint32_t reserved78;   /* ofs:0x01A4 */
  volatile uint32_t reserved79;   /* ofs:0x01A8 */
  volatile uint32_t reserved80;   /* ofs:0x01AC */
  volatile uint32_t reserved81;   /* ofs:0x01B0 */
  volatile uint32_t reserved82;   /* ofs:0x01B4 */
  volatile uint32_t reserved83;   /* ofs:0x01B8 */
  volatile uint32_t reserved84;   /* ofs:0x01BC */
  volatile uint32_t reserved85;   /* ofs:0x01C0 */
  volatile uint32_t reserved86;   /* ofs:0x01C4 */
  volatile uint32_t reserved87;   /* ofs:0x01C8 */
  volatile uint32_t reserved88;   /* ofs:0x01CC */
  volatile uint32_t reserved90;   /* ofs:0x01D0 */
  volatile uint32_t reserved91;   /* ofs:0x01D4 */
  volatile uint32_t reserved92;   /* ofs:0x01D8 */
  volatile uint32_t reserved93;   /* ofs:0x01DC */
  volatile uint32_t reserved94;   /* ofs:0x01E0 */
  volatile uint32_t reserved95;   /* ofs:0x01E4 */
  volatile uint32_t reserved96;   /* ofs:0x01E8 */
  volatile uint32_t reserved97;   /* ofs:0x01F0 */
  volatile uint32_t reserved98;   /* ofs:0x01F4 */
  volatile uint32_t reserved99;   /* ofs:0x01F8 */
  volatile uint32_t reserved100;  /* ofs:0x01FC */
  volatile uint32_t HLCR;         /* ofs:0x0200 */
};

struct stm32l4_octspim {
  volatile uint32_t cr;           /* ofs:0x0000 */
  volatile uint32_t p1cr;         /* ofs:0x0004 */
  volatile uint32_t p2cr;         /* ofs:0x0008 */
};

/* USARTレジスタ情報 */
typedef struct {
	/* USART */
	volatile struct stm32l4_octspi *octspi_base_addr;
} OCTSPI_REG;

static const OCTSPI_REG octspi_reg_table[OCTSPI_NUM] =
{
		OCTSPI1_BASE_ADDR,
		OCTSPI2_BASE_ADDR,
};

/* OCTSPI制御ブロック */
typedef struct {
	uint8_t ch;
	uint32_t intafece;
	uint8_t recv_buf_idx;
	uint8_t send_buf_idx;
	OCTSPI_RECV_CALLBACK recv_callback;
	OCTSPI_SEND_CALLBACK send_callback;
	uint32_t data_num;
	uint32_t state;
} OCTSPI_CTL;
static OCTSPI_CTL octspi_ctl[OCTSPI_NUM];
#define get_myself(n) (&octspi_ctl[(n)])

void opctspi_common_handler(uint32_t ch){
	volatile struct stm32l4_octspi *octspi_base_addr;
	octspi_base_addr = &octspi_reg_table[ch];
}

void opctspi1_handler(void){
	opctspi_common_handler(OCTSPI1);
}

void opctspi2_handler(void){
	opctspi_common_handler(OCTSPI2);
}

static uint32_t prescaler_calc(uint32_t bps)
{
	return (uint32_t)((4000000/bps)-1);
}

static uint8_t get_expornent(uint32_t value)
{
	uint8_t i;
	uint32_t base = 2;
	uint8_t expornent = 0;

	/* 256乗まで */
	for(i=0;i<256;i++){
		base = base << i;
		if(base > value){
			expornent = i;
		}
	}

	return expornent;
}

static void octspi1_open(OCTSPI_OPEN_CFG *cfg)
{
	volatile struct stm32l4_octspi *octspi_base_addr;
	OCTSPI_CTL *this = get_myself(cfg->ch);
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_OSPI;
    PeriphClkInit.OspiClockSelection = RCC_OSPICLKSOURCE_SYSCLK;

    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
      Error_Handler();
    }

    /* Peripheral clock enable */
    HAL_RCC_OSPIM_CLK_ENABLED++;
    if(HAL_RCC_OSPIM_CLK_ENABLED==1){
      __HAL_RCC_OSPIM_CLK_ENABLE();
    }
    __HAL_RCC_OSPI1_CLK_ENABLE();

    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    /**OCTOSPI1 GPIO Configuration
    PF6     ------> OCTOSPIM_P1_IO3
    PF7     ------> OCTOSPIM_P1_IO2
    PF8     ------> OCTOSPIM_P1_IO0
    PF9     ------> OCTOSPIM_P1_IO1
    PF10     ------> OCTOSPIM_P1_CLK
    PC2     ------> OCTOSPIM_P1_IO5
    PC3     ------> OCTOSPIM_P1_IO6
    PA4     ------> OCTOSPIM_P1_NCS
    PC4     ------> OCTOSPIM_P1_IO7
    PD4     ------> OCTOSPIM_P1_IO4
    */
    GPIO_InitStruct.Pin = GPIO_PIN_6|GPIO_PIN_7|GPIO_PIN_8|GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF10_OCTOSPIM_P1;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF3_OCTOSPIM_P1;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF10_OCTOSPIM_P1;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_4;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF3_OCTOSPIM_P1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_4;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF10_OCTOSPIM_P1;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    /* OCTOSPI1 interrupt Init */
    HAL_NVIC_SetPriority(OCTOSPI1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(OCTOSPI1_IRQn);

    /* レジスタ設定 */
    /* dual-quad configuration */
    /* indirect mode */
    octspi_base_addr = &octspi_reg_table[cfg->ch];
    /* OCTSPI system configuration */
    /*  Functional mode with FMODE[1:0] */
    /*  FIFO level with FTHRES */
    /*  DMA usage with DMAEN */
    /*  Timeout counter usage with TCEN */
    /*  Dual-quad configuration, if needed, with DMM (only for Quad-SPI mode */
    //octspi_base_addr->cr = CR_FTIE_ENABLE | CR_TCIE_ENABLE | CR_EN_ENABLE;
    /* OCTOSPI device configuration */
    /*  Device size with DEVSIZE[4:0] */
    /*  Chip-select minimum high time with CSHT[5:0] */
    /*  Clock mode with FRCK and CKMODE */
    /*  Device frequency with PRESCALER[7:0] */
    /* MTYP[2:0] */
    /* DCR1の設定 */
    octspi_base_addr->dcr1 = DCR1_MTYPE_STANDARDMODE
    					  | (get_expornent(cfg->devsize) << 16)
						  | (cfg->chip_select_high_time  << 8 )
						  | (cfg->delay_block_bypass     << 4 )
						  | (cfg ->clock_mode            << 0 );

    /* DCR2の設定 */
    octspi_base_addr->dcr2 = prescaler_calc(cfg->bps);

    /* DCR3の設定 */
    /* 特に設定しない */
    /* CRの設定 */
    /*  FTHRESは特に設定しない。送信時は1つでも空きがある場合、受信時は1つでもデータがある場合にFTFが発生する。(b'00000) */
    /* TCRの設定 */
    if(cfg->sample_shift_en){
    	octspi_base_addr->tcr = TCR_SSHIFT_ENABLE;
    }
    /* 制御ブロックの設定 */
    this->intafece = cfg->interface;
}

static void octspi2_open(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_OSPI;
    PeriphClkInit.OspiClockSelection = RCC_OSPICLKSOURCE_SYSCLK;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
      Error_Handler();
    }

    /* Peripheral clock enable */
    HAL_RCC_OSPIM_CLK_ENABLED++;
    if(HAL_RCC_OSPIM_CLK_ENABLED==1){
      __HAL_RCC_OSPIM_CLK_ENABLE();
    }
    __HAL_RCC_OSPI2_CLK_ENABLE();

    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    /**OCTOSPI2 GPIO Configuration
    PF0     ------> OCTOSPIM_P2_IO0
    PF1     ------> OCTOSPIM_P2_IO1
    PF2     ------> OCTOSPIM_P2_IO2
    PF3     ------> OCTOSPIM_P2_IO3
    PF4     ------> OCTOSPIM_P2_CLK
    PD3     ------> OCTOSPIM_P2_NCS
    */
    GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3
                          |GPIO_PIN_4;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF5_OCTOSPIM_P2;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF10_OCTOSPIM_P2;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    /* OCTOSPI2 interrupt Init */
    HAL_NVIC_SetPriority(OCTOSPI2_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(OCTOSPI2_IRQn);

    /* レジスタ設定 */
    /* dual-quad configuration */

}

void octspi_open(OCTSPI_OPEN_CFG *cfg)
{
	OSPIM_CfgTypeDef OSPIM_Cfg_Struct = {0};

	/* octspi1の初期化 */
	octspi1_open(cfg);

	/* octspi2の初期化 */
	//octspi2_open();

}

void octspi_init(uint32_t ch, OCTSPI_RECV_CALLBACK rcv_callback, OCTSPI_SEND_CALLBACK snd_callback)
{
	OCTSPI_CTL *this = get_myself(cfg->ch);

	/* リングバッファの取得 */
	buf_create(&(this->recv_buf_idx));
	buf_create(&(this->send_buf_idx));

	/* コールバックの設定 */
	this->recv_callback = rcv_callback;
	this->send_callback = snd_callback;

	this->state = IDLE;
}

uint32_t octspi_send(uint32_t ch, OCTSPI_COMMAND *cfg)
{
	volatile struct stm32l4_octspi *octspi_base_addr;
	OCTSPI_CTL *this = get_myself(cfg->ch);
	uint32_t i;

	octspi_base_addr = &octspi_reg_table[ch];
    /* Instruction phase : */
	/*  INSTRUCTION[31:0] of OCTOSPI_IR */
	/*  IMODE[2:0] of OCTOSPI_CCR */
	/*  IDTR in OCTOSPI_CCR -> double transfer rate */
    /* Address phase : */
	/*  ADSIZE[1:0] of OCTOSPI_CCR -> address size */
	/*  ADDRESS[31:0] of OCTOSPI_AR ->address to be send */
	/*  ADMODE[2:0] of OCTOSPI_CCR */
    /* Alternate-bytes phase : */
	/*  ABSIZE[1:0] of OCTOSPI_CCR -> data to be sent */
	/*  ABMODE[2:0] of OCTOSPI_CCR */
	/*  ABDTR of OCTOSPI_CCR -> DTRモード */
    /* Dummy-cycle phase : */
	/*  DCYC[4:0] of OCTOSPI_TCR : number of clocks */
    /* Data phase : */
	/*   OCTOSPI_DLR : the number of bytes */
	/*   OCTOSPI_DR  : data to be send */
	/*   DMODE[2:0] of OCTOSPI_CCR */
	/*   QSE of OCTOSPI_CCR  */

	/* clear FMODE */
	octspi_base_addr->cr &= ~CR_FMODE_MASK;
	/* Specify the frame timing in OCTOSPI_TCR */
	if(cfg->dummy_cycle_en){
		octspi_base_addr->tcr |= (TCR_DCYC_MASK & cfg->dummy_cycle);
	}
	/* Specify the frame format in OCTOSPI_CCR */
	/*  DMODE */
	if(cfg->data_en){
		octspi_base_addr->dlr = cfg->data_size;
		octspi_base_addr->ccr = (this->intafece << 24);
		/* SDR */
	}else{
		octspi_base_addr->ccr &= ~CCR_DMODE_MASK;
	}
	/*  ABSIZE ABMODE */
	if(cfg->alternate_en){
		octspi_base_addr->ccr = (cfg->alternate_size << 20);
		octspi_base_addr->ccr = (this->intafece << 16);
		/* SDR */
	}else{
		octspi_base_addr->ccr &= ~CCR_AB_MASK;
	}
	/*  ADSIZE ADMODE */
	if(cfg->address_en){
		octspi_base_addr->ccr = (cfg->address_size << 12);
		octspi_base_addr->ccr = (this->intafece << 8);
		/* SDR */
	}else{
		octspi_base_addr->ccr &= ~CCR_ADMODE_MASK;
	}
	/*  ISIZE IMODE */
	if(cfg->instruction_en){
		octspi_base_addr->ccr = (cfg->instruction_size << 4);
		octspi_base_addr->ccr = (this->intafece << 0);
		/* SDR */
	}else{
		octspi_base_addr->ccr &= ~CCR_IMODE_MASK;
	}
	/* set WRITE_MODE to FMODE*/
	octspi_base_addr->cr |= INDIRECT_MODE_WRITE;
	/* Specify the instruction in OCTOSPI_IR */
	if(cfg->instruction_en){
		octspi_base_addr->ir = cfg->instruction;
	}
	/* Specify the optional alternate byte to be sent right after the address phase in OCTOSPI_ABR */
	if(cfg->alternate_en){
		octspi_base_addr->abr = cfg->alternate;
	}
	/* Specify the targeted address in OCTOSPI_AR */
	if(cfg->address_en){
		octspi_base_addr->ar = cfg->address;
	}
	/* Read/write the data from/to the FIFO through OCTOSPI_DR (if no DMA usage) */
#if 0
	for(i = 0;i < cfg->data_size;i++){
		put_buf(this->send_buf_idx, SEND, cfg->p_data[i]);
	}
#endif
	octspi_base_addr->dr = *(cfg->p_data);
	this->state = TX;
	/* 割り込み有効 */
	octspi_base_addr->cr |= CR_TCIE_ENABLE;
}

uint32_t octspi_recv(uint32_t ch, uint8_t *data, uint32_t size)
{
	volatile struct stm32l4_octspi *octspi_base_addr;
	OCTSPI_CTL *this = get_myself(cfg->ch);
	uint32_t i;

	octspi_base_addr = &octspi_reg_table[ch];
    /* Instruction phase : */
	/*  INSTRUCTION[31:0] of OCTOSPI_IR */
	/*  IMODE[2:0] of OCTOSPI_CCR */
	/*  IDTR in OCTOSPI_CCR -> double transfer rate */
    /* Address phase : */
	/*  ADSIZE[1:0] of OCTOSPI_CCR -> address size */
	/*  ADDRESS[31:0] of OCTOSPI_AR ->address to be send */
	/*  ADMODE[2:0] of OCTOSPI_CCR */
    /* Alternate-bytes phase : */
	/*  ABSIZE[1:0] of OCTOSPI_CCR -> data to be sent */
	/*  ABMODE[2:0] of OCTOSPI_CCR */
	/*  ABDTR of OCTOSPI_CCR -> DTRモード */
    /* Dummy-cycle phase : */
	/*  DCYC[4:0] of OCTOSPI_TCR : number of clocks */
    /* Data phase : */
	/*   OCTOSPI_DLR : the number of bytes */
	/*   OCTOSPI_DR  : data to be send */
	/*   DMODE[2:0] of OCTOSPI_CCR */
	/*   QSE of OCTOSPI_CCR  */

	octspi_base_addr->cr |= INDIRECT_MODE_READ;
	/* Specify a number of data bytes to read or write in OCTOSPI_DLR */
	if(cfg->data_en){
		octspi_base_addr->dlr = cfg->data_size;
	}
	/* Specify the frame timing in OCTOSPI_TCR */
	if(cfg->dummy_cycle_en){
		octspi_base_addr->tcr |= (TCR_DCYC_MASK & cfg->dummy_cycle);
	}
	/* Specify the frame format in OCTOSPI_CCR */
	if(cfg->data_en){
		octspi_base_addr->ccr = (this->intafece << 24);
	}
	if(cfg->alternate_en){
		octspi_base_addr->ccr = (cfg->alternate_size << 20);
		octspi_base_addr->ccr = (this->intafece << 16);
	}
	if(cfg->address_en){
		octspi_base_addr->ccr = (cfg->address_size << 12);
		octspi_base_addr->ccr = (this->intafece << 8);
	}
	if(cfg->instruction_en){
		octspi_base_addr->ccr = (cfg->instruction_size << 4);
		octspi_base_addr->ccr = (this->intafece << 0);
	}
	/* Specify the instruction in OCTOSPI_IR */
	octspi_base_addr->ir = cfg->instruction;
	/* Specify the optional alternate byte to be sent right after the address phase in OCTOSPI_ABR */
	if(cfg->alternate_en){
		octspi_base_addr->abr = cfg->alternate;
	}
	/* Specify the targeted address in OCTOSPI_AR */
	if(cfg->address_en){
		octspi_base_addr->ar = cfg->address;
	}


	/* Read/write the data from/to the FIFO through OCTOSPI_DR (if no DMA usage) */
	//for(i=0;i<BUF_SIZE;i++){

	//}
}
