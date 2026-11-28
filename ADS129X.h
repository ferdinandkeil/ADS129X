/**
 * ADS129X.h
 *
 * Teensy 3.x/4.x and original-ESP32 driver for the TI ADS1299-4/6/8
 * EEG ADCs (SBAS499C). This is the ADS1299 register map, not the ADS1298
 * ECG map: no respiration, pace, or Wilson terminals. Bias registers are
 * BIAS_*, and address 0x15 is MISC1 (SRB1), not PACE.
 *
 * SPI is mode 1 (CPOL = 0, CPHA = 1): DIN is sampled on the falling SCLK
 * edge and DOUT changes on the rising edge. SCLK is requested at 8 MHz,
 * under the 20 MHz limit (tSCLK >= 50 ns at DVDD >= 2.7 V) and above the
 * ~3.46 MHz floor for an 8-channel 16 kSPS continuous read.
 *
 * Continuous samples are RDATAC plus the falling edge of DRDY. An 8-channel
 * part returns 27 bytes (3 status + 8 x 3). A -6 returns 21, a -4 returns 15.
 * Host packets are always the 33-byte 8-channel OpenBCI frame; channels the
 * chip does not have are zero.
 *
 * Based on Conor Russomanno's ADS1299 code and Ferdinand Keil's ADS129X port.
 */

#ifndef ADS129X_H
#define ADS129X_H

#include <Arduino.h>

// Override a pin before including this header if your carrier differs.
// Teensy defaults match the stacked Teensy 4.0 + ADS1299 carrier.
// ESP32 defaults are a jumper map for the Adafruit ESP32 Feather V2
// (original ESP32-PICO), not that carrier.
#ifndef ADS1299_PIN_DRDY
#if defined(ARDUINO_ARCH_ESP32)
#define ADS1299_PIN_DRDY 34
#else
#define ADS1299_PIN_DRDY 2
#endif
#endif
#ifndef ADS1299_PIN_PWDN
#if defined(ARDUINO_ARCH_ESP32)
#define ADS1299_PIN_PWDN 26
#else
#define ADS1299_PIN_PWDN 3
#endif
#endif
#ifndef ADS1299_PIN_RESET
#if defined(ARDUINO_ARCH_ESP32)
#define ADS1299_PIN_RESET 25
#else
#define ADS1299_PIN_RESET 5
#endif
#endif
#ifndef ADS1299_PIN_START
#if defined(ARDUINO_ARCH_ESP32)
#define ADS1299_PIN_START 27
#else
#define ADS1299_PIN_START 6
#endif
#endif
#ifndef ADS1299_PIN_CS
#if defined(ARDUINO_ARCH_ESP32)
#define ADS1299_PIN_CS 33
#else
#define ADS1299_PIN_CS 10
#endif
#endif
#ifndef ADS1299_PIN_SCLK
#if defined(ARDUINO_ARCH_ESP32)
#define ADS1299_PIN_SCLK 5
#else
#define ADS1299_PIN_SCLK 13
#endif
#endif
#ifndef ADS1299_PIN_DIN
#if defined(ARDUINO_ARCH_ESP32)
#define ADS1299_PIN_DIN 19
#else
#define ADS1299_PIN_DIN 11
#endif
#endif
#ifndef ADS1299_PIN_DOUT
#if defined(ARDUINO_ARCH_ESP32)
#define ADS1299_PIN_DOUT 21
#else
#define ADS1299_PIN_DOUT 12
#endif
#endif

namespace ADS1299Board {
static const int DRDY = ADS1299_PIN_DRDY;
static const int PWDN = ADS1299_PIN_PWDN;
static const int RESET = ADS1299_PIN_RESET;
static const int START = ADS1299_PIN_START;
static const int CS = ADS1299_PIN_CS;
static const int SCLK = ADS1299_PIN_SCLK; // Teensy SCK / ESP32 SCK
static const int DIN = ADS1299_PIN_DIN;   // MOSI -> ADS1299 DIN
static const int DOUT = ADS1299_PIN_DOUT; // MISO <- ADS1299 DOUT
}

// SPI commands (SBAS499C Table 10).
static const uint8_t ADS1299_CMD_WAKEUP = 0x02;
static const uint8_t ADS1299_CMD_STANDBY = 0x04;
static const uint8_t ADS1299_CMD_RESET = 0x06;
static const uint8_t ADS1299_CMD_START = 0x08;
static const uint8_t ADS1299_CMD_STOP = 0x0A;
static const uint8_t ADS1299_CMD_RDATAC = 0x10;
static const uint8_t ADS1299_CMD_SDATAC = 0x11;
static const uint8_t ADS1299_CMD_RDATA = 0x12;
static const uint8_t ADS1299_CMD_RREG = 0x20;
static const uint8_t ADS1299_CMD_WREG = 0x40;

// Register map. Addresses 0x18 and 0x19 (WCT) do not exist on the ADS1299.
static const uint8_t ADS1299_REG_ID = 0x00;
static const uint8_t ADS1299_REG_CONFIG1 = 0x01;
static const uint8_t ADS1299_REG_CONFIG2 = 0x02;
static const uint8_t ADS1299_REG_CONFIG3 = 0x03;
static const uint8_t ADS1299_REG_LOFF = 0x04;
static const uint8_t ADS1299_REG_CH1SET = 0x05;
static const uint8_t ADS1299_REG_CH2SET = 0x06;
static const uint8_t ADS1299_REG_CH3SET = 0x07;
static const uint8_t ADS1299_REG_CH4SET = 0x08;
static const uint8_t ADS1299_REG_CH5SET = 0x09;
static const uint8_t ADS1299_REG_CH6SET = 0x0A;
static const uint8_t ADS1299_REG_CH7SET = 0x0B;
static const uint8_t ADS1299_REG_CH8SET = 0x0C;
static const uint8_t ADS1299_REG_BIAS_SENSP = 0x0D;
static const uint8_t ADS1299_REG_BIAS_SENSN = 0x0E;
static const uint8_t ADS1299_REG_LOFF_SENSP = 0x0F;
static const uint8_t ADS1299_REG_LOFF_SENSN = 0x10;
static const uint8_t ADS1299_REG_LOFF_FLIP = 0x11;
static const uint8_t ADS1299_REG_LOFF_STATP = 0x12;
static const uint8_t ADS1299_REG_LOFF_STATN = 0x13;
static const uint8_t ADS1299_REG_GPIO = 0x14;
static const uint8_t ADS1299_REG_MISC1 = 0x15; // bit 5 = SRB1
static const uint8_t ADS1299_REG_MISC2 = 0x16;
static const uint8_t ADS1299_REG_CONFIG4 = 0x17;
static const uint8_t ADS1299_REG_COUNT = 24;

// ID register: bit 4 is 1, DEV_ID[1:0] is 11, NU_CH[1:0] is the channel count.
// REV_ID[2:0] may change. Common production values are 0x3C / 0x3D / 0x3E.
static const uint8_t ADS1299_ID_ADS1299_4 = 0x3C; // NU_CH = 00
static const uint8_t ADS1299_ID_ADS1299_6 = 0x3D; // NU_CH = 01
static const uint8_t ADS1299_ID_ADS1299 = 0x3E;   // NU_CH = 10

// CONFIG1 DR[2:0]. fMOD = fCLK / 2. At the internal 2.048 MHz clock:
// 000 = 16 kSPS ... 110 = 250 SPS. 111 is reserved.
static const uint8_t ADS1299_SAMPLERATE_16k = 0x00;
static const uint8_t ADS1299_SAMPLERATE_8k = 0x01;
static const uint8_t ADS1299_SAMPLERATE_4k = 0x02;
static const uint8_t ADS1299_SAMPLERATE_2k = 0x03;
static const uint8_t ADS1299_SAMPLERATE_1k = 0x04;
static const uint8_t ADS1299_SAMPLERATE_500 = 0x05;
static const uint8_t ADS1299_SAMPLERATE_250 = 0x06;

// CHnSET GAIN[2:0]. 111 is reserved.
static const uint8_t ADS1299_GAIN_1 = 0x00;
static const uint8_t ADS1299_GAIN_2 = 0x01;
static const uint8_t ADS1299_GAIN_4 = 0x02;
static const uint8_t ADS1299_GAIN_6 = 0x03;
static const uint8_t ADS1299_GAIN_8 = 0x04;
static const uint8_t ADS1299_GAIN_12 = 0x05;
static const uint8_t ADS1299_GAIN_24 = 0x06;

// CHnSET MUX[2:0].
static const uint8_t ADS1299_MUX_NORMAL = 0x00;
static const uint8_t ADS1299_MUX_SHORT = 0x01;
static const uint8_t ADS1299_MUX_BIAS_MEAS = 0x02;
static const uint8_t ADS1299_MUX_MVDD = 0x03;
static const uint8_t ADS1299_MUX_TEMP = 0x04;
static const uint8_t ADS1299_MUX_TEST = 0x05;
static const uint8_t ADS1299_MUX_BIAS_DRP = 0x06;
static const uint8_t ADS1299_MUX_BIAS_DRN = 0x07;

// CONFIG2 CAL_FREQ[1:0]. 10b is reserved.
static const uint8_t ADS1299_CAL_FCLK_2_21 = 0x00; // ~1 Hz
static const uint8_t ADS1299_CAL_FCLK_2_20 = 0x01; // ~2 Hz
static const uint8_t ADS1299_CAL_DC = 0x03;

// Timings from SBAS499C. tPOR = 2^18 tCLK = 128 ms at 2.048 MHz.
// Electrical characteristics list internal-reference start-up and
// power-up to DRDY as 150 ms typical. VCAP1 must also be above 1.1 V.
static const uint16_t ADS1299_TPOR_MS = 150;
static const uint16_t ADS1299_TREF_MS = 150;
static const uint32_t ADS1299_SCLK_HZ = 8000000UL;
static const uint32_t ADS1299_FCLK_HZ = 2048000UL;

// His photographed carrier is an ADS1299-6. Used when the ID read fails.
static const uint8_t ADS1299_DEFAULT_CHANNELS = 6;

// OpenBCI Cyton binary packet (docs.openbci.com Cyton Data Format).
static const uint8_t ADS1299_OPENBCI_BYTES = 33;
static const uint8_t ADS1299_OPENBCI_START = 0xA0;
static const uint8_t ADS1299_OPENBCI_END = 0xC0; // aux bytes are accel; we send zeros

class ADS129X {
public:
    // Pass -1 for reset, pwdn, or start if that pin is strapped on the board
    // and is not driven by the microcontroller.
    ADS129X(int drdy, int cs, int reset = -1, int pwdn = -1, int start = -1);

    // Call before begin() to ignore the ID register and use 4, 6, or 8.
    // After begin(), this forces the SPI frame size (still call it while stopped).
    void setChannelCount(uint8_t count);

    // ESP32 only: SCK, MISO, MOSI. Call before begin(). -1 keeps the board defaults.
    void setSpiPins(int sck, int miso, int mosi);

    // Poll DRDY in getData() instead of a falling-edge interrupt.
    // Call before START(). One interrupt-mode device is supported.
    void setPolling(bool enabled);

    // Queue 33-byte OpenBCI packets filled from the DRDY path. A slow
    // WiFi or Bluetooth write then does not have to clock SPI, and does
    // not overwrite the one sample getData() keeps. On ESP32 the GPIO
    // ISR only wakes a high-priority task, because the Arduino SPI driver
    // is not safe to call from that interrupt; that task does the read.
    // Call before begin(). Returns false if the queue cannot be allocated.
    bool enableSampleRing(uint16_t capacity = 512);
    bool popOpenBciPacket(uint8_t dest[ADS1299_OPENBCI_BYTES]);
    uint32_t ringDropped() const { return ringDrop; }
    // CHnSET gain code (0..6) for channel 1..8.
    uint8_t channelGainCode(uint8_t channel) const;

    // Power-up, tPOR, reset pulse, SDATAC, ID read. Does not start conversions.
    // Returns true when the ID byte looks like an ADS1299-4/6/8.
    bool begin();

    void WAKEUP();
    void STANDBY();
    void RESET(); // software or pin reset, then rewrite the cached registers
    void START(); // START pin high, or START command if no pin, then DRDY
    void STOP();
    void RDATAC();
    void SDATAC();

    uint8_t RREG(uint8_t address);
    void RREG(uint8_t address, uint8_t count, uint8_t *dest);
    void WREG(uint8_t address, uint8_t value);

    // Referential EEG: internal 4.5 V ref, bias amp on, SRB1 closed,
    // every present channel normal-input and in BIAS_SENSP/N.
    void configureReferential(uint8_t gain, uint8_t rate);

    // Differential pairs: SRB1 open, bias amp on, normal mux.
    void configureDifferential(uint8_t gain, uint8_t rate);

    // Internal square wave on every present channel (CONFIG2 INT_CAL, mux test).
    // calFreq is ADS1299_CAL_*.
    void configureTestSignal(uint8_t gain, uint8_t rate, uint8_t calFreq);

    // sample[0] is the raw 24-bit status word (bits 23:20 should be 0b1100).
    // sample[1..channels] are sign-extended counts. The rest are 0.
    // Returns false if no new DRDY has arrived.
    bool getData(int32_t sample[9]);

    // 33-byte OpenBCI packet. Channels above channelCount(), and channels
    // turned off with the '1'..'8' commands, are 0x000000. Aux is six 0x00
    // bytes and the footer is 0xC0 (no accelerometer on this board).
    void fillOpenBciPacket(uint8_t *dest, const int32_t sample[9]);
    void fillOpenBciPacket(uint8_t *dest, const int32_t sample[9], uint8_t sampleNumber);
    void sendOpenBciPacket(Print &out, const int32_t sample[9]);

    // Eight space-separated signed counts, unused channels 0. For the plotter.
    void printCounts(Print &out, const int32_t sample[9]);

    // ASCII banner ending in $$$ so an OpenBCI host can sync.
    void printBanner(Print &out);
    void printTextHeader(Print &out, const char *note);

    // One byte from the OpenBCI ASCII command set. streaming is updated.
    // Text replies are suppressed while streaming so they cannot split a packet.
    void handleHostByte(uint8_t cmd, Print &reply, bool &streaming);

    uint8_t deviceId() const { return id; }
    bool idValid() const { return idOk; }
    uint8_t channelCount() const { return channels; }
    uint8_t detectedChannelCount() const { return detectedChannels; }
    uint8_t frameBytes() const { return (uint8_t)(3 + 3 * channels); }
    const char *deviceName() const;
    uint16_t sampleRateHz() const;
    uint8_t gainSetting() const { return activeGain; }
    uint32_t missedSamples() const { return missed; }

    static int32_t signExtend24(uint32_t raw24);
    static uint8_t channelsFromId(uint8_t idByte);
    static bool idLooksLikeAds1299(uint8_t idByte);
    static uint8_t gainMultiplier(uint8_t gainCode);

private:
    struct Channel {
        bool powerDown;
        uint8_t gain;
        uint8_t mux;
        bool srb2;
        bool bias;
        bool packetEnable;
    };

    struct Defaults {
        bool valid;
        uint8_t gain;
        uint8_t mux;
        uint8_t rate;
        uint8_t config2;
        bool srb1;
        bool bias;
        bool biasAmp;
        bool biasMeas;
        bool internalRef;
    };

    enum CmdState : uint8_t { CMD_IDLE, CMD_X, CMD_Z, CMD_TILDE, CMD_SLASH };

    int pinDrdy;
    int pinCs;
    int pinReset;
    int pinPwdn;
    int pinStart;
    int spiSck;
    int spiMiso;
    int spiMosi;

    uint8_t id;
    bool idOk;
    uint8_t channels;
    uint8_t detectedChannels;
    bool channelCountForced;
    bool polling;
    bool inRdatac;
    bool irqArmed;
    bool savedRearm;
    bool savedRdatac;
    uint8_t accessDepth;
    uint8_t activeGain;
    uint8_t sampleRate;
    bool srb1On;
    Defaults defaults;
    Channel chCfg[8];
    uint8_t shadow[ADS1299_REG_COUNT];
    bool shadowSet[ADS1299_REG_COUNT];

    volatile bool dataReady;
    volatile uint32_t missed;
    volatile int32_t isrSample[9];
    uint8_t sampleCounter;

    CmdState cmdState;
    uint8_t cmdBuf[8];
    uint8_t cmdLen;
    uint32_t cmdStartMs;

    bool ringOn;
    bool boardReady;
    uint8_t *ringPkts;
    uint16_t ringCap;
    volatile uint16_t ringHead;
    volatile uint16_t ringTail;
    volatile uint32_t ringDrop;
    void *drdySem;
    void *spiMutex;
    bool readerStarted;

    static ADS129X *isrInstance;
#if defined(ARDUINO_ARCH_ESP32)
    static void IRAM_ATTR drdyIsr();
#else
    static void drdyIsr();
#endif

    void pinWrite(int pin, uint8_t level);
    void select();
    void release();
    void sendCommand(uint8_t cmd);
    void writeRegRaw(uint8_t address, uint8_t value);
    uint8_t readRegRaw(uint8_t address);
    void writeShadow(uint8_t address, uint8_t value);
    void beginRegisterAccess();
    void endRegisterAccess();
    void armIrq();
    void disarmIrq();
    void readFrame(int32_t *dest);
    void writeChannelRaw(uint8_t channel);
    void writeBiasRaw();
    void applyDefaults();
    void pulseReset();
    void setChannelPower(uint8_t channel, bool powerDown);
    void say(Print &out, bool streaming, const char *msg);
    uint8_t channelMask() const;
    void lockSpiBus();
    void unlockSpiBus();
    bool pushRing(const uint8_t *packet);
    void captureIntoRing();
#if defined(ARDUINO_ARCH_ESP32)
    void startEsp32Reader();
    static void readerTask(void *arg);
#endif
};

#endif
