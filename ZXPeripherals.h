#pragma once
#include "Settings.h"

class ZXPeripherals
{
	alarm_pool_t* m_pCore1Pool = NULL;
	struct repeating_timer m_clockTimer;
	int m_cyclesDone = 0;
public:
	ZXPeripherals() {};
	~ZXPeripherals();
	bool init();
	void update();
private:
	static bool onTimer(struct repeating_timer* timer);
};

