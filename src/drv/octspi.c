/*
 * octspi.c
 *
 *  Created on: 2022/08/19
 *      Author: User
 */
/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include "defines.h"
#include "kozos.h"
#include "console.h"
#include "stm32l4xx_hal_rcc.h"
#include "stm32l4xx_hal_gpio.h"
#include "stm32l4xx_hal_pwr_ex.h"
#include "stm32l4xx_hal_cortex.h"
#include "stm32l4xx.h"
#include "pin_function.h"
#include "intr.h"

#include "octspi.h"

// common macro
#define BUF_SIZE	(256)
#define FIFO_SIZE	(32)
#define TIMEOUT		(100000)	// 100000cycle

// state
#define ST_NOT_INTIALIZED	(0U)	// not initialize
#define ST_INTIALIZED		(1U)	// initialized
#define ST_OPENED			(2U)	// opened
#define ST_RUN				(3U)	// running

/* OCTSPI base register addr */
#define OCTSPI1_BASE_ADDR (0xA0001000)
#define OCTSPI2_BASE_ADDR (0xA0001400)
#define OCTSPIM_BASE_ADDR (0x50061C00)

// register define
// OCTOSPI
// CR
#define CR_DMM				(1UL << 6)
#define CR_EN				(1UL << 0)
#define CR_FTHRES(v)		(((v) & 0x1F) << 8)
#define CR_TCIE				(1UL << 17)
#define CR_TEIE				(1UL << 16)
#define CR_FTHRES(v)		(((v) & 0x1F) << 8)
#define CR_FTHRES(v)		(((v) & 0x1F) << 8)
#define CR_FMODE(v)			(((v) & 0x3) << 28)
// DCR1
#define DCR1_MTYP(v)		(((v) & 0x7) << 24)
#define DCR1_DEVSIZE(v)		(((v) & 0x1F) << 16)
#define DCR1_CSHT(v)		(((v) & 0x3F) << 16)
#define DCR1_DLYBYP			(1UL << 3)
#define DCR1_FRCK			(1UL << 1)
#define DCR1_CKMODE			(1UL << 0)
// DCR2
#define DCR2_PRESCALER(v)	(((v) & 0xFF) << 0)
// DCR3
#define DCR3_CSBOUND(v)		(((v) & 0x1F) << 16)
#define DCR3_MAXTRAN(v)		(((v) & 0xFF) << 0)
// DCR4
#define DCR4_REFRESH(v)		(((v) & 0xFFFFFFFF) << 0)
// CCR
#define CCR_SIOO			(1UL << 31)
#define CCR_DQSE			(1UL << 29)
#define CCR_DDTR			(1UL << 27)
#define CCR_DMODE(v)		(((v) & 0x07) << 24)
#define CCR_ABSIZE(v)		(((v) & 0x03) << 20)
#define CCR_ABDTR			(1UL << 19)
#define CCR_ABMODE(v)		(((v) & 0x07) << 16)
#define CCR_ADSIZE(v)		(((v) & 0x03) << 12)
#define CCR_ADDTRE			(1UL << 11)
#define CCR_ADMODE(v)		(((v) & 0x07) << 8)
#define CCR_ISIZE(v)		(((v) & 0x03) << 4)
#define CCR_IDTRE			(1UL << 3)
#define CCR_IMODE(v)		(((v) & 0x07) << 0)
// TCR
#define TCR_SSHIFT			(1UL << 30)
#define TCR_DHQC			(1UL << 28)
#define TCR_DCYC(v)			(((v) & 0x1F) << 0)
// SR
#define SR_FLEVELS(v)		(((v) & 0x3F) << 8)
#define SR_BUSY				(1UL << 5)
#define SR_TOF				(1UL << 4)
#define SR_SMF				(1UL << 3)
#define SR_FTF				(1UL << 2)
#define SR_TCF				(1UL << 1)
#define SR_TEF				(1UL << 0)
// FCR
#define FCR_CTOF			(1UL << 4)
#define FCR_CSMF			(1UL << 3)
#define FCR_CTCF			(1UL << 1)
#define FCR_CTEF			(1UL << 0)

// OCTOSPIM
// PCR
#define PCR_CLKEN		(1UL << 0)
#define PCR_CLKSRC		(1UL << 1)
#define PCR_DQSEN		(1UL << 4)
#define PCR_DQSSRC		(1UL << 5)
#define PCR_NCSEN		(1UL << 8)
#define PCRNCSSRC		(1UL << 9)
#define PCR_IOLEN		(1UL << 16)
#define PCR_IOLSRC(v)	(((v) & 0x3) << 17)
#define PCR_IOHEN		(1UL << 24)
#define PCR_IOHSRC(v)	(((v) & 0x3) << 25)

// register setting val
#define FMODE_INDIRECT_WRITE				(0)
#define FMODE_INDIRECT_READ					(1)
#define FMODE_AUTOMATIC_STATUS_POLLING		(2)
#define FMODE_MEMORY_MAPPED					(3)

/* OCTSPI register info */
struct stm32l4_octspi {
	volatile uint32_t cr;           /* ofs:0x0000 */
	volatile uint32_t reserved0;    /* ofs:0x0004 */
	volatile uint32_t dcr1;         /* ofs:0x0008 */
	volatile uint32_t dcr2;         /* ofs:0x000C */
	volatile uint32_t dcr3;         /* ofs:0x0010 */
	volatile uint32_t dcr4;         /* ofs:0x0014 */
	volatile uint32_t reserved1;    /* ofs:0x0018 */
	volatile uint32_t reserved2;    /* ofs:0x001C */
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
	//volatile uint32_t dr;           /* ofs:0x0050 */
	union {
		volatile uint32_t word;
		volatile uint8_t byte[4];
	} dr;
	volatile uint32_t reserved12;   /* ofs:0x0054 */
	volatile uint32_t reserved13;   /* ofs:0x0058 */
	volatile uint32_t reserved14;   /* ofs:0x005C */
	volatile uint32_t reserved15;   /* ofs:0x0060 */
	volatile uint32_t reserved16;   /* ofs:0x0064 */
	volatile uint32_t reserved17;   /* ofs:0x0068 */
	volatile uint32_t reserved18;   /* ofs:0x006C */
	volatile uint32_t reserved19;   /* ofs:0x0070 */
	volatile uint32_t reserved20;   /* ofs:0x0074 */
	volatile uint32_t reserved21;   /* ofs:0x0078 */
	volatile uint32_t reserved22;   /* ofs:0x007C */
	volatile uint32_t psmkr;        /* ofs:0x0080 */
	volatile uint32_t reserved22_1; /* ofs:0x0084 */
	volatile uint32_t psmar;        /* ofs:0x0088 */
	volatile uint32_t reserved23_1; /* ofs:0x008C */
	volatile uint32_t pir;          /* ofs:0x0090 */
	volatile uint32_t reserved23_2; /* ofs:0x0094 */
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

// prototype
static void opctspi1_handler(void);
static void opctspi2_handler(void);

/* channel info */
typedef struct {
	volatile struct stm32l4_octspi *octspi_base_addr;	// OCTSPI base register addr
	IRQn_Type irq_type;									// interrupt type
	INT_HANDLER handler;								// interrupt handler
	uint32_t	vec_no;									// interrupt number
	uint32_t	int_priority;							// priority
	uint32_t	clk;									// clcok define
} OCTSPI_CFG;

/* channel info table */
static const OCTSPI_CFG octspi_cfg[OCTOSPI_CH_MAX] =
{
	{(volatile struct stm32l4_octspi*)OCTSPI1_BASE_ADDR,	OCTOSPI1_IRQn,		opctspi1_handler,	OCTOSPI1_GLOBAL_INTERRUPT_NO,	INTERRPUT_PRIORITY_5,	RCC_PERIPHCLK_OSPI},
	{(volatile struct stm32l4_octspi*)OCTSPI2_BASE_ADDR,	OCTOSPI2_IRQn,		opctspi2_handler,	OCTOSPI2_GLOBAL_INTERRUPT_NO,	INTERRPUT_PRIORITY_5,	RCC_PERIPHCLK_OSPI},
};
#define get_reg(ch)			(octspi_cfg[ch].octspi_base_addr)		// get base register addr
#define get_ire_type(ch)	(octspi_cfg[ch].irq_type)				// get interrupt type
#define get_handler(ch)		(octspi_cfg[ch].handler)				// get interrupt handler
#define get_vec_no(ch)		(octspi_cfg[ch].vec_no)					// get interrupt number
#define get_int_prio(ch)	(octspi_cfg[ch].int_priority)			// get priority
#define get_clk_no(ch)		(octspi_cfg[ch].clk)					// get clcok define

/* OCTSPI control info */
typedef struct {
	OCTOSPI_CH			ch;				// channel
	uint32_t			status;			// state
	uint8_t				*data;			// data
	int32_t				data_size;		// data size
} OCTSPI_CTL;
static OCTSPI_CTL octspi_ctl[OCTOSPI_CH_MAX];
#define get_myself(n) (&octspi_ctl[(n)])

// common interrupt handler
void opctspi_common_handler(uint32_t ch)
{
	// not used
}

// interrupt handler CH1
static void opctspi1_handler(void) {
	opctspi_common_handler(OCTOSPI_CH_1);
}

// interrupt handler CH2
static void opctspi2_handler(void) {
	opctspi_common_handler(OCTOSPI_CH_2);
}

// get expornent
static uint8_t get_expornent(uint32_t value)
{
	uint8_t i;
	uint32_t base = 1;
	uint8_t expornent = 0;
	
	/* up to 256 */
	for (i = 1; i < 256; i++) {
		base = base * 2;
		if (base >= value) {
			expornent = i;
			break;
		}
	}
	
	return expornent;
}

// clock setting
static void set_clk(OCTOSPI_CH ch, uint32_t clk)
{
	volatile struct stm32l4_octspi *octspi_base_addr = get_reg(ch);
	uint32_t octospi_clk;
	uint32_t prescale;
	
	// get octospi clok
	octospi_clk = HAL_RCCEx_GetPeriphCLKFreq(get_clk_no(ch));
	
	// calc prescaler
	prescale = octospi_clk/clk;
	if (prescale == 0) {
		prescale = 1;
	}
	// set prescaler
	octspi_base_addr->dcr2 = DCR2_PRESCALER(prescale);
}

// pin function setting
static void pin_config(OCTOSPI_CH ch, OCTOSPI_OPEN *par)
{
	volatile struct stm32l4_octspim *octspim_base_addr = (volatile struct stm32l4_octspim*)OCTSPIM_BASE_ADDR;
	const uint32_t pcr_tbl[OCTOSPI_CH_MAX] = {
		(uint32_t)(&(octspim_base_addr->p1cr)),
		(uint32_t)(&(octspim_base_addr->p2cr)),
	};
	uint32_t *pcr_reg;
	
	// set OCTOSPIM
	// not set CR
	//octspim_base_addr->cr =
	
	// get pcr base register
	pcr_reg = (uint32_t*)pcr_tbl[ch];
	
	// enable all port
	*pcr_reg |= (PCR_IOHEN | PCR_IOLEN | PCR_NCSEN | PCR_CLKEN);
	//*pcr_reg |= (PCR_IOLEN | PCR_NCSEN | PCR_CLKEN);
}

// wait until a specific bit is set
static int32_t wait_status_set(OCTOSPI_CH ch, uint32_t bit, uint32_t timeout)
{
	volatile struct stm32l4_octspi *octspi_base_addr;
	
	// get base address
	octspi_base_addr = get_reg(ch);
	
	while((octspi_base_addr->sr & bit) == 0) {
		if (timeout-- == 0) {
			return E_TMOUT;
		}
	}
	
	return E_OK;
}

// wait until a specific bit is clear
static int32_t wait_status_clear(OCTOSPI_CH ch, uint32_t bit, uint32_t timeout)
{
	volatile struct stm32l4_octspi *octspi_base_addr;
	
	// get base address
	octspi_base_addr = get_reg(ch);
	
	while((octspi_base_addr->sr & bit) != 0) {
		if (timeout-- == 0) {
			return E_TMOUT;
		}
	}
	
	return E_OK;
}

// Init
void octospi_init(void)
{
	OCTSPI_CTL *this;
	uint8_t ch;
	
	for (ch = 0; ch < OCTOSPI_CH_MAX; ch++) {
		// get ctx
		this = get_myself(ch);
		// initialize ctx
		memset(this, 0, sizeof(OCTSPI_CTL));
		// set interrupt handler
		kz_setintr(get_vec_no(ch), get_handler(ch));
		// update state
		this->status = ST_INTIALIZED;
	}
}

// Open
int32_t octospi_open(OCTOSPI_CH ch, OCTOSPI_OPEN *par)
{
	volatile struct stm32l4_octspi *octspi_base_addr;
	OCTSPI_CTL *this;
	
	// check channel
	if (ch >= OCTOSPI_CH_MAX) {
		return E_PAR;
	}
	
	// check mem_type
	if (par->mem_type >= OCTOSPI_MEM_MAX) {
		return E_PAR;
	}
	
	// check interface
	if (par->interface >= OCTOSPI_IF_OCTO) {
		return E_PAR;
	}
	
	// get ctx
	this = get_myself(ch);
	
	// exit if not initialized
	if (this->status != ST_INTIALIZED) {
		return E_PAR;
	}
	
	// get base address
	octspi_base_addr = get_reg(ch);
	
	// set DCR1
	octspi_base_addr->dcr1 = DCR1_MTYP(par->mem_type) | DCR1_DEVSIZE(get_expornent(par->size) - 1) | (par->clk_mode & DCR1_CKMODE)
	;
	
	// set DCR3
	//octspi_base_addr->dcr3 = DCR3_CSBOUND(3);
	
	// set DCR4 (*)not used reflesh
	
	// set CR
	//octspi_base_addr->cr |= CR_FTHRES(FIFO_SIZE/2);
	// for now, FTF is set when one or more data is stored in the FIFO.
	// set dual_mode
	if (par->dual_mode != FALSE) {
		octspi_base_addr->cr |= CR_DMM;
	}
	
	// set DCR2
	set_clk(ch, par->clk);
	
	// set DQS and SSIO
	if (par->ssio_used != FALSE) {
		octspi_base_addr->ccr |= CCR_SIOO;
	}
	if (par->dqs_used != FALSE) {
		octspi_base_addr->ccr |= CCR_DQSE;
	}
	
	// set pin config
	pin_config(ch, par);
	
	// enable OCTOSPI
	octspi_base_addr->cr |= CR_EN;
	
    // set NVIC
	HAL_NVIC_SetPriority(get_ire_type(ch), get_int_prio(ch), 0);
	HAL_NVIC_EnableIRQ(get_ire_type(ch));
	
	// update state
	this->status = ST_OPENED;
	
	return E_OK;
}

// set memory mapped mode
int32_t octospi_memory_mapped(OCTOSPI_CH ch, OCTOSPI_COM_CFG *read_cfg, OCTOSPI_COM_CFG *write_cfg)
{
	volatile struct stm32l4_octspi *octspi_base_addr;
	OCTSPI_CTL *this;
	uint32_t tmp_ccr = 0UL;
	
	// check channel
	if (ch >= OCTOSPI_CH_MAX) {
		return E_PAR;
	}
	
	// check parameter
	if ((read_cfg == NULL) || (write_cfg == NULL)) {
		return E_PAR;
	}
	
	// get ctx
	this = get_myself(ch);
	
	// exit if not opened
	if (this->status != ST_OPENED) {
		return E_PAR;
	}
	
	// wait until BUSY bit is cleared
	if (wait_status_clear(ch, SR_BUSY, TIMEOUT) != E_OK) {
		console_str_send("octospi busy wait error\n");
		return E_TMOUT;
	}
	
	// get base address
	octspi_base_addr = get_reg(ch);
	
	/*
		Setup procedure described in the manual
		1. Specify the frame timing in OCTOSPI_TCR for read operation.
		2. Specify the frame format in OCTOSPI_CCR for read operation.
		3. Specify the instruction in OCTOSPI_IR.
		4. Specify the optional alternate byte to be sent right after the address phase in OCTOSPI_ABR for read operation.
		5. Specify the frame timing in OCTOSPI_WTCR for write operation.
		6. Specify the frame format in OCTOSPI_WCCR for write operation.
		7. Specify the instruction in OCTOSPI_WIR.
		8. Specify the optional alternate byte to be sent right after the address phase in OCTOSPI_WABR for read operation.
	*/
	
	//octspi_base_addr->dcr3 = DCR3_CSBOUND(3);
	
	//octspi_base_addr->dcr4 = DCR4_REFRESH(47);
	
	// set mode
	octspi_base_addr->cr &= 0xCFFFFFFF;
	octspi_base_addr->cr |= CR_FMODE(FMODE_MEMORY_MAPPED);
	
	// initialize TCR and CCR
	octspi_base_addr->tcr = 0x00000000;
	octspi_base_addr->ccr = 0x00000000;
	
	// set LPTR
	//octspi_base_addr->lptr |= 0x00000020;
	
	// set TCR
	octspi_base_addr->tcr |= read_cfg->dummy_cycle;
	//octspi_base_addr->tcr |= TCR_SSHIFT;
	
	// set data
	tmp_ccr |= CCR_DMODE(read_cfg->data_if);
	
	// set alternate byte
	if (read_cfg->alternate_all_size > 0) {
		octspi_base_addr->abr = read_cfg->alternate_all_size;
		tmp_ccr |= CCR_ABSIZE(read_cfg->alternate_size) | CCR_ABMODE(read_cfg->alternate_if);
	}
	// set addr
	if ((read_cfg->addr_if != OCTOSPI_IF_NONE) && (read_cfg->addr_if != OCTOSPI_IF_MAX)) {
		tmp_ccr |= CCR_ADSIZE(read_cfg->addr_size) | CCR_ADMODE(read_cfg->addr_if);
	}
	// set instruction
	tmp_ccr |= CCR_ISIZE(read_cfg->inst_size) | CCR_IMODE(read_cfg->inst_if);
	//tmp_ccr |= CCR_SIOO;
	
	// set CCR
	octspi_base_addr->ccr = tmp_ccr;
	
	// set read instruction
	octspi_base_addr->ir = read_cfg->inst;
	
	// MemoryMappedMode is not used for write
	
	return E_OK;
}

// Close
int32_t octospi_close(OCTOSPI_CH ch)
{
	volatile struct stm32l4_octspi *octspi_base_addr;
	OCTSPI_CTL *this;
	
	// check channel
	if (ch >= OCTOSPI_CH_MAX) {
		return E_PAR;
	}
	
	// get ctx
	this = get_myself(ch);
	
	// exit if not opend
	if (this->status != ST_OPENED) {
		return E_PAR;
	}
		
	// get base address
	octspi_base_addr = get_reg(ch);
	
	// disable CR
	octspi_base_addr->cr &= ~CR_EN;
	
	// disable NVIC
	HAL_NVIC_DisableIRQ(get_ire_type(ch));
	
	// update state
	this->status = ST_INTIALIZED;
	
	return E_OK;
}

// read/write proc
static int32_t octospi_proc(OCTOSPI_CH ch, OCTOSPI_COM_CFG *cfg, uint32_t mode, uint8_t *data, uint32_t size)
{
	volatile struct stm32l4_octspi *octspi_base_addr;
	OCTSPI_CTL *this;
	uint32_t tmp_ccr = 0UL;
	
	// get ctx
	this = get_myself(ch);
	
	/*
		Setup procedure described in the manual
		1. Specify a number of data bytes to read or write in OCTOSPI_DLR.
		2. Specify the frame timing in OCTOSPI_TCR.
		3. Specify the frame format in OCTOSPI_CCR.
		4. Specify the instruction in OCTOSPI_IR.
		5. Specify the optional alternate byte to be sent right after the address phase in
		OCTOSPI_ABR.
		6. Specify the targeted address in OCTOSPI_AR.
		7. Enable the DMA channel if needed.
		8. Read/write the data from/to the FIFO through OCTOSPI_DR (if no DMA usage).
		If neither the address register (OCTOSPI_AR) nor the data register (OCTOSPI_DR) need
	*/
	
	// get base address
	octspi_base_addr = get_reg(ch);
	
	// wait until BUSY bit is cleared
	if (wait_status_clear(ch, SR_BUSY, TIMEOUT) != E_OK) {
		console_str_send("octospi busy wait error\n");
		return E_TMOUT;
	}
	
	// set mode
	octspi_base_addr->cr &= 0xCFFFFFFF;
	octspi_base_addr->cr |= CR_FMODE(mode);
	
	// Indirect Mode
	if ((mode == FMODE_INDIRECT_WRITE)||(mode == FMODE_INDIRECT_READ)) {
		// initialize TCR and CCR
		octspi_base_addr->tcr = 0x00000000;
		octspi_base_addr->ccr = 0x00000000;
		
		// set TCR
		octspi_base_addr->tcr |= cfg->dummy_cycle;
		//octspi_base_addr->tcr |= TCR_SSHIFT;
		
		// set data info
		if (size > 0) {
			octspi_base_addr->dlr = size - 1;
			tmp_ccr |= CCR_DMODE(cfg->data_if);
		}
		// set alternate info
		if (cfg->alternate_all_size > 0) {
			octspi_base_addr->abr = cfg->alternate_all_size;
			tmp_ccr |= CCR_ABSIZE(cfg->alternate_size) | CCR_ABMODE(cfg->alternate_if);
		}
		// set addr info
		if ((cfg->addr_if != OCTOSPI_IF_NONE) && (cfg->addr_if != OCTOSPI_IF_MAX)) {
			tmp_ccr |= CCR_ADSIZE(cfg->addr_size) | CCR_ADMODE(cfg->addr_if);
		}
		
		// set instruction info
		tmp_ccr |= CCR_ISIZE(cfg->inst_size) | CCR_IMODE(cfg->inst_if);
		
		// set CCR
		octspi_base_addr->ccr = tmp_ccr;
		
		// set instruction
		octspi_base_addr->ir = cfg->inst;
		
		// no address and no data
		if (((cfg->addr_if == OCTOSPI_IF_NONE) || (cfg->addr_if == OCTOSPI_IF_MAX)) && (size == 0)) {
			;
		// either address or data
		} else {
			// set address
			if ((cfg->addr_if != OCTOSPI_IF_NONE) && (cfg->addr_if != OCTOSPI_IF_MAX)) {
				octspi_base_addr->ar = cfg->addr;
			}
			// set data
			if (size > 0) {
				// remember data pointer and size
				this->data = data;
				this->data_size = size;
				
				// execute read/write for data size
				while (this->data_size-- > 0) {
					// Check if there is one or more data in the FIFO
					if (wait_status_set(ch, (SR_FTF | SR_TCF), TIMEOUT) != E_OK) {
						return E_TMOUT;
					}
					// indirect mode write
					if (mode == FMODE_INDIRECT_WRITE) {
						// 1 byte write
						octspi_base_addr->dr.byte[0] = *(this->data++);
						
					// indirect mode read
					} else {
						// 1 byte read
						*(this->data++) = octspi_base_addr->dr.byte[0];
						
					}
				}
			}
		}
		// wait until transmission is complete.
		if (wait_status_set(ch, SR_TCF, TIMEOUT) != 0) {
			return E_TMOUT;
		}
		// clear transmission completion flag
		octspi_base_addr->fcr |= FCR_CTCF;
		
		// check error flags
		if ((octspi_base_addr->sr & SR_TEF) != 0) {
			// clear error flags
			octspi_base_addr->fcr |= FCR_CTEF;
			return E_STS;
			
		}
		
		// wait until BUSY bit is cleared
		if (wait_status_clear(ch, SR_BUSY, TIMEOUT) != E_OK) {
			console_str_send("octospi busy wait error\n");
			return E_TMOUT;
		}
		
	// Status Polling Mode
	} else if (mode == FMODE_AUTOMATIC_STATUS_POLLING) {
		// not used
		;
	// Memory Mapped Mode
	} else if (mode == FMODE_MEMORY_MAPPED) {
		// not used
		;
		
	// Other
	} else {
		;
	}
	
	return E_OK;
}

// Send
int32_t octospi_send(OCTOSPI_CH ch, OCTOSPI_COM_CFG *cfg, uint8_t *data, uint32_t size)
{
	OCTSPI_CTL *this;
	int32_t ret;
	
	// check channel
	if (ch >= OCTOSPI_CH_MAX) {
		return -1;
	}
	
	// get ctx
	this = get_myself(ch);
	
	// exit if not opened
	if (this->status != ST_OPENED) {
		return E_PAR;
	}
	
	// update state
	this->status = ST_RUN;
	
	// execute write
	ret = octospi_proc(ch, cfg, FMODE_INDIRECT_WRITE, data, size);
	
	// update state
	this->status = ST_OPENED;
	
	return ret;
}

// Read
int32_t octospi_recv(OCTOSPI_CH ch, OCTOSPI_COM_CFG *cfg, uint8_t *data, uint32_t size)
{
	OCTSPI_CTL *this;
	int32_t ret;
	
	// check channel
	if (ch >= OCTOSPI_CH_MAX) {
		return -1;
	}
	
	// get ctx
	this = get_myself(ch);
	
	// exit if not opened
	if (this->status != ST_OPENED) {
		return -1;
	}
	
	// update state
	this->status = ST_RUN;
	
	// execute read
	ret = octospi_proc(ch, cfg, FMODE_INDIRECT_READ, data, size);
	
	// update state
	this->status = ST_OPENED;
	
	return ret;
}
