#include "ZXPeripherals.h"

ZXPeripherals::~ZXPeripherals()
{
	alarm_pool_destroy(m_pAlarmPool);
}

bool ZXPeripherals::init()
{
	m_pAlarmPool = alarm_pool_create_with_unused_hardware_alarm(16);
	return true;
}

void ZXPeripherals::update()
{
	uint32_t ctrlData = 0;
	rp2040.fifo.pop_nb(&ctrlData);
	if (ctrlData == START_FRAME) alarm_pool_add_repeating_timer_us(m_pAlarmPool, SOUND_CLOCK, onTimer, this, &m_clockTimer);
}

bool ZXPeripherals::onTimer(struct repeating_timer* pTimer)
{
	ZXPeripherals* pInstance = (ZXPeripherals*)pTimer->user_data;
	pInstance->m_cyclesDone += (SOUND_CLOCK / 8) * 28;
	if (pInstance->m_cyclesDone < LOOPCYCLES) return true;
	rp2040.fifo.push(STOP_FRAME);
	pInstance->m_cyclesDone -= LOOPCYCLES;
	//cyclesDone = 0;
	return false;
}