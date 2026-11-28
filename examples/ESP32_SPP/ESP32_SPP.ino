// Same referential EEG setup and 33-byte OpenBCI packets as Serial_OBCI,
// on the original ESP32 (ESP32-WROOM-32E or ESP32-PICO). Bluetooth Classic
// SPP is required. ESP32-S2, S3, C3, and C6 do not have it.
//
// Board: Adafruit ESP32 Feather V2, or an ESP32 Dev Module wired to the
// same GPIO numbers (not the DevKit's default VSPI pins).
// Pair with "ADS1299-EEG". USB serial is 115200 via the Feather's USB-UART.
// 'b' / 's' / 'v' work on either port. Packets go to both.

#include <Arduino.h>

#if !defined(CONFIG_IDF_TARGET_ESP32)
#error "Classic Bluetooth SPP needs the original ESP32 (ESP32-WROOM-32E or ESP32-PICO, for example the Adafruit ESP32 Feather V2). ESP32-S2/S3/C3/C6 do not implement Bluetooth Classic."
#elif !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error "Enable Bluetooth for this ESP32 board. Bluetooth Classic is off in the current build."
#else

#include <ADS129X.h>
#include <BluetoothSerial.h>

namespace {
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
BluetoothSerial SerialBT;
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
} // namespace

ADS129X ads(ADS1299Board::DRDY, ADS1299Board::CS, ADS1299Board::RESET,
            ADS1299Board::PWDN, ADS1299Board::START);

static bool streaming = false;
static bool clientWasConnected = false;

static void service(Stream &io) {
    while (io.available()) {
        ads.handleHostByte((uint8_t)io.read(), io, streaming);
    }
}

void setup() {
    Serial.begin(115200);
    delay(200);

    ads.begin();
    ads.configureReferential(ADS1299_GAIN_24, ADS1299_SAMPLERATE_250);
    ads.printBanner(Serial);

    if (!SerialBT.begin("ADS1299-EEG")) {
        Serial.println("Bluetooth SPP failed to start");
    }
}

void loop() {
    service(Serial);
    service(SerialBT);

    bool client = SerialBT.hasClient();
    if (client && !clientWasConnected) {
        ads.printBanner(SerialBT);
    }
    clientWasConnected = client;

    if (!streaming) {
        return;
    }
    int32_t sample[9];
    if (!ads.getData(sample)) {
        return;
    }
    uint8_t packet[ADS1299_OPENBCI_BYTES];
    ads.fillOpenBciPacket(packet, sample);
    Serial.write(packet, ADS1299_OPENBCI_BYTES);
    if (client) {
        SerialBT.write(packet, ADS1299_OPENBCI_BYTES);
    }
}

#endif
