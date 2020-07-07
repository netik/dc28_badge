
#ifndef _SX1262_LLDH_
#define _SX1262_LLDH_

/*
 * The SX1261/1262 is accessed using command transactions over the SPI
 * bus. Some of those commands involve reading/writing internal registers.
 * We document the command interface first, then the internal registers.
 */

#ifdef notdef

/* For easy reference, this is a list of all commands in numerical order. */

#define SX_CMD_RESETSTATS	0x00	/* Reset statistics */
#define SX_CMD_ACKIRQ		0x02	/* Clear IRQ events */
#define SX_CMD_CLEARERRS	0x07	/* Clear device errors */
#define SX_CMD_SETDIOIRQ	0x08	/* Set IRQ/DIO modes */
#define SX_CMD_REGWRITE		0x0D	/* Write an internal register */
#define SX_CMD_BUFWRITE		0x0E	/* Write to an internal buffer */
#define SX_CMD_GETSTATS		0x10	/* Get statistics */
#define SX_CMD_GETPKT		0x11	/* Get packet type */
#define SX_CMD_GETIRQ		0x12	/* Get IRQ events */
#define SX_CMD_GETRXBUFSTS	0x13	/* Get buffer status */
#define SX_CMD_GETPKTSTS	0x14	/* Get packet status */
#define SX_CMD_GETRSSI		0x15	/* Get instantaneous RSSI */
#define SX_CMD_GETERRS		0x17	/* Get device errors */
#define SX_CMD_REGREAD		0x1D	/* Read an internal register */
#define SX_CMD_BUFREAD		0x1E	/* Read from an internal buffer */
#define SX_CMD_SETSTDBY		0x80	/* Set chip in standby mode */
#define SX_CMD_SETRX		0x82	/* Set chip in RX mode */
#define SX_CMD_SETTX		0x83	/* Set chip in TX mode */
#define SX_CMD_SETSLEEP		0x84	/* Set chip in sleep mode */
#define SX_CMD_SETFREQ		0x86	/* Set RF frequency */
#define SX_CMD_SETCADPARAM	0x88	/* Set CAD params */
#define SX_CMD_CALIBRATE	0x89	/* Calibrate modules */
#define SX_CMD_SETPKT		0x8A	/* Set packet type */
#define SX_CMD_SETMODPARAM	0x8B	/* Set modulation params */
#define SX_CMD_SETPKTPARAM	0x8C	/* Set packet params */
#define SX_CMD_SETTXPARAM	0x8E	/* Set TX params */
#define SX_CMD_SETBUFBASE	0x8F	/* Set RX/TX buffer base addresses */
#define SX_CMD_SETRXTXFB	0x93	/* Set RX/TX fallback mode */
#define SX_CMD_SETRXDUTY	0x94	/* Set RX duty cycle */
#define SX_CMD_SETPACFG		0x95	/* Set power amp config */
#define SX_CMD_SETREGMODE	0x96	/* Set regulator modes */
#define SX_CMD_SETDIO3ASTXCO	0x97	/* Use TXCO controlled by DIO3 */
#define SX_CMD_CALIMG		0x98	/* Start image calibration */
#define SX_CMD_SETDIO2ASRFSW	0x9D	/* Use RF switch controlled by DIO2 */
#define SX_CMD_STOPTIMER	0x9F	/* Stop RX timer on preamble det. */
#define SX_CMD_SETLORASYMTO	0xA0	/* Set LoRa symbol number timeout */
#define SX_CMD_GETSTS		0xC0	/* Set device status */
#define SX_CMD_SETFS		0xC1	/* Set chip in freq. synthesis mode */
#define SX_CMD_SETCAD		0xC5	/* Set chip in RX with passed CAD */
#define SX_CMD_TXCONTINUOUS	0xD1	/* TX dead carrier */
#define SX_CMD_TXINFPREAMBLE	0xD2	/* Set TX infinite preamble */

#endif

/* -------------------- Operational Modes Functions ------------------------ */

#define SX_CMD_SETSLEEP		0x84	/* Set chip in sleep mode */
#define SX_CMD_SETSTDBY		0x80	/* Set chip in standby mode */
#define SX_CMD_SETFS		0xC1	/* Set chip in freq. synthesis mode */
#define SX_CMD_SETRX		0x82	/* Set chip in RX mode */
#define SX_CMD_SETTX		0x83	/* Set chip in TX mode */
#define SX_CMD_STOPTIMER	0x9F	/* Stop RX timer on preamble det. */
#define SX_CMD_SETRXDUTY	0x94	/* Set RX duty cycle */
#define SX_CMD_SETCAD		0xC5	/* Set chip in RX with passed CAD */
#define SX_CMD_TXCONTINUOUS	0xD1	/* TX dead carrier */
#define SX_CMD_TXINFPREAMBLE	0xD2	/* Set TX infinite preamble */
#define SX_CMD_SETREGMODE	0x96	/* Set regulator modes */
#define SX_CMD_CALIBRATE	0x89	/* Calibrate modules */
#define SX_CMD_CALIMG		0x98	/* Start image calibration */
#define SX_CMD_SETPACFG		0x95	/* Set power amp config */
#define SX_CMD_SETRXTXFB	0x93	/* Set RX/TX fallback mode */

typedef struct sx_setsleep {
	uint8_t		sx_opcode;
	uint8_t		sx_sleepscf;
} SX_SETSLEEP;

#define SX_SLEEPCFG_RTCTIMEO	0x01
#define SX_SLEEPCFG_WARMSTART	0x02

typedef struct sx_setstdby {
	uint8_t		sx_opcode;
	uint8_t		sx_stdbymode;
} SX_SETSTDBY;

#define SX_STDBY_RC		0x00
#define SX_STDBY_XOSC		0x01

#pragma pack(1)
typedef struct sx_setfs {
	uint8_t		sx_opcode;
} SX_SETFS;
#pragma pack()

typedef struct sx_settx {
	uint8_t		sx_opcode;
	uint8_t		sx_timeo2;
	uint8_t		sx_timeo1;
	uint8_t		sx_timeo0;
} SX_SETTX;

typedef struct sx_setrx {
	uint8_t		sx_opcode;
	uint8_t		sx_timeo2;
	uint8_t		sx_timeo1;
	uint8_t		sx_timeo0;
} SX_SETRX;

typedef struct sx_stoptimer {
	uint8_t		sx_opcode;
	uint8_t		sx_stoptimerparam;
} SX_STOPTIMER;

#define SX_STOPTIMER_SYNCWORD	0x00	/* Timer stop on sync/hdr detect */
#define SX_STOPTIMER_PREAMBLE	0x01	/* Timer stop on preamble detect */

#pragma pack(1)
typedef struct sx_setrxduty {
	uint8_t		sx_opcode;
	uint8_t		sx_rxperiod[3];
	uint8_t		sx_sleepperiod[3];
} SX_SETRXDUTY;
#pragma pack()

#define SX_RXPERIOD 15.625		/* microseconds */

#pragma pack(1)
typedef struct sx_setcad {
	uint8_t		sx_opcode;
} SX_SETCAD;
#pragma pack()

#pragma pack(1)
typedef struct sx_txcontinuous {
	uint8_t		sx_opcode;
} SX_TXCONTINUOUS;
#pragma pack()

#pragma pack(1)
typedef struct sx_txinfpreamble {
	uint8_t		sx_opcode;
} SX_TXINFPREAMBLE;
#pragma pack()

typedef struct sx_setregmode {
	uint8_t		sx_opcode;
	uint8_t		sx_regparam;
} SX_SETREGMODE;

#define SX_REGMODE_LDOONLY	0x00
#define SX_REGMODE_LDO_DCDC	0x01

typedef struct sx_calibrate {
	uint8_t		sx_opcode;
	uint8_t		sx_calparam;
} SX_CALIBRATE;

#define SX_CALIBRATE_RC64K	0x01
#define SX_CALIBRATE_RC13M	0x02
#define SX_CALIBRATE_PLL	0x04
#define SX_CALIBRATE_ADCPULSE	0x08
#define SX_CALIBRATE_ADCBULKN	0x10
#define SX_CALIBRATE_ADCBULKP	0x20
#define SX_CALIBRATE_IMAGEREJ	0x40

#pragma pack(1)
typedef struct sx_calimg {
	uint8_t		sx_opcode;
	uint8_t		sx_freq1;
	uint8_t		sx_freq2;
} SX_CALIMG;
#pragma pack()

#pragma pack(1)
typedef struct sx_setpacfg {
	uint8_t		sx_opcode;
	uint8_t		sx_padutycycle;
	uint8_t		sx_hpmax;
	uint8_t		sx_devsel;
	uint8_t		sx_palut;
} SX_SETPACFG;
#pragma pack()

#define SX_SETPACFG_DEV1262	0x00
#define SX_SETPACFG_DEV1261	0x01

typedef struct sx_setrxtxfb {
	uint8_t		sx_opcode;
	uint8_t		sx_fbmode;
} SX_SETRXTXFB;

#define SX_FBMODE_STDBYRC	0x20
#define SX_FBMODE_STDBYXOSC	0x30
#define SX_FBMODE_FS		0x40

/* -------------------- Registers and Buffer Access ------------------------ */

#define SX_CMD_REGWRITE		0x0D	/* Write an internal register */
#define SX_CMD_REGREAD		0x1D	/* Read an internal register */
#define SX_CMD_BUFWRITE		0x0E	/* Write to an internal buffer */
#define SX_CMD_BUFREAD		0x1E	/* Read from an internal buffer */

#pragma pack(1)
typedef struct sx_regwrite {
	uint8_t		sx_opcode;
	uint16_t	sx_address;
	uint8_t		sx_val;
} SX_REGWRITE;
#pragma pack()

#pragma pack(1)
typedef struct sx_regread {
	uint8_t		sx_opcode;
	uint16_t	sx_address;
	uint8_t		sx_status;
	uint8_t		sx_val;
} SX_REGREAD;
#pragma pack()

typedef struct sx_bufwrite {
	uint8_t		sx_opcode;
	uint8_t		sx_offset;
} SX_BUFWRITE;

#pragma pack(1)
typedef struct sx_bufread {
	uint8_t		sx_opcode;
	uint8_t		sx_offset;
	uint8_t		sx_status;
} SX_BUFREAD;
#pragma pack()

/* ------------------- DIO and IRQ Control Functions ----------------------- */

#define SX_CMD_SETDIOIRQ	0x08	/* Set IRQ/DIO modes */
#define SX_CMD_GETIRQ		0x12	/* Get IRQ events */
#define SX_CMD_ACKIRQ		0x02	/* Clear IRQ events */
#define SX_CMD_SETDIO2ASRFSW	0x9D	/* Use RF switch controlled by DIO2 */
#define SX_CMD_SETDIO3ASTXCO	0x97	/* Use TXCO controlled by DIO3 */

#pragma pack(1)
typedef struct sx_setdioirq {
	uint8_t		sx_opcode;
	uint16_t	sx_irqmask;
	uint16_t	sx_dio1mask;
	uint16_t	sx_dio2mask;
	uint16_t	sx_dio3mask;
} SX_SETDIOIRQ;
#pragma pack()

#pragma pack(1)
typedef struct sx_getirq {
	uint8_t		sx_opcode;
	uint8_t		sx_status;
	uint16_t	sx_irqsts;
} SX_GETIRQ;
#pragma pack()

#define SX_IRQ_TXDONE		0x0001	/* Packet transmission complete */
#define SX_IRQ_RXDONE		0x0002	/* Packet received */
#define SX_IRQ_PREAMDET		0x0004	/* Preamble detected */
#define SX_IRQ_SYNCVALID	0x0008	/* Valid FSK sync word detected */
#define SX_IRQ_HDRVALID		0x0010	/* LORA header received */
#define SX_IRQ_HDRERR		0x0020	/* LORA header CRC error */
#define SX_IRQ_CRCERR		0x0040	/* Bad CRC received (all modes) */
#define SX_IRQ_CADDONE		0x0080	/* Channel activity detection done */
#define SX_IRQ_CADDET		0x0100	/* Channel activity detected */
#define SX_IRQ_TIMEO		0x0200	/* RX or TX timeout */

#pragma pack(1)
typedef struct sx_ackirq {
	uint8_t		sx_opcode;
	uint16_t	sx_irqsts;
} SX_ACKIRQ;
#pragma pack()

#pragma pack(1)
typedef struct sx_setdio2asrfsw {
	uint8_t		sx_opcode;
	uint8_t		sx_enable;
} SX_SETDIO2ASRFSW;
#pragma pack()

#define SX_DIO2_IRQ		0x00	/* DIO2 is an interrupt */
#define SX_DIO2_RFSW		0x01	/* DIO2 is RX/TX RF switch control */

#pragma pack(1)
typedef struct sx_setdio3astxco {
	uint8_t		sx_opcode;
	uint8_t		sx_txcovoltage;
	uint8_t		sx_delay[3];
} SX_SETDIO3ASTXCO;
#pragma pack()

#define SX_TXCO_1_6V		0x00
#define SX_TXCO_1_7V		0x01
#define SX_TXCO_1_8V		0x02
#define SX_TXCO_2_2V		0x03
#define SX_TXCO_2_4V		0x04
#define SX_TXCO_2_7V		0x05
#define SX_TXCO_3_0V		0x06
#define SX_TXCO_3_3V		0x07

#define SX_TXCOPERIOD		15.625		/* microseconds */

/* -------------- Modulation and Packet-Related Functions ------------------ */

#define SX_CMD_SETFREQ		0x86	/* Set RF frequency */
#define SX_CMD_GETPKT		0x11	/* Get packet type */
#define SX_CMD_SETPKT		0x8A	/* Set packet type */
#define SX_CMD_SETTXPARAM	0x8E	/* Set TX params */
#define SX_CMD_SETMODPARAM	0x8B	/* Set modulation params */
#define SX_CMD_SETPKTPARAM	0x8C	/* Set packet params */
#define SX_CMD_SETCADPARAM	0x88	/* Set CAD params */
#define SX_CMD_SETBUFBASE	0x8F	/* Set RX/TX buffer base addresses */
#define SX_CMD_SETLORASYMTO	0xA0	/* Set LoRa symbol number timeout */

#pragma pack(1)
typedef struct sx_setfreq {
	uint8_t		sx_opcode;
	uint32_t	sx_freq;
} SX_SETFREQ;
#pragma pack()

/*
 * The radio frequency is defined as:
 *
 *            [desired RF freq] * XTAL freq
 * sx_freq =  -----------------------------
 *                         25
 *                        2
 *
 * The reference design uses a 32MHz external crystal.
 */

#define SX_XTALFREQ		((double)32000000)
#define SX_POW			((double)0x2000000)
#define SX_FREQ(x)	(uint32_t) (((double)(x) * SX_XTALFREQ) / SX_POW)

#pragma pack(1)
typedef struct sx_setpkt {
	uint8_t		sx_opcode;
	uint8_t		sx_pkttype;
} SX_SETPKT;
#pragma pack()

#define SX_PKT_GFSK		0x00
#define SX_PKT_LORA		0x01

#pragma pack(1)
typedef struct sx_getpkt {
	uint8_t		sx_opcode;
	uint8_t		sx_status;
	uint8_t		sx_pkttype;
} SX_GETPKT;
#pragma pack()

#pragma pack(1)
typedef struct sx_settxparam {
	uint8_t		sx_opcode;
	uint8_t		sx_power;
	uint8_t		sx_ramptime;
} SX_SETTXPARAM;
#pragma pack()

#define SX_RAMPTIME_10US	0x00
#define SX_RAMPTIME_20US	0x01
#define SX_RAMPTIME_40US	0x02
#define SX_RAMPTIME_80US	0x03
#define SX_RAMPTIME_200US	0x04
#define SX_RAMPTIME_800US	0x05
#define SX_RAMPTIME_1700US	0x06
#define SX_RAMPTIME_3400US	0x07

/* GFSK parameters */

#pragma pack(1)
typedef struct sx_setmodparam_gfsk {
	uint8_t		sx_opcode;
	uint8_t		sx_gfskbr[3];	/* GFSK bitrate */
	uint8_t		sx_pshape;	/* Pulse shape */
	uint8_t		sx_bw;		/* Bandwidth */
	uint8_t		sx_fdev[3];	/* Frequency deviation */
} SX_SETMODPARAM_GFSK;
#pragma pack()

#define SX_GFSKBR(x)		((32 * SX_XTALFREQ) / (x))

#define SX_PSHAPE_NONE		0x00
#define SX_PSHAPE_GAUSSBT0_3	0x08
#define SX_PSHAPE_GAUSSBT0_5	0x09
#define SX_PSHAPE_GAUSSBT0_7	0x0A
#define SX_PSHAPE_GAUSSBT1	0x0B

#define SX_RX_BW_4800		0x1F
#define SX_RX_BW_5800		0x17
#define SX_RX_BW_7300		0x0F
#define SX_RX_BW_9700		0x1E
#define SX_RX_BW_11700		0x16
#define SX_RX_BW_14600		0x0E
#define SX_RX_BW_19500		0x1D
#define SX_RX_BW_23400		0x15
#define SX_RX_BW_29300		0x0D
#define SX_RX_BW_39000		0x1C
#define SX_RX_BW_46900		0x14
#define SX_RX_BW_58600		0x0C
#define SX_RX_BW_78200		0x1B
#define SX_RX_BW_93800		0x13
#define SX_RX_BW_117300		0x0B
#define SX_RX_BW_156200		0x1A
#define SX_RX_BW_187200		0x12
#define SX_RX_BW_234300		0x0A
#define SX_RX_BW_312000		0x19
#define SX_RX_BW_373600		0x11
#define SX_RX_BW_467000		0x09

/* LoRa parameters */

#pragma pack(1)
typedef struct sx_setmodparam_lora {
	uint8_t		sx_opcode;
	uint8_t		sx_sf;		/* Spreading factor */
	uint8_t		sx_bw;		/* Bandwidth */
	uint8_t		sx_cr;		/* Coding rate */
	uint8_t		sx_ldopt;	/* Low data rate optimize */
} SX_SETMODPARAM_LORA;
#pragma pack()

#define SX_LORA_SF5		0x05
#define SX_LORA_SF6		0x06
#define SX_LORA_SF7		0x07
#define SX_LORA_SF8		0x08
#define SX_LORA_SF9		0x09
#define SX_LORA_SF10		0x0A
#define SX_LORA_SF11		0x0B
#define SX_LORA_SF12		0x0C

#define SX_LORA_BW_7		0x00
#define SX_LORA_BW_10		0x08
#define SX_LORA_BW_15		0x01
#define SX_LORA_BW_20		0x09
#define SX_LORA_BW_31		0x02
#define SX_LORA_BW_41		0x0A
#define SX_LORA_BW_62		0x03
#define SX_LORA_BW_125		0x04
#define SX_LORA_BW_250		0x05
#define SX_LORA_BW_500		0x06

#define SX_LORA_CR_4_5		0x01
#define SX_LORA_CR_4_6		0x02
#define SX_LORA_CR_4_7		0x03
#define SX_LORA_CR_4_8		0x03

#define SX_LORA_LDOPT_OFF	0x00
#define SX_LORA_LDOPT_ON	0x01

/* GFSK packet parameters */

#pragma pack(1)
typedef struct sx_setpktparam_gfsk {
	uint8_t		sx_opcode;
	uint16_t	sx_preamlen;	/* Preamble length in bits */
	uint8_t		sx_preamdetlen;	/* Preamble detector length */
	uint8_t		sx_syncwordlen;	/* Sync word len in bits */
	uint8_t		sx_addrcomp;	/* Address filtering config */
	uint8_t		sx_pkttype;	/* Packet length included or not */
	uint8_t		sx_paylen;	/* Payload length */
	uint8_t		sx_crctype;
	uint8_t		sx_whitening;
} SX_SETPKTPARAM_GFSK;
#pragma pack()

#define SX_GFSK_PREAM_OFF	0x00
#define SX_GFSK_PREAM_8BITS	0x04
#define SX_GFSK_PREAM_16BITS	0x05
#define SX_GFSK_PREAM_24BITS	0x06
#define SX_GFSK_PREAM_32BITS	0x07

#define SX_GFSK_ADDRCOMP_OFF	0x00	/* Filtering off */
#define SX_GFSK_ADDRCOMP_NODE	0x01	/* Filtering on unicast node addr */
#define SX_GFSK_ADDRCOMP_NODEBR	0x02	/* Filtering on both ucast and bcast */

#define SK_GFSK_PKTTYPE_NOLEN	0x00	/* Length not included in pkt */
#define SK_GFSK_PKTTYPE_LEN	0x01	/* Length included in pkt */

#define SK_GFSK_CRCTYPE_OFF	0x01	/* No CRC */
#define SK_GFSK_CRCTYPE_1BYTE	0x00	/* CRC computed on 1 byte */
#define SK_GFSK_CRCTYPE_2BYTE	0x02	/* CRC computed on 2 bytes */
#define SK_GFSK_CRCTYPE_1BYTEIN	0x04	/* computed on 1 byte and inverted */
#define SK_GFSK_CRCTYPE_2BYTEIN	0x06	/* computed on 2 bytes and inverted */

#define SK_GFSK_WHITENING_OFF	0x00
#define SK_GFSK_WHITENING_ON	0x01

/* LoRa packet parameters */

#pragma pack(1)
typedef struct sx_setpktparam_lora {
	uint8_t		sx_opcode;
	uint16_t	sx_preamlen;	/* Preamble length in # of symbols */
	uint8_t		sx_headertype;	/* Variable or fixed payload */
	uint8_t		sx_payloadlen;	/* Payload length in bytes */
	uint8_t		sx_crctype;	/* CRC off or on */
	uint8_t		sx_invertiq;	/* Standard or inverted IQ */
} SX_SETPKTPARAM_LORA;
#pragma pack()

#define SX_LORA_HDR_VARIABLE	0x00	/* Variable size, explicit header */
#define SX_LORA_HDR_FIXED	0x01	/* Fixed size, implicit header */

#define SX_LORA_CRCTYPE_OFF	0x00
#define SX_LORA_CRCTYPE_ON	0x01

#define SX_LORA_IQ_STANDARD	0x00
#define SX_LORA_IQ_INVERT	0x01

#pragma pack(1)
typedef struct sx_setcadparams {
	uint8_t		sx_opcode;
	uint8_t		sx_cadsymnum;
	uint8_t		sx_caddetpeak;
	uint8_t		sx_caddetmin;
	uint8_t		sx_cadexitmode;
	uint8_t		sx_cadtimeo[3];
} SX_SETCADPARAMS;
#pragma pack()

#define SX_CAD_SYMB_1		0x00
#define SX_CAD_SYMB_2		0x01
#define SX_CAD_SYMB_4		0x02
#define SX_CAD_SYMB_8		0x03
#define SX_CAD_SYMB_16		0x04

#define SX_CAD_EXITMODE_CADONLY	0x00
#define SX_CAD_EXITMODE_CADRX	0x01

#pragma pack(1)
typedef struct sx_setbufbase {
	uint8_t		sx_opcode;
	uint8_t		sx_txbase;
	uint8_t		sx_rxbase;
} SX_SETBUFBASE;
#pragma pack()

#pragma pack(1)
typedef struct sx_setlorasymto {
	uint8_t		sx_opcode;
	uint8_t		sx_symbnum;
} SX_SETLORASYMTO;
#pragma pack()

/* ---------------- Communication Status Information ----------------------- */

#define SX_CMD_GETSTS		0xC0	/* Set device status */
#define SX_CMD_GETRXBUFSTS	0x13	/* Get buffer status */
#define SX_CMD_GETPKTSTS	0x14	/* Get packet status */
#define SX_CMD_GETRSSI		0x15	/* Get instantaneous RSSI */
#define SX_CMD_GETSTATS		0x10	/* Get statistics */
#define SX_CMD_RESETSTATS	0x00	/* Reset statistics */

#pragma pack(1)
typedef struct sx_getsts {
	uint8_t		sx_opcode;
	uint8_t		sx_status;
} SX_GETSTS;
#pragma pack()

#define SX_GETSTS_RSVD0		0x01
#define SX_GETSTS_CMDSTS	0x0E
#define SX_GETSTS_CHIPMODE	0x70
#define SX_GETSTS_RSVD1		0x80

#define SX_CMDSTS(x)		(((x) & SX_GETSTS_CMDSTS) >> 1)
#define SX_CHIPMODE(x)		(((x) & SX_GETSTS_CHIPMODE) >> 4)

#define SX_CMDSTS_RSVD0		0x00
#define SX_CMDSTS_RSVD1		0x01
#define SX_CMDSTS_DATAAVAIL	0x02	/* Data available to host */
#define SX_CMDSTS_CMDTIMEO	0x03	/* Command timeout */
#define SX_CMDSTS_CMDPROCERR	0x04	/* Command processing error */
#define SX_CMDSTS_CMDEXFAIL	0x05	/* Failed to execute command */
#define SX_CMDSTS_CMDTXDONE	0x06	/* Packet transmission completed */

#define SX_CHIPMODE_UNUSED	0x00
#define SX_CHIPMODE_RSVD0	0x01
#define SX_CHIPMODE_STDBYRC	0x02	/* Standby using RC oscillator */
#define SX_CHIPMODE_STDBXOSC	0x03	/* Standby using Xtal oscillator */
#define SX_CHIPMODE_FS		0x04	/* Frequency synthesis mode */
#define SX_CHIPMODE_RX		0x05	/* RX mode */
#define SX_CHIPMODE_TX		0x06	/* TX mode */

#pragma pack(1)
typedef struct sx_getrxbufsts {
	uint8_t		sx_opcode;
	uint8_t		sx_status;
	uint8_t		sx_rxpaylen;
	uint8_t		sx_rxstbufptr;
} SX_GETRXBUFSTS;
#pragma pack()

#pragma pack(1)
typedef struct sx_getpktsts_gfsk {
	uint8_t		sx_opcode;
	uint8_t		sx_status;
	uint8_t		sx_rxstatus;
	uint8_t		sx_rssisync;
	uint8_t		sx_rssiavg;
} SX_GETPKTSTS_GFSK;
#pragma pack()

#pragma pack(1)
typedef struct sx_getpktsts_lora {
	uint8_t		sx_opcode;
	uint8_t		sx_status;
	uint8_t		sx_rssipkt;
	uint8_t		sx_snrpkt;
	uint8_t		sx_sigrssipkt;
} SX_GETPKTSTS_LORA;
#pragma pack()

#define SX_RXSTS_PKTSENT	0x01	/* packet sent */
#define SX_RXSTS_PKTRCVD	0x02	/* packet received */
#define SX_RXSTS_ABTERR		0x04	/* abort error */
#define SX_RXSTS_LENERR		0x08	/* length error */
#define SX_RXSTS_CRCERR		0x10	/* CRC error */
#define SX_RXSTS_ADRERR		0x20	/* Address error */
#define SX_RXSTS_SYNCERR	0x40	/* sync pattern error */
#define SX_RXSTS_PREAMERR	0x80	/* Preamble error */

#pragma pack(1)
typedef struct sx_getrssi {
	uint8_t		sx_opcode;
	uint8_t		sx_status;
	uint8_t		sx_rssi;
} SX_GETRSSI;
#pragma pack()

#pragma pack(1)
typedef struct sx_getstats_gfsk {
	uint8_t		sx_opcode;
	uint8_t		sx_status;
	uint16_t	sx_pktrcvd;
	uint16_t	sx_pktcrcerr;
	uint16_t	sx_pktlenerr;
} SX_GETSTATS_GFSK;
#pragma pack()

#pragma pack(1)
typedef struct sx_getstats_lora {
	uint8_t		sx_opcode;
	uint8_t		sx_status;
	uint16_t	sx_pktrcvd;
	uint16_t	sx_pktcrcerr;
	uint16_t	sx_pkthdrerr;
} SX_GETSTATS_LORA;
#pragma pack()

#pragma pack(1)
typedef struct sx_resetstats {
	uint8_t		sx_opcode;
	uint8_t		sx_dummy[6];
} SX_RESETSTATS;
#pragma pack()

/* ---------------------------- Miscellaneous ------------------------------ */

#define SX_CMD_GETERRS		0x17	/* Get device errors */
#define SX_CMD_CLEARERRS	0x07	/* Clear device errors */

#pragma pack(1)
typedef struct sx_geterrs {
	uint8_t		sx_opcode;
	uint8_t		sx_status;
	uint16_t	sx_errs;
} SX_GETERRS;
#pragma pack()

#pragma pack(1)
typedef struct sx_clrerrs {
	uint8_t		sx_opcode;
	uint8_t		sx_stat1;
	uint8_t		sx_stat2;
} SX_CLRERRS;
#pragma pack()

#define SX_OPERR_RC64K_CAL_ERR	0x0001	/* RC64K calibration failed */
#define SX_OPERR_RC13M_CAL_ERR	0x0002	/* RC13M calibration failed */
#define SX_OPERR_PLL_CAL_ERR	0x0004	/* PLL calibration failed */
#define SX_OPERR_ADC_CAL_ERR	0x0008	/* ADC calibration failed */
#define SX_OPERR_IMG_CAL_ERR	0x0010	/* IMG calibration failed */
#define SX_OPERR_XOSC_CAL_ERR	0x0020	/* XOSC calibration failed */
#define SX_OPERR_PLL_LOCK_ERR	0x0040	/* PLL failed to lock */
#define SX_OPERR_PA_RAMP_ERR	0x0100	/* PA ramping failed */

/* -------------------------- Register table ------------------------------- */

#define SX_REG_DIOX_OUT_ENB	0x0580	/* Non-standard DIO control */
#define SX_REG_DIOX_IN_ENB	0x0583	/* Non-standard DIO control */
#define SX_REG_DIOX_PULLUP_CTL	0x0584	/* Non-standard DIO control */
#define SX_REG_DIOX_PULLDWN_CTL	0x0585	/* Non-standard DIO control */
#define SX_REG_WHITEINIT_MSB	0x06B8	/* Whitening initial value MSB */
#define SX_REG_WHITEINIT_LSB	0x06B9	/* Whitening initial value LSB */
#define SX_REG_CRCINIT_MSB	0x06BC	/* CRC initial value MSB */
#define SX_REG_CRCINIT_LSB	0x06BD	/* CRC initial value LSB */
#define SX_REG_CRCPOLY_MSB	0x06BE	/* CRC polynomial value MSB */
#define SX_REG_CRCPOLY_LSB	0x06BF	/* CRC polynomial value LSB */
#define SX_REG_SYNCWORD_0	0x06C0	/* Syncword in FSK mode */
#define SX_REG_SYNCWORD_1	0x06C1	/* Syncword in FSK mode */
#define SX_REG_SYNCWORD_2	0x06C2	/* Syncword in FSK mode */
#define SX_REG_SYNCWORD_3	0x06C3	/* Syncword in FSK mode */
#define SX_REG_SYNCWORD_4	0x06C4	/* Syncword in FSK mode */
#define SX_REG_SYNCWORD_5	0x06C5	/* Syncword in FSK mode */
#define SX_REG_SYNCWORD_6	0x06C6	/* Syncword in FSK mode */
#define SX_REG_SYNCWORD_7	0x06C7	/* Syncword in FSK mode */
#define SX_REG_NODEADDR		0x06CD	/* Node address in FSK mode */
#define SX_REG_BCASTADDR	0x06CE	/* Broadcast address in FSK mode */
#define SX_REG_PAYLEN		0x0702	/* Payload length */
#define SX_REG_PTKPARMS		0x0704	/* Packet configuration */
#define SX_REG_SYNCTIMEOUT	0x0706	/* Recalculated # of symbols */
#define SX_REG_IQPOL_SETUP	0x0736	/* Optimize inverted IQ operation */
#define SX_REG_LORA_SYNC_MSB	0x0740	/* LORA sync word MSB */
#define SX_REG_LORA_SYNC_LSB	0x0741	/* LORA sync word LSB */
#define SX_REG_RNG_0		0x0819	/* 32-bit random number byte 0 */
#define SX_REG_RNG_1		0x081A	/* 32-bit random number byte 1 */
#define SX_REG_RNG_2		0x081B	/* 32-bit random number byte 2 */
#define SX_REG_RNG_3		0x081C	/* 32-bit random number byte 3 */
#define SX_REG_TXMOD		0x0889	/* TX modulation quality */
#define SX_REG_RXGAIN		0x08AC	/* Set gain in RX mode */
#define SX_REG_TXCLAMPCFG	0x08D8	/* PA clamping mechanism */
#define SX_REG_OCPCFG		0x08E7	/* Overcurrent protection level */
#define SX_REG_RTCCTL		0x0902	/* Enable or disable RTC timer */
#define SX_REG_XTATRIM		0x0911	/* Value of XTA trim cap */
#define SX_REG_XTBTRIM		0x0912	/* Value of XTB trim cap */
#define SX_REG_DIO3_OUTCVYL	0x0920	/* Non-standard DIO2 control */
#define SC_REG_EVENT_MASK	0x0944	/* Used to clear events */

#define SX_CRC_SEED_IBM		0xFFFF
#define SX_CRC_POLY_IBM		0x8005

/* CCITT is the default */

#define SX_CRC_SEED_CCITT	0x1D0F
#define SX_CRC_POLY_CCITT	0x1201

#define SX_LORA_SYNC_PUBLIC	0x3444	/* Sync word for public network */
#define SX_LORA_SYNC_PRIVATE	0x1424	/* Sync word for private network */

#define SX_RXGAIN_PWRSAVE	0x94	/* RX power saving gain */
#define SX_RXGAIN_BOOSTED	0x96	/* RX boosted gain */

#define SX_OCP_1261_60MA	0x18	/* Overcurrent level for SX1261 */
#define SX_OCP_1262_140MA	0x38	/* Overcurrent level for SX1262 */

/* -------------------------- Driver-specific ------------------------------ */

#define SX_DELAY		100

#define SX_MAX_PKT		256
#define SX_MAX_CMD		64
#define SX_ALIGNMENT		CACHE_LINE_SIZE
#define roundup(x, y)		((((x)+((y)-1))/(y))*(y))

#undef SX_TX_POWER_22DB
#define SX_TX_POWER_14DB

#undef SX_MANUAL_CAD

typedef struct sx1262_driver {
	SPIDriver *		sx_spi;
	uint32_t		sx_freq;
	uint16_t		sx_syncword;
	uint8_t			sx_symtimeout;
	uint8_t			sx_preamlen;
	uint8_t			sx_pktlen;
	thread_reference_t	sx_threadref;
	thread_reference_t	sx_txwait;
	uint8_t			sx_service;
	bool			sx_txdone;
	thread_t *		sx_thread;
	uint8_t *		sx_rxbuf_orig;
	uint8_t *		sx_rxbuf;
	uint8_t *		sx_cmdbuf;
	void *			sx_netif;
} SX1262_Driver;

extern SX1262_Driver SX1262D1;

extern void sx1262Start (SX1262_Driver *);
extern void sx1262Stop (SX1262_Driver *);
extern uint32_t sx1262Random (SX1262_Driver *);

#endif /* _SX1262_LLDH_ */

