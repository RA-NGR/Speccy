#include "ZXPeripherals.h"

ZXPeripherals::~ZXPeripherals()
{
	alarm_pool_destroy(m_pCore1Pool);
}

bool ZXPeripherals::init()
{
	m_pCore1Pool = alarm_pool_create_with_unused_hardware_alarm(16);
	return true;
}

void ZXPeripherals::update()
{
	uint32_t ctrlData = 0;
	rp2040.fifo.pop_nb(&ctrlData);
	if (ctrlData == START_FRAME) alarm_pool_add_repeating_timer_us(m_pCore1Pool, SOUND_CLOCK, onTimer, this, &m_clockTimer);
}

bool ZXPeripherals::onTimer(struct repeating_timer* timer)
{
	ZXPeripherals* pInstance = (ZXPeripherals*)timer->user_data;
	pInstance->m_cyclesDone += (SOUND_CLOCK / 8) * 28;
	if (pInstance->m_cyclesDone < LOOPCYCLES) return true;
	rp2040.fifo.push(STOP_FRAME);
	pInstance->m_cyclesDone -= LOOPCYCLES;
	//cyclesDone = 0;
	return false;
}