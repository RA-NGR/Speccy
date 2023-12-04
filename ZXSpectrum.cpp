#include "ZXSpectrum.h"
#include "ZXMacros.h"

ZXSpectrum::~ZXSpectrum()
{
	free(m_pZXMemory);
	free(m_pContendTable);
}

void ZXSpectrum::drawLine(int posY)
{
	uint16_t posX, zxPixelMapAddr, zxColorAttribAddr, tftMemAddr;
	uint8_t i, zxColorAttrib, zxPixelMap, zxBrightFlag, zxInkColor, zxPaperColor, bitPos;
	int buffSwitch = (posY / DMA_BUFF_SIZE) & 1;
	int buffOffset = (posY % DMA_BUFF_SIZE) * 320;

	for (posX = 0; posX < 4; posX++) // Left border
	{
		if (m_pbRIndex != m_pbWIndex && posY == m_borderColors[m_pbRIndex].y && posX == m_borderColors[m_pbRIndex].x)
		{
			m_borderColor = m_borderColors[m_pbRIndex].color; m_pbRIndex = (++m_pbRIndex) & (BORDER_BUFFER_SIZE - 1);
		}
		for (i = 0; i < 8; i++) m_pScreenBuffer[buffSwitch][((posX * 8 + i) ^ 1) + buffOffset] = m_borderColor;
	}
	for (posX = 0; posX < 32; posX++) // Main area
	{
		if (m_pbRIndex != m_pbWIndex && posY == m_borderColors[m_pbRIndex].y && posX + 4 == m_borderColors[m_pbRIndex].x)
		{
			m_borderColor = m_borderColors[m_pbRIndex].color; m_pbRIndex = (++m_pbRIndex) & (BORDER_BUFFER_SIZE - 1);
		}
		if (posY < 24 || posY > 215)
			for (i = 0; i < 8; i++) m_pScreenBuffer[buffSwitch][((posX * 8 + i + 32) ^ 1) + buffOffset] = m_borderColor;
		else
		{
			zxColorAttribAddr = 0x5800 + (((posY - 24) / 8) * 32) + posX; zxColorAttrib = m_pZXMemory[zxColorAttribAddr];
			zxBrightFlag = (zxColorAttrib & 0x40) >> 3;
			zxPixelMapAddr = 0x4000 + (((posY - 24) % 64) / 8) * 32 + ((posY - 24) % 8) * 256 + ((posY - 24) / 64) * 2048 + posX;
			zxPixelMap = m_pZXMemory[zxPixelMapAddr] ^ ((zxColorAttrib & 0x80) ? ((m_frameCounter & 0x10) ? 255 : 0) : 0);
			zxInkColor = zxColorAttrib & 0x07; zxPaperColor = (zxColorAttrib & 0x38) >> 3;
			for (i = 0; i < 8; i++)
			{
				tftMemAddr = posX * 8 + i + 32;	bitPos = (0x80 >> i);
				if ((zxPixelMap & bitPos) != 0)
					m_pScreenBuffer[buffSwitch][tftMemAddr + buffOffset] = m_colorLookup[zxInkColor + zxBrightFlag];
				else
					m_pScreenBuffer[buffSwitch][tftMemAddr + buffOffset] = m_colorLookup[zxPaperColor + zxBrightFlag];
			}
		}
	}
	for (posX = 0; posX < 4; posX++) // Right border
	{
		if (m_pbRIndex != m_pbWIndex && posY == m_borderColors[m_pbRIndex].y && posX + 36 == m_borderColors[m_pbRIndex].x)
		{
			m_borderColor = m_borderColors[m_pbRIndex].color; m_pbRIndex = (++m_pbRIndex) & (BORDER_BUFFER_SIZE - 1);
		}
		for (i = 0; i < 8; i++) m_pScreenBuffer[buffSwitch][((posX * 8 + i + 288) ^ 1) + buffOffset] = m_borderColor;
	}
	for (posX = 0; posX < 16; posX++) // Retrace
	{
		if (m_pbRIndex != m_pbWIndex && posY == m_borderColors[m_pbRIndex].y && posX + 40 == m_borderColors[m_pbRIndex].x)
		{
			m_borderColor = m_borderColors[m_pbRIndex].color; m_pbRIndex = (++m_pbRIndex) & (BORDER_BUFFER_SIZE - 1);
		}
	}
	if (posY % DMA_BUFF_SIZE == DMA_BUFF_SIZE - 1) m_pDisplayInstance->drawBuffer(buffSwitch);
}

int8_t ZXSpectrum::intZ80()
{
	WORD inttemp;

	if (IFF1 && m_Z80Processor.tCount < IRQ_LENGTH)
	{
		if (m_Z80Processor.tCount == m_Z80Processor.intEnabledAt) return 0;
		if (m_Z80Processor.halted)
		{
			PC++; m_Z80Processor.halted = 0;
		}
		IFF1 = IFF2 = 0;
		R++;
		m_Z80Processor.tCount += 7; /* Longer than usual M1 cycle */
		writeMem(--SP, PCH); writeMem(--SP, PCL);
		switch (IM)
		{
		case 0:
			PC = 0x0038;
			break;
		case 1:
			PC = 0x0038;
			break;
		case 2:
			inttemp = (0x100 * I) + 0xFF;
			PCL = readMem(inttemp++); PCH = readMem(inttemp);
			break;
		default:
			break;
		}
		m_Z80Processor.memptr.w = PC;
		Q = 0;
		return 1;
	}
	return 0;
}

void ZXSpectrum::processTape()
{
	for (int i = 0; i < 8; i++) m_inPortFE[i] ^= 0x40;
	m_ZXTape.statesCount--;
	m_ZXTape.stateCycles = m_tapeStates[m_ZXTape.tapeState].stateCycles + m_ZXTape.stateCycles; // restore state cycles
	if (m_ZXTape.statesCount > 0) return;
	if (m_ZXTape.tapeState > 1 && m_ZXTape.tapeState < 4)
		m_ZXTape.tapeState++; // Next state (PILOT->SYNCRO HIGH->SYNCRO LOW)
	else
	{
		m_ZXTape.tapeState = (m_TAPSection.data[m_TAPSection.bit >> 3] & (1 << (7 - (m_TAPSection.bit & 7)))) ? 1 : 0;
		m_TAPSection.bit++;
		if (m_TAPSection.bit > (m_TAPSection.size << 3)) stopTape();
	}
	m_ZXTape.stateCycles = m_tapeStates[m_ZXTape.tapeState].stateCycles; // renew state cycles
	m_ZXTape.statesCount = m_tapeStates[m_ZXTape.tapeState].statesCount; // set state count
}

void ZXSpectrum::writeMem(WORD address, BYTE data)
{
	contendedAccess(address, 3);
	if (address >= 0x4000) m_pZXMemory[address] = data;
}

ZXSpectrum::BYTE ZXSpectrum::readMem(WORD address)
{
	contendedAccess(address, 3);
	return m_pZXMemory[address];
}

ZXSpectrum::BYTE ZXSpectrum::unattachedPort()
{
	int posY, tCount, posX;

	if (m_Z80Processor.tCount < 14320) return 0xFF;
	posY = (m_Z80Processor.tCount - 14320) / 224;
	if (posY >= 192) return 0xFF;
	tCount = m_Z80Processor.tCount - (posY * 224 + 14320) + 8;
	if (tCount < 24 || tCount >= 152) return 0xFF;
	posX = ((tCount - 24) / 8) * 2;
	switch (tCount % 8)
	{
	case 5:
		posX++;
	case 3:
		return m_pZXMemory[0x5800 + ((posY / 8) * 32) + posX];
	case 4:
		posX++;
	case 2:
		return m_pZXMemory[0x4000 + ((posY % 64) / 8) * 32 + (posY % 8) * 256 + (posY / 64) * 2048 + posX];
	case 0:
	case 1:
	case 6:
	case 7:
		return 0xFF;
	}
	return 0;
}

ZXSpectrum::BYTE ZXSpectrum::readPort(WORD port)
{
	BYTE retVal = 0xFF;
	uint32_t periphData = 0x0000001F, timeOut = 0;

	contendedAccess(port, 1);
	if (!(port & 0x0001))
	{
		contendedAccess(CONTENDED, 2);
#ifdef KBD_EMULATED
		for (int i = 0; i < 8; i++) if (!((port >> (i + 8)) & 0x01)) retVal = retVal = m_inPortFE[i];
#else
		uint8_t decodedPort = ((uint8_t)(port >> 8) & 0x0F) << 4 | ((uint8_t)(port >> 8) & 0xF0) >> 4;
		m_rpTime = micros();
		writeReg(0x14, decodedPort);
		/*uint8_t keysData*/retVal = readKeys();
		m_rpTime = micros() - m_rpTime;
		//writeReg(0x14, 0xFF);
		//		if (retVal != 0xFF) DBG_PRINTF("%02X\n", retVal);
			//		for (int i = 0; i < 8; i++) if (!((port >> (i + 8)) & 0x01)) retVal = Wire1.re;
#endif // KBD_EMULATED
			retVal &= (m_inPortFE[7] | 0xBF); // Tape bit
	}
	else
	{
		if ((port & 0xC000) == 0x4000)
		{
			contendedAccess(CONTENDED, 1); contendedAccess(CONTENDED, 1); contendedAccess(CONTENDED, 0);
		}
		else
		{
			m_Z80Processor.tCount += 2;
			retVal = unattachedPort();
		}

	}
	if ((port & 0x00FF) <= 0x1F)
	{
		//#ifdef KBD_EMULATED
		//		retVal = m_inPortFE[8];
		//#else
		writeReg(0x15, 0x60);
		retVal = readKeys() ^ 0xFF;
		writeReg(0x15, 0xE0);
	}
//#endif // KBD_EMULATED
	m_Z80Processor.tCount++;
	return retVal;
}

void ZXSpectrum::writePort(WORD port, BYTE data)
{
	contendedAccess(port, 1);
	if ((port & 0x00FF) == 0x00FE)
	{
		if (m_outPortFE.borderColor != (data & 7))
		{
			if (m_Z80Processor.tCount >= STARTSCREEN && m_Z80Processor.tCount <= ENDSCREEN)
			{
				m_borderColors[m_pbWIndex].y = (m_Z80Processor.tCount - STARTSCREEN) / 224; m_borderColors[m_pbWIndex].x = ((m_Z80Processor.tCount - STARTSCREEN) % 224) / 4;
				m_borderColors[m_pbWIndex].color = m_colorLookup[data & 0x07]; m_pbWIndex = (++m_pbWIndex) & (BORDER_BUFFER_SIZE - 1);
			}
			else
				m_borderColor = m_colorLookup[data & 0x07];
		}
		if (m_outPortFE.soundOut != ((data >> 4) & 1)) rp2040.fifo.push_nb(m_Z80Processor.tCount & 0x00FFFFFF | WR_PORT);
		m_outPortFE.rawData = data;
	}
	if (!(port & 0x0001))
	{
		contendedAccess(CONTENDED, 2);
	}
	else
	{
		if ((port & 0xC000) == 0x4000)
		{
			contendedAccess(CONTENDED, 1); contendedAccess(CONTENDED, 1); contendedAccess(CONTENDED, 0);
		}
		else
			m_Z80Processor.tCount += 2;
	}
	m_Z80Processor.tCount++;
}

void ZXSpectrum::stepCB(BYTE opcode)
{
	BYTE bytetemp;
	switch (opcode)
	{
	case 0x00:		/* RLC B */
		RLC(B);
		break;
	case 0x01:		/* RLC C */
		RLC(C);
		break;
	case 0x02:		/* RLC D */
		RLC(D);
		break;
	case 0x03:		/* RLC E */
		RLC(E);
		break;
	case 0x04:		/* RLC H */
		RLC(H);
		break;
	case 0x05:		/* RLC L */
		RLC(L);
		break;
	case 0x06:		/* RLC (HL) */
		bytetemp = readMem(HL);
		contendedAccess(HL, 1);
		RLC(bytetemp);
		writeMem(HL, bytetemp);
		break;
	case 0x07:		/* RLC A */
		RLC(A);
		break;
	case 0x08:		/* RRC B */
		RRC(B);
		break;
	case 0x09:		/* RRC C */
		RRC(C);
		break;
	case 0x0a:		/* RRC D */
		RRC(D);
		break;
	case 0x0b:		/* RRC E */
		RRC(E);
		break;
	case 0x0c:		/* RRC H */
		RRC(H);
		break;
	case 0x0d:		/* RRC L */
		RRC(L);
		break;
	case 0x0e:		/* RRC (HL) */
		bytetemp = readMem(HL);
		contendedAccess(HL, 1);
		RRC(bytetemp);
		writeMem(HL, bytetemp);
		break;
	case 0x0f:		/* RRC A */
		RRC(A);
		break;
	case 0x10:		/* RL B */
		RL(B);
		break;
	case 0x11:		/* RL C */
		RL(C);
		break;
	case 0x12:		/* RL D */
		RL(D);
		break;
	case 0x13:		/* RL E */
		RL(E);
		break;
	case 0x14:		/* RL H */
		RL(H);
		break;
	case 0x15:		/* RL L */
		RL(L);
		break;
	case 0x16:		/* RL (HL) */
		bytetemp = readMem(HL);
		contendedAccess(HL, 1);
		RL(bytetemp);
		writeMem(HL, bytetemp);
		break;
	case 0x17:		/* RL A */
		RL(A);
		break;
	case 0x18:		/* RR B */
		RR(B);
		break;
	case 0x19:		/* RR C */
		RR(C);
		break;
	case 0x1a:		/* RR D */
		RR(D);
		break;
	case 0x1b:		/* RR E */
		RR(E);
		break;
	case 0x1c:		/* RR H */
		RR(H);
		break;
	case 0x1d:		/* RR L */
		RR(L);
		break;
	case 0x1e:		/* RR (HL) */
		bytetemp = readMem(HL);
		contendedAccess(HL, 1);
		RR(bytetemp);
		writeMem(HL, bytetemp);
		break;
	case 0x1f:		/* RR A */
		RR(A);
		break;
	case 0x20:		/* SLA B */
		SLA(B);
		break;
	case 0x21:		/* SLA C */
		SLA(C);
		break;
	case 0x22:		/* SLA D */
		SLA(D);
		break;
	case 0x23:		/* SLA E */
		SLA(E);
		break;
	case 0x24:		/* SLA H */
		SLA(H);
		break;
	case 0x25:		/* SLA L */
		SLA(L);
		break;
	case 0x26:		/* SLA (HL) */
		bytetemp = readMem(HL);
		contendedAccess(HL, 1);
		SLA(bytetemp);
		writeMem(HL, bytetemp);
		break;
	case 0x27:		/* SLA A */
		SLA(A);
		break;
	case 0x28:		/* SRA B */
		SRA(B);
		break;
	case 0x29:		/* SRA C */
		SRA(C);
		break;
	case 0x2a:		/* SRA D */
		SRA(D);
		break;
	case 0x2b:		/* SRA E */
		SRA(E);
		break;
	case 0x2c:		/* SRA H */
		SRA(H);
		break;
	case 0x2d:		/* SRA L */
		SRA(L);
		break;
	case 0x2e:		/* SRA (HL) */
		bytetemp = readMem(HL);
		contendedAccess(HL, 1);
		SRA(bytetemp);
		writeMem(HL, bytetemp);
		break;
	case 0x2f:		/* SRA A */
		SRA(A);
		break;
	case 0x30:		/* SLL B */
		SLL(B);
		break;
	case 0x31:		/* SLL C */
		SLL(C);
		break;
	case 0x32:		/* SLL D */
		SLL(D);
		break;
	case 0x33:		/* SLL E */
		SLL(E);
		break;
	case 0x34:		/* SLL H */
		SLL(H);
		break;
	case 0x35:		/* SLL L */
		SLL(L);
		break;
	case 0x36:		/* SLL (HL) */
		bytetemp = readMem(HL);
		contendedAccess(HL, 1);
		SLL(bytetemp);
		writeMem(HL, bytetemp);
		break;
	case 0x37:		/* SLL A */
		SLL(A);
		break;
	case 0x38:		/* SRL B */
		SRL(B);
		break;
	case 0x39:		/* SRL C */
		SRL(C);
		break;
	case 0x3a:		/* SRL D */
		SRL(D);
		break;
	case 0x3b:		/* SRL E */
		SRL(E);
		break;
	case 0x3c:		/* SRL H */
		SRL(H);
		break;
	case 0x3d:		/* SRL L */
		SRL(L);
		break;
	case 0x3e:		/* SRL (HL) */
		bytetemp = readMem(HL);
		contendedAccess(HL, 1);
		SRL(bytetemp);
		writeMem(HL, bytetemp);
		break;
	case 0x3f:		/* SRL A */
		SRL(A);
		break;
	case 0x40:		/* BIT 0,B */
		BIT_REG(0, B);
		break;
	case 0x41:		/* BIT 0,C */
		BIT_REG(0, C);
		break;
	case 0x42:		/* BIT 0,D */
		BIT_REG(0, D);
		break;
	case 0x43:		/* BIT 0,E */
		BIT_REG(0, E);
		break;
	case 0x44:		/* BIT 0,H */
		BIT_REG(0, H);
		break;
	case 0x45:		/* BIT 0,L */
		BIT_REG(0, L);
		break;
	case 0x46:		/* BIT 0,(HL) */
		bytetemp = readMem(HL);
		contendedAccess(HL, 1);
		BIT_MEMPTR(0, bytetemp);
		break;
	case 0x47:		/* BIT 0,A */
		BIT_REG(0, A);
		break;
	case 0x48:		/* BIT 1,B */
		BIT_REG(1, B);
		break;
	case 0x49:		/* BIT 1,C */
		BIT_REG(1, C);
		break;
	case 0x4a:		/* BIT 1,D */
		BIT_REG(1, D);
		break;
	case 0x4b:		/* BIT 1,E */
		BIT_REG(1, E);
		break;
	case 0x4c:		/* BIT 1,H */
		BIT_REG(1, H);
		break;
	case 0x4d:		/* BIT 1,L */
		BIT_REG(1, L);
		break;
	case 0x4e:		/* BIT 1,(HL) */
		bytetemp = readMem(HL);
		contendedAccess(HL, 1);
		BIT_MEMPTR(1, bytetemp);
		break;
	case 0x4f:		/* BIT 1,A */
		BIT_REG(1, A);
		break;
	case 0x50:		/* BIT 2,B */
		BIT_REG(2, B);
		break;
	case 0x51:		/* BIT 2,C */
		BIT_REG(2, C);
		break;
	case 0x52:		/* BIT 2,D */
		BIT_REG(2, D);
		break;
	case 0x53:		/* BIT 2,E */
		BIT_REG(2, E);
		break;
	case 0x54:		/* BIT 2,H */
		BIT_REG(2, H);
		break;
	case 0x55:		/* BIT 2,L */
		BIT_REG(2, L);
		break;
	case 0x56:		/* BIT 2,(HL) */
		bytetemp = readMem(HL);
		contendedAccess(HL, 1);
		BIT_MEMPTR(2, bytetemp);
		break;
	case 0x57:		/* BIT 2,A */
		BIT_REG(2, A);
		break;
	case 0x58:		/* BIT 3,B */
		BIT_REG(3, B);
		break;
	case 0x59:		/* BIT 3,C */
		BIT_REG(3, C);
		break;
	case 0x5a:		/* BIT 3,D */
		BIT_REG(3, D);
		break;
	case 0x5b:		/* BIT 3,E */
		BIT_REG(3, E);
		break;
	case 0x5c:		/* BIT 3,H */
		BIT_REG(3, H);
		break;
	case 0x5d:		/* BIT 3,L */
		BIT_REG(3, L);
		break;
	case 0x5e:		/* BIT 3,(HL) */
		bytetemp = readMem(HL);
		contendedAccess(HL, 1);
		BIT_MEMPTR(3, bytetemp);
		break;
	case 0x5f:		/* BIT 3,A */
		BIT_REG(3, A);
		break;
	case 0x60:		/* BIT 4,B */
		BIT_REG(4, B);
		break;
	case 0x61:		/* BIT 4,C */
		BIT_REG(4, C);
		break;
	case 0x62:		/* BIT 4,D */
		BIT_REG(4, D);
		break;
	case 0x63:		/* BIT 4,E */
		BIT_REG(4, E);
		break;
	case 0x64:		/* BIT 4,H */
		BIT_REG(4, H);
		break;
	case 0x65:		/* BIT 4,L */
		BIT_REG(4, L);
		break;
	case 0x66:		/* BIT 4,(HL) */
		bytetemp = readMem(HL);
		contendedAccess(HL, 1);
		BIT_MEMPTR(4, bytetemp);
		break;
	case 0x67:		/* BIT 4,A */
		BIT_REG(4, A);
		break;
	case 0x68:		/* BIT 5,B */
		BIT_REG(5, B);
		break;
	case 0x69:		/* BIT 5,C */
		BIT_REG(5, C);
		break;
	case 0x6a:		/* BIT 5,D */
		BIT_REG(5, D);
		break;
	case 0x6b:		/* BIT 5,E */
		BIT_REG(5, E);
		break;
	case 0x6c:		/* BIT 5,H */
		BIT_REG(5, H);
		break;
	case 0x6d:		/* BIT 5,L */
		BIT_REG(5, L);
		break;
	case 0x6e:		/* BIT 5,(HL) */
		bytetemp = readMem(HL);
		contendedAccess(HL, 1);
		BIT_MEMPTR(5, bytetemp);
		break;
	case 0x6f:		/* BIT 5,A */
		BIT_REG(5, A);
		break;
	case 0x70:		/* BIT 6,B */
		BIT_REG(6, B);
		break;
	case 0x71:		/* BIT 6,C */
		BIT_REG(6, C);
		break;
	case 0x72:		/* BIT 6,D */
		BIT_REG(6, D);
		break;
	case 0x73:		/* BIT 6,E */
		BIT_REG(6, E);
		break;
	case 0x74:		/* BIT 6,H */
		BIT_REG(6, H);
		break;
	case 0x75:		/* BIT 6,L */
		BIT_REG(6, L);
		break;
	case 0x76:		/* BIT 6,(HL) */
		bytetemp = readMem(HL);
		contendedAccess(HL, 1);
		BIT_MEMPTR(6, bytetemp);
		break;
	case 0x77:		/* BIT 6,A */
		BIT_REG(6, A);
		break;
	case 0x78:		/* BIT 7,B */
		BIT_REG(7, B);
		break;
	case 0x79:		/* BIT 7,C */
		BIT_REG(7, C);
		break;
	case 0x7a:		/* BIT 7,D */
		BIT_REG(7, D);
		break;
	case 0x7b:		/* BIT 7,E */
		BIT_REG(7, E);
		break;
	case 0x7c:		/* BIT 7,H */
		BIT_REG(7, H);
		break;
	case 0x7d:		/* BIT 7,L */
		BIT_REG(7, L);
		break;
	case 0x7e:		/* BIT 7,(HL) */
		bytetemp = readMem(HL);
		contendedAccess(HL, 1);
		BIT_MEMPTR(7, bytetemp);
		break;
	case 0x7f:		/* BIT 7,A */
		BIT_REG(7, A);
		break;
	case 0x80:		/* RES 0,B */
		B &= 0xfe;
		break;
	case 0x81:		/* RES 0,C */
		C &= 0xfe;
		break;
	case 0x82:		/* RES 0,D */
		D &= 0xfe;
		break;
	case 0x83:		/* RES 0,E */
		E &= 0xfe;
		break;
	case 0x84:		/* RES 0,H */
		H &= 0xfe;
		break;
	case 0x85:		/* RES 0,L */
		L &= 0xfe;
		break;
	case 0x86:		/* RES 0,(HL) */
		bytetemp = readMem(HL);
		contendedAccess(HL, 1);
		writeMem(HL, bytetemp & 0xfe);
		break;
	case 0x87:		/* RES 0,A */
		A &= 0xfe;
		break;
	case 0x88:		/* RES 1,B */
		B &= 0xfd;
		break;
	case 0x89:		/* RES 1,C */
		C &= 0xfd;
		break;
	case 0x8a:		/* RES 1,D */
		D &= 0xfd;
		break;
	case 0x8b:		/* RES 1,E */
		E &= 0xfd;
		break;
	case 0x8c:		/* RES 1,H */
		H &= 0xfd;
		break;
	case 0x8d:		/* RES 1,L */
		L &= 0xfd;
		break;
	case 0x8e:		/* RES 1,(HL) */
		bytetemp = readMem(HL);
		contendedAccess(HL, 1);
		writeMem(HL, bytetemp & 0xfd);
		break;
	case 0x8f:		/* RES 1,A */
		A &= 0xfd;
		break;
	case 0x90:		/* RES 2,B */
		B &= 0xfb;
		break;
	case 0x91:		/* RES 2,C */
		C &= 0xfb;
		break;
	case 0x92:		/* RES 2,D */
		D &= 0xfb;
		break;
	case 0x93:		/* RES 2,E */
		E &= 0xfb;
		break;
	case 0x94:		/* RES 2,H */
		H &= 0xfb;
		break;
	case 0x95:		/* RES 2,L */
		L &= 0xfb;
		break;
	case 0x96:		/* RES 2,(HL) */
		bytetemp = readMem(HL);
		contendedAccess(HL, 1);
		writeMem(HL, bytetemp & 0xfb);
		break;
	case 0x97:		/* RES 2,A */
		A &= 0xfb;
		break;
	case 0x98:		/* RES 3,B */
		B &= 0xf7;
		break;
	case 0x99:		/* RES 3,C */
		C &= 0xf7;
		break;
	case 0x9a:		/* RES 3,D */
		D &= 0xf7;
		break;
	case 0x9b:		/* RES 3,E */
		E &= 0xf7;
		break;
	case 0x9c:		/* RES 3,H */
		H &= 0xf7;
		break;
	case 0x9d:		/* RES 3,L */
		L &= 0xf7;
		break;
	case 0x9e:		/* RES 3,(HL) */
		bytetemp = readMem(HL);
		contendedAccess(HL, 1);
		writeMem(HL, bytetemp & 0xf7);
		break;
	case 0x9f:		/* RES 3,A */
		A &= 0xf7;
		break;
	case 0xa0:		/* RES 4,B */
		B &= 0xef;
		break;
	case 0xa1:		/* RES 4,C */
		C &= 0xef;
		break;
	case 0xa2:		/* RES 4,D */
		D &= 0xef;
		break;
	case 0xa3:		/* RES 4,E */
		E &= 0xef;
		break;
	case 0xa4:		/* RES 4,H */
		H &= 0xef;
		break;
	case 0xa5:		/* RES 4,L */
		L &= 0xef;
		break;
	case 0xa6:		/* RES 4,(HL) */
		bytetemp = readMem(HL);
		contendedAccess(HL, 1);
		writeMem(HL, bytetemp & 0xef);
		break;
	case 0xa7:		/* RES 4,A */
		A &= 0xef;
		break;
	case 0xa8:		/* RES 5,B */
		B &= 0xdf;
		break;
	case 0xa9:		/* RES 5,C */
		C &= 0xdf;
		break;
	case 0xaa:		/* RES 5,D */
		D &= 0xdf;
		break;
	case 0xab:		/* RES 5,E */
		E &= 0xdf;
		break;
	case 0xac:		/* RES 5,H */
		H &= 0xdf;
		break;
	case 0xad:		/* RES 5,L */
		L &= 0xdf;
		break;
	case 0xae:		/* RES 5,(HL) */
		bytetemp = readMem(HL);
		contendedAccess(HL, 1);
		writeMem(HL, bytetemp & 0xdf);
		break;
	case 0xaf:		/* RES 5,A */
		A &= 0xdf;
		break;
	case 0xb0:		/* RES 6,B */
		B &= 0xbf;
		break;
	case 0xb1:		/* RES 6,C */
		C &= 0xbf;
		break;
	case 0xb2:		/* RES 6,D */
		D &= 0xbf;
		break;
	case 0xb3:		/* RES 6,E */
		E &= 0xbf;
		break;
	case 0xb4:		/* RES 6,H */
		H &= 0xbf;
		break;
	case 0xb5:		/* RES 6,L */
		L &= 0xbf;
		break;
	case 0xb6:		/* RES 6,(HL) */
		bytetemp = readMem(HL);
		contendedAccess(HL, 1);
		writeMem(HL, bytetemp & 0xbf);
		break;
	case 0xb7:		/* RES 6,A */
		A &= 0xbf;
		break;
	case 0xb8:		/* RES 7,B */
		B &= 0x7f;
		break;
	case 0xb9:		/* RES 7,C */
		C &= 0x7f;
		break;
	case 0xba:		/* RES 7,D */
		D &= 0x7f;
		break;
	case 0xbb:		/* RES 7,E */
		E &= 0x7f;
		break;
	case 0xbc:		/* RES 7,H */
		H &= 0x7f;
		break;
	case 0xbd:		/* RES 7,L */
		L &= 0x7f;
		break;
	case 0xbe:		/* RES 7,(HL) */
		bytetemp = readMem(HL);
		contendedAccess(HL, 1);
		writeMem(HL, bytetemp & 0x7f);
		break;
	case 0xbf:		/* RES 7,A */
		A &= 0x7f;
		break;
	case 0xc0:		/* SET 0,B */
		B |= 0x01;
		break;
	case 0xc1:		/* SET 0,C */
		C |= 0x01;
		break;
	case 0xc2:		/* SET 0,D */
		D |= 0x01;
		break;
	case 0xc3:		/* SET 0,E */
		E |= 0x01;
		break;
	case 0xc4:		/* SET 0,H */
		H |= 0x01;
		break;
	case 0xc5:		/* SET 0,L */
		L |= 0x01;
		break;
	case 0xc6:		/* SET 0,(HL) */
		bytetemp = readMem(HL);
		contendedAccess(HL, 1);
		writeMem(HL, bytetemp | 0x01);
		break;
	case 0xc7:		/* SET 0,A */
		A |= 0x01;
		break;
	case 0xc8:		/* SET 1,B */
		B |= 0x02;
		break;
	case 0xc9:		/* SET 1,C */
		C |= 0x02;
		break;
	case 0xca:		/* SET 1,D */
		D |= 0x02;
		break;
	case 0xcb:		/* SET 1,E */
		E |= 0x02;
		break;
	case 0xcc:		/* SET 1,H */
		H |= 0x02;
		break;
	case 0xcd:		/* SET 1,L */
		L |= 0x02;
		break;
	case 0xce:		/* SET 1,(HL) */
		bytetemp = readMem(HL);
		contendedAccess(HL, 1);
		writeMem(HL, bytetemp | 0x02);
		break;
	case 0xcf:		/* SET 1,A */
		A |= 0x02;
		break;
	case 0xd0:		/* SET 2,B */
		B |= 0x04;
		break;
	case 0xd1:		/* SET 2,C */
		C |= 0x04;
		break;
	case 0xd2:		/* SET 2,D */
		D |= 0x04;
		break;
	case 0xd3:		/* SET 2,E */
		E |= 0x04;
		break;
	case 0xd4:		/* SET 2,H */
		H |= 0x04;
		break;
	case 0xd5:		/* SET 2,L */
		L |= 0x04;
		break;
	case 0xd6:		/* SET 2,(HL) */
		bytetemp = readMem(HL);
		contendedAccess(HL, 1);
		writeMem(HL, bytetemp | 0x04);
		break;
	case 0xd7:		/* SET 2,A */
		A |= 0x04;
		break;
	case 0xd8:		/* SET 3,B */
		B |= 0x08;
		break;
	case 0xd9:		/* SET 3,C */
		C |= 0x08;
		break;
	case 0xda:		/* SET 3,D */
		D |= 0x08;
		break;
	case 0xdb:		/* SET 3,E */
		E |= 0x08;
		break;
	case 0xdc:		/* SET 3,H */
		H |= 0x08;
		break;
	case 0xdd:		/* SET 3,L */
		L |= 0x08;
		break;
	case 0xde:		/* SET 3,(HL) */
		bytetemp = readMem(HL);
		contendedAccess(HL, 1);
		writeMem(HL, bytetemp | 0x08);
		break;
	case 0xdf:		/* SET 3,A */
		A |= 0x08;
		break;
	case 0xe0:		/* SET 4,B */
		B |= 0x10;
		break;
	case 0xe1:		/* SET 4,C */
		C |= 0x10;
		break;
	case 0xe2:		/* SET 4,D */
		D |= 0x10;
		break;
	case 0xe3:		/* SET 4,E */
		E |= 0x10;
		break;
	case 0xe4:		/* SET 4,H */
		H |= 0x10;
		break;
	case 0xe5:		/* SET 4,L */
		L |= 0x10;
		break;
	case 0xe6:		/* SET 4,(HL) */
		bytetemp = readMem(HL);
		contendedAccess(HL, 1);
		writeMem(HL, bytetemp | 0x10);
		break;
	case 0xe7:		/* SET 4,A */
		A |= 0x10;
		break;
	case 0xe8:		/* SET 5,B */
		B |= 0x20;
		break;
	case 0xe9:		/* SET 5,C */
		C |= 0x20;
		break;
	case 0xea:		/* SET 5,D */
		D |= 0x20;
		break;
	case 0xeb:		/* SET 5,E */
		E |= 0x20;
		break;
	case 0xec:		/* SET 5,H */
		H |= 0x20;
		break;
	case 0xed:		/* SET 5,L */
		L |= 0x20;
		break;
	case 0xee:		/* SET 5,(HL) */
		bytetemp = readMem(HL);
		contendedAccess(HL, 1);
		writeMem(HL, bytetemp | 0x20);
		break;
	case 0xef:		/* SET 5,A */
		A |= 0x20;
		break;
	case 0xf0:		/* SET 6,B */
		B |= 0x40;
		break;
	case 0xf1:		/* SET 6,C */
		C |= 0x40;
		break;
	case 0xf2:		/* SET 6,D */
		D |= 0x40;
		break;
	case 0xf3:		/* SET 6,E */
		E |= 0x40;
		break;
	case 0xf4:		/* SET 6,H */
		H |= 0x40;
		break;
	case 0xf5:		/* SET 6,L */
		L |= 0x40;
		break;
	case 0xf6:		/* SET 6,(HL) */
		bytetemp = readMem(HL);
		contendedAccess(HL, 1);
		writeMem(HL, bytetemp | 0x40);
		break;
	case 0xf7:		/* SET 6,A */
		A |= 0x40;
		break;
	case 0xf8:		/* SET 7,B */
		B |= 0x80;
		break;
	case 0xf9:		/* SET 7,C */
		C |= 0x80;
		break;
	case 0xfa:		/* SET 7,D */
		D |= 0x80;
		break;
	case 0xfb:		/* SET 7,E */
		E |= 0x80;
		break;
	case 0xfc:		/* SET 7,H */
		H |= 0x80;
		break;
	case 0xfd:		/* SET 7,L */
		L |= 0x80;
		break;
	case 0xfe:		/* SET 7,(HL) */
		bytetemp = readMem(HL);
		contendedAccess(HL, 1);
		writeMem(HL, bytetemp | 0x80);
		break;
	case 0xff:		/* SET 7,A */
		A |= 0x80;
		break;
	}
}

void ZXSpectrum::stepED(BYTE opcode)
{
	BYTE bytetemp, value, lookup, initemp, initemp2, outitemp, outitemp2;
	switch (opcode)
	{
	case 0x40:		/* IN B,(C) */
		INP(B, BC);
		break;
	case 0x41:		/* OUT (C),B */
		writePort(BC, B);
		m_Z80Processor.memptr.w = BC + 1;
		break;
	case 0x42:		/* SBC HL,BC */
		contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1);
		contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1);
		SBC16(BC);
		break;
	case 0x43:		/* LD (nnnn),BC */
		LD16_NNRR(C, B);
		break;
	case 0x44:
	case 0x4c:
	case 0x54:
	case 0x5c:
	case 0x64:
	case 0x6c:
	case 0x74:
	case 0x7c:		/* NEG */
		bytetemp = A;
		A = 0;
		SUB(bytetemp);
		break;
	case 0x45:
	case 0x4d:
	case 0x55:
	case 0x5d:
	case 0x65:
	case 0x6d:
	case 0x75:
	case 0x7d:		/* RETN */
		IFF1 = IFF2;
		RET();
		//        z80_retn();
		break;
	case 0x46:
	case 0x4e:
	case 0x66:
	case 0x6e:		/* IM 0 */
		IM = 0;
		break;
	case 0x47:		/* LD I,A */
		contendedAccess(IR, 1);
		I = A;
		break;
	case 0x48:		/* IN C,(C) */
		INP(C, BC);
		break;
	case 0x49:		/* OUT (C),C */
		writePort(BC, C);
		m_Z80Processor.memptr.w = BC + 1;
		break;
	case 0x4a:		/* ADC HL,BC */
		contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1);
		contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1);
		ADC16(BC);
		break;
	case 0x4b:		/* LD BC,(nnnn) */
		LD16_RRNN(C, B);
		break;
	case 0x4f:		/* LD R,A */
		contendedAccess(IR, 1);
		/* Keep the RZX instruction counter right */
  //      rzx_instructions_offset += ( R - A );
		R = R7 = A;
		break;
	case 0x50:		/* IN D,(C) */
		INP(D, BC);
		break;
	case 0x51:		/* OUT (C),D */
		writePort(BC, D);
		m_Z80Processor.memptr.w = BC + 1;
		break;
	case 0x52:		/* SBC HL,DE */
		contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1);
		contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1);
		SBC16(DE);
		break;
	case 0x53:		/* LD (nnnn),DE */
		LD16_NNRR(E, D);
		break;
	case 0x56:
	case 0x76:		/* IM 1 */
		IM = 1;
		break;
	case 0x57:		/* LD A,I */
		contendedAccess(IR, 1);
		A = I;
		FL = (FL & FLAG_C) | m_sz53Table[A] | (IFF2 ? FLAG_V : 0);
		Q = FL;
		m_Z80Processor.iff2_read = 1;
		//        event_add(tstates, z80_nmos_iff2_event);
		break;
	case 0x58:		/* IN E,(C) */
		INP(E, BC);
		break;
	case 0x59:		/* OUT (C),E */
		writePort(BC, E);
		m_Z80Processor.memptr.w = BC + 1;
		break;
	case 0x5a:		/* ADC HL,DE */
		contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1);
		contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1);
		ADC16(DE);
		break;
	case 0x5b:		/* LD DE,(nnnn) */
		LD16_RRNN(E, D);
		break;
	case 0x5e:
	case 0x7e:		/* IM 2 */
		IM = 2;
		break;
	case 0x5f:		/* LD A,R */
		contendedAccess(IR, 1);
		A = (R & 0x7f) | (R7 & 0x80);
		FL = (FL & FLAG_C) | m_sz53Table[A] | (IFF2 ? FLAG_V : 0);
		Q = FL;
		m_Z80Processor.iff2_read = 1;
		//        event_add(tstates, z80_nmos_iff2_event);
		break;
	case 0x60:		/* IN H,(C) */
		INP(H, BC);
		break;
	case 0x61:		/* OUT (C),H */
		writePort(BC, H);
		m_Z80Processor.memptr.w = BC + 1;
		break;
	case 0x62:		/* SBC HL,HL */
		contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1);
		contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1);
		SBC16(HL);
		break;
	case 0x63:		/* LD (nnnn),HL */
		LD16_NNRR(L, H);
		break;
	case 0x67:		/* RRD */
		bytetemp = readMem(HL);
		contendedAccess(HL, 1); contendedAccess(HL, 1); contendedAccess(HL, 1); contendedAccess(HL, 1);
		writeMem(HL, (A << 4) | (bytetemp >> 4));
		A = (A & 0xf0) | (bytetemp & 0x0f);
		FL = (FL & FLAG_C) | m_sz53pTable[A];
		Q = FL;
		m_Z80Processor.memptr.w = HL + 1;
		break;
	case 0x68:		/* IN L,(C) */
		INP(L, BC);
		break;
	case 0x69:		/* OUT (C),L */
		writePort(BC, L);
		m_Z80Processor.memptr.w = BC + 1;
		break;
	case 0x6a:		/* ADC HL,HL */
		contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1);
		contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1);
		ADC16(HL);
		break;
	case 0x6b:		/* LD HL,(nnnn) */
		LD16_RRNN(L, H);
		break;
	case 0x6f:		/* RLD */
		bytetemp = readMem(HL);
		contendedAccess(HL, 1); contendedAccess(HL, 1); contendedAccess(HL, 1); contendedAccess(HL, 1);
		writeMem(HL, (bytetemp << 4) | (A & 0x0f));
		A = (A & 0xf0) | (bytetemp >> 4);
		FL = (FL & FLAG_C) | m_sz53pTable[A];
		Q = FL;
		m_Z80Processor.memptr.w = HL + 1;
		break;
	case 0x70:		/* IN F,(C) */
		INP(bytetemp, BC);
		break;
	case 0x71:		/* OUT (C),0 */
		writePort(BC, 0);
		m_Z80Processor.memptr.w = BC + 1;
		break;
	case 0x72:		/* SBC HL,SP */
		contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1);
		contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1);
		SBC16(SP);
		break;
	case 0x73:		/* LD (nnnn),SP */
		LD16_NNRR(SPL, SPH);
		break;
	case 0x78:		/* IN A,(C) */
		INP(A, BC);
		break;
	case 0x79:		/* OUT (C),A */
		writePort(BC, A);
		m_Z80Processor.memptr.w = BC + 1;
		break;
	case 0x7a:		/* ADC HL,SP */
		contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1);
		contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1);
		ADC16(SP);
		break;
	case 0x7b:		/* LD SP,(nnnn) */
		LD16_RRNN(SPL, SPH);
		break;
	case 0xa0:		/* LDI */
		bytetemp = readMem(HL);
		BC--;
		writeMem(DE, bytetemp);
		contendedAccess(DE, 1); contendedAccess(DE, 1);
		DE++; HL++;
		bytetemp += A;
		FL = (FL & (FLAG_C | FLAG_Z | FLAG_S)) | (BC ? FLAG_V : 0) | (bytetemp & FLAG_3) | ((bytetemp & 0x02) ? FLAG_5 : 0);
		Q = FL;
		break;
	case 0xa1:		/* CPI */
		value = readMem(HL);
		contendedAccess(HL, 1); contendedAccess(HL, 1); contendedAccess(HL, 1); contendedAccess(HL, 1);
		contendedAccess(HL, 1);
		bytetemp = A - value;
		lookup = ((A & 0x08) >> 3) | (((value) & 0x08) >> 2) | ((bytetemp & 0x08) >> 1);
		HL++; BC--;
		FL = (FL & FLAG_C) | (BC ? (FLAG_V | FLAG_N) : FLAG_N) | m_halfcarrySubTable[lookup] | (bytetemp ? 0 : FLAG_Z) | (bytetemp & FLAG_S);
		if (FL & FLAG_H) bytetemp--;
		FL |= (bytetemp & FLAG_3) | ((bytetemp & 0x02) ? FLAG_5 : 0);
		Q = FL;
		m_Z80Processor.memptr.w++;
		break;
	case 0xa2:		/* INI */
		contendedAccess(IR, 1);
		initemp = readPort(BC);
		writeMem(HL, initemp);
		m_Z80Processor.memptr.w = BC + 1;
		B--; HL++;
		initemp2 = initemp + C + 1;
		FL = (initemp & 0x80 ? FLAG_N : 0) | ((initemp2 < initemp) ? FLAG_H | FLAG_C : 0) | (m_parityTable[(initemp2 & 0x07) ^ B] ? FLAG_P : 0) | m_sz53Table[B];
		Q = FL;
		break;
	case 0xa3:		/* OUTI */
		contendedAccess(IR, 1);
		outitemp = readMem(HL);
		B--; /* This does happen first, despite what the specs say */
		m_Z80Processor.memptr.w = BC + 1;
		writePort(BC, outitemp);
		HL++;
		outitemp2 = outitemp + L;
		FL = (outitemp & 0x80 ? FLAG_N : 0) | ((outitemp2 < outitemp) ? FLAG_H | FLAG_C : 0) | (m_parityTable[(outitemp2 & 0x07) ^ B] ? FLAG_P : 0) | m_sz53Table[B];
		Q = FL;
		break;
	case 0xa8:		/* LDD */
		bytetemp = readMem(HL);
		BC--;
		writeMem(DE, bytetemp);
		contendedAccess(DE, 1); contendedAccess(DE, 1);
		DE--; HL--;
		bytetemp += A;
		FL = (FL & (FLAG_C | FLAG_Z | FLAG_S)) | (BC ? FLAG_V : 0) | (bytetemp & FLAG_3) | ((bytetemp & 0x02) ? FLAG_5 : 0);
		Q = FL;
		break;
	case 0xa9:		/* CPD */
		value = readMem(HL);
		bytetemp = A - value;
		lookup = ((A & 0x08) >> 3) | (((value) & 0x08) >> 2) | ((bytetemp & 0x08) >> 1);
		contendedAccess(HL, 1); contendedAccess(HL, 1); contendedAccess(HL, 1); contendedAccess(HL, 1);
		contendedAccess(HL, 1);
		HL--; BC--;
		FL = (FL & FLAG_C) | (BC ? (FLAG_V | FLAG_N) : FLAG_N) | m_halfcarrySubTable[lookup] | (bytetemp ? 0 : FLAG_Z) | (bytetemp & FLAG_S);
		if (FL & FLAG_H) bytetemp--;
		FL |= (bytetemp & FLAG_3) | ((bytetemp & 0x02) ? FLAG_5 : 0);
		Q = FL;
		m_Z80Processor.memptr.w--;
		break;
	case 0xaa:		/* IND */
		contendedAccess(IR, 1);
		initemp = readPort(BC);
		writeMem(HL, initemp);
		m_Z80Processor.memptr.w = BC - 1;
		B--; HL--;
		initemp2 = initemp + C - 1;
		FL = (initemp & 0x80 ? FLAG_N : 0) | ((initemp2 < initemp) ? FLAG_H | FLAG_C : 0) | (m_parityTable[(initemp2 & 0x07) ^ B] ? FLAG_P : 0) | m_sz53Table[B];
		Q = FL;
		break;
	case 0xab:		/* OUTD */
		contendedAccess(IR, 1);
		outitemp = readMem(HL);
		B--; /* This does happen first, despite what the specs say */
		m_Z80Processor.memptr.w = BC - 1;
		writePort(BC, outitemp);
		HL--;
		outitemp2 = outitemp + L;
		FL = (outitemp & 0x80 ? FLAG_N : 0) | ((outitemp2 < outitemp) ? FLAG_H | FLAG_C : 0) | (m_parityTable[(outitemp2 & 0x07) ^ B] ? FLAG_P : 0) | m_sz53Table[B];
		Q = FL;
		break;
	case 0xb0:		/* LDIR */
		bytetemp = readMem(HL);
		writeMem(DE, bytetemp);
		contendedAccess(DE, 1); contendedAccess(DE, 1);
		BC--;
		bytetemp += A;
		FL = (FL & (FLAG_C | FLAG_Z | FLAG_S)) | (BC ? FLAG_V : 0) | (bytetemp & FLAG_3) | ((bytetemp & 0x02) ? FLAG_5 : 0);
		Q = FL;
		if (BC)
		{
			contendedAccess(DE, 1); contendedAccess(DE, 1); contendedAccess(DE, 1); contendedAccess(DE, 1);
			contendedAccess(DE, 1);
			PC -= 2;
			m_Z80Processor.memptr.w = PC + 1;
		}
		HL++; DE++;
		break;
	case 0xb1:		/* CPIR */
		value = readMem(HL);
		bytetemp = A - value;
		lookup = ((A & 0x08) >> 3) | (((value) & 0x08) >> 2) | ((bytetemp & 0x08) >> 1);
		contendedAccess(HL, 1); contendedAccess(HL, 1); contendedAccess(HL, 1); contendedAccess(HL, 1);
		contendedAccess(HL, 1);
		BC--;
		FL = (FL & FLAG_C) | (BC ? (FLAG_V | FLAG_N) : FLAG_N) | m_halfcarrySubTable[lookup] | (bytetemp ? 0 : FLAG_Z) | (bytetemp & FLAG_S);
		if (FL & FLAG_H) bytetemp--;
		FL |= (bytetemp & FLAG_3) | ((bytetemp & 0x02) ? FLAG_5 : 0);
		Q = FL;
		if ((FL & (FLAG_V | FLAG_Z)) == FLAG_V)
		{
			contendedAccess(HL, 1); contendedAccess(HL, 1); contendedAccess(HL, 1); contendedAccess(HL, 1);
			contendedAccess(HL, 1);
			PC -= 2;
			m_Z80Processor.memptr.w = PC + 1;
		}
		else
			m_Z80Processor.memptr.w++;
		HL++;
		break;
	case 0xb2:		/* INIR */
		contendedAccess(IR, 1);
		initemp = readPort(BC);
		writeMem(HL, initemp);
		m_Z80Processor.memptr.w = BC + 1;
		B--;
		initemp2 = initemp + C + 1;
		FL = (initemp & 0x80 ? FLAG_N : 0) | ((initemp2 < initemp) ? FLAG_H | FLAG_C : 0) | (m_parityTable[(initemp2 & 0x07) ^ B] ? FLAG_P : 0) | m_sz53Table[B];
		Q = FL;
		if (B)
		{
			contendedAccess(HL, 1); contendedAccess(HL, 1); contendedAccess(HL, 1); contendedAccess(HL, 1);
			contendedAccess(HL, 1);
			PC -= 2;
		}
		HL++;
		break;
	case 0xb3:		/* OTIR */
		contendedAccess(IR, 1);
		outitemp = readMem(HL);
		B--; /* This does happen first, despite what the specs say */
		m_Z80Processor.memptr.w = BC + 1;
		writePort(BC, outitemp);
		HL++;
		outitemp2 = outitemp + L;
		FL = (outitemp & 0x80 ? FLAG_N : 0) | ((outitemp2 < outitemp) ? FLAG_H | FLAG_C : 0) | (m_parityTable[(outitemp2 & 0x07) ^ B] ? FLAG_P : 0) | m_sz53Table[B];
		Q = FL;
		if (B)
		{
			contendedAccess(BC, 1); contendedAccess(BC, 1); contendedAccess(BC, 1); contendedAccess(BC, 1);
			contendedAccess(BC, 1);
			PC -= 2;
		}
		break;
	case 0xb8:		/* LDDR */
		bytetemp = readMem(HL);
		writeMem(DE, bytetemp);
		contendedAccess(DE, 1); contendedAccess(DE, 1);
		BC--;
		bytetemp += A;
		FL = (FL & (FLAG_C | FLAG_Z | FLAG_S)) | (BC ? FLAG_V : 0) | (bytetemp & FLAG_3) | ((bytetemp & 0x02) ? FLAG_5 : 0);
		Q = FL;
		if (BC)
		{
			contendedAccess(DE, 1); contendedAccess(DE, 1); contendedAccess(DE, 1); contendedAccess(DE, 1);
			contendedAccess(DE, 1);
			PC -= 2;
			m_Z80Processor.memptr.w = PC + 1;
		}
		HL--; DE--;
		break;
	case 0xb9:		/* CPDR */
		value = readMem(HL);
		bytetemp = A - value;
		lookup = ((A & 0x08) >> 3) | (((value) & 0x08) >> 2) | ((bytetemp & 0x08) >> 1);
		contendedAccess(HL, 1); contendedAccess(HL, 1); contendedAccess(HL, 1); contendedAccess(HL, 1);
		contendedAccess(HL, 1);
		BC--;
		FL = (FL & FLAG_C) | (BC ? (FLAG_V | FLAG_N) : FLAG_N) | m_halfcarrySubTable[lookup] | (bytetemp ? 0 : FLAG_Z) | (bytetemp & FLAG_S);
		if (FL & FLAG_H) bytetemp--;
		FL |= (bytetemp & FLAG_3) | ((bytetemp & 0x02) ? FLAG_5 : 0);
		Q = FL;
		if ((FL & (FLAG_V | FLAG_Z)) == FLAG_V)
		{
			contendedAccess(HL, 1); contendedAccess(HL, 1); contendedAccess(HL, 1); contendedAccess(HL, 1);
			contendedAccess(HL, 1);
			PC -= 2;
			m_Z80Processor.memptr.w = PC + 1;
		}
		else
			m_Z80Processor.memptr.w--;
		HL--;
		break;
	case 0xba:		/* INDR */
		contendedAccess(IR, 1);
		initemp = readPort(BC);
		writeMem(HL, initemp);
		m_Z80Processor.memptr.w = BC - 1;
		B--;
		initemp2 = initemp + C - 1;
		FL = (initemp & 0x80 ? FLAG_N : 0) | ((initemp2 < initemp) ? FLAG_H | FLAG_C : 0) | (m_parityTable[(initemp2 & 0x07) ^ B] ? FLAG_P : 0) | m_sz53Table[B];
		Q = FL;
		if (B)
		{
			contendedAccess(HL, 1); contendedAccess(HL, 1); contendedAccess(HL, 1); contendedAccess(HL, 1);
			contendedAccess(HL, 1);
			PC -= 2;
		}
		HL--;
		break;
	case 0xbb:		/* OTDR */
		contendedAccess(IR, 1);
		outitemp = readMem(HL);
		B--; /* This does happen first, despite what the specs say */
		m_Z80Processor.memptr.w = BC - 1;
		writePort(BC, outitemp);
		HL--;
		outitemp2 = outitemp + L;
		FL = (outitemp & 0x80 ? FLAG_N : 0) | ((outitemp2 < outitemp) ? FLAG_H | FLAG_C : 0) | (m_parityTable[(outitemp2 & 0x07) ^ B] ? FLAG_P : 0) | m_sz53Table[B];
		Q = FL;
		if (B)
		{
			contendedAccess(BC, 1); contendedAccess(BC, 1); contendedAccess(BC, 1); contendedAccess(BC, 1);
			contendedAccess(BC, 1);
			PC -= 2;
		}
		break;
	case 0xfb:		/* slttrap */
	//        slt_trap(HL, A);
		break;
	default:		/* All other opcodes are NOPD */
		break;
	}
}

bool ZXSpectrum::stepDD(BYTE opcode)
{
	BYTE value, offset, bytetemp, opcode2, bytetempl, bytetemph;
	switch (opcode)
	{
	case 0x09:		/* ADD IX,BC */
		contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1);
		contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1);
		ADD16(IX, BC);
		break;
	case 0x19:		/* ADD IX,DE */
		contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1);
		contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1);
		ADD16(IX, DE);
		break;
	case 0x21:		/* LD IX,nnnn */
		IXL = readMem(PC++);
		IXH = readMem(PC++);
		break;
	case 0x22:		/* LD (nnnn),IX */
		LD16_NNRR(IXL, IXH);
		break;
	case 0x23:		/* INC IX */
		contendedAccess(IR, 1); contendedAccess(IR, 1);
		IX++;
		break;
	case 0x24:		/* INC IXH */
		INC8(IXH);
		break;
	case 0x25:		/* DEC IXH */
		DEC8(IXH);
		break;
	case 0x26:		/* LD IXH,nn */
		IXH = readMem(PC++);
		break;
	case 0x29:		/* ADD IX,IX */
		contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1);
		contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1);
		ADD16(IX, IX);
		break;
	case 0x2a:		/* LD IX,(nnnn) */
		LD16_RRNN(IXL, IXH);
		break;
	case 0x2b:		/* DEC IX */
		contendedAccess(IR, 1); contendedAccess(IR, 1);
		IX--;
		break;
	case 0x2c:		/* INC IXL */
		INC8(IXL);
		break;
	case 0x2d:		/* DEC IXL */
		DEC8(IXL);
		break;
	case 0x2e:		/* LD IXL,nn */
		IXL = readMem(PC++);
		break;
	case 0x34:		/* INC (IX+dd) */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IX + (OFFSET)offset;
		bytetemp = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		INC8(bytetemp);
		writeMem(m_Z80Processor.memptr.w, bytetemp);
		break;
	case 0x35:		/* DEC (IX+dd) */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IX + (OFFSET)offset;
		bytetemp = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		DEC8(bytetemp);
		writeMem(m_Z80Processor.memptr.w, bytetemp);
		break;
	case 0x36:		/* LD (IX+dd),nn */
		offset = readMem(PC++);
		value = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IX + (OFFSET)offset;
		writeMem(m_Z80Processor.memptr.w, value);
		break;
	case 0x39:		/* ADD IX,SP */
		contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1);
		contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1);
		ADD16(IX, SP);
		break;
	case 0x44:		/* LD B,IXH */
		B = IXH;
		break;
	case 0x45:		/* LD B,IXL */
		B = IXL;
		break;
	case 0x46:		/* LD B,(IX+dd) */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IX + (OFFSET)offset;
		B = readMem(m_Z80Processor.memptr.w);
		break;
	case 0x4c:		/* LD C,IXH */
		C = IXH;
		break;
	case 0x4d:		/* LD C,IXL */
		C = IXL;
		break;
	case 0x4e:		/* LD C,(IX+dd) */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IX + (OFFSET)offset;
		C = readMem(m_Z80Processor.memptr.w);
		break;
	case 0x54:		/* LD D,IXH */
		D = IXH;
		break;
	case 0x55:		/* LD D,IXL */
		D = IXL;
		break;
	case 0x56:		/* LD D,(IX+dd) */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IX + (OFFSET)offset;
		D = readMem(m_Z80Processor.memptr.w);
		break;
	case 0x5c:		/* LD E,IXH */
		E = IXH;
		break;
	case 0x5d:		/* LD E,IXL */
		E = IXL;
		break;
	case 0x5e:		/* LD E,(IX+dd) */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IX + (OFFSET)offset;
		E = readMem(m_Z80Processor.memptr.w);
		break;
	case 0x60:		/* LD IXH,B */
		IXH = B;
		break;
	case 0x61:		/* LD IXH,C */
		IXH = C;
		break;
	case 0x62:		/* LD IXH,D */
		IXH = D;
		break;
	case 0x63:		/* LD IXH,E */
		IXH = E;
		break;
	case 0x64:		/* LD IXH,IXH */
		break;
	case 0x65:		/* LD IXH,IXL */
		IXH = IXL;
		break;
	case 0x66:		/* LD H,(IX+dd) */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IX + (OFFSET)offset;
		H = readMem(m_Z80Processor.memptr.w);
		break;
	case 0x67:		/* LD IXH,A */
		IXH = A;
		break;
	case 0x68:		/* LD IXL,B */
		IXL = B;
		break;
	case 0x69:		/* LD IXL,C */
		IXL = C;
		break;
	case 0x6a:		/* LD IXL,D */
		IXL = D;
		break;
	case 0x6b:		/* LD IXL,E */
		IXL = E;
		break;
	case 0x6c:		/* LD IXL,IXH */
		IXL = IXH;
		break;
	case 0x6d:		/* LD IXL,IXL */
		break;
	case 0x6e:		/* LD L,(IX+dd) */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IX + (OFFSET)offset;
		L = readMem(m_Z80Processor.memptr.w);
		break;
	case 0x6f:		/* LD IXL,A */
		IXL = A;
		break;
	case 0x70:		/* LD (IX+dd),B */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IX + (OFFSET)offset;
		writeMem(m_Z80Processor.memptr.w, B);
		break;
	case 0x71:		/* LD (IX+dd),C */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IX + (OFFSET)offset;
		writeMem(m_Z80Processor.memptr.w, C);
		break;
	case 0x72:		/* LD (IX+dd),D */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IX + (OFFSET)offset;
		writeMem(m_Z80Processor.memptr.w, D);
		break;
	case 0x73:		/* LD (IX+dd),E */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IX + (OFFSET)offset;
		writeMem(m_Z80Processor.memptr.w, E);
		break;
	case 0x74:		/* LD (IX+dd),H */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IX + (OFFSET)offset;
		writeMem(m_Z80Processor.memptr.w, H);
		break;
	case 0x75:		/* LD (IX+dd),L */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IX + (OFFSET)offset;
		writeMem(m_Z80Processor.memptr.w, L);
		break;
	case 0x77:		/* LD (IX+dd),A */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IX + (OFFSET)offset;
		writeMem(m_Z80Processor.memptr.w, A);
		break;
	case 0x7c:		/* LD A,IXH */
		A = IXH;
		break;
	case 0x7d:		/* LD A,IXL */
		A = IXL;
		break;
	case 0x7e:		/* LD A,(IX+dd) */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IX + (OFFSET)offset;
		A = readMem(m_Z80Processor.memptr.w);
		break;
	case 0x84:		/* ADD A,IXH */
		ADD(IXH);
		break;
	case 0x85:		/* ADD A,IXL */
		ADD(IXL);
		break;
	case 0x86:		/* ADD A,(IX+dd) */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IX + (OFFSET)offset;
		bytetemp = readMem(m_Z80Processor.memptr.w);
		ADD(bytetemp);
		break;
	case 0x8c:		/* ADC A,IXH */
		ADC(IXH);
		break;
	case 0x8d:		/* ADC A,IXL */
		ADC(IXL);
		break;
	case 0x8e:		/* ADC A,(IX+dd) */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IX + (OFFSET)offset;
		bytetemp = readMem(m_Z80Processor.memptr.w);
		ADC(bytetemp);
		break;
	case 0x94:		/* SUB A,IXH */
		SUB(IXH);
		break;
	case 0x95:		/* SUB A,IXL */
		SUB(IXL);
		break;
	case 0x96:		/* SUB A,(IX+dd) */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IX + (OFFSET)offset;
		bytetemp = readMem(m_Z80Processor.memptr.w);
		SUB(bytetemp);
		break;
	case 0x9c:		/* SBC A,IXH */
		SBC(IXH);
		break;
	case 0x9d:		/* SBC A,IXL */
		SBC(IXL);
		break;
	case 0x9e:		/* SBC A,(IX+dd) */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IX + (OFFSET)offset;
		bytetemp = readMem(m_Z80Processor.memptr.w);
		SBC(bytetemp);
		break;
	case 0xa4:		/* AND A,IXH */
		AND(IXH);
		break;
	case 0xa5:		/* AND A,IXL */
		AND(IXL);
		break;
	case 0xa6:		/* AND A,(IX+dd) */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IX + (OFFSET)offset;
		bytetemp = readMem(m_Z80Processor.memptr.w);
		AND(bytetemp);
		break;
	case 0xac:		/* XOR A,IXH */
		XOR(IXH);
		break;
	case 0xad:		/* XOR A,IXL */
		XOR(IXL);
		break;
	case 0xae:		/* XOR A,(IX+dd) */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IX + (OFFSET)offset;
		bytetemp = readMem(m_Z80Processor.memptr.w);
		XOR(bytetemp);
		break;
	case 0xb4:		/* OR A,IXH */
		OR(IXH);
		break;
	case 0xb5:		/* OR A,IXL */
		OR(IXL);
		break;
	case 0xb6:		/* OR A,(IX+dd) */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IX + (OFFSET)offset;
		bytetemp = readMem(m_Z80Processor.memptr.w);
		OR(bytetemp);
		break;
	case 0xbc:		/* CP A,IXH */
		CP(IXH);
		break;
	case 0xbd:		/* CP A,IXL */
		CP(IXL);
		break;
	case 0xbe:		/* CP A,(IX+dd) */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IX + (OFFSET)offset;
		bytetemp = readMem(m_Z80Processor.memptr.w);
		CP(bytetemp);
		break;
	case 0xcb:		/* shift DDFDCB */
		contendedAccess(PC, 3);
		m_Z80Processor.memptr.w = IX + (OFFSET)m_pZXMemory[PC];
		PC++;
		contendedAccess(PC, 3);
		opcode2 = m_pZXMemory[PC];
		contendedAccess(PC, 1); contendedAccess(PC, 1);
		PC++;
		stepXXCB(opcode2);
		break;
	case 0xe1:		/* POP IX */
		POP16(IXL, IXH);
		break;
	case 0xe3:		/* EX (SP),IX */
		bytetempl = readMem(SP);
		bytetemph = readMem(SP + 1);
		contendedAccess(SP + 1, 1);
		writeMem(SP + 1, IXH);
		writeMem(SP, IXL);
		contendedAccess(SP, 1); contendedAccess(SP, 1);
		IXL = m_Z80Processor.memptr.b.l = bytetempl;
		IXH = m_Z80Processor.memptr.b.h = bytetemph;
		break;
	case 0xe5:		/* PUSH IX */
		contendedAccess(IR, 1);
		PUSH16(IXL, IXH);
		break;
	case 0xe9:		/* JP IX */
		PC = IX; /* NB: NOT INDIRECT! */
		break;
	case 0xf9:		/* LD SP,IX */
		contendedAccess(IR, 1); contendedAccess(IR, 1);
		SP = IX;
		break;
	default:		/* Instruction did not involve H or L, so backtrack one instruction and parse again */
		PC--;
		R--;
		return false;
	}
	return true;
}

bool ZXSpectrum::stepFD(BYTE opcode)
{
	BYTE value, offset, bytetemp, opcode2, bytetempl, bytetemph;
	switch (opcode)
	{
	case 0x09:		/* ADD IY,BC */
		contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1);
		contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1);
		ADD16(IY, BC);
		break;
	case 0x19:		/* ADD IY,DE */
		contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1);
		contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1);
		ADD16(IY, DE);
		break;
	case 0x21:		/* LD IY,nnnn */
		IYL = readMem(PC++);
		IYH = readMem(PC++);
		break;
	case 0x22:		/* LD (nnnn),IY */
		LD16_NNRR(IYL, IYH);
		break;
	case 0x23:		/* INC IY */
		contendedAccess(IR, 1); contendedAccess(IR, 1);
		IY++;
		break;
	case 0x24:		/* INC IYH */
		INC8(IYH);
		break;
	case 0x25:		/* DEC IYH */
		DEC8(IYH);
		break;
	case 0x26:		/* LD IYH,nn */
		IYH = readMem(PC++);
		break;
	case 0x29:		/* ADD IY,IY */
		contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1);
		contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1);
		ADD16(IY, IY);
		break;
	case 0x2a:		/* LD IY,(nnnn) */
		LD16_RRNN(IYL, IYH);
		break;
	case 0x2b:		/* DEC IY */
		contendedAccess(IR, 1); contendedAccess(IR, 1);
		IY--;
		break;
	case 0x2c:		/* INC IYL */
		INC8(IYL);
		break;
	case 0x2d:		/* DEC IYL */
		DEC8(IYL);
		break;
	case 0x2e:		/* LD IYL,nn */
		IYL = readMem(PC++);
		break;
	case 0x34:		/* INC (IY+dd) */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IY + (OFFSET)offset;
		bytetemp = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		INC8(bytetemp);
		writeMem(m_Z80Processor.memptr.w, bytetemp);
		break;
	case 0x35:		/* DEC (IY+dd) */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IY + (OFFSET)offset;
		bytetemp = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		DEC8(bytetemp);
		writeMem(m_Z80Processor.memptr.w, bytetemp);
		break;
	case 0x36:		/* LD (IY+dd),nn */
		offset = readMem(PC++);
		value = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IY + (OFFSET)offset;
		writeMem(m_Z80Processor.memptr.w, value);
		break;
	case 0x39:		/* ADD IY,SP */
		contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1);
		contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1);
		ADD16(IY, SP);
		break;
	case 0x44:		/* LD B,IYH */
		B = IYH;
		break;
	case 0x45:		/* LD B,IYL */
		B = IYL;
		break;
	case 0x46:		/* LD B,(IY+dd) */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IY + (OFFSET)offset;
		B = readMem(m_Z80Processor.memptr.w);
		break;
	case 0x4c:		/* LD C,IYH */
		C = IYH;
		break;
	case 0x4d:		/* LD C,IYL */
		C = IYL;
		break;
	case 0x4e:		/* LD C,(IY+dd) */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IY + (OFFSET)offset;
		C = readMem(m_Z80Processor.memptr.w);
		break;
	case 0x54:		/* LD D,IYH */
		D = IYH;
		break;
	case 0x55:		/* LD D,IYL */
		D = IYL;
		break;
	case 0x56:		/* LD D,(IY+dd) */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IY + (OFFSET)offset;
		D = readMem(m_Z80Processor.memptr.w);
		break;
	case 0x5c:		/* LD E,IYH */
		E = IYH;
		break;
	case 0x5d:		/* LD E,IYL */
		E = IYL;
		break;
	case 0x5e:		/* LD E,(IY+dd) */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IY + (OFFSET)offset;
		E = readMem(m_Z80Processor.memptr.w);
		break;
	case 0x60:		/* LD IYH,B */
		IYH = B;
		break;
	case 0x61:		/* LD IYH,C */
		IYH = C;
		break;
	case 0x62:		/* LD IYH,D */
		IYH = D;
		break;
	case 0x63:		/* LD IYH,E */
		IYH = E;
		break;
	case 0x64:		/* LD IYH,IYH */
		break;
	case 0x65:		/* LD IYH,IYL */
		IYH = IYL;
		break;
	case 0x66:		/* LD H,(IY+dd) */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IY + (OFFSET)offset;
		H = readMem(m_Z80Processor.memptr.w);
		break;
	case 0x67:		/* LD IYH,A */
		IYH = A;
		break;
	case 0x68:		/* LD IYL,B */
		IYL = B;
		break;
	case 0x69:		/* LD IYL,C */
		IYL = C;
		break;
	case 0x6a:		/* LD IYL,D */
		IYL = D;
		break;
	case 0x6b:		/* LD IYL,E */
		IYL = E;
		break;
	case 0x6c:		/* LD IYL,IYH */
		IYL = IYH;
		break;
	case 0x6d:		/* LD IYL,IYL */
		break;
	case 0x6e:		/* LD L,(IY+dd) */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IY + (OFFSET)offset;
		L = readMem(m_Z80Processor.memptr.w);
		break;
	case 0x6f:		/* LD IYL,A */
		IYL = A;
		break;
	case 0x70:		/* LD (IY+dd),B */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IY + (OFFSET)offset;
		writeMem(m_Z80Processor.memptr.w, B);
		break;
	case 0x71:		/* LD (IY+dd),C */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IY + (OFFSET)offset;
		writeMem(m_Z80Processor.memptr.w, C);
		break;
	case 0x72:		/* LD (IY+dd),D */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IY + (OFFSET)offset;
		writeMem(m_Z80Processor.memptr.w, D);
		break;
	case 0x73:		/* LD (IY+dd),E */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IY + (OFFSET)offset;
		writeMem(m_Z80Processor.memptr.w, E);
		break;
	case 0x74:		/* LD (IY+dd),H */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IY + (OFFSET)offset;
		writeMem(m_Z80Processor.memptr.w, H);
		break;
	case 0x75:		/* LD (IY+dd),L */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IY + (OFFSET)offset;
		writeMem(m_Z80Processor.memptr.w, L);
		break;
	case 0x77:		/* LD (IY+dd),A */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IY + (OFFSET)offset;
		writeMem(m_Z80Processor.memptr.w, A);
		break;
	case 0x7c:		/* LD A,IYH */
		A = IYH;
		break;
	case 0x7d:		/* LD A,IYL */
		A = IYL;
		break;
	case 0x7e:		/* LD A,(IY+dd) */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IY + (OFFSET)offset;
		A = readMem(m_Z80Processor.memptr.w);
		break;
	case 0x84:		/* ADD A,IYH */
		ADD(IYH);
		break;
	case 0x85:		/* ADD A,IYL */
		ADD(IYL);
		break;
	case 0x86:		/* ADD A,(IY+dd) */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IY + (OFFSET)offset;
		bytetemp = readMem(m_Z80Processor.memptr.w);
		ADD(bytetemp);
		break;
	case 0x8c:		/* ADC A,IYH */
		ADC(IYH);
		break;
	case 0x8d:		/* ADC A,IYL */
		ADC(IYL);
		break;
	case 0x8e:		/* ADC A,(IY+dd) */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IY + (OFFSET)offset;
		bytetemp = readMem(m_Z80Processor.memptr.w);
		ADC(bytetemp);
		break;
	case 0x94:		/* SUB A,IYH */
		SUB(IYH);
		break;
	case 0x95:		/* SUB A,IYL */
		SUB(IYL);
		break;
	case 0x96:		/* SUB A,(IY+dd) */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IY + (OFFSET)offset;
		bytetemp = readMem(m_Z80Processor.memptr.w);
		SUB(bytetemp);
		break;
	case 0x9c:		/* SBC A,IYH */
		SBC(IYH);
		break;
	case 0x9d:		/* SBC A,IYL */
		SBC(IYL);
		break;
	case 0x9e:		/* SBC A,(IY+dd) */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IY + (OFFSET)offset;
		bytetemp = readMem(m_Z80Processor.memptr.w);
		SBC(bytetemp);
		break;
	case 0xa4:		/* AND A,IYH */
		AND(IYH);
		break;
	case 0xa5:		/* AND A,IYL */
		AND(IYL);
		break;
	case 0xa6:		/* AND A,(IY+dd) */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IY + (OFFSET)offset;
		bytetemp = readMem(m_Z80Processor.memptr.w);
		AND(bytetemp);
		break;
	case 0xac:		/* XOR A,IYH */
		XOR(IYH);
		break;
	case 0xad:		/* XOR A,IYL */
		XOR(IYL);
		break;
	case 0xae:		/* XOR A,(IY+dd) */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IY + (OFFSET)offset;
		bytetemp = readMem(m_Z80Processor.memptr.w);
		XOR(bytetemp);
		break;
	case 0xb4:		/* OR A,IYH */
		OR(IYH);
		break;
	case 0xb5:		/* OR A,IYL */
		OR(IYL);
		break;
	case 0xb6:		/* OR A,(IY+dd) */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IY + (OFFSET)offset;
		bytetemp = readMem(m_Z80Processor.memptr.w);
		OR(bytetemp);
		break;
	case 0xbc:		/* CP A,IYH */
		CP(IYH);
		break;
	case 0xbd:		/* CP A,IYL */
		CP(IYL);
		break;
	case 0xbe:		/* CP A,(IY+dd) */
		offset = readMem(PC);
		contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);
		contendedAccess(PC, 1);
		PC++;
		m_Z80Processor.memptr.w = IY + (OFFSET)offset;
		bytetemp = readMem(m_Z80Processor.memptr.w);
		CP(bytetemp);
		break;
	case 0xcb:		/* shift DDFDCB */
		contendedAccess(PC, 3);
		m_Z80Processor.memptr.w = IY + (OFFSET)m_pZXMemory[PC];
		PC++;
		contendedAccess(PC, 3);
		opcode2 = m_pZXMemory[PC];
		contendedAccess(PC, 1); contendedAccess(PC, 1);
		PC++;
		stepXXCB(opcode2);
		break;
	case 0xe1:		/* POP IY */
		POP16(IYL, IYH);
		break;
	case 0xe3:		/* EX (SP),IY */
		bytetempl = readMem(SP);
		bytetemph = readMem(SP + 1);
		contendedAccess(SP + 1, 1);
		writeMem(SP + 1, IYH);
		writeMem(SP, IYL);
		contendedAccess(SP, 1); contendedAccess(SP, 1);
		IYL = m_Z80Processor.memptr.b.l = bytetempl;
		IYH = m_Z80Processor.memptr.b.h = bytetemph;
		break;
	case 0xe5:		/* PUSH IY */
		contendedAccess(IR, 1);
		PUSH16(IYL, IYH);
		break;
	case 0xe9:		/* JP IY */
		PC = IY; /* NB: NOT INDIRECT! */
		break;
	case 0xf9:		/* LD SP,IY */
		contendedAccess(IR, 1); contendedAccess(IR, 1);
		SP = IY;
		break;
	default:		/* Instruction did not involve H or L, so backtrack one instruction and parse again */
		PC--;
		R--;
		return false;
	}
	return true;
}

void ZXSpectrum::stepXXCB(BYTE opcode)
{
	BYTE bytetemp;
	switch (opcode)
	{
	case 0x00:		/* LD B,RLC (REGISTER+dd) */
		B = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		RLC(B);
		writeMem(m_Z80Processor.memptr.w, B);
		break;
	case 0x01:		/* LD C,RLC (REGISTER+dd) */
		C = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		RLC(C);
		writeMem(m_Z80Processor.memptr.w, C);
		break;
	case 0x02:		/* LD D,RLC (REGISTER+dd) */
		D = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		RLC(D);
		writeMem(m_Z80Processor.memptr.w, D);
		break;
	case 0x03:		/* LD E,RLC (REGISTER+dd) */
		E = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		RLC(E);
		writeMem(m_Z80Processor.memptr.w, E);
		break;
	case 0x04:		/* LD H,RLC (REGISTER+dd) */
		H = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		RLC(H);
		writeMem(m_Z80Processor.memptr.w, H);
		break;
	case 0x05:		/* LD L,RLC (REGISTER+dd) */
		L = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		RLC(L);
		writeMem(m_Z80Processor.memptr.w, L);
		break;
	case 0x06:		/* RLC (REGISTER+dd) */
		bytetemp = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		RLC(bytetemp);
		writeMem(m_Z80Processor.memptr.w, bytetemp);
		break;
	case 0x07:		/* LD A,RLC (REGISTER+dd) */
		A = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		RLC(A);
		writeMem(m_Z80Processor.memptr.w, A);
		break;
	case 0x08:		/* LD B,RRC (REGISTER+dd) */
		B = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		RRC(B);
		writeMem(m_Z80Processor.memptr.w, B);
		break;
	case 0x09:		/* LD C,RRC (REGISTER+dd) */
		C = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		RRC(C);
		writeMem(m_Z80Processor.memptr.w, C);
		break;
	case 0x0a:		/* LD D,RRC (REGISTER+dd) */
		D = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		RRC(D);
		writeMem(m_Z80Processor.memptr.w, D);
		break;
	case 0x0b:		/* LD E,RRC (REGISTER+dd) */
		E = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		RRC(E);
		writeMem(m_Z80Processor.memptr.w, E);
		break;
	case 0x0c:		/* LD H,RRC (REGISTER+dd) */
		H = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		RRC(H);
		writeMem(m_Z80Processor.memptr.w, H);
		break;
	case 0x0d:		/* LD L,RRC (REGISTER+dd) */
		L = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		RRC(L);
		writeMem(m_Z80Processor.memptr.w, L);
		break;
	case 0x0e:		/* RRC (REGISTER+dd) */
		bytetemp = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		RRC(bytetemp);
		writeMem(m_Z80Processor.memptr.w, bytetemp);
		break;
	case 0x0f:		/* LD A,RRC (REGISTER+dd) */
		A = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		RRC(A);
		writeMem(m_Z80Processor.memptr.w, A);
		break;
	case 0x10:		/* LD B,RL (REGISTER+dd) */
		B = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		RL(B);
		writeMem(m_Z80Processor.memptr.w, B);
		break;
	case 0x11:		/* LD C,RL (REGISTER+dd) */
		C = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		RL(C);
		writeMem(m_Z80Processor.memptr.w, C);
		break;
	case 0x12:		/* LD D,RL (REGISTER+dd) */
		D = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		RL(D);
		writeMem(m_Z80Processor.memptr.w, D);
		break;
	case 0x13:		/* LD E,RL (REGISTER+dd) */
		E = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		RL(E);
		writeMem(m_Z80Processor.memptr.w, E);
		break;
	case 0x14:		/* LD H,RL (REGISTER+dd) */
		H = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		RL(H);
		writeMem(m_Z80Processor.memptr.w, H);
		break;
	case 0x15:		/* LD L,RL (REGISTER+dd) */
		L = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		RL(L);
		writeMem(m_Z80Processor.memptr.w, L);
		break;
	case 0x16:		/* RL (REGISTER+dd) */
		bytetemp = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		RL(bytetemp);
		writeMem(m_Z80Processor.memptr.w, bytetemp);
		break;
	case 0x17:		/* LD A,RL (REGISTER+dd) */
		A = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		RL(A);
		writeMem(m_Z80Processor.memptr.w, A);
		break;
	case 0x18:		/* LD B,RR (REGISTER+dd) */
		B = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		RR(B);
		writeMem(m_Z80Processor.memptr.w, B);
		break;
	case 0x19:		/* LD C,RR (REGISTER+dd) */
		C = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		RR(C);
		writeMem(m_Z80Processor.memptr.w, C);
		break;
	case 0x1a:		/* LD D,RR (REGISTER+dd) */
		D = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		RR(D);
		writeMem(m_Z80Processor.memptr.w, D);
		break;
	case 0x1b:		/* LD E,RR (REGISTER+dd) */
		E = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		RR(E);
		writeMem(m_Z80Processor.memptr.w, E);
		break;
	case 0x1c:		/* LD H,RR (REGISTER+dd) */
		H = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		RR(H);
		writeMem(m_Z80Processor.memptr.w, H);
		break;
	case 0x1d:		/* LD L,RR (REGISTER+dd) */
		L = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		RR(L);
		writeMem(m_Z80Processor.memptr.w, L);
		break;
	case 0x1e:		/* RR (REGISTER+dd) */
		bytetemp = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		RR(bytetemp);
		writeMem(m_Z80Processor.memptr.w, bytetemp);
		break;
	case 0x1f:		/* LD A,RR (REGISTER+dd) */
		A = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		RR(A);
		writeMem(m_Z80Processor.memptr.w, A);
		break;
	case 0x20:		/* LD B,SLA (REGISTER+dd) */
		B = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		SLA(B);
		writeMem(m_Z80Processor.memptr.w, B);
		break;
	case 0x21:		/* LD C,SLA (REGISTER+dd) */
		C = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		SLA(C);
		writeMem(m_Z80Processor.memptr.w, C);
		break;
	case 0x22:		/* LD D,SLA (REGISTER+dd) */
		D = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		SLA(D);
		writeMem(m_Z80Processor.memptr.w, D);
		break;
	case 0x23:		/* LD E,SLA (REGISTER+dd) */
		E = readMem(m_Z80Processor.memptr.w);
		SLA(E);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, E);
		break;
	case 0x24:		/* LD H,SLA (REGISTER+dd) */
		H = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		SLA(H);
		writeMem(m_Z80Processor.memptr.w, H);
		break;
	case 0x25:		/* LD L,SLA (REGISTER+dd) */
		L = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		SLA(L);
		writeMem(m_Z80Processor.memptr.w, L);
		break;
	case 0x26:		/* SLA (REGISTER+dd) */
		bytetemp = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		SLA(bytetemp);
		writeMem(m_Z80Processor.memptr.w, bytetemp);
		break;
	case 0x27:		/* LD A,SLA (REGISTER+dd) */
		A = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		SLA(A);
		writeMem(m_Z80Processor.memptr.w, A);
		break;
	case 0x28:		/* LD B,SRA (REGISTER+dd) */
		B = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		SRA(B);
		writeMem(m_Z80Processor.memptr.w, B);
		break;
	case 0x29:		/* LD C,SRA (REGISTER+dd) */
		C = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		SRA(C);
		writeMem(m_Z80Processor.memptr.w, C);
		break;
	case 0x2a:		/* LD D,SRA (REGISTER+dd) */
		D = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		SRA(D);
		writeMem(m_Z80Processor.memptr.w, D);
		break;
	case 0x2b:		/* LD E,SRA (REGISTER+dd) */
		E = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		SRA(E);
		writeMem(m_Z80Processor.memptr.w, E);
		break;
	case 0x2c:		/* LD H,SRA (REGISTER+dd) */
		H = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		SRA(H);
		writeMem(m_Z80Processor.memptr.w, H);
		break;
	case 0x2d:		/* LD L,SRA (REGISTER+dd) */
		L = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		SRA(L);
		writeMem(m_Z80Processor.memptr.w, L);
		break;
	case 0x2e:		/* SRA (REGISTER+dd) */
		bytetemp = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		SRA(bytetemp);
		writeMem(m_Z80Processor.memptr.w, bytetemp);
		break;
	case 0x2f:		/* LD A,SRA (REGISTER+dd) */
		A = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		SRA(A);
		writeMem(m_Z80Processor.memptr.w, A);
		break;
	case 0x30:		/* LD B,SLL (REGISTER+dd) */
		B = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		SLL(B);
		writeMem(m_Z80Processor.memptr.w, B);
		break;
	case 0x31:		/* LD C,SLL (REGISTER+dd) */
		C = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		SLL(C);
		writeMem(m_Z80Processor.memptr.w, C);
		break;
	case 0x32:		/* LD D,SLL (REGISTER+dd) */
		D = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		SLL(D);
		writeMem(m_Z80Processor.memptr.w, D);
		break;
	case 0x33:		/* LD E,SLL (REGISTER+dd) */
		E = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		SLL(E);
		writeMem(m_Z80Processor.memptr.w, E);
		break;
	case 0x34:		/* LD H,SLL (REGISTER+dd) */
		H = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		SLL(H);
		writeMem(m_Z80Processor.memptr.w, H);
		break;
	case 0x35:		/* LD L,SLL (REGISTER+dd) */
		L = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		SLL(L);
		writeMem(m_Z80Processor.memptr.w, L);
		break;
	case 0x36:		/* SLL (REGISTER+dd) */
		bytetemp = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		SLL(bytetemp);
		writeMem(m_Z80Processor.memptr.w, bytetemp);
		break;
	case 0x37:		/* LD A,SLL (REGISTER+dd) */
		A = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		SLL(A);
		writeMem(m_Z80Processor.memptr.w, A);
		break;
	case 0x38:		/* LD B,SRL (REGISTER+dd) */
		B = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		SRL(B);
		writeMem(m_Z80Processor.memptr.w, B);
		break;
	case 0x39:		/* LD C,SRL (REGISTER+dd) */
		C = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		SRL(C);
		writeMem(m_Z80Processor.memptr.w, C);
		break;
	case 0x3a:		/* LD D,SRL (REGISTER+dd) */
		D = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		SRL(D);
		writeMem(m_Z80Processor.memptr.w, D);
		break;
	case 0x3b:		/* LD E,SRL (REGISTER+dd) */
		E = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		SRL(E);
		writeMem(m_Z80Processor.memptr.w, E);
		break;
	case 0x3c:		/* LD H,SRL (REGISTER+dd) */
		H = readMem(m_Z80Processor.memptr.w);
		SRL(H);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, H);
		break;
	case 0x3d:		/* LD L,SRL (REGISTER+dd) */
		L = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		SRL(L);
		writeMem(m_Z80Processor.memptr.w, L);
		break;
	case 0x3e:		/* SRL (REGISTER+dd) */
		bytetemp = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		SRL(bytetemp);
		writeMem(m_Z80Processor.memptr.w, bytetemp);
		break;
	case 0x3f:		/* LD A,SRL (REGISTER+dd) */
		A = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		SRL(A);
		writeMem(m_Z80Processor.memptr.w, A);
		break;
	case 0x40:
	case 0x41:
	case 0x42:
	case 0x43:
	case 0x44:
	case 0x45:
	case 0x46:
	case 0x47:		/* BIT 0,(REGISTER+dd) */
		bytetemp = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		BIT_MEMPTR(0, bytetemp);
		break;
	case 0x48:
	case 0x49:
	case 0x4a:
	case 0x4b:
	case 0x4c:
	case 0x4d:
	case 0x4e:
	case 0x4f:		/* BIT 1,(REGISTER+dd) */
		bytetemp = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		BIT_MEMPTR(1, bytetemp);
		break;
	case 0x50:
	case 0x51:
	case 0x52:
	case 0x53:
	case 0x54:
	case 0x55:
	case 0x56:
	case 0x57:		/* BIT 2,(REGISTER+dd) */
		bytetemp = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		BIT_MEMPTR(2, bytetemp);
		break;
	case 0x58:
	case 0x59:
	case 0x5a:
	case 0x5b:
	case 0x5c:
	case 0x5d:
	case 0x5e:
	case 0x5f:		/* BIT 3,(REGISTER+dd) */
		bytetemp = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		BIT_MEMPTR(3, bytetemp);
		break;
	case 0x60:
	case 0x61:
	case 0x62:
	case 0x63:
	case 0x64:
	case 0x65:
	case 0x66:
	case 0x67:		/* BIT 4,(REGISTER+dd) */
		bytetemp = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		BIT_MEMPTR(4, bytetemp);
		break;
	case 0x68:
	case 0x69:
	case 0x6a:
	case 0x6b:
	case 0x6c:
	case 0x6d:
	case 0x6e:
	case 0x6f:		/* BIT 5,(REGISTER+dd) */
		bytetemp = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		BIT_MEMPTR(5, bytetemp);
		break;
	case 0x70:
	case 0x71:
	case 0x72:
	case 0x73:
	case 0x74:
	case 0x75:
	case 0x76:
	case 0x77:		/* BIT 6,(REGISTER+dd) */
		bytetemp = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		BIT_MEMPTR(6, bytetemp);
		break;
	case 0x78:
	case 0x79:
	case 0x7a:
	case 0x7b:
	case 0x7c:
	case 0x7d:
	case 0x7e:
	case 0x7f:		/* BIT 7,(REGISTER+dd) */
		bytetemp = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		BIT_MEMPTR(7, bytetemp);
		break;
	case 0x80:		/* LD B,RES 0,(REGISTER+dd) */
		B = readMem(m_Z80Processor.memptr.w) & 0xfe;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, B);
		break;
	case 0x81:		/* LD C,RES 0,(REGISTER+dd) */
		C = readMem(m_Z80Processor.memptr.w) & 0xfe;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, C);
		break;
	case 0x82:		/* LD D,RES 0,(REGISTER+dd) */
		D = readMem(m_Z80Processor.memptr.w) & 0xfe;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, D);
		break;
	case 0x83:		/* LD E,RES 0,(REGISTER+dd) */
		E = readMem(m_Z80Processor.memptr.w) & 0xfe;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, E);
		break;
	case 0x84:		/* LD H,RES 0,(REGISTER+dd) */
		H = readMem(m_Z80Processor.memptr.w) & 0xfe;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, H);
		break;
	case 0x85:		/* LD L,RES 0,(REGISTER+dd) */
		L = readMem(m_Z80Processor.memptr.w) & 0xfe;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, L);
		break;
	case 0x86:		/* RES 0,(REGISTER+dd) */
		bytetemp = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, bytetemp & 0xfe);
		break;
	case 0x87:		/* LD A,RES 0,(REGISTER+dd) */
		A = readMem(m_Z80Processor.memptr.w) & 0xfe;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, A);
		break;
	case 0x88:		/* LD B,RES 1,(REGISTER+dd) */
		B = readMem(m_Z80Processor.memptr.w) & 0xfd;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, B);
		break;
	case 0x89:		/* LD C,RES 1,(REGISTER+dd) */
		C = readMem(m_Z80Processor.memptr.w) & 0xfd;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, C);
		break;
	case 0x8a:		/* LD D,RES 1,(REGISTER+dd) */
		D = readMem(m_Z80Processor.memptr.w) & 0xfd;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, D);
		break;
	case 0x8b:		/* LD E,RES 1,(REGISTER+dd) */
		E = readMem(m_Z80Processor.memptr.w) & 0xfd;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, E);
		break;
	case 0x8c:		/* LD H,RES 1,(REGISTER+dd) */
		H = readMem(m_Z80Processor.memptr.w) & 0xfd;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, H);
		break;
	case 0x8d:		/* LD L,RES 1,(REGISTER+dd) */
		L = readMem(m_Z80Processor.memptr.w) & 0xfd;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, L);
		break;
	case 0x8e:		/* RES 1,(REGISTER+dd) */
		bytetemp = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, bytetemp & 0xfd);
		break;
	case 0x8f:		/* LD A,RES 1,(REGISTER+dd) */
		A = readMem(m_Z80Processor.memptr.w) & 0xfd;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, A);
		break;
	case 0x90:		/* LD B,RES 2,(REGISTER+dd) */
		B = readMem(m_Z80Processor.memptr.w) & 0xfb;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, B);
		break;
	case 0x91:		/* LD C,RES 2,(REGISTER+dd) */
		C = readMem(m_Z80Processor.memptr.w) & 0xfb;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, C);
		break;
	case 0x92:		/* LD D,RES 2,(REGISTER+dd) */
		D = readMem(m_Z80Processor.memptr.w) & 0xfb;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, D);
		break;
	case 0x93:		/* LD E,RES 2,(REGISTER+dd) */
		E = readMem(m_Z80Processor.memptr.w) & 0xfb;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, E);
		break;
	case 0x94:		/* LD H,RES 2,(REGISTER+dd) */
		H = readMem(m_Z80Processor.memptr.w) & 0xfb;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, H);
		break;
	case 0x95:		/* LD L,RES 2,(REGISTER+dd) */
		L = readMem(m_Z80Processor.memptr.w) & 0xfb;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, L);
		break;
	case 0x96:		/* RES 2,(REGISTER+dd) */
		bytetemp = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, bytetemp & 0xfb);
		break;
	case 0x97:		/* LD A,RES 2,(REGISTER+dd) */
		A = readMem(m_Z80Processor.memptr.w) & 0xfb;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, A);
		break;
	case 0x98:		/* LD B,RES 3,(REGISTER+dd) */
		B = readMem(m_Z80Processor.memptr.w) & 0xf7;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, B);
		break;
	case 0x99:		/* LD C,RES 3,(REGISTER+dd) */
		C = readMem(m_Z80Processor.memptr.w) & 0xf7;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, C);
		break;
	case 0x9a:		/* LD D,RES 3,(REGISTER+dd) */
		D = readMem(m_Z80Processor.memptr.w) & 0xf7;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, D);
		break;
	case 0x9b:		/* LD E,RES 3,(REGISTER+dd) */
		E = readMem(m_Z80Processor.memptr.w) & 0xf7;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, E);
		break;
	case 0x9c:		/* LD H,RES 3,(REGISTER+dd) */
		H = readMem(m_Z80Processor.memptr.w) & 0xf7;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, H);
		break;
	case 0x9d:		/* LD L,RES 3,(REGISTER+dd) */
		L = readMem(m_Z80Processor.memptr.w) & 0xf7;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, L);
		break;
	case 0x9e:		/* RES 3,(REGISTER+dd) */
		bytetemp = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, bytetemp & 0xf7);
		break;
	case 0x9f:		/* LD A,RES 3,(REGISTER+dd) */
		A = readMem(m_Z80Processor.memptr.w) & 0xf7;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, A);
		break;
	case 0xa0:		/* LD B,RES 4,(REGISTER+dd) */
		B = readMem(m_Z80Processor.memptr.w) & 0xef;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, B);
		break;
	case 0xa1:		/* LD C,RES 4,(REGISTER+dd) */
		C = readMem(m_Z80Processor.memptr.w) & 0xef;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, C);
		break;
	case 0xa2:		/* LD D,RES 4,(REGISTER+dd) */
		D = readMem(m_Z80Processor.memptr.w) & 0xef;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, D);
		break;
	case 0xa3:		/* LD E,RES 4,(REGISTER+dd) */
		E = readMem(m_Z80Processor.memptr.w) & 0xef;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, E);
		break;
	case 0xa4:		/* LD H,RES 4,(REGISTER+dd) */
		H = readMem(m_Z80Processor.memptr.w) & 0xef;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, H);
		break;
	case 0xa5:		/* LD L,RES 4,(REGISTER+dd) */
		L = readMem(m_Z80Processor.memptr.w) & 0xef;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, L);
		break;
	case 0xa6:		/* RES 4,(REGISTER+dd) */
		bytetemp = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, bytetemp & 0xef);
		break;
	case 0xa7:		/* LD A,RES 4,(REGISTER+dd) */
		A = readMem(m_Z80Processor.memptr.w) & 0xef;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, A);
		break;
	case 0xa8:		/* LD B,RES 5,(REGISTER+dd) */
		B = readMem(m_Z80Processor.memptr.w) & 0xdf;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, B);
		break;
	case 0xa9:		/* LD C,RES 5,(REGISTER+dd) */
		C = readMem(m_Z80Processor.memptr.w) & 0xdf;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, C);
		break;
	case 0xaa:		/* LD D,RES 5,(REGISTER+dd) */
		D = readMem(m_Z80Processor.memptr.w) & 0xdf;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, D);
		break;
	case 0xab:		/* LD E,RES 5,(REGISTER+dd) */
		E = readMem(m_Z80Processor.memptr.w) & 0xdf;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, E);
		break;
	case 0xac:		/* LD H,RES 5,(REGISTER+dd) */
		H = readMem(m_Z80Processor.memptr.w) & 0xdf;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, H);
		break;
	case 0xad:		/* LD L,RES 5,(REGISTER+dd) */
		L = readMem(m_Z80Processor.memptr.w) & 0xdf;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, L);
		break;
	case 0xae:		/* RES 5,(REGISTER+dd) */
		bytetemp = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, bytetemp & 0xdf);
		break;
	case 0xaf:		/* LD A,RES 5,(REGISTER+dd) */
		A = readMem(m_Z80Processor.memptr.w) & 0xdf;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, A);
		break;
	case 0xb0:		/* LD B,RES 6,(REGISTER+dd) */
		B = readMem(m_Z80Processor.memptr.w) & 0xbf;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, B);
		break;
	case 0xb1:		/* LD C,RES 6,(REGISTER+dd) */
		C = readMem(m_Z80Processor.memptr.w) & 0xbf;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, C);
		break;
	case 0xb2:		/* LD D,RES 6,(REGISTER+dd) */
		D = readMem(m_Z80Processor.memptr.w) & 0xbf;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, D);
		break;
	case 0xb3:		/* LD E,RES 6,(REGISTER+dd) */
		E = readMem(m_Z80Processor.memptr.w) & 0xbf;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, E);
		break;
	case 0xb4:		/* LD H,RES 6,(REGISTER+dd) */
		H = readMem(m_Z80Processor.memptr.w) & 0xbf;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, H);
		break;
	case 0xb5:		/* LD L,RES 6,(REGISTER+dd) */
		L = readMem(m_Z80Processor.memptr.w) & 0xbf;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, L);
		break;
	case 0xb6:		/* RES 6,(REGISTER+dd) */
		bytetemp = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, bytetemp & 0xbf);
		break;
	case 0xb7:		/* LD A,RES 6,(REGISTER+dd) */
		A = readMem(m_Z80Processor.memptr.w) & 0xbf;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, A);
		break;
	case 0xb8:		/* LD B,RES 7,(REGISTER+dd) */
		B = readMem(m_Z80Processor.memptr.w) & 0x7f;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, B);
		break;
	case 0xb9:		/* LD C,RES 7,(REGISTER+dd) */
		C = readMem(m_Z80Processor.memptr.w) & 0x7f;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, C);
		break;
	case 0xba:		/* LD D,RES 7,(REGISTER+dd) */
		D = readMem(m_Z80Processor.memptr.w) & 0x7f;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, D);
		break;
	case 0xbb:		/* LD E,RES 7,(REGISTER+dd) */
		E = readMem(m_Z80Processor.memptr.w) & 0x7f;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, E);
		break;
	case 0xbc:		/* LD H,RES 7,(REGISTER+dd) */
		H = readMem(m_Z80Processor.memptr.w) & 0x7f;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, H);
		break;
	case 0xbd:		/* LD L,RES 7,(REGISTER+dd) */
		L = readMem(m_Z80Processor.memptr.w) & 0x7f;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, L);
		break;
	case 0xbe:		/* RES 7,(REGISTER+dd) */
		bytetemp = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, bytetemp & 0x7f);
		break;
	case 0xbf:		/* LD A,RES 7,(REGISTER+dd) */
		A = readMem(m_Z80Processor.memptr.w) & 0x7f;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, A);
		break;
	case 0xc0:		/* LD B,SET 0,(REGISTER+dd) */
		B = readMem(m_Z80Processor.memptr.w) | 0x01;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, B);
		break;
	case 0xc1:		/* LD C,SET 0,(REGISTER+dd) */
		C = readMem(m_Z80Processor.memptr.w) | 0x01;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, C);
		break;
	case 0xc2:		/* LD D,SET 0,(REGISTER+dd) */
		D = readMem(m_Z80Processor.memptr.w) | 0x01;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, D);
		break;
	case 0xc3:		/* LD E,SET 0,(REGISTER+dd) */
		E = readMem(m_Z80Processor.memptr.w) | 0x01;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, E);
		break;
	case 0xc4:		/* LD H,SET 0,(REGISTER+dd) */
		H = readMem(m_Z80Processor.memptr.w) | 0x01;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, H);
		break;
	case 0xc5:		/* LD L,SET 0,(REGISTER+dd) */
		L = readMem(m_Z80Processor.memptr.w) | 0x01;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, L);
		break;
	case 0xc6:		/* SET 0,(REGISTER+dd) */
		bytetemp = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, bytetemp | 0x01);
		break;
	case 0xc7:		/* LD A,SET 0,(REGISTER+dd) */
		A = readMem(m_Z80Processor.memptr.w) | 0x01;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, A);
		break;
	case 0xc8:		/* LD B,SET 1,(REGISTER+dd) */
		B = readMem(m_Z80Processor.memptr.w) | 0x02;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, B);
		break;
	case 0xc9:		/* LD C,SET 1,(REGISTER+dd) */
		C = readMem(m_Z80Processor.memptr.w) | 0x02;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, C);
		break;
	case 0xca:		/* LD D,SET 1,(REGISTER+dd) */
		D = readMem(m_Z80Processor.memptr.w) | 0x02;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, D);
		break;
	case 0xcb:		/* LD E,SET 1,(REGISTER+dd) */
		E = readMem(m_Z80Processor.memptr.w) | 0x02;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, E);
		break;
	case 0xcc:		/* LD H,SET 1,(REGISTER+dd) */
		H = readMem(m_Z80Processor.memptr.w) | 0x02;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, H);
		break;
	case 0xcd:		/* LD L,SET 1,(REGISTER+dd) */
		L = readMem(m_Z80Processor.memptr.w) | 0x02;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, L);
		break;
	case 0xce:		/* SET 1,(REGISTER+dd) */
		bytetemp = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, bytetemp | 0x02);
		break;
	case 0xcf:		/* LD A,SET 1,(REGISTER+dd) */
		A = readMem(m_Z80Processor.memptr.w) | 0x02;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, A);
		break;
	case 0xd0:		/* LD B,SET 2,(REGISTER+dd) */
		B = readMem(m_Z80Processor.memptr.w) | 0x04;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, B);
		break;
	case 0xd1:		/* LD C,SET 2,(REGISTER+dd) */
		C = readMem(m_Z80Processor.memptr.w) | 0x04;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, C);
		break;
	case 0xd2:		/* LD D,SET 2,(REGISTER+dd) */
		D = readMem(m_Z80Processor.memptr.w) | 0x04;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, D);
		break;
	case 0xd3:		/* LD E,SET 2,(REGISTER+dd) */
		E = readMem(m_Z80Processor.memptr.w) | 0x04;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, E);
		break;
	case 0xd4:		/* LD H,SET 2,(REGISTER+dd) */
		H = readMem(m_Z80Processor.memptr.w) | 0x04;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, H);
		break;
	case 0xd5:		/* LD L,SET 2,(REGISTER+dd) */
		L = readMem(m_Z80Processor.memptr.w) | 0x04;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, L);
		break;
	case 0xd6:		/* SET 2,(REGISTER+dd) */
		bytetemp = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, bytetemp | 0x04);
		break;
	case 0xd7:		/* LD A,SET 2,(REGISTER+dd) */
		A = readMem(m_Z80Processor.memptr.w) | 0x04;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, A);
		break;
	case 0xd8:		/* LD B,SET 3,(REGISTER+dd) */
		B = readMem(m_Z80Processor.memptr.w) | 0x08;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, B);
		break;
	case 0xd9:		/* LD C,SET 3,(REGISTER+dd) */
		C = readMem(m_Z80Processor.memptr.w) | 0x08;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, C);
		break;
	case 0xda:		/* LD D,SET 3,(REGISTER+dd) */
		D = readMem(m_Z80Processor.memptr.w) | 0x08;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, D);
		break;
	case 0xdb:		/* LD E,SET 3,(REGISTER+dd) */
		E = readMem(m_Z80Processor.memptr.w) | 0x08;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, E);
		break;
	case 0xdc:		/* LD H,SET 3,(REGISTER+dd) */
		H = readMem(m_Z80Processor.memptr.w) | 0x08;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, H);
		break;
	case 0xdd:		/* LD L,SET 3,(REGISTER+dd) */
		L = readMem(m_Z80Processor.memptr.w) | 0x08;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, L);
		break;
	case 0xde:		/* SET 3,(REGISTER+dd) */
		bytetemp = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, bytetemp | 0x08);
		break;
	case 0xdf:		/* LD A,SET 3,(REGISTER+dd) */
		A = readMem(m_Z80Processor.memptr.w) | 0x08;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, A);
		break;
	case 0xe0:		/* LD B,SET 4,(REGISTER+dd) */
		B = readMem(m_Z80Processor.memptr.w) | 0x10;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, B);
		break;
	case 0xe1:		/* LD C,SET 4,(REGISTER+dd) */
		C = readMem(m_Z80Processor.memptr.w) | 0x10;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, C);
		break;
	case 0xe2:		/* LD D,SET 4,(REGISTER+dd) */
		D = readMem(m_Z80Processor.memptr.w) | 0x10;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, D);
		break;
	case 0xe3:		/* LD E,SET 4,(REGISTER+dd) */
		E = readMem(m_Z80Processor.memptr.w) | 0x10;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, E);
		break;
	case 0xe4:		/* LD H,SET 4,(REGISTER+dd) */
		H = readMem(m_Z80Processor.memptr.w) | 0x10;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, H);
		break;
	case 0xe5:		/* LD L,SET 4,(REGISTER+dd) */
		L = readMem(m_Z80Processor.memptr.w) | 0x10;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, L);
		break;
	case 0xe6:		/* SET 4,(REGISTER+dd) */
		bytetemp = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, bytetemp | 0x10);
		break;
	case 0xe7:		/* LD A,SET 4,(REGISTER+dd) */
		A = readMem(m_Z80Processor.memptr.w) | 0x10;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, A);
		break;
	case 0xe8:		/* LD B,SET 5,(REGISTER+dd) */
		B = readMem(m_Z80Processor.memptr.w) | 0x20;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, B);
		break;
	case 0xe9:		/* LD C,SET 5,(REGISTER+dd) */
		C = readMem(m_Z80Processor.memptr.w) | 0x20;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, C);
		break;
	case 0xea:		/* LD D,SET 5,(REGISTER+dd) */
		D = readMem(m_Z80Processor.memptr.w) | 0x20;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, D);
		break;
	case 0xeb:		/* LD E,SET 5,(REGISTER+dd) */
		E = readMem(m_Z80Processor.memptr.w) | 0x20;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, E);
		break;
	case 0xec:		/* LD H,SET 5,(REGISTER+dd) */
		H = readMem(m_Z80Processor.memptr.w) | 0x20;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, H);
		break;
	case 0xed:		/* LD L,SET 5,(REGISTER+dd) */
		L = readMem(m_Z80Processor.memptr.w) | 0x20;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, L);
		break;
	case 0xee:		/* SET 5,(REGISTER+dd) */
		bytetemp = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, bytetemp | 0x20);
		break;
	case 0xef:		/* LD A,SET 5,(REGISTER+dd) */
		A = readMem(m_Z80Processor.memptr.w) | 0x20;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, A);
		break;
	case 0xf0:		/* LD B,SET 6,(REGISTER+dd) */
		B = readMem(m_Z80Processor.memptr.w) | 0x40;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, B);
		break;
	case 0xf1:		/* LD C,SET 6,(REGISTER+dd) */
		C = readMem(m_Z80Processor.memptr.w) | 0x40;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, C);
		break;
	case 0xf2:		/* LD D,SET 6,(REGISTER+dd) */
		D = readMem(m_Z80Processor.memptr.w) | 0x40;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, D);
		break;
	case 0xf3:		/* LD E,SET 6,(REGISTER+dd) */
		E = readMem(m_Z80Processor.memptr.w) | 0x40;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, E);
		break;
	case 0xf4:		/* LD H,SET 6,(REGISTER+dd) */
		H = readMem(m_Z80Processor.memptr.w) | 0x40;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, H);
		break;
	case 0xf5:		/* LD L,SET 6,(REGISTER+dd) */
		L = readMem(m_Z80Processor.memptr.w) | 0x40;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, L);
		break;
	case 0xf6:		/* SET 6,(REGISTER+dd) */
		bytetemp = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, bytetemp | 0x40);
		break;
	case 0xf7:		/* LD A,SET 6,(REGISTER+dd) */
		A = readMem(m_Z80Processor.memptr.w) | 0x40;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, A);
		break;
	case 0xf8:		/* LD B,SET 7,(REGISTER+dd) */
		B = readMem(m_Z80Processor.memptr.w) | 0x80;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, B);
		break;
	case 0xf9:		/* LD C,SET 7,(REGISTER+dd) */
		C = readMem(m_Z80Processor.memptr.w) | 0x80;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, C);
		break;
	case 0xfa:		/* LD D,SET 7,(REGISTER+dd) */
		D = readMem(m_Z80Processor.memptr.w) | 0x80;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, D);
		break;
	case 0xfb:		/* LD E,SET 7,(REGISTER+dd) */
		E = readMem(m_Z80Processor.memptr.w) | 0x80;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, E);
		break;
	case 0xfc:		/* LD H,SET 7,(REGISTER+dd) */
		H = readMem(m_Z80Processor.memptr.w) | 0x80;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, H);
		break;
	case 0xfd:		/* LD L,SET 7,(REGISTER+dd) */
		L = readMem(m_Z80Processor.memptr.w) | 0x80;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, L);
		break;
	case 0xfe:		/* SET 7,(REGISTER+dd) */
		bytetemp = readMem(m_Z80Processor.memptr.w);
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, bytetemp | 0x80);
		break;
	case 0xff:		/* LD A,SET 7,(REGISTER+dd) */
		A = readMem(m_Z80Processor.memptr.w) | 0x80;
		contendedAccess(m_Z80Processor.memptr.w, 1);
		writeMem(m_Z80Processor.memptr.w, A);
		break;
	}
}

void ZXSpectrum::stepZ80()
{
	BYTE opcode, opcode2, last_Q, bytetemp, nn, add, carry, bytetempl, bytetemph;
	WORD wordtemp, outtemp, intemp;
	contendedAccess(PC, 4);
	opcode = m_pZXMemory[PC];
	PC++; R++;
	last_Q = Q; /* keep Q value from previous opcode for SCF and CCF */
	Q = 0; /* preempt Q value assuming next opcode doesn't set flags */

	switch (opcode)
	{
	case 0x00:		/* NOP */
		break;
	case 0x01:		/* LD BC,nnnn */
		C = readMem(PC++);
		B = readMem(PC++);
		break;
	case 0x02:		/* LD (BC),A */
		m_Z80Processor.memptr.b.l = BC + 1;
		m_Z80Processor.memptr.b.h = A;
		writeMem(BC, A);
		break;
	case 0x03:		/* INC BC */
		contendedAccess(IR, 1); contendedAccess(IR, 1);
		BC++;
		break;
	case 0x04:		/* INC B */
		INC8(B);
		break;
	case 0x05:		/* DEC B */
		DEC8(B);
		break;
	case 0x06:		/* LD B,nn */
		B = readMem(PC++);
		break;
	case 0x07:		/* RLCA */
		A = (A << 1) | (A >> 7);
		FL = (FL & (FLAG_P | FLAG_Z | FLAG_S)) | (A & (FLAG_C | FLAG_3 | FLAG_5));
		Q = FL;
		break;
	case 0x08:		/* EX AF,AF' */
	  /* Tape saving trap: note this traps the EX AF,AF' at #04d0, not
	 #04d1 as PC has already been incremented */
	 /* 0x76 - Timex 2068 save routine in EXROM */
	 /*        if (PC == 0x04d1 || PC == 0x0077) {
	             if (tape_save_trap() == 0) break;
	     }
	     {*/
		wordtemp = AF;
		AF = AF_;
		AF_ = wordtemp;
		//        }
		break;
	case 0x09:		/* ADD HL,BC */
		contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1);
		contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1);
		ADD16(HL, BC);
		break;
	case 0x0a:		/* LD A,(BC) */
		m_Z80Processor.memptr.w = BC + 1;
		A = readMem(BC);
		break;
	case 0x0b:		/* DEC BC */
		contendedAccess(IR, 1); contendedAccess(IR, 1);
		BC--;
		break;
	case 0x0c:		/* INC C */
		INC8(C);
		break;
	case 0x0d:		/* DEC C */
		DEC8(C);
		break;
	case 0x0e:		/* LD C,nn */
		C = readMem(PC++);
		break;
	case 0x0f:		/* RRCA */
		FL = (FL & (FLAG_P | FLAG_Z | FLAG_S)) | (A & FLAG_C);
		A = (A >> 1) | (A << 7);
		FL |= (A & (FLAG_3 | FLAG_5));
		Q = FL;
		break;
	case 0x10:		/* DJNZ offset */
		contendedAccess(IR, 1);
		B--;
		if (B)
		{
			JR();
		}
		else
		{
			contendedAccess(PC, 3);
			PC++;
		}
		break;
	case 0x11:		/* LD DE,nnnn */
		E = readMem(PC++);
		D = readMem(PC++);
		break;
	case 0x12:		/* LD (DE),A */
		m_Z80Processor.memptr.b.l = DE + 1;
		m_Z80Processor.memptr.b.h = A;
		writeMem(DE, A);
		break;
	case 0x13:		/* INC DE */
		contendedAccess(IR, 1); contendedAccess(IR, 1);
		DE++;
		break;
	case 0x14:		/* INC D */
		INC8(D);
		break;
	case 0x15:		/* DEC D */
		DEC8(D);
		break;
	case 0x16:		/* LD D,nn */
		D = readMem(PC++);
		break;
	case 0x17:		/* RLA */
		bytetemp = A;
		A = (A << 1) | (FL & FLAG_C);
		FL = (FL & (FLAG_P | FLAG_Z | FLAG_S)) | (A & (FLAG_3 | FLAG_5)) | (bytetemp >> 7);
		Q = FL;
		break;
	case 0x18:		/* JR offset */
		JR();
		break;
	case 0x19:		/* ADD HL,DE */
		contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1);
		contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1);
		ADD16(HL, DE);
		break;
	case 0x1a:		/* LD A,(DE) */
		m_Z80Processor.memptr.w = DE + 1;
		A = readMem(DE);
		break;
	case 0x1b:		/* DEC DE */
		contendedAccess(IR, 1); contendedAccess(IR, 1)
		    DE--;
		break;
	case 0x1c:		/* INC E */
		INC8(E);
		break;
	case 0x1d:		/* DEC E */
		DEC8(E);
		break;
	case 0x1e:		/* LD E,nn */
		E = readMem(PC++);
		break;
	case 0x1f:		/* RRA */
		bytetemp = A;
		A = (A >> 1) | (FL << 7);
		FL = (FL & (FLAG_P | FLAG_Z | FLAG_S)) | (A & (FLAG_3 | FLAG_5)) | (bytetemp & FLAG_C);
		Q = FL;
		break;
	case 0x20:		/* JR NZ,offset */
		if (!(FL & FLAG_Z))
		{
			JR();
		}
		else
		{
			contendedAccess(PC, 3);
			PC++;
		}
		break;
	case 0x21:		/* LD HL,nnnn */
		L = readMem(PC++);
		H = readMem(PC++);
		break;
	case 0x22:		/* LD (nnnn),HL */
		LD16_NNRR(L, H);
		break;
	case 0x23:		/* INC HL */
		contendedAccess(IR, 1); contendedAccess(IR, 1);
		HL++;
		break;
	case 0x24:		/* INC H */
		INC8(H);
		break;
	case 0x25:		/* DEC H */
		DEC8(H);
		break;
	case 0x26:		/* LD H,nn */
		H = readMem(PC++);
		break;
	case 0x27:		/* DAA */
		add = 0; carry = (FL & FLAG_C);
		if ((FL & FLAG_H) || ((A & 0x0f) > 9)) add = 6;
		if (carry || (A > 0x99)) add |= 0x60;
		if (A > 0x99) carry = FLAG_C;
		if (FL & FLAG_N)
		{
			SUB(add);
		}
		else
		{
			ADD(add);
		}
		FL = (FL & ~(FLAG_C | FLAG_P)) | carry | m_parityTable[A];
		Q = FL;
		break;
	case 0x28:		/* JR Z,offset */
		if (FL & FLAG_Z)
		{
			JR();
		}
		else
		{
			contendedAccess(PC, 3);
			PC++;
		}
		break;
	case 0x29:		/* ADD HL,HL */
		contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1);
		contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1);
		ADD16(HL, HL);
		break;
	case 0x2a:		/* LD HL,(nnnn) */
		LD16_RRNN(L, H);
		break;
	case 0x2b:		/* DEC HL */
		contendedAccess(IR, 1); contendedAccess(IR, 1);
		HL--;
		break;
	case 0x2c:		/* INC L */
		INC8(L);
		break;
	case 0x2d:		/* DEC L */
		DEC8(L);
		break;
	case 0x2e:		/* LD L,nn */
		L = readMem(PC++);
		break;
	case 0x2f:		/* CPL */
		A ^= 0xff;
		FL = (FL & (FLAG_C | FLAG_P | FLAG_Z | FLAG_S)) | (A & (FLAG_3 | FLAG_5)) | (FLAG_N | FLAG_H);
		Q = FL;
		break;
	case 0x30:		/* JR NC,offset */
		if (!(FL & FLAG_C))
		{
			JR();
		}
		else
		{
			contendedAccess(PC, 3);
			PC++;
		}
		break;
	case 0x31:		/* LD SP,nnnn */
		SPL = readMem(PC++);
		SPH = readMem(PC++);
		break;
	case 0x32:		/* LD (nnnn),A */
		wordtemp = readMem(PC++);
		wordtemp |= readMem(PC++) << 8;
		m_Z80Processor.memptr.b.l = wordtemp + 1;
		m_Z80Processor.memptr.b.h = A;
		writeMem(wordtemp, A);
		break;
	case 0x33:		/* INC SP */
		contendedAccess(IR, 1); contendedAccess(IR, 1);
		SP++;
		break;
	case 0x34:		/* INC (HL) */
		bytetemp = readMem(HL);
		contendedAccess(HL, 1);
		INC8(bytetemp);
		writeMem(HL, bytetemp);
		break;
	case 0x35:		/* DEC (HL) */
		bytetemp = readMem(HL);
		contendedAccess(HL, 1);
		DEC8(bytetemp);
		writeMem(HL, bytetemp);
		break;
	case 0x36:		/* LD (HL),nn */
		writeMem(HL, readMem(PC++));
		break;
	case 0x37:		/* SCF */
		FL = (FL & (FLAG_P | FLAG_Z | FLAG_S)) | (((last_Q ^ FL) | A) & (FLAG_3 | FLAG_5)) | FLAG_C;
		Q = FL;
		break;
	case 0x38:		/* JR C,offset */
		if (FL & FLAG_C)
		{
			JR();
		}
		else
		{
			contendedAccess(PC, 3);
			PC++;
		}
		break;
	case 0x39:		/* ADD HL,SP */
		contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1);
		contendedAccess(IR, 1); contendedAccess(IR, 1); contendedAccess(IR, 1);
		ADD16(HL, SP);
		break;
	case 0x3a:		/* LD A,(nnnn) */
		m_Z80Processor.memptr.b.l = readMem(PC++);
		m_Z80Processor.memptr.b.h = readMem(PC++);
		A = readMem(m_Z80Processor.memptr.w++);
		break;
	case 0x3b:		/* DEC SP */
		contendedAccess(IR, 1); contendedAccess(IR, 1);
		SP--;
		break;
	case 0x3c:		/* INC A */
		INC8(A);
		break;
	case 0x3d:		/* DEC A */
		DEC8(A);
		break;
	case 0x3e:		/* LD A,nn */
		A = readMem(PC++);
		break;
	case 0x3f:		/* CCF */
		FL = (FL & (FLAG_P | FLAG_Z | FLAG_S)) | ((FL & FLAG_C) ? FLAG_H : FLAG_C) | (((last_Q ^ FL) | A) & (FLAG_3 | FLAG_5));
		Q = FL;
		break;
	case 0x40:		/* LD B,B */
		break;
	case 0x41:		/* LD B,C */
		B = C;
		break;
	case 0x42:		/* LD B,D */
		B = D;
		break;
	case 0x43:		/* LD B,E */
		B = E;
		break;
	case 0x44:		/* LD B,H */
		B = H;
		break;
	case 0x45:		/* LD B,L */
		B = L;
		break;
	case 0x46:		/* LD B,(HL) */
		B = readMem(HL);
		break;
	case 0x47:		/* LD B,A */
		B = A;
		break;
	case 0x48:		/* LD C,B */
		C = B;
		break;
	case 0x49:		/* LD C,C */
		break;
	case 0x4a:		/* LD C,D */
		C = D;
		break;
	case 0x4b:		/* LD C,E */
		C = E;
		break;
	case 0x4c:		/* LD C,H */
		C = H;
		break;
	case 0x4d:		/* LD C,L */
		C = L;
		break;
	case 0x4e:		/* LD C,(HL) */
		C = readMem(HL);
		break;
	case 0x4f:		/* LD C,A */
		C = A;
		break;
	case 0x50:		/* LD D,B */
		D = B;
		break;
	case 0x51:		/* LD D,C */
		D = C;
		break;
	case 0x52:		/* LD D,D */
		break;
	case 0x53:		/* LD D,E */
		D = E;
		break;
	case 0x54:		/* LD D,H */
		D = H;
		break;
	case 0x55:		/* LD D,L */
		D = L;
		break;
	case 0x56:		/* LD D,(HL) */
		D = readMem(HL);
		break;
	case 0x57:		/* LD D,A */
		D = A;
		break;
	case 0x58:		/* LD E,B */
		E = B;
		break;
	case 0x59:		/* LD E,C */
		E = C;
		break;
	case 0x5a:		/* LD E,D */
		E = D;
		break;
	case 0x5b:		/* LD E,E */
		break;
	case 0x5c:		/* LD E,H */
		E = H;
		break;
	case 0x5d:		/* LD E,L */
		E = L;
		break;
	case 0x5e:		/* LD E,(HL) */
		E = readMem(HL);
		break;
	case 0x5f:		/* LD E,A */
		E = A;
		break;
	case 0x60:		/* LD H,B */
		H = B;
		break;
	case 0x61:		/* LD H,C */
		H = C;
		break;
	case 0x62:		/* LD H,D */
		H = D;
		break;
	case 0x63:		/* LD H,E */
		H = E;
		break;
	case 0x64:		/* LD H,H */
		break;
	case 0x65:		/* LD H,L */
		H = L;
		break;
	case 0x66:		/* LD H,(HL) */
		H = readMem(HL);
		break;
	case 0x67:		/* LD H,A */
		H = A;
		break;
	case 0x68:		/* LD L,B */
		L = B;
		break;
	case 0x69:		/* LD L,C */
		L = C;
		break;
	case 0x6a:		/* LD L,D */
		L = D;
		break;
	case 0x6b:		/* LD L,E */
		L = E;
		break;
	case 0x6c:		/* LD L,H */
		L = H;
		break;
	case 0x6d:		/* LD L,L */
		break;
	case 0x6e:		/* LD L,(HL) */
		L = readMem(HL);
		break;
	case 0x6f:		/* LD L,A */
		L = A;
		break;
	case 0x70:		/* LD (HL),B */
		writeMem(HL, B);
		break;
	case 0x71:		/* LD (HL),C */
		writeMem(HL, C);
		break;
	case 0x72:		/* LD (HL),D */
		writeMem(HL, D);
		break;
	case 0x73:		/* LD (HL),E */
		writeMem(HL, E);
		break;
	case 0x74:		/* LD (HL),H */
		writeMem(HL, H);
		break;
	case 0x75:		/* LD (HL),L */
		writeMem(HL, L);
		break;
	case 0x76:		/* HALT */
		m_Z80Processor.halted = 1;
		PC--;
		break;
	case 0x77:		/* LD (HL),A */
		writeMem(HL, A);
		break;
	case 0x78:		/* LD A,B */
		A = B;
		break;
	case 0x79:		/* LD A,C */
		A = C;
		break;
	case 0x7a:		/* LD A,D */
		A = D;
		break;
	case 0x7b:		/* LD A,E */
		A = E;
		break;
	case 0x7c:		/* LD A,H */
		A = H;
		break;
	case 0x7d:		/* LD A,L */
		A = L;
		break;
	case 0x7e:		/* LD A,(HL) */
		A = readMem(HL);
		break;
	case 0x7f:		/* LD A,A */
		break;
	case 0x80:		/* ADD A,B */
		ADD(B);
		break;
	case 0x81:		/* ADD A,C */
		ADD(C);
		break;
	case 0x82:		/* ADD A,D */
		ADD(D);
		break;
	case 0x83:		/* ADD A,E */
		ADD(E);
		break;
	case 0x84:		/* ADD A,H */
		ADD(H);
		break;
	case 0x85:		/* ADD A,L */
		ADD(L);
		break;
	case 0x86:		/* ADD A,(HL) */
		bytetemp = readMem(HL);
		ADD(bytetemp);
		break;
	case 0x87:		/* ADD A,A */
		ADD(A);
		break;
	case 0x88:		/* ADC A,B */
		ADC(B);
		break;
	case 0x89:		/* ADC A,C */
		ADC(C);
		break;
	case 0x8a:		/* ADC A,D */
		ADC(D);
		break;
	case 0x8b:		/* ADC A,E */
		ADC(E);
		break;
	case 0x8c:		/* ADC A,H */
		ADC(H);
		break;
	case 0x8d:		/* ADC A,L */
		ADC(L);
		break;
	case 0x8e:		/* ADC A,(HL) */
		bytetemp = readMem(HL);
		ADC(bytetemp);
		break;
	case 0x8f:		/* ADC A,A */
		ADC(A);
		break;
	case 0x90:		/* SUB A,B */
		SUB(B);
		break;
	case 0x91:		/* SUB A,C */
		SUB(C);
		break;
	case 0x92:		/* SUB A,D */
		SUB(D);
		break;
	case 0x93:		/* SUB A,E */
		SUB(E);
		break;
	case 0x94:		/* SUB A,H */
		SUB(H);
		break;
	case 0x95:		/* SUB A,L */
		SUB(L);
		break;
	case 0x96:		/* SUB A,(HL) */
		bytetemp = readMem(HL);
		SUB(bytetemp);
		break;
	case 0x97:		/* SUB A,A */
		SUB(A);
		break;
	case 0x98:		/* SBC A,B */
		SBC(B);
		break;
	case 0x99:		/* SBC A,C */
		SBC(C);
		break;
	case 0x9a:		/* SBC A,D */
		SBC(D);
		break;
	case 0x9b:		/* SBC A,E */
		SBC(E);
		break;
	case 0x9c:		/* SBC A,H */
		SBC(H);
		break;
	case 0x9d:		/* SBC A,L */
		SBC(L);
		break;
	case 0x9e:		/* SBC A,(HL) */
		bytetemp = readMem(HL);
		SBC(bytetemp);
		break;
	case 0x9f:		/* SBC A,A */
		SBC(A);
		break;
	case 0xa0:		/* AND A,B */
		AND(B);
		break;
	case 0xa1:		/* AND A,C */
		AND(C);
		break;
	case 0xa2:		/* AND A,D */
		AND(D);
		break;
	case 0xa3:		/* AND A,E */
		AND(E);
		break;
	case 0xa4:		/* AND A,H */
		AND(H);
		break;
	case 0xa5:		/* AND A,L */
		AND(L);
		break;
	case 0xa6:		/* AND A,(HL) */
		bytetemp = readMem(HL);
		AND(bytetemp);
		break;
	case 0xa7:		/* AND A,A */
		AND(A);
		break;
	case 0xa8:		/* XOR A,B */
		XOR(B);
		break;
	case 0xa9:		/* XOR A,C */
		XOR(C);
		break;
	case 0xaa:		/* XOR A,D */
		XOR(D);
		break;
	case 0xab:		/* XOR A,E */
		XOR(E);
		break;
	case 0xac:		/* XOR A,H */
		XOR(H);
		break;
	case 0xad:		/* XOR A,L */
		XOR(L);
		break;
	case 0xae:		/* XOR A,(HL) */
		bytetemp = readMem(HL);
		XOR(bytetemp);
		break;
	case 0xaf:		/* XOR A,A */
		XOR(A);
		break;
	case 0xb0:		/* OR A,B */
		OR(B);
		break;
	case 0xb1:		/* OR A,C */
		OR(C);
		break;
	case 0xb2:		/* OR A,D */
		OR(D);
		break;
	case 0xb3:		/* OR A,E */
		OR(E);
		break;
	case 0xb4:		/* OR A,H */
		OR(H);
		break;
	case 0xb5:		/* OR A,L */
		OR(L);
		break;
	case 0xb6:		/* OR A,(HL) */
		bytetemp = readMem(HL);
		OR(bytetemp);
		break;
	case 0xb7:		/* OR A,A */
		OR(A);
		break;
	case 0xb8:		/* CP B */
		CP(B);
		break;
	case 0xb9:		/* CP C */
		CP(C);
		break;
	case 0xba:		/* CP D */
		CP(D);
		break;
	case 0xbb:		/* CP E */
		CP(E);
		break;
	case 0xbc:		/* CP H */
		CP(H);
		break;
	case 0xbd:		/* CP L */
		CP(L);
		break;
	case 0xbe:		/* CP (HL) */
		bytetemp = readMem(HL);
		CP(bytetemp);
		break;
	case 0xbf:		/* CP A */
		CP(A);
		break;
	case 0xc0:		/* RET NZ */
		contendedAccess(IR, 1);
		/*        if (PC == 0x056c || PC == 0x0112)
		        {
		            if (tape_load_trap() == 0) break;
				}*/
		if (!(FL & FLAG_Z))
		{
			RET();
		}
		break;
	case 0xc1:		/* POP BC */
		POP16(C, B);
		break;
	case 0xc2:		/* JP NZ,nnnn */
		m_Z80Processor.memptr.b.l = readMem(PC++);
		m_Z80Processor.memptr.b.h = readMem(PC);
		if (!(FL & FLAG_Z))
		{
			JP();
		}
		else
		{
			PC++;
		}
		break;
	case 0xc3:		/* JP nnnn */
		m_Z80Processor.memptr.b.l = readMem(PC++);
		m_Z80Processor.memptr.b.h = readMem(PC);
		JP();
		break;
	case 0xc4:		/* CALL NZ,nnnn */
		m_Z80Processor.memptr.b.l = readMem(PC++);
		m_Z80Processor.memptr.b.h = readMem(PC);
		if (!(FL & FLAG_Z))
		{
			CALL();
		}
		else
		{
			PC++;
		}
		break;
	case 0xc5:		/* PUSH BC */
		contendedAccess(IR, 1);
		PUSH16(C, B);
		break;
	case 0xc6:		/* ADD A,nn */
		bytetemp = readMem(PC++);
		ADD(bytetemp);
		break;
	case 0xc7:		/* RST 00 */
		contendedAccess(IR, 1);
		RST(0x00);
		break;
	case 0xc8:		/* RET Z */
		contendedAccess(IR, 1);
		if (FL & FLAG_Z)
		{
			RET();
		}
		break;
	case 0xc9:		/* RET */
		RET();
		break;
	case 0xca:		/* JP Z,nnnn */
		m_Z80Processor.memptr.b.l = readMem(PC++);
		m_Z80Processor.memptr.b.h = readMem(PC);
		if (FL & FLAG_Z)
		{
			JP();
		}
		else
		{
			PC++;
		}
		break;
	case 0xcb:		/* shift CB */
		contendedAccess(PC, 4);
		opcode2 = m_pZXMemory[PC];
		PC++;
		R++;
		stepCB(opcode2);
		break;
	case 0xcc:		/* CALL Z,nnnn */
		m_Z80Processor.memptr.b.l = readMem(PC++);
		m_Z80Processor.memptr.b.h = readMem(PC);
		if (FL & FLAG_Z)
		{
			CALL();
		}
		else
		{
			PC++;
		}
		break;
	case 0xcd:		/* CALL nnnn */
		m_Z80Processor.memptr.b.l = readMem(PC++);
		m_Z80Processor.memptr.b.h = readMem(PC);
		CALL();
		break;
	case 0xce:		/* ADC A,nn */
		bytetemp = readMem(PC++);
		ADC(bytetemp);
		break;
	case 0xcf:		/* RST 8 */
		contendedAccess(IR, 1);
		RST(0x08);
		break;
	case 0xd0:		/* RET NC */
		contendedAccess(IR, 1);
		if (!(FL & FLAG_C))
		{
			RET();
		}
		break;
	case 0xd1:		/* POP DE */
		POP16(E, D);
		break;
	case 0xd2:		/* JP NC,nnnn */
		m_Z80Processor.memptr.b.l = readMem(PC++);
		m_Z80Processor.memptr.b.h = readMem(PC);
		if (!(FL & FLAG_C))
		{
			JP();
		}
		else
		{
			PC++;
		}
		break;
	case 0xd3:		/* OUT (nn),A */
		nn = readMem(PC++);
		outtemp = nn | (A << 8);
		m_Z80Processor.memptr.b.h = A;
		m_Z80Processor.memptr.b.l = (nn + 1);
		writePort(outtemp, A);
		break;
	case 0xd4:		/* CALL NC,nnnn */
		m_Z80Processor.memptr.b.l = readMem(PC++);
		m_Z80Processor.memptr.b.h = readMem(PC);
		if (!(FL & FLAG_C))
		{
			CALL();
		}
		else
		{
			PC++;
		}
		break;
	case 0xd5:		/* PUSH DE */
		contendedAccess(IR, 1);
		PUSH16(E, D);
		break;
	case 0xd6:		/* SUB nn */
		bytetemp = readMem(PC++);
		SUB(bytetemp);
		break;
	case 0xd7:		/* RST 10 */
		contendedAccess(IR, 1);
		RST(0x10);
		break;
	case 0xd8:		/* RET C */
		contendedAccess(IR, 1);
		if (FL & FLAG_C)
		{
			RET();
		}
		break;
	case 0xd9:		/* EXX */
		wordtemp = BC; BC = BC_; BC_ = wordtemp;
		wordtemp = DE; DE = DE_; DE_ = wordtemp;
		wordtemp = HL; HL = HL_; HL_ = wordtemp;
		break;
	case 0xda:		/* JP C,nnnn */
		m_Z80Processor.memptr.b.l = readMem(PC++);
		m_Z80Processor.memptr.b.h = readMem(PC);
		if (FL & FLAG_C)
		{
			JP();
		}
		else
		{
			PC++;
		}
		break;
	case 0xdb:		/* IN A,(nn) */
		intemp = readMem(PC++) + (A << 8);
		A = readPort(intemp);
		m_Z80Processor.memptr.w = intemp + 1;
		break;
	case 0xdc:		/* CALL C,nnnn */
		m_Z80Processor.memptr.b.l = readMem(PC++);
		m_Z80Processor.memptr.b.h = readMem(PC);
		if (FL & FLAG_C)
		{
			CALL();
		}
		else
		{
			PC++;
		}
		break;
	case 0xdd:		/* shift DD */
		contendedAccess(PC, 4);
		opcode2 = m_pZXMemory[PC];
		PC++;
		R++;
		/*isDDFDok = */stepDD(opcode2);
		break;
	case 0xde:		/* SBC A,nn */
		bytetemp = readMem(PC++);
		SBC(bytetemp);
		break;
	case 0xdf:		/* RST 18 */
		contendedAccess(IR, 1);
		RST(0x18);
		break;
	case 0xe0:		/* RET PO */
		contendedAccess(IR, 1);
		if (!(FL & FLAG_P))
		{
			RET();
		}
		break;
	case 0xe1:		/* POP HL */
		POP16(L, H);
		break;
	case 0xe2:		/* JP PO,nnnn */
		m_Z80Processor.memptr.b.l = readMem(PC++);
		m_Z80Processor.memptr.b.h = readMem(PC);
		if (!(FL & FLAG_P))
		{
			JP();
		}
		else
		{
			PC++;
		}
		break;
	case 0xe3:		/* EX (SP),HL */
		bytetempl = readMem(SP);
		bytetemph = readMem(SP + 1);
		contendedAccess(SP + 1, 1);
		writeMem(SP + 1, H);
		writeMem(SP, L);
		contendedAccess(SP, 1); contendedAccess(SP, 1);
		L = m_Z80Processor.memptr.b.l = bytetempl;
		H = m_Z80Processor.memptr.b.h = bytetemph;
		break;
	case 0xe4:		/* CALL PO,nnnn */
		m_Z80Processor.memptr.b.l = readMem(PC++);
		m_Z80Processor.memptr.b.h = readMem(PC);
		if (!(FL & FLAG_P))
		{
			CALL();
		}
		else
		{
			PC++;
		}
		break;
	case 0xe5:		/* PUSH HL */
		contendedAccess(IR, 1);
		PUSH16(L, H);
		break;
	case 0xe6:		/* AND nn */
		bytetemp = readMem(PC++);
		AND(bytetemp);
		break;
	case 0xe7:		/* RST 20 */
		contendedAccess(IR, 1);
		RST(0x20);
		break;
	case 0xe8:		/* RET PE */
		contendedAccess(IR, 1);
		if (FL & FLAG_P)
		{
			RET();
		}
		break;
	case 0xe9:		/* JP HL */
		PC = HL; /* NB: NOT INDIRECT! */
		break;
	case 0xea:		/* JP PE,nnnn */
		m_Z80Processor.memptr.b.l = readMem(PC++);
		m_Z80Processor.memptr.b.h = readMem(PC);
		if (FL & FLAG_P)
		{
			JP();
		}
		else
		{
			PC++;
		}
		break;
	case 0xeb:		/* EX DE,HL */
		wordtemp = DE; DE = HL; HL = wordtemp;
		break;
	case 0xec:		/* CALL PE,nnnn */
		m_Z80Processor.memptr.b.l = readMem(PC++);
		m_Z80Processor.memptr.b.h = readMem(PC);
		if (FL & FLAG_P)
		{
			CALL();
		}
		else
		{
			PC++;
		}
		break;
	case 0xed:		/* shift ED */
		contendedAccess(PC, 4);
		opcode2 = m_pZXMemory[PC];
		PC++;
		R++;
		stepED(opcode2);
		break;
	case 0xee:		/* XOR A,nn */
		bytetemp = readMem(PC++);
		XOR(bytetemp);
		break;
	case 0xef:		/* RST 28 */
		contendedAccess(IR, 1);
		RST(0x28);
		break;
	case 0xf0:		/* RET P */
		contendedAccess(IR, 1);
		if (!(FL & FLAG_S))
		{
			RET();
		}
		break;
	case 0xf1:		/* POP AF */
		POP16(FL, A);
		break;
	case 0xf2:		/* JP P,nnnn */
		m_Z80Processor.memptr.b.l = readMem(PC++);
		m_Z80Processor.memptr.b.h = readMem(PC);
		if (!(FL & FLAG_S))
		{
			JP();
		}
		else
		{
			PC++;
		}
		break;
	case 0xf3:		/* DI */
		IFF1 = IFF2 = 0;
		break;
	case 0xf4:		/* CALL P,nnnn */
		m_Z80Processor.memptr.b.l = readMem(PC++);
		m_Z80Processor.memptr.b.h = readMem(PC);
		if (!(FL & FLAG_S))
		{
			CALL();
		}
		else
		{
			PC++;
		}
		break;
	case 0xf5:		/* PUSH AF */
		contendedAccess(IR, 1);
		PUSH16(FL, A);
		break;
	case 0xf6:		/* OR nn */
		bytetemp = readMem(PC++);
		OR(bytetemp);
		break;
	case 0xf7:		/* RST 30 */
		contendedAccess(IR, 1);
		RST(0x30);
		break;
	case 0xf8:		/* RET M */
		contendedAccess(IR, 1);
		if (FL & FLAG_S) { RET(); }
		break;
	case 0xf9:		/* LD SP,HL */
		contendedAccess(IR, 1); contendedAccess(IR, 1);
		SP = HL;
		break;
	case 0xfa:		/* JP M,nnnn */
		m_Z80Processor.memptr.b.l = readMem(PC++);
		m_Z80Processor.memptr.b.h = readMem(PC);
		if (FL & FLAG_S)
		{
			JP();
		}
		else
		{
			PC++;
		}
		break;
	case 0xfb:		/* EI */
		IFF1 = IFF2 = 1;
		m_Z80Processor.intEnabledAt = m_Z80Processor.tCount;
		break;
	case 0xfc:		/* CALL M,nnnn */
		m_Z80Processor.memptr.b.l = readMem(PC++);
		m_Z80Processor.memptr.b.h = readMem(PC);
		if (FL & FLAG_S)
		{
			CALL();
		}
		else
		{
			PC++;
		}
		break;
	case 0xfd:		/* shift FD */
		contendedAccess(PC, 4);
		opcode2 = m_pZXMemory[PC];
		PC++;
		R++;
		/*isDDFDok = */stepFD(opcode2);
		break;
	case 0xfe:		/* CP nn */
		bytetemp = readMem(PC++);
		CP(bytetemp);
		break;
	case 0xff:		/* RST 38 */
		contendedAccess(IR, 1);
		RST(0x38);
		break;
	}
}
/// Public
bool ZXSpectrum::init(Display* pDisplayInstance)
{
	m_pDisplayInstance = pDisplayInstance;
	for (uint8_t i = 0; i < 2; i++) m_pScreenBuffer[i] = m_pDisplayInstance->getBuffer(i);
	if ((m_pZXMemory = (uint8_t*)malloc(65535)) == NULL) { printf("Error allocating ZXMemory"); return false; }
	if ((m_pContendTable = (uint8_t*)malloc(42910)) == NULL) { printf("Error allocating contended access table"); return false; }
	memset(m_pContendTable, 0, 42910);
	uint8_t contPattern[] = { 6, 5, 4, 3, 2, 1, 0, 0 };
	for (uint32_t i = 0; i < 42910; i++) m_pContendTable[i] = (((i % 224) > 127) ? 0 : contPattern[(i % 224) % 8]);
	if (!LittleFS.begin()) { DBG_PRINTLN("SPIFFS Mount Failed"); return false; }
	if (!LittleFS.exists(ROMFILENAME)) { DBG_PRINTLN("ROM image not found"); return false; }
	File romFile = LittleFS.open(ROMFILENAME, "r");
	if (!(romFile.read(m_pZXMemory, 16384) == 16384)) { DBG_PRINTLN("Error reading ROM image"); return false; }
	LittleFS.end();
	Wire1.begin(); Wire1.setClock(1000000);
	writeReg(0x0A, 0x20); // Disable SEQOP on port A
	writeReg(0x0B, 0x20); // Disable SEQOP on port B
	writeReg(0x00, 0x00); // I/O direction register A - all bits to output, Spectrum A8...A15 
	writeReg(0x01, 0x1F); // I/O direction register B - bits 0...4 to input, Spectrum D0...D4
	writeReg(0x04, 0x00); // Disable INT on port A 
	writeReg(0x05, 0x00); // Disable INT on port B
	writeReg(0x0D, 0x1F); // Pullup input bits
	writeReg(0x14, 0xFF); // Set latches to high for all bits port A
	writeReg(0x15, 0xE0); // Set latches to high for bits 5...7 portB
	m_initComplete = true;
	return m_initComplete;
}

void ZXSpectrum::resetZ80()
{
	stopTape();
	m_Z80Processor = { 0 };
	AF = AF_ = 0xffff;
	SP = 0xffff;
	m_Z80Processor.intEnabledAt = -1;
}

void ZXSpectrum::loopZ80()
{
#ifdef DBG
	uint64_t startTime = micros();
#endif // DBG
	int32_t usedCycles;
	rp2040.fifo.push(START_FRAME);
	intZ80();
	while (m_Z80Processor.tCount < LOOPCYCLES)
	{
		usedCycles = m_Z80Processor.tCount;
		stepZ80();
		usedCycles = m_Z80Processor.tCount - usedCycles;
		if (m_ZXTape.isTapeActive)
		{
			m_ZXTape.stateCycles -= usedCycles;
			if (m_ZXTape.stateCycles <= 0) processTape();
		}
		int16_t scanLine = m_Z80Processor.tCount / 224 - 1;
		if (m_scanLine != scanLine)
		{
			m_scanLine = scanLine;
			if (m_scanLine >= SCREENOFFSET && m_scanLine <= SCREENOFFSET + 239) drawLine(m_scanLine - SCREENOFFSET);
		}
	}
	while (m_pbRIndex != m_pbWIndex)
	{
		m_borderColor = m_borderColors[m_pbRIndex].color; m_pbRIndex = (++m_pbRIndex) & (BORDER_BUFFER_SIZE - 1);
	}
	m_Z80Processor.tCount -= LOOPCYCLES;
	m_frameCounter = (++m_frameCounter) & 0x1F;
	if (m_Z80Processor.intEnabledAt >= 0) m_Z80Processor.intEnabledAt -= LOOPCYCLES;
#ifdef DBG
	m_emulationTime = micros() - startTime;
#endif // DBG
	m_waitTime = micros();
	while (!(rp2040.fifo.pop() & STOP_FRAME));
	m_waitTime = micros() - m_waitTime;
}

void ZXSpectrum::startTape(BYTE* pBuffer, uint32_t bufferSize)
{
	m_TAPSection = { 0 };
	m_TAPSection.data = pBuffer; m_TAPSection.size = bufferSize;
	m_TAPSection.bit = 0;
	m_ZXTape.isTapeActive = true; // start
	m_ZXTape.tapeState = 2; // PILOT tone
	m_ZXTape.stateCycles = m_tapeStates[m_ZXTape.tapeState].stateCycles;
	m_ZXTape.statesCount = m_tapeStates[m_ZXTape.tapeState].statesCount;
}

void ZXSpectrum::tape2X()
{
	m_pZXMemory[1409] = 206;
	m_pZXMemory[1416] = 227;
	m_pZXMemory[1424] = 228;
	m_pZXMemory[1432] = 236;
	m_pZXMemory[1446] = 216;
	m_pZXMemory[1479] = 217;
	m_pZXMemory[1487] = 229;
	m_pZXMemory[1492] = 215;
	m_pZXMemory[1512] = 5;
	m_tapeStates[0] = { 427, 2 };
	m_tapeStates[1] = { 855, 2 };
	m_tapeStates[2] = { 1084, 4846 };
	m_tapeStates[3] = { 333, 1 };
	m_tapeStates[4] = { 367, 1 };
}

void ZXSpectrum::tape1X()
{
	m_pZXMemory[1409] = 156;
	m_pZXMemory[1416] = 198;
	m_pZXMemory[1424] = 201;
	m_pZXMemory[1432] = 212;
	m_pZXMemory[1446] = 176;
	m_pZXMemory[1479] = 178;
	m_pZXMemory[1487] = 203;
	m_pZXMemory[1492] = 176;
	m_pZXMemory[1512] = 22;
	m_tapeStates[0] = { 855, 2 };
	m_tapeStates[1] = { 1710, 2 };
	m_tapeStates[2] = { 2168, 4846 };
	m_tapeStates[3] = { 667, 1 };
	m_tapeStates[4] = { 735, 1 };
}

void ZXSpectrum::writeReg(uint8_t reg, uint8_t data)
{
	Wire1.beginTransmission(0x20);
	Wire1.write(reg); Wire1.write(data);
	Wire1.endTransmission();
}

uint8_t ZXSpectrum::readKeys()
{
	Wire1.beginTransmission(0x20); Wire1.write(0x13); Wire1.endTransmission(); // Request GPIOB state
	Wire1.requestFrom(0x20, 1);
	return (Wire1.read() | 0xE0);
}

