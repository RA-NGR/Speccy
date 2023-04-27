#pragma once
#include "COmmon.h"

class Display
{
	const uint16_t m_pioProgramInstructions[3] = {
		0x98a0, //  0: pull   block           side 1     
		0x7100, //  1: out    pins, 32        side 0 [1] 
		0x1800, //  2: jmp    0               side 1     
	};
	const struct pio_program m_pioProgram = {
		.instructions = m_pioProgramInstructions,
		.length = 3,
		.origin = -1,
	};
public:
	Display() { };
	~Display();
	bool init();
	uint16_t* getBuffer(uint8_t bufferIndex) { return (!m_initComplete ? NULL : m_pDMABuffers[bufferIndex & 0x01]); };
	void drawBuffer(uint8_t bufferIndex);
private:
	bool m_initComplete = false;
	uint16_t* m_pDMABuffers[2] = { 0 };
	PIO m_pio = 0;
	int m_pioSM = -1;
	uint32_t m_pioInstrSetDC = 0;
	uint32_t m_pioInstrClrDC = 0;
	uint32_t m_pullStallMask = 0;
	int m_dmaChannel = -1;
	dma_channel_config m_dmaConfig;
	void writeCommand(uint8_t cmd);
	void writeData(uint8_t data) { m_pio->txf[m_pioSM] = (data); m_pio->fdebug = m_pullStallMask; while (!(m_pio->fdebug & m_pullStallMask)); };
};

