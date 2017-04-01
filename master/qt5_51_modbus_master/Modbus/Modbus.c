/*
** Modbus.c - Source for Modbus Base Library
*/
#include <stdlib.h>	/* malloc */
#include "Modbus.h"

#define FALSE	(1 == 0)
#define TRUE	(1 == 1)

/* Inner Globle Variable */
#ifdef MB_MODE_SLAVE
static MB_BYTE_T *s_ReplyFrame;
static MB_BYTE_T s_RLen;
static MB_BYTE_T s_ReplyType;
static MB_BYTE_T s_Addr;
static MB_Sender s_sender;
static TRegister *localRegsHead = 0;
static TRegister *localRegsLast = 0;
#endif

#ifdef MB_MODE_MASTER
static MB_BYTE_T *m_QueryFrame;
static MB_BYTE_T m_QLen;
static MB_Sender m_sender;
static RemoteRegister *remoteRegsHead = 0;
static RemoteRegister *remoteRegsLast = 0;
#endif

/* Inner Global function defination */
static MB_WORD_T MB_CRC(MB_BYTE_T *data, MB_BYTE_T length);
static int check_integrity(MB_BYTE_T *msg, int msg_length);
#ifdef MB_MODE_SLAVE
static TRegister *searchRegister(MB_WORD_T addr);
static void addReg(MB_WORD_T address, MB_WORD_T value);
static MB_BOOL_T setReg(MB_WORD_T address, MB_WORD_T value);
static MB_WORD_T getReg(MB_WORD_T address);
static void readRegisters(MB_WORD_T startreg, MB_WORD_T numregs);
static void writeSingleRegister(MB_WORD_T reg, MB_WORD_T value);
static void writeMultipleRegisters(MB_BYTE_T* frame,MB_WORD_T startreg, MB_WORD_T numoutputs, MB_BYTE_T BYTEcount);
static void exceptionResponse(MB_BYTE_T fcode, MB_BYTE_T excode);
static void readInputRegisters(MB_WORD_T startreg, MB_WORD_T numregs);
#endif
#ifdef MB_MODE_MASTER
static RemoteRegister *searchRemoteRegister(MB_BYTE_T SlaveAddr, MB_BYTE_T RegType, MB_WORD_T RegAddr);
static MB_BOOL_T setRemoteReg(MB_BYTE_T SlaveAddr, MB_BYTE_T RegType, MB_WORD_T RegAddr, MB_WORD_T value);
static MB_WORD_T getRemoteReg(MB_BYTE_T SlaveAddr, MB_BYTE_T RegType, MB_WORD_T RegAddr);
static MB_BOOL_T setRemoteRegWritten(MB_BYTE_T SlaveAddr, MB_BYTE_T RegType, MB_WORD_T RegAddr);
static MB_BOOL_T getRemoteRegWritten(MB_BYTE_T SlaveAddr, MB_BYTE_T RegType, MB_WORD_T RegAddr);
static MB_BOOL_T clearRemoteRegWritten(MB_BYTE_T SlaveAddr, MB_BYTE_T RegType, MB_WORD_T RegAddr);
static MB_BOOL_T setRemoteRegRead(MB_BYTE_T SlaveAddr, MB_BYTE_T RegType, MB_WORD_T RegAddr);
static MB_BOOL_T getRemoteRegRead(MB_BYTE_T SlaveAddr, MB_BYTE_T RegType, MB_WORD_T RegAddr);
static MB_BOOL_T clearRemoteRegRead(MB_BYTE_T SlaveAddr, MB_BYTE_T RegType, MB_WORD_T RegAddr);
static void read_regs_reply(MB_BYTE_T SlaveAddr, MB_BYTE_T RegType, MB_BYTE_T *frame, MB_WORD_T startreg, MB_WORD_T numregs, MB_BYTE_T BYTEcount);
#endif

static MB_WORD_T MB_CRC(MB_BYTE_T *data, MB_BYTE_T length)
{
	MB_WORD_T mid = 0;
	MB_WORD_T CRC = 0xFFFF;
	MB_WORD_T times = 0, index = 0;

	while (length) {
		CRC = data[index]^CRC;
		for (times = 0; times < 8; times++) {
			mid = CRC;
			CRC = CRC>>1;
			if (mid&0x0001)
				CRC = CRC^0xA001;
		}
		index++;
		length--;
	}

	return CRC;
}

static int check_integrity(MB_BYTE_T *msg, int msg_length)
{
	MB_WORD_T crc_calculated;
	MB_WORD_T crc_received;

	if (msg_length < 2)
	return -1;

	crc_calculated = MB_CRC(msg, msg_length - 2);
	crc_received = (msg[msg_length - 2] << 8) | msg[msg_length - 1];

	/* Check CRC of msg */
	if (crc_calculated == crc_received) {
		return msg_length;
	} else {
		// printf("crc_calculated: %04X, crc_received: %04X\n", crc_calculated, crc_received);
		return -1;
	}
}


#ifdef MB_MODE_SLAVE
static TRegister *searchRegister(MB_WORD_T address)
{
	TRegister *reg = localRegsHead;
	/* if there is no register configured, bail */
	if(reg == 0) return(0);
	/*
	** scan through the linked list until the end of the list or the
	** register is found. return the pointer.
	*/
	do {
		if (reg->address == address)
			return(reg);
		reg = reg->next;
	} while(reg);
	return(0);
}

static void addReg(MB_WORD_T address, MB_WORD_T value)
{
	TRegister *newreg;

	newreg = (TRegister *) malloc(sizeof(TRegister));
	newreg->address = address;
	newreg->value = value;
	newreg->next = 0;

	if(localRegsHead == 0) {
		localRegsHead = newreg;
		localRegsLast = localRegsHead;
	} else {
		/* Assign the last register's next pointer to newreg. */
		localRegsLast->next = newreg;
		/* then make temp the last register in the list. */
		localRegsLast = newreg;
	}
}

static MB_BOOL_T setReg(MB_WORD_T address, MB_WORD_T value)
{
	TRegister *reg;
	/* search for the register address */
	reg = searchRegister(address);
	/* if found then assign the register value to the new value. */
	if (reg) {
		reg->value = value;
		return TRUE;
	} else {
		return FALSE;
	}
}

static MB_WORD_T getReg(MB_WORD_T address)
{
	TRegister *reg;
	reg = searchRegister(address);
	if (reg)
		return(reg->value);
	else
		return(0);
}

static void exceptionResponse(MB_BYTE_T fcode, MB_BYTE_T excode)
{
	MB_WORD_T CRC;

	/* Clean frame buffer */
	free(s_ReplyFrame);
	s_RLen = 5;
	s_ReplyFrame = (MB_BYTE_T *) malloc(s_RLen);
	s_ReplyFrame[0] = s_Addr;
	s_ReplyFrame[1] = fcode + 0x80;
	s_ReplyFrame[2] = excode;
	CRC = MB_CRC(s_ReplyFrame, s_RLen-2);
	s_ReplyFrame[s_RLen-2] = (CRC&0xFF00)>>8;
	s_ReplyFrame[s_RLen-1] = CRC&0x00FF;

	s_ReplyType = MB_REPLY_NORMAL;
}

static void readRegisters(MB_WORD_T startreg, MB_WORD_T numregs)
{
	MB_WORD_T CRC;

	/* Check value (numregs) */
	if (numregs < 0x0001 || numregs > 0x007D) {
		exceptionResponse(MB_FC_READ_REGS, MB_EX_ILLEGAL_VALUE);
		return;
	}

	/* Check Address */
	//*** See comments on readCoils method.
	if (!searchRegister(startreg + 40001)) {
		exceptionResponse(MB_FC_READ_REGS, MB_EX_ILLEGAL_ADDRESS);
		return;
	}

	/* Clean frame buffer */
	free(s_ReplyFrame);
	s_RLen = 0;

	/*
	** calculate the query reply message length for each register
	** queried add 5 BYTEs
	*/
	s_RLen = 9 + numregs * 2;

	s_ReplyFrame = (MB_BYTE_T *) malloc(s_RLen);
	if (!s_ReplyFrame) {
		exceptionResponse(MB_FC_READ_REGS, MB_EX_SLAVE_FAILURE);
		return;
	}

	s_ReplyFrame[0] = s_Addr;
	s_ReplyFrame[1] = MB_FC_READ_REGS;
	s_ReplyFrame[2] = startreg >> 8;
	s_ReplyFrame[3] = startreg & 0x00FF;
	s_ReplyFrame[4] = numregs >> 8;
	s_ReplyFrame[5] = numregs & 0x00FF;
	s_ReplyFrame[6] = s_RLen - 9;   //byte count

	MB_WORD_T val;
	MB_WORD_T i = 0;
	while(numregs--) {
		/* retrieve the value from the register bank for the current register */
		val = MB_getHreg(startreg + i);
		/* write the high byte of the register value */
		s_ReplyFrame[7 + i * 2]  = val >> 8;
		/* write the low byte of the register value */
		s_ReplyFrame[8 + i * 2] = val & 0xFF;
		i++;
	}

	CRC = MB_CRC(s_ReplyFrame, s_RLen-2);
	s_ReplyFrame[s_RLen-2] = (CRC&0xFF00)>>8;
	s_ReplyFrame[s_RLen-1] = CRC&0x00FF;

	s_ReplyType = MB_REPLY_NORMAL;
}

static void writeSingleRegister(MB_WORD_T reg, MB_WORD_T value)
{
	/*
	** No necessary verify illegal value (EX_ILLEGAL_VALUE) - because using
	** Word (0x0000 - 0x0FFFF) Check Address and execute (reg exists?)
	*/
	if (!MB_setHreg(reg, value)) {
		exceptionResponse(MB_FC_WRITE_REG, MB_EX_ILLEGAL_ADDRESS);
		return;
	}

	/* Check for failure */
	if (MB_getHreg(reg) != value) {
		exceptionResponse(MB_FC_WRITE_REG, MB_EX_SLAVE_FAILURE);
		return;
	}

	s_ReplyType = MB_REPLY_ECHO;
}

static void writeMultipleRegisters(MB_BYTE_T* frame,MB_WORD_T startreg, MB_WORD_T numoutputs, MB_BYTE_T BYTEcount)
{
	int i;
	MB_WORD_T val;
	MB_WORD_T k = 0;
	MB_WORD_T CRC;

	/* Check value */
	if (numoutputs < 0x0001 || numoutputs > 0x007B || BYTEcount != 2 * numoutputs) {
		exceptionResponse(MB_FC_WRITE_REGS, MB_EX_ILLEGAL_VALUE);
		return;
	}

	/* Check Address (startreg...startreg + numregs) */
	for (i = 0; i < numoutputs; i++) {
		if (!searchRegister(startreg + 40001 + i)) {
			exceptionResponse(MB_FC_WRITE_REGS, MB_EX_ILLEGAL_ADDRESS);
			return;
		}
	}

	/* Clean frame buffer */
	free(s_ReplyFrame);
	s_RLen = 8;
	s_ReplyFrame = (MB_BYTE_T *) malloc(s_RLen);
	if (!s_ReplyFrame) {
		exceptionResponse(MB_FC_WRITE_REGS, MB_EX_SLAVE_FAILURE);
		return;
	}

	s_ReplyFrame[0] = s_Addr;
	s_ReplyFrame[1] = MB_FC_WRITE_REGS;
	s_ReplyFrame[2] = startreg >> 8;
	s_ReplyFrame[3] = startreg & 0x00FF;
	s_ReplyFrame[4] = numoutputs >> 8;
	s_ReplyFrame[5] = numoutputs & 0x00FF;
	CRC = MB_CRC(s_ReplyFrame, s_RLen-2);
	s_ReplyFrame[s_RLen-2] = (CRC&0xFF00)>>8;
	s_ReplyFrame[s_RLen-1] = CRC&0x00FF;

	while(numoutputs--) {
		val = (MB_WORD_T)frame[7+k*2] << 8 | (MB_WORD_T)frame[8+k*2];
		MB_setHreg(startreg + k, val);
		k++;
	}

	s_ReplyType = MB_REPLY_NORMAL;
}

static void readInputRegisters(MB_WORD_T startreg, MB_WORD_T numregs)
{
	MB_WORD_T CRC;

	/* Check value (numregs) */
	if (numregs < 0x0001 || numregs > 0x007D) {
		exceptionResponse(MB_FC_READ_INPUT_REGS, MB_EX_ILLEGAL_VALUE);
		return;
	}

	/* Check Address */
	//*** See comments on readCoils method.
	if (!searchRegister(startreg + 30001)) {
		exceptionResponse(MB_FC_READ_INPUT_REGS, MB_EX_ILLEGAL_ADDRESS);
		return;
	}

	/* Clean frame buffer */
	free(s_ReplyFrame);
	s_RLen = 0;

	/*
	** calculate the query reply message length
	** for each register queried add 2 BYTEs
	*/
	s_RLen = 9 + numregs * 2;

	s_ReplyFrame = (MB_BYTE_T *) malloc(s_RLen);
	if (!s_ReplyFrame) {
		exceptionResponse(MB_FC_READ_INPUT_REGS, MB_EX_SLAVE_FAILURE);
		return;
	}

	s_ReplyFrame[0] = s_Addr;
	s_ReplyFrame[1] = MB_FC_READ_INPUT_REGS;
	s_ReplyFrame[2] = startreg >> 8;
	s_ReplyFrame[3] = startreg & 0x00FF;
	s_ReplyFrame[4] = numregs >> 8;
	s_ReplyFrame[5] = numregs & 0x00FF;
	s_ReplyFrame[6] = s_RLen - 9;   //byte count

	MB_WORD_T val;
	MB_WORD_T i = 0;
	while(numregs--) {
		/* retrieve the value from the register bank for the current register */
		val = MB_getIreg(startreg + i);
		/* write the high byte of the register value */
		s_ReplyFrame[7 + i * 2]  = val >> 8;
		/* write the low byte of the register value */
		s_ReplyFrame[8 + i * 2] = val & 0xFF;
		i++;
	}

	CRC = MB_CRC(s_ReplyFrame, s_RLen-2);
	s_ReplyFrame[s_RLen-2] = (CRC&0xFF00)>>8;
	s_ReplyFrame[s_RLen-1] = CRC&0x00FF;

	s_ReplyType = MB_REPLY_NORMAL;
}
#endif

#ifdef MB_MODE_MASTER
static RemoteRegister *searchRemoteRegister(MB_BYTE_T SlaveAddr, MB_BYTE_T RegType, MB_WORD_T RegAddr)
{
	RemoteRegister *reg = remoteRegsHead;
	/* if there is no register configured, bail */
	if(reg == 0) return(0);
	/*
	** scan through the linked list until the end of the list or the
	** register is found. return the pointer.
	*/
	do {
		if ((reg->SlaveAddr == SlaveAddr) && (reg->RegType == RegType) && (reg->RegAddr == RegAddr))
			return(reg);
		reg = reg->next;
	} while(reg);
	return(0);
}

static MB_BOOL_T setRemoteReg(MB_BYTE_T SlaveAddr, MB_BYTE_T RegType, MB_WORD_T RegAddr, MB_WORD_T value)
{
	RemoteRegister *reg;
	/* search for the register address */
	reg = searchRemoteRegister(SlaveAddr, RegType, RegAddr);
	/* if found then assign the register value to the new value. */
	if (reg) {
		reg->value = value;
		return TRUE;
	} else {
		return FALSE;
	}
}

static MB_WORD_T getRemoteReg(MB_BYTE_T SlaveAddr, MB_BYTE_T RegType, MB_WORD_T RegAddr)
{
	RemoteRegister *reg;
	reg = searchRemoteRegister(SlaveAddr, RegType, RegAddr);
	if (reg)
		return(reg->value);
	else
		return(0);
}

static MB_BOOL_T setRemoteRegWritten(MB_BYTE_T SlaveAddr, MB_BYTE_T RegType, MB_WORD_T RegAddr)
{
	RemoteRegister *reg;
	/* search for the register address */
	reg = searchRemoteRegister(SlaveAddr, RegType, RegAddr);
	/* if found then assign the register value to the new value. */
	if (reg) {
		reg->Written = TRUE;
		return TRUE;
	} else {
		return FALSE;
	}
}

static MB_BOOL_T getRemoteRegWritten(MB_BYTE_T SlaveAddr, MB_BYTE_T RegType, MB_WORD_T RegAddr)
{
	RemoteRegister *reg;
	/* search for the register address */
	reg = searchRemoteRegister(SlaveAddr, RegType, RegAddr);
	/* if found then assign the register value to the new value. */
	if (reg) {
		return reg->Written;
	} else {
		return FALSE;
	}
}

static MB_BOOL_T clearRemoteRegWritten(MB_BYTE_T SlaveAddr, MB_BYTE_T RegType, MB_WORD_T RegAddr)
{
	RemoteRegister *reg;
	/* search for the register address */
	reg = searchRemoteRegister(SlaveAddr, RegType, RegAddr);
	/* if found then assign the register value to the new value. */
	if (reg) {
		reg->Written = FALSE;
		return TRUE;
	} else {
		return FALSE;
	}
}

static MB_BOOL_T setRemoteRegRead(MB_BYTE_T SlaveAddr, MB_BYTE_T RegType, MB_WORD_T RegAddr)
{
	RemoteRegister *reg;
	/* search for the register address */
	reg = searchRemoteRegister(SlaveAddr, RegType, RegAddr);
	/* if found then assign the register value to the new value. */
	if (reg) {
		reg->Read = TRUE;
		return TRUE;
	} else {
		return FALSE;
	}
}

static MB_BOOL_T getRemoteRegRead(MB_BYTE_T SlaveAddr, MB_BYTE_T RegType, MB_WORD_T RegAddr)
{
	RemoteRegister *reg;
	/* search for the register address */
	reg = searchRemoteRegister(SlaveAddr, RegType, RegAddr);
	/* if found then assign the register value to the new value. */
	if (reg) {
		return reg->Read;
	} else {
		return FALSE;
	}
}

static MB_BOOL_T clearRemoteRegRead(MB_BYTE_T SlaveAddr, MB_BYTE_T RegType, MB_WORD_T RegAddr)
{
	RemoteRegister *reg;
	/* search for the register address */
	reg = searchRemoteRegister(SlaveAddr, RegType, RegAddr);
	/* if found then assign the register value to the new value. */
	if (reg) {
		reg->Read = FALSE;
		return TRUE;
	} else {
		return FALSE;
	}
}

static void read_regs_reply(MB_BYTE_T SlaveAddr, MB_BYTE_T RegType, MB_BYTE_T *frame, MB_WORD_T startreg, MB_WORD_T numregs, MB_BYTE_T BYTEcount)
{
	MB_WORD_T val;
	MB_WORD_T k = 0;

	/* Check value */
	if (numregs < 0x0001 || numregs > 0x007B || BYTEcount != 2 * numregs) {
		return;
	}

	while(numregs--) {
		val = (MB_WORD_T)frame[7+k*2] << 8 | (MB_WORD_T)frame[8+k*2];
		setRemoteReg(SlaveAddr, RegType, startreg + k, val);
		setRemoteRegRead(SlaveAddr, RegType, startreg + k);
		k++;
	}
}
#endif

/* Implementation */
#ifdef MB_MODE_SLAVE
void MB_initSlave(MB_BYTE_T byAddr, MB_Sender sender)
{
	s_Addr = byAddr;
	s_sender = sender;
}

void MB_addHreg(MB_WORD_T offset, MB_WORD_T value)
{
	addReg(offset + 40001, value);
}

MB_BOOL_T MB_setHreg(MB_WORD_T offset, MB_WORD_T value)
{
	return setReg(offset + 40001, value);
}

MB_WORD_T MB_getHreg(MB_WORD_T offset)
{
	return getReg(offset + 40001);
}

void MB_addIreg(MB_WORD_T offset, MB_WORD_T value)
{
	addReg(offset + 30001, value);
}

MB_BOOL_T MB_setIreg(MB_WORD_T offset, MB_WORD_T value)
{
	return setReg(offset + 30001, value);
}

MB_WORD_T MB_getIreg(MB_WORD_T offset)
{
	return getReg(offset + 30001);
}

void MB_slaveHandler(MB_BYTE_T *frame, MB_BYTE_T length)
{
	MB_BYTE_T byAddr = frame[0];
	MB_BYTE_T fcode  = frame[1];
	MB_WORD_T field1 = (MB_WORD_T)frame[2] << 8 | (MB_WORD_T)frame[3];
	MB_WORD_T field2 = (MB_WORD_T)frame[4] << 8 | (MB_WORD_T)frame[5];

	if (check_integrity(frame, length) > 0) {
		if (s_Addr == byAddr) {
			switch (fcode) {
			case MB_FC_WRITE_REG:
				/* field1 = reg, field2 = value */
				writeSingleRegister(field1, field2);
				break;
			case MB_FC_READ_REGS:
				/* field1 = startreg, field2 = numregs */
				readRegisters(field1, field2);
				break;
			case MB_FC_WRITE_REGS:
				/* field1 = startreg, field2 = numregs */
				writeMultipleRegisters(frame,field1, field2, frame[6]);
				break;
			case MB_FC_READ_INPUT_REGS:
				/* field1 = startreg, field2 = numregs */
				readInputRegisters(field1, field2);
				break;
			default:
				exceptionResponse(fcode, MB_EX_ILLEGAL_FUNCTION);
			}
		} else {
			return;	/* address error */
		}
	} else {
		return;	/* crc error */
	}

	switch(s_ReplyType) {
	case MB_REPLY_OFF:
		break;
	case MB_REPLY_ECHO:
		s_sender(frame, length);
		break;
	case MB_REPLY_NORMAL:
		s_sender(s_ReplyFrame, s_RLen);
		break;
	default:
		break;
	}
}
#endif

#ifdef MB_MODE_MASTER
void MB_initMaster(MB_Sender sender)
{
	m_QueryFrame = NULL;
	m_QLen = 0;
	m_sender = sender;
	remoteRegsHead = 0;
	remoteRegsLast = 0;
}

void MB_addRreg(MB_BYTE_T byAddr, MB_BYTE_T type, MB_WORD_T offset)
{
	RemoteRegister *newreg;

	newreg = (RemoteRegister *) malloc(sizeof(RemoteRegister));
	newreg->SlaveAddr = byAddr;
	newreg->RegType = type;
	newreg->RegAddr = offset;
	newreg->value = 0;
	newreg->Written = FALSE;
	newreg->Read = FALSE;
	newreg->next = 0;

	if(remoteRegsHead == 0) {
		remoteRegsHead = newreg;
		remoteRegsLast = remoteRegsHead;
	} else {
		/* Assign the last register's next pointer to newreg. */
		remoteRegsLast->next = newreg;
		/* then make temp the last register in the list. */
		remoteRegsLast = newreg;
	}
}

MB_BOOL_T MB_readHregs(MB_BYTE_T step, MB_BYTE_T byAddr, MB_WORD_T offset, MB_WORD_T count, MB_BYTE_T *data)
{
	MB_BYTE_T i, NotReady;
	MB_WORD_T CRC;
	MB_WORD_T val;

	switch (step) {
	case MB_RStep_StartQuery:
		/* Clean frame buffer */
		free(m_QueryFrame);
		m_QLen = 8;
		m_QueryFrame = (MB_BYTE_T *) malloc(m_QLen);

		m_QueryFrame[0] = byAddr;
		m_QueryFrame[1] = MB_FC_READ_REGS;
		m_QueryFrame[2] = offset >> 8;
		m_QueryFrame[3] = offset & 0x00FF;
		m_QueryFrame[4] = count >> 8;
		m_QueryFrame[5] = count & 0x00FF;

		CRC = MB_CRC(m_QueryFrame, m_QLen-2);
		m_QueryFrame[m_QLen-2] = (CRC&0xFF00)>>8;
		m_QueryFrame[m_QLen-1] = CRC&0x00FF;

		for (i = 0; i < count; i++) {
			clearRemoteRegRead(byAddr, MB_REG_HOLDING, offset+i);
		}

		m_sender(m_QueryFrame, m_QLen);
		break;
	case MB_RStep_WaitingReply:
		NotReady = 0;
		for (i = 0; i < count; i++) {
			if (!getRemoteRegRead(byAddr, MB_REG_HOLDING, offset+i)) {
				NotReady++;
			} else {
				val = getRemoteReg(byAddr, MB_REG_HOLDING, offset+i);
				data[i*2]   = val >> 8;
				data[i*2+1] = val & 0xFF;
			}
		}
		if (!NotReady) {
			return TRUE;
		}
		break;
	default:
		break;
	}

	return FALSE;
}

MB_BOOL_T MB_writeHreg(MB_BYTE_T step, MB_BYTE_T byAddr, MB_WORD_T offset, MB_WORD_T data)
{
	MB_WORD_T CRC;

	switch (step) {
	case MB_RStep_StartQuery:
		/* Clean frame buffer */
		free(m_QueryFrame);
		m_QLen = 8;
		m_QueryFrame = (MB_BYTE_T *) malloc(m_QLen);

		m_QueryFrame[0] = byAddr;
		m_QueryFrame[1] = MB_FC_WRITE_REG;
		m_QueryFrame[2] = offset >> 8;
		m_QueryFrame[3] = offset & 0x00FF;
		m_QueryFrame[4] = data >> 8;
		m_QueryFrame[5] = data & 0x00FF;

		CRC = MB_CRC(m_QueryFrame, m_QLen-2);
		m_QueryFrame[m_QLen-2] = (CRC&0xFF00)>>8;
		m_QueryFrame[m_QLen-1] = CRC&0x00FF;

		clearRemoteRegWritten(byAddr, MB_REG_HOLDING, offset);

		m_sender(m_QueryFrame, m_QLen);
		break;
	case MB_RStep_WaitingReply:
		if (getRemoteRegWritten(byAddr, MB_REG_HOLDING, offset)) {
			return TRUE;
		}
		break;
	default:
		break;
	}

	return FALSE;
}

MB_BOOL_T MB_writeHregs(MB_BYTE_T step, MB_BYTE_T byAddr, MB_WORD_T offset, MB_WORD_T count, MB_BYTE_T *data)
{
	MB_BYTE_T i, NotReady;
	MB_WORD_T CRC;

	switch (step) {
	case MB_RStep_StartQuery:
		/* Check value (count) */
		if (count < 0x0001 || count > 0x007D) {
			return FALSE;
		}

		/* Clean frame buffer */
		free(m_QueryFrame);
		m_QLen = 9 + count * 2;
		m_QueryFrame = (MB_BYTE_T *) malloc(m_QLen);

		m_QueryFrame[0] = byAddr;
		m_QueryFrame[1] = MB_FC_WRITE_REGS;
		m_QueryFrame[2] = offset >> 8;
		m_QueryFrame[3] = offset & 0x00FF;
		m_QueryFrame[4] = count >> 8;
		m_QueryFrame[5] = count & 0x00FF;
		m_QueryFrame[6] = count*2;

		for (i = 0; i < count; i++) {
			m_QueryFrame[7 + i] = data[i*2];
			m_QueryFrame[8 + i] = data[i*2+1];
			clearRemoteRegWritten(byAddr, MB_REG_HOLDING, offset+i);
		}

		CRC = MB_CRC(m_QueryFrame, m_QLen-2);
		m_QueryFrame[m_QLen-2] = (CRC&0xFF00)>>8;
		m_QueryFrame[m_QLen-1] = CRC&0x00FF;

		m_sender(m_QueryFrame, m_QLen);
		break;
	case MB_RStep_WaitingReply:
		NotReady = 0;
		for (i = 0; i < count; i++) {
			if (!getRemoteRegWritten(byAddr, MB_REG_HOLDING, offset+i)) {
				NotReady++;
			}
		}
		if (!NotReady) {
			return TRUE;
		}
		break;
	default:
		break;
	}

	return FALSE;
}

MB_BOOL_T MB_readIregs(MB_BYTE_T step, MB_BYTE_T byAddr, MB_WORD_T offset, MB_WORD_T count, MB_BYTE_T *data)
{
	MB_BYTE_T i, NotReady;
	MB_WORD_T CRC;
	MB_WORD_T val;

	switch (step) {
	case MB_RStep_StartQuery:
		/* Clean frame buffer */
		free(m_QueryFrame);
		m_QLen = 8;
		m_QueryFrame = (MB_BYTE_T *) malloc(m_QLen);

		m_QueryFrame[0] = byAddr;
		m_QueryFrame[1] = MB_FC_READ_INPUT_REGS;
		m_QueryFrame[2] = offset >> 8;
		m_QueryFrame[3] = offset & 0x00FF;
		m_QueryFrame[4] = count >> 8;
		m_QueryFrame[5] = count & 0x00FF;

		CRC = MB_CRC(m_QueryFrame, m_QLen-2);
		m_QueryFrame[m_QLen-2] = (CRC&0xFF00)>>8;
		m_QueryFrame[m_QLen-1] = CRC&0x00FF;

		for (i = 0; i < count; i++) {
			clearRemoteRegRead(byAddr, MB_REG_INPUT, offset+i);
		}

		m_sender(m_QueryFrame, m_QLen);
		break;
	case MB_RStep_WaitingReply:
		NotReady = 0;
		for (i = 0; i < count; i++) {
			if (!getRemoteRegRead(byAddr, MB_REG_INPUT, offset+i)) {
				NotReady++;
			} else {
				val = getRemoteReg(byAddr, MB_REG_INPUT, offset+i);
				data[i*2]   = val >> 8;
				data[i*2+1] = val & 0xFF;
			}
		}
		if (!NotReady) {
			return TRUE;
		}
		break;
	default:
		break;
	}

	return FALSE;
}

void MB_masterHandler(MB_BYTE_T *frame, MB_BYTE_T length)
{
	MB_BYTE_T byAddr = frame[0];
	MB_BYTE_T fcode  = frame[1];
	MB_WORD_T startreg = (MB_WORD_T)frame[2] << 8 | (MB_WORD_T)frame[3];
	MB_WORD_T numregs = (MB_WORD_T)frame[4] << 8 | (MB_WORD_T)frame[5];

	if (check_integrity(frame, length) > 0) {
		switch (fcode) {
		case MB_FC_WRITE_REG:
			setRemoteRegWritten(byAddr, MB_REG_HOLDING, startreg);
			break;
		case MB_FC_READ_REGS:
			read_regs_reply(byAddr, MB_REG_HOLDING, frame, startreg, numregs, frame[6]);
			break;
		case MB_FC_WRITE_REGS:
			for (int i = 0; i < numregs; i++) {
				setRemoteRegWritten(byAddr, MB_REG_HOLDING, startreg+i);
			}
			break;
		case MB_FC_READ_INPUT_REGS:
			read_regs_reply(byAddr, MB_REG_INPUT, frame, startreg, numregs, frame[6]);
			break;
		default:
			break;
		}
	} else {
		return;	/* crc error */
	}
}
#endif