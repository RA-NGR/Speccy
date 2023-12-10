#include <PWMAudio.h>
#include "Common.h"
#include "Display.h"
#include "ZXSpectrum.h"
#include "ZXPeripherals.h"
//#include "fonts.h"

Display g_mainDisplay;
ZXSpectrum g_zxEmulator;
ZXPeripherals g_zxPeripherals;

enum systemMode
{
	modeEmulator,
	modeBrowser
};
enum systemMode g_sysMode = systemMode::modeEmulator;

uint8_t* tapBuffer = NULL;
uint16_t tapSize = 0;
File tapFile;
bool tapActive = false;
int32_t tapePause = -1;

void setup()
{
//	vreg_set_voltage(VREG_VOLTAGE_1_15);
	set_sys_clock_khz(250000, true);
#if defined(DBG)
	Serial.begin(115200);
	delay(5000);
#endif // DBG
	g_mainDisplay.init();
	delay(100);
	g_zxEmulator.init(&g_mainDisplay);
	g_zxEmulator.resetZ80();
	DBG_PRINTF("Free mem: %d\n", rp2040.getFreeHeap());

}

bool readTAPSection(File& file)
{
	free(tapBuffer); tapBuffer = NULL;
	if (file.readBytes((char*)&tapSize, 2) != 2) return false;
	tapBuffer = (uint8_t*)malloc(tapSize + 1);
	if (tapBuffer == NULL)
	{
		DBG_PRINTF("Error allocating %ld bytes\n", tapSize);
		return false;
	}
	tapBuffer[tapSize] = 0x00;
	if (file.readBytes((char*)tapBuffer, tapSize) != tapSize) return false;
	return true;
}

void loop()
{
	if (g_sysMode == modeEmulator)
	{
#ifdef DBG
		static uint32_t loopCounter = 0;
		static uint32_t maxTime = 0;
#endif // DBG
		if (tapActive && !g_zxEmulator.tapeActive())
		{
			if (tapePause == -1) tapePause = millis();
			if (millis() - tapePause > 500)
			{
				if (!readTAPSection(tapFile))
				{
					tapFile.close(); tapActive = false;
					SD.end();
				}
				else
				{
					g_zxEmulator.startTape(tapBuffer, tapSize);
					tapePause = -1;
				}
			}
		}
		int zxKey;
		while ((zxKey = Serial.read()) != -1)
		{
			if (zxKey == '#')
			{
				if ((zxKey = Serial.read()) != -1)
					switch (zxKey)
					{
					case '1': // F1
						g_zxEmulator.tape1X();
						break;
					case '2':
						g_zxEmulator.tape2X();
						break;
					case '4':
						free(tapBuffer);
						g_zxEmulator.resetZ80();
						break;
					case '5':
						maxTime = 0;
					default:
						break;
					}
			}
			if (zxKey == '^')
			{
				g_zxEmulator.stopTape();
				tapFile.close(); tapActive = false; tapePause = -1;
				Serial.flush();
				break;
			}
			if (zxKey == '&')
			{
				String fileName = "/Games/";
				while ((zxKey = Serial.read()) != -1) fileName += (char)zxKey;
				if (!SD.begin(SS))
				{
					DBG_PRINTLN("SDC Mount Failed");
					break;
				}
				if (!SD.exists(fileName))
				{
					DBG_PRINTF("File %s not found\n", fileName);
					break;
				}
				if (!(tapFile = SD.open(fileName, "r")))
				{
					DBG_PRINTLN("Error opening file.");
					break;
				}
				if (readTAPSection(tapFile))
				{
					tapActive = true; tapePause = -1;
					g_zxEmulator.startTape(tapBuffer, tapSize);
				}
				Serial.flush();
				break;
			}
		}
		g_zxEmulator.loopZ80();
#ifdef DBG
		uint32_t emulTime = g_zxEmulator.getEmulationTime();
		if (emulTime > maxTime) maxTime = emulTime;
		loopCounter++;
		if (loopCounter > 89)
		{
			DBG_PRINTF("Core temp: %.2f'C, FPS: %3.1f (min: %3.1f)\n", analogReadTemp(), 1000000.0 / emulTime, 1000000.0 / maxTime);
//			DBG_PRINTF("RP time: %d\n", g_zxEmulator.getRPTime());
			loopCounter = 0;
		}
#endif // DBG
	}
}

void setup1()
{
	g_zxPeripherals.init();
}

void loop1()
{
	g_zxPeripherals.update();
}

