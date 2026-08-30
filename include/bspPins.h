
// --- I2C Pins (ES8311 Control) ---
#define BSP_I2C_SDA         47
#define BSP_I2C_SCL         48
#define BSP_I2C_FREQ_HZ     400000
#define BSP_I2C_TIMEOUT_MS  100

// --- I2S Pins (Audio Data Stream) ---
#define BSP_I2S_SCLK      43  // BCLK
#define BSP_I2S_LCLK      38  // LRC / WS
#define BSP_I2S_DOUT      21  // Data Out to ES8311
#define BSP_I2S_MCLK      12  // Master Clock
#define BSP_POWER_AMP_IO  11  // Speaker Amp Enable Pin (Active HIGH)

// --- MicroSD Card (SD_MMC 1-Bit Mode) ---
#define BSP_SD_CLK        1
#define BSP_SD_CMD        44
#define BSP_SD_D0         17

#define ES8311_ADDR 0x18

// Waveshare ESP32-S3-RGB-Matrix Hardware Pin Mapping
#define R1_PIN  4
#define G1_PIN  6
#define B1_PIN  5
#define R2_PIN  7
#define G2_PIN  16
#define B2_PIN  15

#define A_PIN   18
#define B_PIN   8
#define C_PIN   3
#define D_PIN   42
#define E_PIN   9
#define LAT_PIN 40
#define OE_PIN  2
#define CLK_PIN 41