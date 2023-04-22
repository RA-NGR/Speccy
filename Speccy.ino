#include "Settings.h"
#include "Display.h"
#include "ZXSpectrum.h"
//
//Display display;
//ZXSpectrum emulator;
//
//void setup() 
//{
//    Serial.begin(115200);
//    delay(5000);
//    display.init();
//    emulator.init(&display);
//}
//
//void loop(void)
//{
//    uint32_t stTime = micros();
//    for (int y = 0; y < 240; y++) emulator.drawLine(y);
//    DBG_PRINTF("FPS: %.2f\n", 1000000.0 / (micros() - stTime));
//    emulator.m_frameCounter = (++emulator.m_frameCounter) & 0x1F;
//}
//

Display g_display;
//ZXPeripherals g_zxPeriph;
ZXSpectrum g_zxEmulator;

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
#if defined(DBG) || defined(KBD_EMULATED)
	Serial.begin(115200);
#endif // DBG || KBD_EMULATED
	g_display.init();
//	g_zxPeriph.init();
	delay(100);
	g_zxEmulator.init(&g_display/*, &g_zxPeriph*/);
	g_zxEmulator.resetZ80();
#ifndef KBD_EMULATED
	g_zxPeriph.init();
#endif // !KBD_EMULATED
	//SD.begin(SS);
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
	static uint32_t loopCounter = 0;
	static uint32_t maxTime = 0;
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
#ifndef KBD_EMULATED
	g_zxPeriph.requestData();
#endif // !KBD_EMULATED
	int zxKey;
	while ((zxKey = Serial.read()) != -1)
	{
#ifdef KBD_EMULATED
		if (zxKey == '#')
		{
			g_zxEmulator.resetZ80();
			Serial.flush();
			maxTime = 0;
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
#ifndef KBD_EMULATED
		if (zxKey == '1')
#else
		if (zxKey == '!')
#endif // !KBD_EMULATED
		{
			g_zxEmulator.tape1X();
			Serial.flush();
		}
#ifndef KBD_EMULATED
		if (zxKey == '2')
#else
		if (zxKey == '@')
#endif // !KBD_EMULATED
		{
			g_zxEmulator.tape2X();
			Serial.flush();
		}
#ifdef KBD_EMULATED
		for (int i = 0; i < 40; i++) if (keyMap[i].chr == zxKey)
			g_zxEmulator.andPortVal(keyMap[i].portIdx, keyMap[i].bits);
#endif // KBD_EMULATED
	}
#ifndef KBD_EMULATED
	for (int i = 0; i < 8; i++) g_zxEmulator.setPortVal(i, g_zxPeriph.getData(i) | 0xA0 | g_zxEmulator.getPortVal(i) & 0x40);
	g_zxEmulator.setPortVal(8, g_zxPeriph.getData(8) ^ 0x1F);
	if (g_zxPeriph.getFKeys() == 0x1E)
	{
		g_zxEmulator.resetZ80(); maxTime = 0;
	}
#endif // !KBD_EMULATED
	g_zxEmulator.loopZ80();
	uint32_t emulTime = g_zxEmulator.getEmulationTime();
	if (emulTime > maxTime) maxTime = emulTime;
	loopCounter++;
	if (loopCounter > 89)
	{
		uint32_t tmp;
		DBG_PRINTF("FPS:%.1f, min:%.1f\n", 1000000.0 / emulTime, 1000000.0 / maxTime);
		loopCounter = 0;
	}
#ifdef KBD_EMULATED
	for (int i = 0; i < 8; i++)
		g_zxEmulator.orPortVal(i, 0xBF);
#endif // KBD_EMULATED
}


