// Same referential EEG setup as Serial_OBCI, printed as eight signed
// counts for the Arduino Serial Plotter. Channels the ADS1299-6 does not
// have stay at 0, so the plotter still has a fixed 8-column line.
// The first lines start with '#' ; reset the plotter after it opens if
// that header is drawn once. USB serial baud is ignored on Teensy.

#include <ADS129X.h>

ADS129X ads(ADS1299Board::DRDY, ADS1299Board::CS, ADS1299Board::RESET,
            ADS1299Board::PWDN, ADS1299Board::START);

void setup() {
    Serial.begin(115200);
    uint32_t started = millis();
    while (!Serial && (millis() - started) < 2000) {
    }

    ads.begin();
    ads.configureReferential(ADS1299_GAIN_24, ADS1299_SAMPLERATE_250);
    ads.printTextHeader(Serial, "referential EEG, SRB1 closed, bias on");
    ads.RDATAC();
    ads.START();
}

void loop() {
    int32_t sample[9];
    if (ads.getData(sample)) {
        ads.printCounts(Serial, sample);
    }
}
