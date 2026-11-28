// Referential EEG on the Teensy 3.6 / 4.0 / 4.1 + ADS1299 carrier.
// Streams standard 33-byte OpenBCI packets (8 channels). An ADS1299-6,
// which is the part on the photographed board, fills channels 7 and 8
// with zero. The ID register selects 4, 6, or 8 when it reads back as
// an ADS1299; otherwise the frame is 6 channels.
//
// Open the port at 115200 (Teensy USB ignores the number). The banner ends
// with $$$. Send 'b' to start binary samples and 's' to stop. The OpenBCI
// GUI scale assumes gain 24 and 4.5 V, which is what this sketch programs.
//
// Wiring is ADS1299Board in ADS129X.h. SPI is mode 1 on pins 11, 12, and 13.

#include <ADS129X.h>

ADS129X ads(ADS1299Board::DRDY, ADS1299Board::CS, ADS1299Board::RESET,
            ADS1299Board::PWDN, ADS1299Board::START);

static bool streaming = false;

void setup() {
    Serial.begin(115200);
    uint32_t started = millis();
    while (!Serial && (millis() - started) < 2000) {
    }

    ads.begin();
    ads.configureReferential(ADS1299_GAIN_24, ADS1299_SAMPLERATE_250);
    ads.printBanner(Serial);
}

void loop() {
    while (Serial.available()) {
        ads.handleHostByte((uint8_t)Serial.read(), Serial, streaming);
    }
    if (!streaming) {
        return;
    }
    int32_t sample[9];
    if (ads.getData(sample)) {
        ads.sendOpenBciPacket(Serial, sample);
    }
}
