#pragma once
#include "Common.h"
#include "ZXPeripherals.h"

class Browser
{
	char m_romFileNames[4][15] = { "/BASIC82.rom", "/BASIC90v1.rom", "/BASIC90v2.rom", "/BASIC91.rom" };
	Display* m_pDisplayInstance = NULL;
	Keyboard* m_pKeyboardInstance = NULL;
	String utf8rus(String source);
	String m_currDir = "/GAMES/";
	String m_browserWindow[21];
	int m_browseFrom, m_selectionPos, m_filesCount;
	uint8_t bufferSwitch = 0;
	String m_selectedFile = "";
	bool m_soundOn = true, m_tapeTurbo = false;
	uint8_t m_currRom = 0;
	void drawChar(const uint8_t ch, uint8_t posX, uint16_t foreColor, uint16_t backColor);
	void listFiles();
	void dir();
	void chDir();
	void countFiles();
	void drawSettingsString();
	void drawSelectedROM();
	void drawFooter();
public:
	Browser(Display* pDisplayInstance, Keyboard* pKeyboardInstance) : m_pDisplayInstance(pDisplayInstance), m_pKeyboardInstance(pKeyboardInstance) {};
	~Browser() {};
	void drawString(const String textStr, uint8_t posX, uint8_t posY, uint16_t foreColor, uint16_t backColor, bool isAnsi = true);
	bool run();
	String getSelectedFile() { return m_selectedFile; };
	bool getSoundState() { return m_soundOn; };
	bool getTapeMode() { return m_tapeTurbo; };
//	void setTapeMode(bool tapeMode) { m_tapeTurbo = tapeMode; };
	const char* getROMFileName() { return m_romFileNames[m_currRom]; };
};

