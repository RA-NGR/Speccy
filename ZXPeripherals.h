#pragma once
#include "Common.h"

class ZXPeripherals
{
	alarm_pool_t* m_pAlarmPool = NULL;
	struct repeating_timer m_clockTimer;
	int m_cyclesDone = 0;
	int16_t* m_pRingBuffer = NULL;
	uint16_t m_rbWrIndex = 0;
	uint16_t m_rbRdIndex = 0;
	int m_minCycles = 70000;
//	uint8_t m_bitState = 0;
public:
	ZXPeripherals() {};
	~ZXPeripherals();
	bool init();
	void update();
#ifdef DBG
	int getMinCycles() { return m_minCycles; };
	void resetMinCycles() { m_minCycles = 70000; };
#endif // DBG
private:
	static bool onTimer(struct repeating_timer* timer);
};

