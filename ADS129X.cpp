/**
 * ADS129X.cpp
 *
 * ADS1299-4/6/8 SPI driver. See ADS129X.h for the datasheet rules this follows.
 */

#include "ADS129X.h"

#include <SPI.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(ARDUINO_ARCH_ESP32)
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#endif

ADS129X *ADS129X::isrInstance = nullptr;

static const uint16_t ADS1299_RATE_HZ[7] = {16000, 8000, 4000, 2000, 1000, 500, 250};
static const uint8_t ADS1299_GAIN_VALUE[7] = {1, 2, 4, 6, 8, 12, 24};
static const char *ADS1299_REG_NAME[ADS1299_REG_COUNT] = {
    "ID",         "CONFIG1",    "CONFIG2",     "CONFIG3",    "LOFF",
    "CH1SET",     "CH2SET",     "CH3SET",      "CH4SET",     "CH5SET",
    "CH6SET",     "CH7SET",     "CH8SET",      "BIAS_SENSP", "BIAS_SENSN",
    "LOFF_SENSP", "LOFF_SENSN", "LOFF_FLIP",   "LOFF_STATP", "LOFF_STATN",
    "GPIO",       "MISC1",      "MISC2",       "CONFIG4"};

static void printHex2(Print &out, uint8_t value) {
    if (value < 16) {
        out.print('0');
    }
    out.print(value, HEX);
}

ADS129X::ADS129X(int drdy, int cs, int reset, int pwdn, int start)
    : pinDrdy(drdy),
      pinCs(cs),
      pinReset(reset),
      pinPwdn(pwdn),
      pinStart(start),
      spiSck(-1),
      spiMiso(-1),
      spiMosi(-1),
      id(0),
      idOk(false),
      channels(ADS1299_DEFAULT_CHANNELS),
      detectedChannels(0),
      channelCountForced(false),
      polling(false),
      inRdatac(false),
      irqArmed(false),
      savedRearm(false),
      savedRdatac(false),
      accessDepth(0),
      activeGain(ADS1299_GAIN_24),
      sampleRate(ADS1299_SAMPLERATE_250),
      srb1On(false),
      dataReady(false),
      missed(0),
      sampleCounter(0),
      cmdState(CMD_IDLE),
      cmdLen(0),
      cmdStartMs(0),
      ringOn(false),
      boardReady(false),
      ringPkts(nullptr),
      ringCap(0),
      ringHead(0),
      ringTail(0),
      ringDrop(0),
      drdySem(nullptr),
      spiMutex(nullptr),
      readerStarted(false) {
    memset(&defaults, 0, sizeof(defaults));
    memset(shadow, 0, sizeof(shadow));
    memset(shadowSet, 0, sizeof(shadowSet));
    memset((void *)isrSample, 0, sizeof(isrSample));
    for (uint8_t i = 0; i < 8; i++) {
        chCfg[i].powerDown = false;
        chCfg[i].gain = ADS1299_GAIN_24;
        chCfg[i].mux = ADS1299_MUX_SHORT;
        chCfg[i].srb2 = false;
        chCfg[i].bias = false;
        chCfg[i].packetEnable = true;
    }
}

void ADS129X::setChannelCount(uint8_t count) {
    if (count <= 4) {
        channels = 4;
    } else if (count <= 6) {
        channels = 6;
    } else {
        channels = 8;
    }
    channelCountForced = true;
}

void ADS129X::setSpiPins(int sck, int miso, int mosi) {
    spiSck = sck;
    spiMiso = miso;
    spiMosi = mosi;
}

void ADS129X::setPolling(bool enabled) {
    polling = enabled;
}

void ADS129X::pinWrite(int pin, uint8_t level) {
    if (pin < 0) {
        return;
    }
#if defined(TEENSYDUINO)
    digitalWriteFast(pin, level);
#else
    digitalWrite(pin, level);
#endif
}

void ADS129X::select() {
    SPI.beginTransaction(SPISettings(ADS1299_SCLK_HZ, MSBFIRST, SPI_MODE1));
    pinWrite(pinCs, LOW);
}

void ADS129X::release() {
    // Four tCLK (~1.95 us at 2.048 MHz) before CS rises, then again before
    // the next command. SBAS499C 9.5.2.1 and the command decode time.
    delayMicroseconds(2);
    pinWrite(pinCs, HIGH);
    SPI.endTransaction();
    delayMicroseconds(2);
}

void ADS129X::sendCommand(uint8_t cmd) {
    select();
    SPI.transfer(cmd);
    release();
}

void ADS129X::writeRegRaw(uint8_t address, uint8_t value) {
    select();
    SPI.transfer(ADS1299_CMD_WREG | (address & 0x1F));
    delayMicroseconds(2); // tSDECODE between multi-byte command bytes
    SPI.transfer(0x00);
    delayMicroseconds(2);
    SPI.transfer(value);
    release();
}

uint8_t ADS129X::readRegRaw(uint8_t address) {
    select();
    SPI.transfer(ADS1299_CMD_RREG | (address & 0x1F));
    delayMicroseconds(2);
    SPI.transfer(0x00);
    delayMicroseconds(2);
    uint8_t value = SPI.transfer(0x00);
    release();
    return value;
}

void ADS129X::writeShadow(uint8_t address, uint8_t value) {
    if (address >= ADS1299_REG_COUNT) {
        return;
    }
    writeRegRaw(address, value);
    shadow[address] = value;
    shadowSet[address] = true;
}

void ADS129X::disarmIrq() {
    if (!irqArmed) {
        return;
    }
    detachInterrupt(digitalPinToInterrupt(pinDrdy));
    irqArmed = false;
}

void ADS129X::armIrq() {
    if (polling || irqArmed) {
        return;
    }
    isrInstance = this;
    dataReady = false;
    attachInterrupt(digitalPinToInterrupt(pinDrdy), drdyIsr, FALLING);
    irqArmed = true;
}

void ADS129X::lockSpiBus() {
#if defined(ARDUINO_ARCH_ESP32)
    if (spiMutex != nullptr) {
        xSemaphoreTake(static_cast<SemaphoreHandle_t>(spiMutex), portMAX_DELAY);
    }
#else
    (void)0;
#endif
}

void ADS129X::unlockSpiBus() {
#if defined(ARDUINO_ARCH_ESP32)
    if (spiMutex != nullptr) {
        xSemaphoreGive(static_cast<SemaphoreHandle_t>(spiMutex));
    }
#endif
}

void ADS129X::beginRegisterAccess() {
    if (accessDepth == 0) {
        savedRearm = irqArmed;
        disarmIrq();
        // Wait until the DRDY task finishes any SPI read already in progress.
        lockSpiBus();
        savedRdatac = inRdatac;
        if (inRdatac) {
            sendCommand(ADS1299_CMD_SDATAC);
            inRdatac = false;
        }
    }
    accessDepth++;
}

void ADS129X::endRegisterAccess() {
    if (accessDepth == 0) {
        return;
    }
    accessDepth--;
    if (accessDepth > 0) {
        return;
    }
    if (savedRdatac) {
        sendCommand(ADS1299_CMD_RDATAC);
        inRdatac = true;
    }
    unlockSpiBus();
    if (savedRearm) {
        armIrq();
    }
}

bool ADS129X::begin() {
    if (pinPwdn >= 0) {
        pinMode(pinPwdn, OUTPUT);
        pinWrite(pinPwdn, LOW);
    }
    if (pinReset >= 0) {
        pinMode(pinReset, OUTPUT);
        pinWrite(pinReset, LOW);
    }
    if (pinStart >= 0) {
        pinMode(pinStart, OUTPUT);
        pinWrite(pinStart, LOW);
    }
    pinMode(pinCs, OUTPUT);
    pinWrite(pinCs, HIGH);
    // DRDY is push-pull. GPIO 34 on the ESP32 Feather has no internal pull-up.
#if defined(ARDUINO_ARCH_ESP32)
    pinMode(pinDrdy, INPUT);
    int sck = spiSck >= 0 ? spiSck : ADS1299Board::SCLK;
    int miso = spiMiso >= 0 ? spiMiso : ADS1299Board::DOUT;
    int mosi = spiMosi >= 0 ? spiMosi : ADS1299Board::DIN;
    SPI.begin(sck, miso, mosi, -1);
#else
    pinMode(pinDrdy, INPUT_PULLUP);
    SPI.begin();
    SPI.usingInterrupt(digitalPinToInterrupt(pinDrdy));
#endif

    isrInstance = this;
    dataReady = false;
    missed = 0;
    irqArmed = false;
    inRdatac = true; // power-up default

    if (pinPwdn >= 0) {
        delay(2);
        pinWrite(pinPwdn, HIGH);
    }
    // RESET stays low through tPOR when the pin is wired. 2 tCLK minimum,
    // and the datasheet wants the pulse after supplies and the clock are up.
    delay(ADS1299_TPOR_MS);
    if (pinReset >= 0) {
        pinWrite(pinReset, HIGH);
        delayMicroseconds(10); // 18 tCLK is ~8.8 us at 2.048 MHz
    } else {
        sendCommand(ADS1299_CMD_SDATAC);
        sendCommand(ADS1299_CMD_RESET);
        delayMicroseconds(10);
        inRdatac = true;
    }
    sendCommand(ADS1299_CMD_SDATAC);
    inRdatac = false;

    id = readRegRaw(ADS1299_REG_ID);
    idOk = idLooksLikeAds1299(id);
    detectedChannels = idOk ? channelsFromId(id) : 0;
    if (!channelCountForced) {
        channels = idOk ? detectedChannels : ADS1299_DEFAULT_CHANNELS;
    }
    boardReady = true;
#if defined(ARDUINO_ARCH_ESP32)
    if (ringOn) {
        startEsp32Reader();
    }
#endif
    return idOk;
}

void ADS129X::WAKEUP() {
    beginRegisterAccess();
    sendCommand(ADS1299_CMD_WAKEUP);
    endRegisterAccess();
}

void ADS129X::STANDBY() {
    beginRegisterAccess();
    sendCommand(ADS1299_CMD_STANDBY);
    endRegisterAccess();
}

void ADS129X::pulseReset() {
    disarmIrq();
    if (pinStart >= 0) {
        pinWrite(pinStart, LOW);
    }
    if (pinReset >= 0) {
        pinWrite(pinReset, LOW);
        delayMicroseconds(2);
        pinWrite(pinReset, HIGH);
    } else {
        if (inRdatac) {
            sendCommand(ADS1299_CMD_SDATAC);
            inRdatac = false;
        }
        sendCommand(ADS1299_CMD_RESET);
    }
    delayMicroseconds(10);
    inRdatac = true;
    sendCommand(ADS1299_CMD_SDATAC);
    inRdatac = false;
    dataReady = false;
}

void ADS129X::RESET() {
    pulseReset();
    beginRegisterAccess();
    for (uint8_t address = 1; address < ADS1299_REG_COUNT; address++) {
        if (!shadowSet[address]) {
            continue;
        }
        if (address == ADS1299_REG_LOFF_STATP || address == ADS1299_REG_LOFF_STATN) {
            continue;
        }
        writeRegRaw(address, shadow[address]);
    }
    endRegisterAccess();
    if (shadowSet[ADS1299_REG_CONFIG3] && (shadow[ADS1299_REG_CONFIG3] & 0x80)) {
        delay(ADS1299_TREF_MS);
    }
}

void ADS129X::RDATAC() {
    sendCommand(ADS1299_CMD_RDATAC);
    inRdatac = true;
}

void ADS129X::SDATAC() {
    disarmIrq();
    sendCommand(ADS1299_CMD_SDATAC);
    inRdatac = false;
}

void ADS129X::START() {
    if (pinStart >= 0) {
        if (!inRdatac) {
            sendCommand(ADS1299_CMD_RDATAC);
            inRdatac = true;
        }
        // Pin control. Do not also send the START command while RDATAC is on;
        // RDATAC ignores SPI commands until SDATAC.
        pinWrite(pinStart, HIGH);
    } else {
        if (inRdatac) {
            sendCommand(ADS1299_CMD_SDATAC);
            inRdatac = false;
        }
        sendCommand(ADS1299_CMD_START);
        sendCommand(ADS1299_CMD_RDATAC);
        inRdatac = true;
    }
    missed = 0;
    if (!polling) {
        armIrq();
    }
}

void ADS129X::STOP() {
    disarmIrq();
    dataReady = false;
    if (pinStart >= 0) {
        pinWrite(pinStart, LOW);
    } else {
        if (inRdatac) {
            sendCommand(ADS1299_CMD_SDATAC);
            inRdatac = false;
        }
        sendCommand(ADS1299_CMD_STOP);
    }
}

uint8_t ADS129X::RREG(uint8_t address) {
    beginRegisterAccess();
    uint8_t value = readRegRaw(address);
    endRegisterAccess();
    return value;
}

void ADS129X::RREG(uint8_t address, uint8_t count, uint8_t *dest) {
    if (count == 0 || dest == nullptr) {
        return;
    }
    beginRegisterAccess();
    select();
    SPI.transfer(ADS1299_CMD_RREG | (address & 0x1F));
    delayMicroseconds(2);
    SPI.transfer((uint8_t)(count - 1));
    delayMicroseconds(2);
    for (uint8_t i = 0; i < count; i++) {
        dest[i] = SPI.transfer(0x00);
        if ((uint8_t)(i + 1) < count) {
            delayMicroseconds(2);
        }
    }
    release();
    endRegisterAccess();
}

void ADS129X::WREG(uint8_t address, uint8_t value) {
    beginRegisterAccess();
    writeShadow(address, value);
    endRegisterAccess();
}

uint8_t ADS129X::channelMask() const {
    if (channels >= 8) {
        return 0xFF;
    }
    if (channels == 6) {
        return 0x3F;
    }
    return 0x0F;
}

void ADS129X::writeChannelRaw(uint8_t channel) {
    const Channel &cfg = chCfg[channel - 1];
    uint8_t mux = cfg.powerDown ? ADS1299_MUX_SHORT : cfg.mux;
    uint8_t value = (cfg.powerDown ? 0x80 : 0x00) | (uint8_t)((cfg.gain & 7) << 4) |
                    (cfg.srb2 ? 0x08 : 0x00) | (mux & 7);
    writeShadow(ADS1299_REG_CH1SET + (channel - 1), value);
}

void ADS129X::writeBiasRaw() {
    uint8_t mask = 0;
    uint8_t present = channelMask();
    for (uint8_t i = 0; i < 8; i++) {
        if ((present & (1 << i)) && chCfg[i].bias && !chCfg[i].powerDown) {
            mask |= (uint8_t)(1 << i);
        }
    }
    writeShadow(ADS1299_REG_BIAS_SENSP, mask);
    writeShadow(ADS1299_REG_BIAS_SENSN, mask);
}

void ADS129X::applyDefaults() {
    bool refWasOn = shadowSet[ADS1299_REG_CONFIG3] && (shadow[ADS1299_REG_CONFIG3] & 0x80);
    beginRegisterAccess();
    writeShadow(ADS1299_REG_CONFIG1, (uint8_t)(0x90 | (defaults.rate & 7)));
    writeShadow(ADS1299_REG_CONFIG2, defaults.config2);
    uint8_t config3 = defaults.internalRef ? 0xE0 : 0x60;
    if (defaults.biasMeas) {
        config3 |= 0x10;
    }
    if (defaults.biasAmp) {
        config3 |= 0x0C; // BIASREF_INT and bias buffer enabled
    }
    writeShadow(ADS1299_REG_CONFIG3, config3);
    writeShadow(ADS1299_REG_CONFIG4, 0x00); // continuous conversion
    writeShadow(ADS1299_REG_MISC1, defaults.srb1 ? 0x20 : 0x00);
    sampleRate = defaults.rate & 7;
    srb1On = defaults.srb1;
    activeGain = defaults.gain & 7;
    for (uint8_t ch = 1; ch <= 8; ch++) {
        bool present = ch <= channels;
        chCfg[ch - 1].powerDown = !present;
        chCfg[ch - 1].gain = activeGain;
        chCfg[ch - 1].mux = present ? defaults.mux : ADS1299_MUX_SHORT;
        chCfg[ch - 1].srb2 = false;
        chCfg[ch - 1].bias = present && defaults.bias;
        chCfg[ch - 1].packetEnable = true;
        if (present) {
            writeChannelRaw(ch);
        }
    }
    writeBiasRaw();
    endRegisterAccess();
    if (defaults.internalRef && !refWasOn) {
        delay(ADS1299_TREF_MS);
    }
}

void ADS129X::configureReferential(uint8_t gain, uint8_t rate) {
    defaults.valid = true;
    defaults.gain = (gain > 6) ? ADS1299_GAIN_24 : gain;
    defaults.mux = ADS1299_MUX_NORMAL;
    defaults.rate = (rate > 6) ? ADS1299_SAMPLERATE_250 : rate;
    defaults.config2 = 0xC0;
    defaults.srb1 = true;
    defaults.bias = true;
    defaults.biasAmp = true;
    defaults.biasMeas = false;
    defaults.internalRef = true;
    applyDefaults();
}

void ADS129X::configureDifferential(uint8_t gain, uint8_t rate) {
    defaults.valid = true;
    defaults.gain = (gain > 6) ? ADS1299_GAIN_12 : gain;
    defaults.mux = ADS1299_MUX_NORMAL;
    defaults.rate = (rate > 6) ? ADS1299_SAMPLERATE_250 : rate;
    defaults.config2 = 0xC0;
    defaults.srb1 = false;
    defaults.bias = true;
    defaults.biasAmp = true;
    defaults.biasMeas = false;
    defaults.internalRef = true;
    applyDefaults();
}

void ADS129X::configureTestSignal(uint8_t gain, uint8_t rate, uint8_t calFreq) {
    if (calFreq == 0x02) {
        calFreq = ADS1299_CAL_FCLK_2_21;
    }
    defaults.valid = true;
    defaults.gain = (gain > 6) ? ADS1299_GAIN_1 : gain;
    defaults.mux = ADS1299_MUX_TEST;
    defaults.rate = (rate > 6) ? ADS1299_SAMPLERATE_250 : rate;
    defaults.config2 = (uint8_t)(0xD0 | (calFreq & 0x03));
    defaults.srb1 = false;
    defaults.bias = false;
    defaults.biasAmp = false;
    defaults.biasMeas = false;
    defaults.internalRef = true;
    applyDefaults();
}

void ADS129X::readFrame(int32_t *dest) {
    uint8_t nch = channels;
    if (nch != 4 && nch != 6 && nch != 8) {
        nch = ADS1299_DEFAULT_CHANNELS;
    }
    uint8_t nbytes = (uint8_t)(3 + 3 * nch);
    uint8_t raw[27];
    select();
    for (uint8_t i = 0; i < nbytes; i++) {
        raw[i] = SPI.transfer(0x00);
    }
    release();

    uint32_t status = ((uint32_t)raw[0] << 16) | ((uint32_t)raw[1] << 8) | raw[2];
    dest[0] = (int32_t)status;
    for (uint8_t ch = 0; ch < 8; ch++) {
        if (ch < nch) {
            uint8_t i = (uint8_t)(3 + 3 * ch);
            uint32_t code = ((uint32_t)raw[i] << 16) | ((uint32_t)raw[i + 1] << 8) | raw[i + 2];
            dest[ch + 1] = signExtend24(code);
        } else {
            dest[ch + 1] = 0;
        }
    }
}

#if defined(ARDUINO_ARCH_ESP32)
void IRAM_ATTR ADS129X::drdyIsr() {
#else
void ADS129X::drdyIsr() {
#endif
    ADS129X *dev = isrInstance;
    if (dev == nullptr) {
        return;
    }
#if defined(ARDUINO_ARCH_ESP32)
    if (dev->ringOn && dev->drdySem != nullptr) {
        BaseType_t woken = pdFALSE;
        if (xSemaphoreGiveFromISR(static_cast<SemaphoreHandle_t>(dev->drdySem), &woken) != pdTRUE) {
            uint32_t missedNow = dev->missed;
            dev->missed = missedNow + 1;
        }
        if (woken == pdTRUE) {
            portYIELD_FROM_ISR(woken);
        }
        return;
    }
#endif
    if (dev->dataReady) {
        // C++20 deprecates ++ on volatile. Read, add, store.
        uint32_t missedNow = dev->missed;
        dev->missed = missedNow + 1;
    }
#if defined(ARDUINO_ARCH_ESP32)
    // Ring buffer is off. loop() clocks the frame out. SPI from this ISR is
    // not safe with the Arduino ESP32 driver.
    dev->dataReady = true;
#else
    int32_t local[9];
    dev->readFrame(local);
    for (uint8_t i = 0; i < 9; i++) {
        dev->isrSample[i] = local[i];
    }
    dev->dataReady = true;
    if (dev->ringOn) {
        uint8_t packet[ADS1299_OPENBCI_BYTES];
        uint8_t number = dev->sampleCounter++;
        dev->fillOpenBciPacket(packet, local, number);
        dev->pushRing(packet);
    }
#endif
}

bool ADS129X::getData(int32_t sample[9]) {
    if (sample == nullptr) {
        return false;
    }
    if (polling) {
        if (digitalRead(pinDrdy) != LOW) {
            return false;
        }
        readFrame(sample);
        return true;
    }
    noInterrupts();
    bool ready = dataReady;
    if (ready) {
        dataReady = false;
#if !defined(ARDUINO_ARCH_ESP32)
        for (uint8_t i = 0; i < 9; i++) {
            sample[i] = isrSample[i];
        }
#endif
    }
    interrupts();
    if (!ready) {
        return false;
    }
#if defined(ARDUINO_ARCH_ESP32)
    readFrame(sample);
#endif
    return true;
}

int32_t ADS129X::signExtend24(uint32_t raw24) {
    raw24 &= 0x00FFFFFFu;
    if (raw24 & 0x00800000u) {
        raw24 |= 0xFF000000u;
    }
    return (int32_t)raw24;
}

uint8_t ADS129X::channelsFromId(uint8_t idByte) {
    switch (idByte & 0x03) {
    case 0x00:
        return 4;
    case 0x01:
        return 6;
    case 0x02:
        return 8;
    default:
        return ADS1299_DEFAULT_CHANNELS;
    }
}

bool ADS129X::idLooksLikeAds1299(uint8_t idByte) {
    // DEV_ID is 11b and the reserved bit 4 is 1. NU_CH == 11b is not a device.
    return ((idByte & 0x1C) == 0x1C) && ((idByte & 0x03) != 0x03);
}

uint8_t ADS129X::gainMultiplier(uint8_t gainCode) {
    if (gainCode > 6) {
        return 1;
    }
    return ADS1299_GAIN_VALUE[gainCode];
}

uint8_t ADS129X::channelGainCode(uint8_t channel) const {
    if (channel < 1 || channel > 8) {
        return activeGain;
    }
    return chCfg[channel - 1].gain;
}

const char *ADS129X::deviceName() const {
    uint8_t n = detectedChannels ? detectedChannels : channels;
    if (n == 4) {
        return "ADS1299-4";
    }
    if (n == 6) {
        return "ADS1299-6";
    }
    if (n == 8) {
        return "ADS1299";
    }
    return "ADS1299-x";
}

uint16_t ADS129X::sampleRateHz() const {
    if (sampleRate > 6) {
        return 0;
    }
    return ADS1299_RATE_HZ[sampleRate];
}

void ADS129X::fillOpenBciPacket(uint8_t *dest, const int32_t sample[9]) {
    fillOpenBciPacket(dest, sample, sampleCounter++);
}

void ADS129X::fillOpenBciPacket(uint8_t *dest, const int32_t sample[9], uint8_t sampleNumber) {
    dest[0] = ADS1299_OPENBCI_START;
    dest[1] = sampleNumber;
    for (uint8_t ch = 0; ch < 8; ch++) {
        int32_t code = 0;
        if (ch < channels && chCfg[ch].packetEnable) {
            code = sample[ch + 1];
        }
        uint32_t raw = (uint32_t)code & 0x00FFFFFFu;
        dest[2 + ch * 3] = (uint8_t)((raw >> 16) & 0xFF);
        dest[3 + ch * 3] = (uint8_t)((raw >> 8) & 0xFF);
        dest[4 + ch * 3] = (uint8_t)(raw & 0xFF);
    }
    for (uint8_t i = 26; i < 32; i++) {
        dest[i] = 0;
    }
    dest[32] = ADS1299_OPENBCI_END;
}

void ADS129X::sendOpenBciPacket(Print &out, const int32_t sample[9]) {
    uint8_t packet[ADS1299_OPENBCI_BYTES];
    fillOpenBciPacket(packet, sample);
    out.write(packet, ADS1299_OPENBCI_BYTES);
}

void ADS129X::printCounts(Print &out, const int32_t sample[9]) {
    for (uint8_t ch = 0; ch < 8; ch++) {
        if (ch) {
            out.print(' ');
        }
        int32_t code = 0;
        if (ch < channels && chCfg[ch].packetEnable) {
            code = sample[ch + 1];
        }
        out.print(code);
    }
    out.println();
}

void ADS129X::printBanner(Print &out) {
    out.println("OpenBCI V3 8-16 channel");
    out.print("ADS1299 Device ID: 0x");
    printHex2(out, id);
    out.println();
    out.print(deviceName());
    if (!idOk) {
        out.print(" (ID not recognized, assuming ");
        out.print(channels);
        out.print(" channels)");
    } else if (detectedChannels && detectedChannels != channels) {
        out.print(" detected ");
        out.print(detectedChannels);
        out.print(" channels; SPI frame forced to ");
        out.print(channels);
    } else {
        out.print(" channels: ");
        out.print(channels);
    }
    out.println();
    out.print("SPI frame: ");
    out.print(frameBytes());
    out.println(" bytes (3 status + 3 per channel)");
    out.println("OpenBCI packet: 33 bytes, 8 channels, unused zero-filled");
    out.print("Sample rate: ");
    out.print(sampleRateHz());
    out.println(" SPS");
    out.print("Gain: ");
    out.println(gainMultiplier(activeGain));
    out.print("VREF: ");
    out.println(defaults.internalRef ? "4.5 V internal" : "external");
    out.print("SRB1: ");
    out.println(srb1On ? "closed" : "open");
    out.println("LIS3DH Device ID: 0x00");
    out.println("Firmware: ADS129X-2.0.0");
    out.println("$$$");
}

void ADS129X::printTextHeader(Print &out, const char *note) {
    out.print("# ");
    out.print(deviceName());
    out.print(" id=0x");
    printHex2(out, id);
    if (!idOk) {
        out.print(" (ID not recognized)");
    }
    out.print(" channels=");
    out.print(channels);
    out.print(" frame_bytes=");
    out.print(frameBytes());
    out.print(" sps=");
    out.print(sampleRateHz());
    out.print(" gain=");
    out.println(gainMultiplier(activeGain));
    out.println("# columns: ch1 ch2 ch3 ch4 ch5 ch6 ch7 ch8");
    out.println("# signed 24-bit counts; channels this chip does not have are 0");
    out.println("# uV = count * (4.5 / 8388607 / gain) * 1000000");
    if (note != nullptr && note[0] != '\0') {
        out.print("# ");
        out.println(note);
    }
}

void ADS129X::say(Print &out, bool streaming, const char *msg) {
    if (streaming || msg == nullptr) {
        return;
    }
    out.print(msg);
    out.println("$$$");
}

void ADS129X::setChannelPower(uint8_t channel, bool powerDown) {
    if (channel < 1 || channel > 8) {
        return;
    }
    chCfg[channel - 1].powerDown = powerDown;
    chCfg[channel - 1].packetEnable = !powerDown;
    if (channel > channels) {
        return;
    }
    beginRegisterAccess();
    writeChannelRaw(channel);
    writeBiasRaw();
    endRegisterAccess();
}

void ADS129X::handleHostByte(uint8_t cmd, Print &reply, bool &streaming) {
    if (cmdState != CMD_IDLE && (millis() - cmdStartMs) > 1000) {
        cmdState = CMD_IDLE;
        cmdLen = 0;
        say(reply, streaming,
            "Timeout processing multi byte message - please send all commands at once as of v2");
    }

    if (cmdState == CMD_X) {
        if (cmd == 'X' || cmdLen >= 7) {
            char buf[64];
            bool ok = (cmd == 'X' && cmdLen == 7);
            if (!ok && cmd != 'X') {
                say(reply, streaming, "Failure: 9th char not X");
            } else if (!ok) {
                say(reply, streaming, "Failure: too few chars");
            } else if (cmdBuf[0] < '1' || cmdBuf[0] > '8' || cmdBuf[1] < '0' || cmdBuf[1] > '1' ||
                       cmdBuf[2] < '0' || cmdBuf[2] > '6' || cmdBuf[3] < '0' || cmdBuf[3] > '7' ||
                       cmdBuf[4] < '0' || cmdBuf[4] > '1' || cmdBuf[5] < '0' || cmdBuf[5] > '1' ||
                       cmdBuf[6] < '0' || cmdBuf[6] > '1') {
                say(reply, streaming, "Failure: bad channel setting");
            } else {
                uint8_t channel = (uint8_t)(cmdBuf[0] - '0');
                if (channel > channels) {
                    snprintf(buf, sizeof(buf), "Failure: channel %u not on this %s", channel, deviceName());
                    say(reply, streaming, buf);
                } else {
                    Channel &cfg = chCfg[channel - 1];
                    cfg.powerDown = cmdBuf[1] != '0';
                    cfg.gain = (uint8_t)(cmdBuf[2] - '0');
                    cfg.mux = (uint8_t)(cmdBuf[3] - '0');
                    cfg.bias = cmdBuf[4] != '0';
                    cfg.srb2 = cmdBuf[5] != '0';
                    cfg.packetEnable = !cfg.powerDown;
                    if (channel == 1) {
                        activeGain = cfg.gain;
                    }
                    srb1On = cmdBuf[6] != '0';
                    beginRegisterAccess();
                    writeChannelRaw(channel);
                    writeBiasRaw();
                    writeShadow(ADS1299_REG_MISC1, srb1On ? 0x20 : 0x00);
                    endRegisterAccess();
                    snprintf(buf, sizeof(buf), "Success: Channel set for %u", channel);
                    say(reply, streaming, buf);
                }
            }
            cmdState = CMD_IDLE;
            cmdLen = 0;
            return;
        }
        cmdBuf[cmdLen++] = cmd;
        return;
    }

    if (cmdState == CMD_Z) {
        if (cmd == 'Z' || cmdLen >= 3) {
            // Consume the whole command so a following 'b' is not eaten.
            if (cmd == 'Z' && cmdLen == 3) {
                say(reply, streaming, "Failure: lead-off command not implemented");
            } else if (cmd != 'Z') {
                say(reply, streaming, "Failure: 5th char not Z");
            } else {
                say(reply, streaming, "Failure: too few chars");
            }
            cmdState = CMD_IDLE;
            cmdLen = 0;
            return;
        }
        cmdBuf[cmdLen++] = cmd;
        return;
    }

    if (cmdState == CMD_TILDE) {
        cmdState = CMD_IDLE;
        if (cmd == '~') {
            char buf[40];
            snprintf(buf, sizeof(buf), "Sample rate is %uHz", sampleRateHz());
            say(reply, streaming, buf);
            return;
        }
        if (cmd >= '0' && cmd <= '6') {
            defaults.rate = (uint8_t)(cmd - '0');
            if (!defaults.valid) {
                defaults.valid = true;
                defaults.gain = activeGain;
                defaults.mux = ADS1299_MUX_NORMAL;
                defaults.config2 = 0xC0;
                defaults.srb1 = srb1On;
                defaults.bias = true;
                defaults.biasAmp = true;
                defaults.biasMeas = false;
                defaults.internalRef = true;
            }
            // CONFIG1 write resets the digital filter. Keep the other settings.
            beginRegisterAccess();
            writeShadow(ADS1299_REG_CONFIG1, (uint8_t)(0x90 | defaults.rate));
            sampleRate = defaults.rate;
            endRegisterAccess();
            char buf[40];
            snprintf(buf, sizeof(buf), "Sample rate set to %uHz", sampleRateHz());
            say(reply, streaming, buf);
            return;
        }
        say(reply, streaming, "Failure: bad sample rate");
        return;
    }

    if (cmdState == CMD_SLASH) {
        cmdState = CMD_IDLE;
        if (cmd == '/' || cmd == '0') {
            say(reply, streaming, "Board mode is default");
        } else {
            say(reply, streaming, "Failure: board mode not supported");
        }
        return;
    }

    if (cmd == '\r' || cmd == '\n' || cmd == ' ') {
        return;
    }

    if (cmd == 'b') {
        if (!streaming) {
            sampleCounter = 0;
            missed = 0;
            RDATAC();
            START();
            streaming = true;
        }
        return;
    }
    if (cmd == 's') {
        if (streaming) {
            streaming = false;
            STOP();
            SDATAC();
        }
        return;
    }
    if (cmd == 'v') {
        streaming = false;
        RESET();
        printBanner(reply);
        return;
    }
    if (cmd == 'V') {
        say(reply, streaming, "ADS129X-2.0.0");
        return;
    }
    if (cmd == '?') {
        if (streaming) {
            return;
        }
        uint8_t regs[ADS1299_REG_COUNT];
        RREG(0, ADS1299_REG_COUNT, regs);
        reply.println("ADS1299 registers");
        for (uint8_t address = 0; address < ADS1299_REG_COUNT; address++) {
            printHex2(reply, address);
            reply.print(' ');
            reply.print(ADS1299_REG_NAME[address]);
            reply.print(" 0x");
            printHex2(reply, regs[address]);
            reply.println();
        }
        reply.print("missed_samples ");
        reply.println(missedSamples());
        reply.println("$$$");
        return;
    }
    if (cmd == 'd') {
        if (defaults.valid) {
            applyDefaults();
            say(reply, streaming, "updating channel settings to default");
        } else {
            say(reply, streaming, "Failure: no default channel settings");
        }
        return;
    }
    if (cmd == 'D') {
        if (streaming) {
            return;
        }
        char buf[7];
        buf[0] = '0';
        buf[1] = (char)('0' + (defaults.valid ? defaults.gain : activeGain));
        buf[2] = (char)('0' + (defaults.valid ? defaults.mux : ADS1299_MUX_NORMAL));
        buf[3] = (defaults.valid && defaults.bias) ? '1' : '0';
        buf[4] = '0';
        buf[5] = (defaults.valid && defaults.srb1) ? '1' : '0';
        buf[6] = '\0';
        reply.print(buf);
        reply.println("$$$");
        return;
    }
    if (cmd == 'x') {
        cmdState = CMD_X;
        cmdLen = 0;
        cmdStartMs = millis();
        return;
    }
    if (cmd == 'z') {
        cmdState = CMD_Z;
        cmdLen = 0;
        cmdStartMs = millis();
        return;
    }
    if (cmd == '~') {
        cmdState = CMD_TILDE;
        cmdStartMs = millis();
        return;
    }
    if (cmd == '/') {
        cmdState = CMD_SLASH;
        cmdStartMs = millis();
        return;
    }

    static const char kOn[8] = {'!', '@', '#', '$', '%', '^', '&', '*'};
    if (cmd >= '1' && cmd <= '8') {
        setChannelPower((uint8_t)(cmd - '0'), true);
        return;
    }
    for (uint8_t i = 0; i < 8; i++) {
        if (cmd == (uint8_t)kOn[i]) {
            setChannelPower((uint8_t)(i + 1), false);
            return;
        }
    }
}

bool ADS129X::enableSampleRing(uint16_t capacity) {
    if (capacity < 16) {
        capacity = 16;
    }
    if (ringPkts != nullptr) {
        free(ringPkts);
        ringPkts = nullptr;
    }
    ringPkts = static_cast<uint8_t *>(malloc((size_t)capacity * ADS1299_OPENBCI_BYTES));
    if (ringPkts == nullptr) {
        ringOn = false;
        return false;
    }
    ringCap = capacity;
    ringHead = 0;
    ringTail = 0;
    ringDrop = 0;
    ringOn = true;
#if defined(ARDUINO_ARCH_ESP32)
    if (boardReady) {
        startEsp32Reader();
    }
#endif
    return true;
}

bool ADS129X::pushRing(const uint8_t *packet) {
    if (!ringOn || ringPkts == nullptr || packet == nullptr) {
        return false;
    }
    uint16_t head = ringHead;
    uint16_t next = (uint16_t)((head + 1) % ringCap);
    if (next == ringTail) {
        uint32_t dropped = ringDrop;
        ringDrop = dropped + 1;
        return false;
    }
    memcpy(ringPkts + (size_t)head * ADS1299_OPENBCI_BYTES, packet, ADS1299_OPENBCI_BYTES);
    __sync_synchronize();
    ringHead = next;
    return true;
}

bool ADS129X::popOpenBciPacket(uint8_t dest[ADS1299_OPENBCI_BYTES]) {
    if (!ringOn || ringPkts == nullptr || dest == nullptr) {
        return false;
    }
    uint16_t tail = ringTail;
    if (tail == ringHead) {
        return false;
    }
    memcpy(dest, ringPkts + (size_t)tail * ADS1299_OPENBCI_BYTES, ADS1299_OPENBCI_BYTES);
    __sync_synchronize();
    ringTail = (uint16_t)((tail + 1) % ringCap);
    return true;
}

void ADS129X::captureIntoRing() {
    int32_t sample[9];
    readFrame(sample);
    uint8_t packet[ADS1299_OPENBCI_BYTES];
    uint8_t number = sampleCounter++;
    fillOpenBciPacket(packet, sample, number);
    pushRing(packet);
}

#if defined(ARDUINO_ARCH_ESP32)
void ADS129X::startEsp32Reader() {
    if (readerStarted || !ringOn || drdySem != nullptr) {
        return;
    }
    drdySem = xSemaphoreCreateCounting(ringCap, 0);
    spiMutex = xSemaphoreCreateMutex();
    if (drdySem == nullptr || spiMutex == nullptr) {
        return;
    }
    xTaskCreatePinnedToCore(readerTask, "ads-drdy", 4096, this, 5, nullptr, 1);
    readerStarted = true;
}

void ADS129X::readerTask(void *arg) {
    ADS129X *dev = static_cast<ADS129X *>(arg);
    for (;;) {
        if (xSemaphoreTake(static_cast<SemaphoreHandle_t>(dev->drdySem), portMAX_DELAY) != pdTRUE) {
            continue;
        }
        if (xSemaphoreTake(static_cast<SemaphoreHandle_t>(dev->spiMutex), pdMS_TO_TICKS(5)) != pdTRUE) {
            uint32_t missedNow = dev->missed;
            dev->missed = missedNow + 1;
            continue;
        }
        dev->captureIntoRing();
        xSemaphoreGive(static_cast<SemaphoreHandle_t>(dev->spiMutex));
    }
}
#endif
