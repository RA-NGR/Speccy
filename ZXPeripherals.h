#pragma once
#include "Common.h"

class ZXPeripherals
{
	//struct
	//{
	//	uint8_t scanMask;
	//	uint8_t xorMask;
	//	uint8_t portNum;
	//} m_scanData[10] = { {0b11101111, 0x00, 0x14}, {0b11011111, 0x00, 0x14}, {0b10111111, 0x00, 0x14}, {0b01111111, 0x00, 0x14}, {0b11111110, 0x00, 0x14}, {0b11111101, 0x00, 0x14}, 
	//					 {0b11111011, 0x00, 0x14}, {0b11110111, 0x00, 0x14}, {0b01100000, 0xFF, 0x15}, {0b10100000, 0xFF, 0x15} };
	alarm_pool_t* m_pAlarmPool = NULL;
	struct repeating_timer m_clockTimer;
	int m_cyclesDone = 0;
	int16_t m_ringBuffer[SOUND_BUFFER_SIZE];
	uint16_t m_rbWrIndex = 0;
	uint16_t m_rbRdIndex = 0;
//	void writeReg(uint8_t reg, uint8_t data);
//	uint8_t readPort();
public:
	ZXPeripherals() {};
	~ZXPeripherals();
	bool init();
	void update();
private:
	static bool onTimer(struct repeating_timer* timer);
};

