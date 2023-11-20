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

#ifdef KBD_EMULATED
struct
{
	char chr;
	uint8_t portIdx;
	uint8_t bits;
} keyMap[40] = {
				 {'1', 3, 0b11111110}, {'2', 3, 0b11111101}, {'3', 3, 0b11111011}, {'4', 3, 0b11110111}, {'5', 3, 0b11101111},
				 {'0', 4, 0b11111110}, {'9', 4, 0b11111101}, {'8', 4, 0b11111011}, {'7', 4, 0b11110111}, {'6', 4, 0b11101111},
				 {'q', 2, 0b11111110}, {'w', 2, 0b11111101}, {'e', 2, 0b11111011}, {'r', 2, 0b11110111}, {'t', 2, 0b11101111},
				 {'p', 5, 0b11111110}, {'o', 5, 0b11111101}, {'i', 5, 0b11111011}, {'u', 5, 0b11110111}, {'y', 5, 0b11101111},
				 {'a', 1, 0b11111110}, {'s', 1, 0b11111101}, {'d', 1, 0b11111011}, {'f', 1, 0b11110111}, {'g', 1, 0b11101111},
				 {'`', 6, 0b11111110}, {'l', 6, 0b11111101}, {'k', 6, 0b11111011}, {'j', 6, 0b11110111}, {'h', 6, 0b11101111},
				 {'=', 0, 0b11111110}, {'z', 0, 0b11111101}, {'x', 0, 0b11111011}, {'c', 0, 0b11110111}, {'v', 0, 0b11101111},
				 {' ', 7, 0b11111110}, {'-', 7, 0b11111101}, {'m', 7, 0b11111011}, {'n', 7, 0b11110111}, {'b', 7, 0b11101111} }; // ` - ENTER, = - CS, - - SS
#endif // KBD_EMULATED
uint8_t* tapBuffer = NULL;
uint16_t tapSize = 0;
File tapFile;
bool tapActive = false;
int32_t tapePause = -1;

void setup()
{
//	vreg_set_voltage(VREG_VOLTAGE_1_15);
	set_sys_clock_khz(250000, true);
#if defined(DBG) || defined(KBD_EMULATED)
	Serial.begin(115200);
	delay(5000);
#endif // DBG || KBD_EMULATED
	g_mainDisplay.init();
	delay(100);
	g_zxEmulator.init(&g_mainDisplay);
	g_zxEmulator.resetZ80();
	DBG_PRINTF("Free mem: %d\n", rp2040.getFreeHeap());
//	DBG_PRINTLN(g_zxPeripherals.check());
//	SD.begin(SS);

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
#ifdef KBD_EMULATED
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
					case '3':
						break;
					case '4':
						g_zxEmulator.resetZ80();
						break;
					case '5':
						maxTime = 0;
						break;
					case 'r':
						g_zxEmulator.orPortVal(8, 0b00000001);
						break;
					case 'l':
						g_zxEmulator.orPortVal(8, 0b00000010);
						break;
					case 'd':
						g_zxEmulator.orPortVal(8, 0b00000100);
						break;
					case 'u':
						g_zxEmulator.orPortVal(8, 0b00001000);
						break;
					case 'f':
						g_zxEmulator.orPortVal(8, 0b00010000);
						break;
					default:
						break;
					}
				Serial.flush();
				break;
			}
#endif // KBD_EMULATED
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
#ifdef KBD_EMULATED
			for (int i = 0; i < 40; i++) if (keyMap[i].chr == zxKey)
				g_zxEmulator.andPortVal(keyMap[i].portIdx, keyMap[i].bits);
#endif // KBD_EMULATED
		}
		g_zxEmulator.loopZ80();
#ifdef DBG
		uint32_t emulTime = g_zxEmulator.getEmulationTime();
		if (emulTime > maxTime) maxTime = emulTime;
		loopCounter++;
		if (loopCounter > 89)
		{
			DBG_PRINTF("Core temp: %.2f'C, FPS: %3.1f (min: %3.1f)\n", analogReadTemp(), 1000000.0 / emulTime, 1000000.0 / maxTime);
			loopCounter = 0;
		}
#endif // DBG
#ifdef KBD_EMULATED
		for (int i = 0; i < 9; i++)
			g_zxEmulator.orPortVal(i, 0xBF);
		g_zxEmulator.andPortVal(8, 0x0);
#endif // KBD_EMULATED
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

