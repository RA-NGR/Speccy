#include "ZXPeripherals.h"
#include "ZXSpectrum.h"

extern ZXSpectrum g_zxEmulator;
extern critical_section g_portLock;

ZXPeripherals::~ZXPeripherals()
{
	alarm_pool_destroy(m_pAlarmPool);
}

bool ZXPeripherals::init()
{
	
	m_pAlarmPool = alarm_pool_create_with_unused_hardware_alarm(16);
//	Wire1.setSDA(PIN_WIRE1_SDA); Wire1.setSCL(PIN_WIRE1_SCL);
//	Wire1.setClock(400000);
	Wire1.begin();
	writeReg(0x0A, 0x20); // Disable SEQOP on port A
	writeReg(0x0B, 0x20); // Disable SEQOP on port B
	writeReg(0x00, 0x00); // I/O direction register A - all bits to output, Spectrum A8...A15 
	writeReg(0x01, 0x1F); // I/O direction register B - bits 0...4 to input, Spectrum D0...D4
	writeReg(0x04, 0x00); // Disable INT on port A 
	writeReg(0x05, 0x00); // Disable INT on port B
	writeReg(0x0D, 0x1F); // Pullup input bits
	writeReg(0x14, 0xFF); // Set latches to high for all bits port A
	writeReg(0x15, 0xE0); // Set latches to high for bits 5...7 portB
	pinMode(SND_PIN, OUTPUT);
	digitalWriteFast(SND_PIN, 0);
	m_fTimer = millis();
	return true;
}

void ZXPeripherals::update()
{
	uint32_t ctrlData = 0;
	rp2040.fifo.pop_nb(&ctrlData);
	if (ctrlData & START_FRAME) alarm_pool_add_repeating_timer_us(m_pAlarmPool, -SOUND_CLOCK, onTimer, this, &m_clockTimer);
	if (ctrlData & WR_PORT)
	{
		int val = (ctrlData & 0x00FFFFFF);
		if (ctrlData & 0x00800000) val |= 0xFF000000; // restore sign bit
		m_ringBuffer[m_rbWrIndex] = val / ((SOUND_CLOCK / 8.0) * 28);
		m_rbWrIndex = (++m_rbWrIndex) & (SOUND_BUFFER_SIZE - 1);
	}
	if (ctrlData & RD_PORT)
	{
		uint8_t decodedPort = ((uint8_t)ctrlData & 0x0F) << 4 | ((uint8_t)ctrlData & 0xF0) >> 4;
		if (decodedPort != 0xFF)
		{
			writeReg(0x14, decodedPort);
			uint8_t portVal = readPort();
			for (int i = 0; i < 8; i++) if (!(((uint8_t)ctrlData >> i) & 0x01))m_portData[i] = portVal;
			writeReg(0x14, 0xFF);
//			if (portVal != 0xFF) DBG_PRINTF("%04X Port - %02X\n", ctrlData, portVal);
		}
		else
		{
			writeReg(0x15, 0x60);
			m_portData[8] = readPort() ^ 0x1F;
			writeReg(0x15, 0xA0);
		}
	}
	if (millis() - m_fTimer >= 20)
	{
		m_fTimer = millis();
		writeReg(0x15, 0xA0);
		m_portData[9] = (readPort() & 0x1F) ^ 0x1F;
		writeReg(0x15, 0xE0);
	}
}


bool ZXPeripherals::onTimer(struct repeating_timer* pTimer)
{
	
	static uint32_t soundBit = 0;
	ZXPeripherals* pInstance = (ZXPeripherals*)pTimer->user_data;
	pInstance->m_cyclesDone++;
	while (pInstance->m_rbRdIndex != pInstance->m_rbWrIndex && pInstance->m_ringBuffer[pInstance->m_rbRdIndex] <= pInstance->m_cyclesDone)
	{
		soundBit ^= HIGH;
		digitalWriteFast(SND_PIN, soundBit);
		pInstance->m_rbRdIndex = (++pInstance->m_rbRdIndex) & (SOUND_BUFFER_SIZE - 1);
	}
	if (pInstance->m_cyclesDone < LOOPCYCLES / ((SOUND_CLOCK / 8.0) * 28)) return true;
	pInstance->m_cyclesDone -= (LOOPCYCLES / ((SOUND_CLOCK / 8.0) * 28));
	rp2040.fifo.push(STOP_FRAME);
	return false;
}

void ZXPeripherals::writeReg(uint8_t reg, uint8_t data)
{
	Wire1.beginTransmission(0x20);
	Wire1.write(reg); Wire1.write(data);
	Wire1.endTransmission();
}

uint8_t ZXPeripherals::readPort()
{
	Wire1.beginTransmission(0x20); Wire1.write(0x13); Wire1.endTransmission(); // Request GPIOB state
	Wire1.requestFrom(0x20, 1);
	return (Wire1.read() | 0xE0);
}
