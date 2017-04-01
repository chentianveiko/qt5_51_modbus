/*
** Modbus.h - Header for Modbus Base Library
*/
#ifndef MODBUS_H
#define MODBUS_H

#define MB_MODE_MASTER
#define MB_MODE_SLAVE

#define MB_WAITER_PATIENTS	5

typedef unsigned char	MB_BYTE_T;
typedef unsigned short	MB_WORD_T;
typedef unsigned char	MB_BOOL_T;

typedef void (* MB_Sender)(MB_BYTE_T *data, MB_BYTE_T length);

/* Register Types */
enum {
	MB_REG_INPUT		= 0x01,
	MB_REG_HOLDING		= 0x02,
};

/* Function Codes */
enum {
	MB_FC_READ_REGS		= 0x03,	/* Read Holding Registers 4xxxx */
	MB_FC_READ_INPUT_REGS	= 0x04,	/* Read Input Registers 3xxxx */
	MB_FC_WRITE_REG		= 0x06,	/* Preset Single Register 4xxxx */
	MB_FC_WRITE_REGS	= 0x10,	/* Write block of contiguous registers 4xxxx */
};

/* Exception Codes */
enum {
	MB_EX_ILLEGAL_FUNCTION	= 0x01,	/* Function Code not Supported */
	MB_EX_ILLEGAL_ADDRESS	= 0x02,	/* Output Address not exists */
	MB_EX_ILLEGAL_VALUE	= 0x03,	/* Output Value not in Range */
	MB_EX_SLAVE_FAILURE	= 0x04,	/* Slave Deive Fails to process request */
};

/* Reply Types */
enum {
	MB_REPLY_OFF		= 0x01,
	MB_REPLY_ECHO		= 0x02,
	MB_REPLY_NORMAL		= 0x03,
};

#ifdef MB_MODE_SLAVE
typedef struct TRegister {
	MB_WORD_T address;
	MB_WORD_T value;
	struct TRegister* next;
} TRegister;
#endif

#ifdef MB_MODE_MASTER
typedef struct RemoteRegister {
	MB_BYTE_T SlaveAddr;
	MB_BYTE_T RegType;
	MB_WORD_T RegAddr;
	MB_WORD_T value;
	MB_BOOL_T Written;	/* Successfully write to remote */
	MB_BOOL_T Read;		/* Successfully read from remote */
	struct RemoteRegister *next;
} RemoteRegister;

enum {
	MB_RStep_StartQuery	= 0x01,
	MB_RStep_WaitingReply	= 0x02,
};
#endif

/* extern */
#ifdef MB_MODE_SLAVE
void MB_initSlave(MB_BYTE_T byAddr, MB_Sender sender);
void MB_addHreg(MB_WORD_T offset, MB_WORD_T value);
MB_BOOL_T MB_setHreg(MB_WORD_T offset, MB_WORD_T value);
MB_WORD_T MB_getHreg(MB_WORD_T offset);
void MB_addIreg(MB_WORD_T offset, MB_WORD_T value);
MB_BOOL_T MB_setIreg(MB_WORD_T offset, MB_WORD_T value);
MB_WORD_T MB_getIreg(MB_WORD_T offset);
void MB_slaveHandler(MB_BYTE_T *frame, MB_BYTE_T length);
#endif

#ifdef MB_MODE_MASTER
void MB_initMaster(MB_Sender sender);
void MB_addRreg(MB_BYTE_T byAddr, MB_BYTE_T type, MB_WORD_T offset);
MB_BOOL_T MB_readHregs(MB_BYTE_T step, MB_BYTE_T byAddr, MB_WORD_T offset, MB_WORD_T count, MB_BYTE_T *data);
MB_BOOL_T MB_writeHreg(MB_BYTE_T step, MB_BYTE_T byAddr, MB_WORD_T offset, MB_WORD_T data);
MB_BOOL_T MB_writeHregs(MB_BYTE_T step, MB_BYTE_T byAddr, MB_WORD_T offset, MB_WORD_T count, MB_BYTE_T *data);
MB_BOOL_T MB_readIregs(MB_BYTE_T step, MB_BYTE_T byAddr, MB_WORD_T offset, MB_WORD_T count, MB_BYTE_T *data);
void MB_masterHandler(MB_BYTE_T *frame, MB_BYTE_T length);
#endif

#endif
