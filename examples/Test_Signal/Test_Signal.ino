// On-chip square wave, no electrodes. CONFIG2 INT_CAL, every present
// channel muxed to the test signal, gain 1, 250 SPS.
// Amplitude is VREF/2400 = 1.875 mV at the internal 4.5 V reference,
// so the counts should step by about +/-3495. That is gain * 2^24 / 4800.
// Output is the same 8-column signed-count text as Serial_Plotter.
// The first few conversions are sinc-filter settling (about 16 ms at 250 SPS).

#include <ADS129X.h>

ADS129X ads(ADS1299Board::DRDY, ADS1299Board::CS, ADS1299Board::RESET,
            ADS1299Board::PWDN, ADS1299Board::START);

void setup() {
    Serial.begin(115200);
    uint32_t started = millis();
    while (!Serial && (millis() - started) < 2000) {
    }

    ads.begin();
    ads.configureTestSignal(ADS1299_GAIN_1, ADS1299_SAMPLERATE_250, ADS1299_CAL_FCLK_2_20);
    ads.printTextHeader(Serial, "internal cal ~2 Hz (fCLK/2^20), expect about +/-3495 counts at gain 1");
    ads.RDATAC();
    ads.START();
}

void loop() {
    int32_t sample[9];
    if (ads.getData(sample)) {
        ads.printCounts(Serial, sample);
    }
}
