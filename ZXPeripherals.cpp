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
	pinMode(SND_PIN, OUTPUT);
	digitalWriteFast(SND_PIN, 0);
	if ((m_pRingBuffer = (int16_t*)calloc(SOUND_BUFFER_SIZE, sizeof(int16_t))) == NULL) return false;
	return true;
}

void ZXPeripherals::update()
{
	uint32_t ctrlData = 0;
#ifdef DBG
	static uint32_t prevVal = 0;
#endif // DBG
	rp2040.fifo.pop_nb(&ctrlData);
	if (ctrlData & START_FRAME) alarm_pool_add_repeating_timer_us(m_pAlarmPool, -SOUND_CLOCK, onTimer, this, &m_clockTimer);
	if (ctrlData & WR_PORT)
	{
		int val = (ctrlData & 0x00FFFFFF);
		if (ctrlData & 0x00800000) val |= 0xFF000000; // restore sign bit
		m_pRingBuffer[m_rbWrIndex] = val / ((SOUND_CLOCK / 8.0) * 28);
#ifdef DBG
		if (m_minCycles > val - prevVal) m_minCycles = val - prevVal;
		prevVal = val;
#endif // DBG
		m_rbWrIndex = (++m_rbWrIndex) & (SOUND_BUFFER_SIZE - 1);
	}
	if (ctrlData & RD_PORT)
	{

	}
}

bool ZXPeripherals::onTimer(struct repeating_timer* pTimer)
{
	
	static uint32_t soundBit = 0;
//	noInterrupts();
	ZXPeripherals* pInstance = (ZXPeripherals*)pTimer->user_data;
	pInstance->m_cyclesDone++;// += (SOUND_CLOCK / 8.0) * 28;
	if (pInstance->m_rbRdIndex != pInstance->m_rbWrIndex && pInstance->m_pRingBuffer[pInstance->m_rbRdIndex] <= pInstance->m_cyclesDone)
	{
		soundBit ^= HIGH;
		digitalWriteFast(SND_PIN, soundBit);
		pInstance->m_rbRdIndex = (++pInstance->m_rbRdIndex) & (SOUND_BUFFER_SIZE - 1);
	}
//	interrupts();
	if (pInstance->m_cyclesDone < LOOPCYCLES / ((SOUND_CLOCK / 8.0) * 28)) return true;
	pInstance->m_cyclesDone -= (LOOPCYCLES / ((SOUND_CLOCK / 8.0) * 28));
	rp2040.fifo.push(STOP_FRAME);
	return false;
}