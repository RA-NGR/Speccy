#include "ZXPeripherals.h"

ZXPeripherals::~ZXPeripherals()
{
	alarm_pool_destroy(m_pAlarmPool);
}

bool ZXPeripherals::init()
{
	m_pAlarmPool = alarm_pool_create_with_unused_hardware_alarm(16);
	Wire1.setSDA(SDA_PIN); Wire1.setSCL(SCL_PIN);
	Wire1.begin();
	return true;
}

void ZXPeripherals::update()
{
	uint32_t ctrlData = 0;
	rp2040.fifo.pop_nb(&ctrlData);
	if (ctrlData & START_FRAME) alarm_pool_add_repeating_timer_us(m_pAlarmPool, SOUND_CLOCK, onTimer, this, &m_clockTimer);
	if (ctrlData & WR_PORT)
	{
		int val = (ctrlData & 0x00FFFFFF);
		if (ctrlData & 0x00800000) val |= 0xFF000000;
		m_ringBuffer[m_rbWrIndex] = val; m_rbWrIndex++;
	}
	if (ctrlData & RD_PORT)
	{

	}
}

bool ZXPeripherals::onTimer(struct repeating_timer* pTimer)
{
	static uint32_t soundBit = 0;
	ZXPeripherals* pInstance = (ZXPeripherals*)pTimer->user_data;
	pInstance->m_cyclesDone += (SOUND_CLOCK / 8) * 28;
	if (pInstance->m_rbRdIndex != pInstance->m_rbWrIndex && pInstance->m_ringBuffer[pInstance->m_rbRdIndex] <= pInstance->m_cyclesDone)
	{
		soundBit ^= HIGH;
		digitalWriteFast(SND_PIN, soundBit);
		pInstance->m_rbRdIndex++;
	}
	if (pInstance->m_cyclesDone < LOOPCYCLES) return true;
	pInstance->m_cyclesDone -= LOOPCYCLES;
	rp2040.fifo.push(STOP_FRAME);
	return false;
}