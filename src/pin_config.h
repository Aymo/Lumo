#pragma once

// Waveshare ESP32-S3-Touch-AMOLED-1.75 pinout.
// Bron: officiele Waveshare demo (Mylibrary/pin_config.h, v3.3.5).

// --- Display (CO5300, QSPI) ---
#define LCD_SDIO0   4
#define LCD_SDIO1   5
#define LCD_SDIO2   6
#define LCD_SDIO3   7
#define LCD_SCLK   38
#define LCD_CS     12
#define LCD_RESET  39
#define LCD_WIDTH  466
#define LCD_HEIGHT 466

// --- Touch (CST9217, gedeelde I2C met IMU/RTC/AXP2101) ---
#define IIC_SDA   15
#define IIC_SCL   14
#define TP_INT    11
#define TP_RESET  40

// --- Audio (ES8311 codec, I2S) ---
// Empirisch gematched aan Waveshare's 08_ES8311 demo: die roept
//   i2s.setPins(BCLK, WS, DIPIN=8, DOPIN=10, MCLK)
// aan, wat in ESP_I2S signature (bclk, ws, dout, din, mclk) betekent dat
// DOUT=8 en DIN=10. De codec-perspectieve naam "I2S_DI" op pin 10 is
// verwarrend maar de hardware volgt deze waardes.
#define I2S_MCK   42
#define I2S_BCK    9
#define I2S_WS    45
#define I2S_DOUT   8   // ESP -> codec -> speaker (matcht Waveshare demo)
#define I2S_DIN   10   // codec (mic) -> ESP
#define PA_EN     46   // speaker amp enable

// --- SD-kaart (SDIO) ---
#define SDMMC_CLK   2
#define SDMMC_CMD   1
#define SDMMC_DATA  3
#define SDMMC_CS   41
