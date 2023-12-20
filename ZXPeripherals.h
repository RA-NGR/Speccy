#pragma once
#include "Common.h"

class Sound
{
	alarm_pool_t* m_pAlarmPool = NULL;
	struct repeating_timer m_clockTimer;
	int m_cyclesDone = 0;
	int16_t m_ringBuffer[SOUND_BUFFER_SIZE];
	uint16_t m_rbWrIndex = 0;
	uint16_t m_rbRdIndex = 0;
public:
	Sound() {};
	~Sound();
	bool init();
	void update();
private:
	static bool onTimer(struct repeating_timer* timer);
};

class Keyboard
{
	struct repeating_timer m_clockTimer;
public:
	Keyboard() {};
	bool init();
	uint8_t* getBuffer() { return m_portData; };
	uint8_t getData(uint8_t portIdx) { return m_portData[portIdx < 10 ? portIdx : 9]; };
	~Keyboard();
private:
	uint8_t m_portScanMask[10] = { 0b11101111, 0b11011111, 0b10111111, 0b01111111, 0b11111110, 0b11111101, 0b11111011, 0b11110111, 0b01100000, 0b10100000 };
	uint8_t m_portScanIdx = 0;
	uint8_t m_portData[10];
	void writeReg(uint8_t reg, uint8_t data);
	uint8_t readKeys();
	static bool onTimer(struct repeating_timer* timer);
};

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
	void drawBuffer(uint8_t bufferIndex, uint16_t bufferSize);
	void setAddrWindow(uint16_t startX, uint16_t startY, uint16_t endX, uint16_t endY);
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

