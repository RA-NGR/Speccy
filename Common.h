#pragma once
#include <Arduino.h>
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/adc.h"
#include <pico/stdlib.h>
#include <LittleFS.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>

#define DBG
#define KBD_EMULATED

#define TFT_WR   20		/* Write strobe pin */
#define TFT_DC   21		/* Data Command control pin */

#define TFT_DATA  0		/* Data bus start pin */
#define SND_PIN  22		/* Sound pin */
#define SDA_PIN  26
#define SCL_PIN  27
#define PIO_CLK_DIV   7
#define PIO_CLK_FRACT 0
#define TFT_REFRESHRATE 0x10 /* 119 Hz */
//#define TFT_REFRESHRATE 0x11 /* 112 Hz */
//#define TFT_REFRESHRATE 0x12 /* 106 Hz */
//#define TFT_REFRESHRATE 0x13 /* 100 Hz */
//#define TFT_REFRESHRATE 0x14 /* 95 Hz	 */
//#define TFT_REFRESHRATE 0x15 /* 90 Hz	 */
//#define TFT_REFRESHRATE 0x16 /* 86 Hz	 */
//#define TFT_REFRESHRATE 0x17 /* 83 Hz	 */
//#define TFT_REFRESHRATE 0x18 /* 79 Hz	 */
//#define TFT_REFRESHRATE 0x19 /* 76 Hz	 */
//#define TFT_REFRESHRATE 0x1A /* 73 Hz	 */
//#define TFT_REFRESHRATE 0x1B /* 70 Hz	 */
//#define TFT_REFRESHRATE 0x1C /* 68 Hz	 */
//#define TFT_REFRESHRATE 0x1D /* 65 Hz	 */
//#define TFT_REFRESHRATE 0x1E /* 63 Hz	 */
//#define TFT_REFRESHRATE 0x1F /* 61 Hz	 */

#define DMA_BUFF_SIZE 40

#define START_FRAME 0x10000000
#define STOP_FRAME  0x20000000
#define WR_PORT     0x40000000
#define RD_PORT     0x80000000
#define LOOPCYCLES 69888
#define SCREENOFFSET 40
#define STARTSCREEN (8960 - 16)
#define ENDSCREEN (62720 - 16)
#define IRQ_LENGTH 32
#define BORDER_BUFFER_SIZE 128
#define SOUND_CLOCK 48 /* in us, 20 kHz should be enough */
//#define SOUND_CLOCK 16
//#define SOUND_BUFFER_SIZE 2048
// 109200 Hz sound timer Dividers are: 146 at 16 MHz, 73 at 8 MHz

#define ROMFILENAME "/BASIC82.rom"
//#define ROMFILENAME "/BASIC90v1.rom"
//#define ROMFILENAME "/BASIC90v2.rom"
//#define ROMFILENAME "/BASIC91.rom"

#ifdef DBG
#define DBG_PRINT(a) Serial.print(a)
#define DBG_PRINTLN(a) Serial.println(a)
#define DBG_PRINTF Serial.printf
#else
#define DBG_PRINT(a) 
#define DBG_PRINTLN(a)
#define DBG_PRINTF //
#endif // DBG
