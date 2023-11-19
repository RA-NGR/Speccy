#pragma once
#include "Common.h"

class ZXPeripherals
{
	alarm_pool_t* m_pAlarmPool = NULL;
	struct repeating_timer m_clockTimer;
	int m_cyclesDone = 0;
	int16_t m_ringBuffer[SOUND_BUFFER_SIZE];
	uint16_t m_rbWrIndex = 0;
	uint16_t m_rbRdIndex = 0;
public:
	ZXPeripherals() {};
	~ZXPeripherals();
	bool init();
	void update();
private:
	static bool onTimer(struct repeating_timer* timer);
};

