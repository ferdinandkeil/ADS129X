// Differential EMG on the same Teensy + ADS1299 carrier as Serial_OBCI.
// Same 33-byte OpenBCI packets. SRB1 stays open so each INxP/INxN pair is
// a real differential input. Gain is 12 and the rate is 2 kSPS, so the
// OpenBCI GUI's default scale (gain 24, 250 SPS) will not match.
// Send 'b' to start and 's' to stop. '~' plus '0'..'6' changes the rate
// (0 = 16 kSPS ... 6 = 250 SPS) while stopped.

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
    ads.configureDifferential(ADS1299_GAIN_12, ADS1299_SAMPLERATE_2k);
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
