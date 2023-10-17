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

<<<<<<< HEAD
/* USARTãƒ¬ã‚¸ã‚¹ã‚¿æƒ…å ± */
=======
/* USARTƒŒƒWƒXƒ^î•ñ */
>>>>>>> d9d46175d1b56cbb3661cb48ee3fe9fab9f5c719
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

<<<<<<< HEAD
// ã‚ªãƒ¼ãƒ—ãƒ³ãƒ‘ãƒ©ãƒ¡ãƒ¼ã‚¿
/*
	ã‚¤ãƒ³ãƒ€ã‚¤ãƒ¬ã‚¯ãƒˆãƒ¢ãƒ¼ãƒ‰ã€ã‚¹ãƒ†ãƒ¼ã‚¿ã‚¹ãƒãƒ¼ãƒªãƒ³ã‚°ãƒ¢ãƒ¼ãƒ‰ã€ãƒ¡ãƒ¢ãƒªãƒžãƒƒãƒ—åº¦ãƒ¢ãƒ¼ãƒ‰ã®é¸æŠž
	SIOOã®è¨­å®š
=======
// ƒI[ƒvƒ“ƒpƒ‰ƒ[ƒ^
/*
	ƒCƒ“ƒ_ƒCƒŒƒNƒgƒ‚[ƒhAƒXƒe[ƒ^ƒXƒ|[ƒŠƒ“ƒOƒ‚[ƒhAƒƒ‚ƒŠƒ}ƒbƒv“xƒ‚[ƒh‚Ì‘I‘ð
	SIOO‚ÌÝ’è
>>>>>>> d9d46175d1b56cbb3661cb48ee3fe9fab9f5c719
	

*/


typedef struct {
<<<<<<< HEAD
	uint32_t inst;				// å‘½ä»¤
	uint32_t inst_size;			// å‘½ä»¤ã‚µã‚¤ã‚º
	uint32_t inst_if;			// å‘½ä»¤ã‚¤ãƒ³ã‚¿ãƒ•ã‚§ãƒ¼ã‚¹
	uint32_t addr;				// ã‚¢ãƒ‰ãƒ¬ã‚¹
	uint32_t addr_size;			// ã‚¢ãƒ‰ãƒ¬ã‚¹ã‚µã‚¤ã‚º
	uint32_t addr_if;			// ã‚¢ãƒ‰ãƒ¬ã‚¹ã‚¤ãƒ³ã‚¿ãƒ•ã‚§ãƒ¼ã‚¹
	uint32_t dummy_cycle;		// ãƒ€ãƒŸãƒ¼ã‚µã‚¤ã‚¯ãƒ«
	uint32_t data_size;			// ãƒ‡ãƒ¼ã‚¿ã‚µã‚¤ã‚º
	uint32_t data_if;			// ãƒ‡ãƒ¼ã‚¿ã‚¤ãƒ³ã‚¿ãƒ•ã‚§ãƒ¼ã‚¹
	uint8_t *data;				// ãƒ‡ãƒ¼ã‚¿
	uint32_t alternate_size;	// ã‚ªãƒ«ã‚¿ãƒŠãƒ†ã‚£ãƒ–ã‚µã‚¤ã‚º
	uint32_t alternate_if;		// ã‚ªãƒ«ã‚¿ãƒŠãƒ†ã‚£ãƒ–ã‚¤ãƒ³ã‚¿ãƒ•ã‚§ãƒ¼ã‚¹
=======
	uint32_t inst;				// –½—ß
	uint32_t inst_size;			// –½—ßƒTƒCƒY
	uint32_t inst_if;			// –½—ßƒCƒ“ƒ^ƒtƒF[ƒX
	uint32_t addr;				// ƒAƒhƒŒƒX
	uint32_t addr_size;			// ƒAƒhƒŒƒXƒTƒCƒY
	uint32_t addr_if;			// ƒAƒhƒŒƒXƒCƒ“ƒ^ƒtƒF[ƒX
	uint32_t dummy_cycle;		// ƒ_ƒ~[ƒTƒCƒNƒ‹
	uint32_t data_size;			// ƒf[ƒ^ƒTƒCƒY
	uint32_t data_if;			// ƒf[ƒ^ƒCƒ“ƒ^ƒtƒF[ƒX
	uint8_t *data;				// ƒf[ƒ^
	uint32_t alternate_size;	// ƒIƒ‹ƒ^ƒiƒeƒBƒuƒTƒCƒY
	uint32_t alternate_if;		// ƒIƒ‹ƒ^ƒiƒeƒBƒuƒCƒ“ƒ^ƒtƒF[ƒX
>>>>>>> d9d46175d1b56cbb3661cb48ee3fe9fab9f5c719
} OCTOSPI_COM_CFG;

typedef struct {
	uint8_t *data;
	uint32_t data_size;
} OCTOSPI_CTL;


uint32_t octspi_open(uint32_t ch, OCTOSPI_OPEN *par)
{
	/*
<<<<<<< HEAD
	SIOOè¨­å®š
=======
	SIOOÝ’è
>>>>>>> d9d46175d1b56cbb3661cb48ee3fe9fab9f5c719
	
	*/
}


/*
<<<<<<< HEAD
	ã‚¤ãƒ³ãƒ€ã‚¤ãƒ¬ã‚¯ãƒˆãƒ¢ãƒ¼ãƒ‰
		å¾“æ¥ã®SPIã‚¤ãƒ³ã‚¿ãƒ•ã‚§ãƒ¼ã‚¹ã¨ã—ã¦å‹•ä½œã—ã€ã™ã¹ã¦ã®å‹•ä½œã¯ãƒ¬ã‚¸ã‚¹ã‚¿ã‚’é€šã˜ã¦å®Ÿè¡Œã•ã‚Œã‚‹
	ã‚¹ãƒ†ãƒ¼ã‚¿ã‚¹ãƒãƒ¼ãƒªãƒ³ã‚°ãƒ¢ãƒ¼ãƒ‰
		Flashã‚¹ãƒ†ãƒ¼ã‚¿ã‚¹ãƒ¬ã‚¸ã‚¹ã‚¿ãŒå‘¨æœŸçš„ã«èª­ã¿ã ã•ã‚Œå‰²ã‚Šè¾¼ã¿ãŒç”Ÿæˆã•ã‚Œã‚‹
	ãƒ¡ãƒ¢ãƒªãƒžãƒƒãƒ—åº¦ãƒ¢ãƒ¼ãƒ‰
		å¤–éƒ¨Flashãƒ¡ãƒ¢ãƒªã‚’å†…éƒ¨ãƒ¡ãƒ¢ãƒªã®æ§˜ã«èª­ã¿å‡ºã™ã“ã¨ãŒã§ãã¾ã™
=======
	ƒCƒ“ƒ_ƒCƒŒƒNƒgƒ‚[ƒh
		]—ˆ‚ÌSPIƒCƒ“ƒ^ƒtƒF[ƒX‚Æ‚µ‚Ä“®ì‚µA‚·‚×‚Ä‚Ì“®ì‚ÍƒŒƒWƒXƒ^‚ð’Ê‚¶‚ÄŽÀs‚³‚ê‚é
	ƒXƒe[ƒ^ƒXƒ|[ƒŠƒ“ƒOƒ‚[ƒh
		FlashƒXƒe[ƒ^ƒXƒŒƒWƒXƒ^‚ªŽüŠú“I‚É“Ç‚Ý‚¾‚³‚êŠ„‚èž‚Ý‚ª¶¬‚³‚ê‚é
	ƒƒ‚ƒŠƒ}ƒbƒv“xƒ‚[ƒh
		ŠO•”Flashƒƒ‚ƒŠ‚ð“à•”ƒƒ‚ƒŠ‚Ì—l‚É“Ç‚Ýo‚·‚±‚Æ‚ª‚Å‚«‚Ü‚·
>>>>>>> d9d46175d1b56cbb3661cb48ee3fe9fab9f5c719
*/
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
<<<<<<< HEAD
	/*  ABDTR of OCTOSPI_CCR -> DTRãƒ¢ãƒ¼ãƒ‰ */
=======
	/*  ABDTR of OCTOSPI_CCR -> DTRƒ‚[ƒh */
>>>>>>> d9d46175d1b56cbb3661cb48ee3fe9fab9f5c719
    /* Dummy-cycle phase : */
	/*  DCYC[4:0] of OCTOSPI_TCR : number of clocks */
    /* Data phase : */
	/*   OCTOSPI_DLR : the number of bytes */
	/*   OCTOSPI_DR  : data to be send */
	/*   DMODE[2:0] of OCTOSPI_CCR */
	/*   QSE of OCTOSPI_CCR  */

/*
<<<<<<< HEAD
	é€ä¿¡ã‚¿ã‚¤ãƒŸãƒ³ã‚°ã¯3ã¤
		ã‚¢ãƒ‰ãƒ¬ã‚¹ãŒãªã„ã€ãƒ‡ãƒ¼ã‚¿ãŒãªã„ã¨ãã¯ã€IRãƒ¬ã‚¸ã‚¹ã‚¿ã«æ›¸ã„ãŸã‚‰é€ä¿¡
		ã‚¢ãƒ‰ãƒ¬ã‚¹ãŒã‚ã‚‹ã€ãƒ‡ãƒ¼ã‚¿ãŒãªã„ã¨ãã¯ã€ARãƒ¬ã‚¸ã‚¹ã‚¿ã«æ›¸ã„ãŸã‚‰é€ä¿¡
		ã‚¢ãƒ‰ãƒ¬ã‚¹ãŒã‚ã‚‹ã€ãƒ‡ãƒ¼ã‚¿ã‚‚ã‚ã‚‹ã¨ãã¯ã€DRãƒ¬ã‚¸ã‚¹ã‚¿ã«æ›¸ã„ãŸã‚‰é€ä¿¡
		â€» ABRã¯ãƒˆãƒªã‚¬ãƒ¼ã«ãªã‚Šå¾—ãªã„

	ã‚µãƒ³ãƒ—ãƒ«ã®æµã‚Œ
=======
	‘—Mƒ^ƒCƒ~ƒ“ƒO‚Í3‚Â
		ƒAƒhƒŒƒX‚ª‚È‚¢Aƒf[ƒ^‚ª‚È‚¢‚Æ‚«‚ÍAIRƒŒƒWƒXƒ^‚É‘‚¢‚½‚ç‘—M
		ƒAƒhƒŒƒX‚ª‚ ‚éAƒf[ƒ^‚ª‚È‚¢‚Æ‚«‚ÍAARƒŒƒWƒXƒ^‚É‘‚¢‚½‚ç‘—M
		ƒAƒhƒŒƒX‚ª‚ ‚éAƒf[ƒ^‚à‚ ‚é‚Æ‚«‚ÍADRƒŒƒWƒXƒ^‚É‘‚¢‚½‚ç‘—M
		¦ ABR‚ÍƒgƒŠƒK[‚É‚È‚è“¾‚È‚¢

	ƒTƒ“ƒvƒ‹‚Ì—¬‚ê
>>>>>>> d9d46175d1b56cbb3661cb48ee3fe9fab9f5c719
		HAL_OSPI_Init
			DCR1
			DCR3
			DCR4
<<<<<<< HEAD
			CR   : FIFOã®è¨­å®š
			DCR2 : ã‚¯ãƒ­ãƒƒã‚¯ã®è¨­å®š
			CR   : ãƒ‡ãƒ¥ã‚¢ãƒ«ã‚¯ã‚¢ãƒƒãƒ‰ãƒ¢ãƒ¼ãƒ‰ã®è¨­å®š
			TCR  : SSFIFTã€é…å»¶ã®è¨­å®š
			Enable OCTOSPI
		HAL_OSPI_Command
			DQSã€SSIOè¨­å®š
			ã‚ªãƒ«ã‚¿ãƒŠãƒ†ã‚£ãƒ–è¨­å®š ABR
			ãƒ€ãƒŸãƒ¼ã‚µã‚¤ã‚¯ãƒ«è¨­å®š TCR
=======
			CR   : FIFO‚ÌÝ’è
			DCR2 : ƒNƒƒbƒN‚ÌÝ’è
			CR   : ƒfƒ…ƒAƒ‹ƒNƒAƒbƒhƒ‚[ƒh‚ÌÝ’è
			TCR  : SSFIFTA’x‰„‚ÌÝ’è
			Enable OCTOSPI
		HAL_OSPI_Command
			DQSASSIOÝ’è
			ƒIƒ‹ƒ^ƒiƒeƒBƒuÝ’è ABR
			ƒ_ƒ~[ƒTƒCƒNƒ‹Ý’è TCR
>>>>>>> d9d46175d1b56cbb3661cb48ee3fe9fab9f5c719
			DLR
			
	BSP_OSPI_NOR_Write
		

*/
uint32_t octspi_send(uint32_t ch, OCTOSPI_COM_CFG *cfg)
{
	volatile struct stm32l4_octspi *octspi_base_addr;
	OCTSPI_CTL *this = get_myself(cfg->ch);
	uint32_t i;
	
<<<<<<< HEAD
	// ãƒ™ãƒ¼ã‚¹ãƒ¬ã‚¸ã‚¹ã‚¿å–å¾—
	octspi_base_addr = &octspi_reg_table[ch];
	
	// ã‚¤ãƒ³ãƒ€ã‚¤ãƒ¬ã‚¯ãƒˆãƒ¢ãƒ¼ãƒ‰
	// ãƒ¢ãƒ¼ãƒ‰ã‚’ã‚¯ãƒªã‚¢
	octspi_base_addr->cr &= ~CR_FMODE_MASK;
	// ã‚¤ãƒ³ãƒ€ã‚¤ãƒ¬ã‚¯ãƒˆãƒ¢ãƒ¼ãƒ‰ã®è¨­å®š
=======
	// ƒx[ƒXƒŒƒWƒXƒ^Žæ“¾
	octspi_base_addr = &octspi_reg_table[ch];
	
	// ƒCƒ“ƒ_ƒCƒŒƒNƒgƒ‚[ƒh
	// ƒ‚[ƒh‚ðƒNƒŠƒA
	octspi_base_addr->cr &= ~CR_FMODE_MASK;
	// ƒCƒ“ƒ_ƒCƒŒƒNƒgƒ‚[ƒh‚ÌÝ’è
>>>>>>> d9d46175d1b56cbb3661cb48ee3fe9fab9f5c719
	
	/*
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
	
<<<<<<< HEAD
	// ã‚µã‚¤ã‚ºã®è¨­å®š
	octspi_base_addr->dlr = cfg->data_size;
	// ã‚ªãƒ«ã‚¿ãƒŠãƒ†ã‚£ãƒ–ãƒ‡ãƒ¼ã‚¿ã®è¨­å®š
	octspi_base_addr->abr = cfg->alternate_size;
	octspi_base_addr->ccr = 
	// ãƒ€ãƒŸãƒ¼ã‚µã‚¤ã‚¯ãƒ«ã®è¨­å®š
	octspi_base_addr->tcr |= cfg->dummy_cycle;
	// ãƒ•ã‚©ãƒ¼ãƒžãƒƒãƒˆã®è¨­å®š
	octspi_base_addr->ccr = 
	// å‘½ä»¤ã®è¨­å®š
	octspi_base_addr->ir = cfg->inst;
	
	// ã‚¢ãƒ‰ãƒ¬ã‚¹ã€ãƒ‡ãƒ¼ã‚¿ã©ã¡ã‚‰ã‚‚ãªã„
	// (*) å‘½ä»¤ãƒ¬ã‚¸ã‚¹ã‚¿ã«å‘½ä»¤ã‚’æ›¸ã„ãŸã‚¿ã‚¤ãƒŸãƒ³ã‚°ã§é€ä¿¡ã•ã‚Œã‚‹
	if ((cfg->addr_size == 0) && (cfg->data_size == 0)) {
		;
	// ã‚¢ãƒ‰ãƒ¬ã‚¹ã¯ã‚ã‚‹ãŒã€ãƒ‡ãƒ¼ã‚¿ãŒãªã„
	// (*) ã‚¢ãƒ‰ãƒ¬ã‚¹ãƒ¬ã‚¸ã‚¹ã‚¿ã«ã‚¢ãƒ‰ãƒ¬ã‚¹ã‚’æ›¸ã„ãŸã‚¿ã‚¤ãƒŸãƒ³ã‚°ã§é€ä¿¡ã•ã‚Œã‚‹
	} else if ((cfg->addr_size > 0) && (cfg->data_size == 0)) {
		// ã‚¢ãƒ‰ãƒ¬ã‚¹ã®è¨­å®š
		octspi_base_addr->ar = cfg->addr;
		
	// ã‚¢ãƒ‰ãƒ¬ã‚¹ã€ãƒ‡ãƒ¼ã‚¿ã©ã¡ã‚‰ã‚‚ã‚ã‚‹
	// (*) ãƒ‡ãƒ¼ã‚¿ãƒ¬ã‚¸ã‚¹ã‚¿ã«ãƒ‡ãƒ¼ã‚¿ã‚’æ›¸ã„ãŸã‚¿ã‚¤ãƒŸãƒ³ã‚°ã§é€ä¿¡ã•ã‚Œã‚‹
	} else if ((cfg->addr_size > 0) && (cfg->data_size > 0)) {
		// ã‚¢ãƒ‰ãƒ¬ã‚¹ã®è¨­å®š
		octspi_base_addr->ar = cfg->addr;
		// ãƒ‡ãƒ¼ã‚¿ãƒã‚¤ãƒ³ã‚¿ã‚’è¨­å®š
		this->data = cfg->data
		// ãƒ‡ãƒ¼ã‚¿ã‚µã‚¤ã‚ºã‚’è¨­å®š
		this->data_size = cfg->data_size
		// ãƒ‡ãƒ¼ã‚¿ã®è¨­å®šã®è¨­å®š
=======
	// ƒTƒCƒY‚ÌÝ’è
	octspi_base_addr->dlr = cfg->data_size;
	// ƒIƒ‹ƒ^ƒiƒeƒBƒuƒf[ƒ^‚ÌÝ’è
	octspi_base_addr->abr = cfg->alternate_size;
	octspi_base_addr->ccr = 
	// ƒ_ƒ~[ƒTƒCƒNƒ‹‚ÌÝ’è
	octspi_base_addr->tcr |= cfg->dummy_cycle;
	// ƒtƒH[ƒ}ƒbƒg‚ÌÝ’è
	octspi_base_addr->ccr = 
	// –½—ß‚ÌÝ’è
	octspi_base_addr->ir = cfg->inst;
	
	// ƒAƒhƒŒƒXAƒf[ƒ^‚Ç‚¿‚ç‚à‚È‚¢
	// (*) –½—ßƒŒƒWƒXƒ^‚É–½—ß‚ð‘‚¢‚½ƒ^ƒCƒ~ƒ“ƒO‚Å‘—M‚³‚ê‚é
	if ((cfg->addr_size == 0) && (cfg->data_size == 0)) {
		;
	// ƒAƒhƒŒƒX‚Í‚ ‚é‚ªAƒf[ƒ^‚ª‚È‚¢
	// (*) ƒAƒhƒŒƒXƒŒƒWƒXƒ^‚ÉƒAƒhƒŒƒX‚ð‘‚¢‚½ƒ^ƒCƒ~ƒ“ƒO‚Å‘—M‚³‚ê‚é
	} else if ((cfg->addr_size > 0) && (cfg->data_size == 0)) {
		// ƒAƒhƒŒƒX‚ÌÝ’è
		octspi_base_addr->ar = cfg->addr;
		
	// ƒAƒhƒŒƒXAƒf[ƒ^‚Ç‚¿‚ç‚à‚ ‚é
	// (*) ƒf[ƒ^ƒŒƒWƒXƒ^‚Éƒf[ƒ^‚ð‘‚¢‚½ƒ^ƒCƒ~ƒ“ƒO‚Å‘—M‚³‚ê‚é
	} else if ((cfg->addr_size > 0) && (cfg->data_size > 0)) {
		// ƒAƒhƒŒƒX‚ÌÝ’è
		octspi_base_addr->ar = cfg->addr;
		// ƒf[ƒ^ƒ|ƒCƒ“ƒ^‚ðÝ’è
		this->data = cfg->data
		// ƒf[ƒ^ƒTƒCƒY‚ðÝ’è
		this->data_size = cfg->data_size
		// ƒf[ƒ^‚ÌÝ’è‚ÌÝ’è
>>>>>>> d9d46175d1b56cbb3661cb48ee3fe9fab9f5c719
		octspi_base_addr->dr = *(this->data);
		
	} else {
		;
	}
	
<<<<<<< HEAD
	/* å‰²ã‚Šè¾¼ã¿æœ‰åŠ¹ */
=======
	/* Š„‚èž‚Ý—LŒø */
>>>>>>> d9d46175d1b56cbb3661cb48ee3fe9fab9f5c719
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
<<<<<<< HEAD
	/*  ABDTR of OCTOSPI_CCR -> DTRãƒ¢ãƒ¼ãƒ‰ */
=======
	/*  ABDTR of OCTOSPI_CCR -> DTRƒ‚[ƒh */
>>>>>>> d9d46175d1b56cbb3661cb48ee3fe9fab9f5c719
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
<<<<<<< HEAD
}
=======
}
>>>>>>> d9d46175d1b56cbb3661cb48ee3fe9fab9f5c719
