// ESP32 / ESP32-S3 dev-board sketch for the ADS1299.
//
// Streams the OpenBCI Cyton protocol:
//   33-byte samples (0xA0, counter, 8x24-bit, 6 aux bytes, 0xC0)
//   ASCII commands b/s, v, 1-8 / !@#$%^&*, x...X, d/D, ?, ~
//
// WiFi follows the OpenBCI WiFi Shield: HTTP on port 80, and the board
// opens a TCP (or UDP) client to the computer. BrainFlow CYTON_WIFI_BOARD
// and the OpenBCI GUI's Cyton WiFi mode use that shape.
//
// Bluetooth:
//   original ESP32  - Classic SPP serial, name ADS1299-EEG (GUI Cyton serial)
//   ESP32-S3        - BLE Nordic UART of the same bytes (no Classic radio)
//
// DRDY wakes a high-priority task that clocks SPI into a 512-packet ring.
// WiFi and Bluetooth writes run from loop(), so a radio stall does not
// skip the SPI read. Overflows show up in ringDropped().
//
// Pins below are the Arduino defaults for ESP32 Dev Module and ESP32-S3
// Dev Module. Override any of them with a build flag, for example
// -DADS_PIN_DRDY=34. They are not the Feather V2 map used by ESP32_SPP.

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>

#if !defined(CONFIG_IDF_TARGET_ESP32) && !defined(CONFIG_IDF_TARGET_ESP32S3)
#error "This sketch targets the original ESP32 or the ESP32-S3. ESP32-C3 and ESP32-S2 are not built here."
#endif

#include <ADS129X.h>

#if defined(CONFIG_IDF_TARGET_ESP32)
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error "Enable Bluetooth so Classic SPP can build on the original ESP32."
#endif
#include <BluetoothSerial.h>
#define ADS_HAS_SPP 1
#endif

#if defined(CONFIG_IDF_TARGET_ESP32S3)
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#define ADS_HAS_BLE 1
#endif

#ifndef STA_SSID
#define STA_SSID ""
#endif
#ifndef STA_PASSWORD
#define STA_PASSWORD ""
#endif
#ifndef AP_SSID
#define AP_SSID "ADS1299-EEG"
#endif
#ifndef AP_PASSWORD
#define AP_PASSWORD "openbci123"
#endif

#if defined(CONFIG_IDF_TARGET_ESP32S3)
#ifndef ADS_PIN_SCLK
#define ADS_PIN_SCLK 12
#endif
#ifndef ADS_PIN_MISO
#define ADS_PIN_MISO 13
#endif
#ifndef ADS_PIN_MOSI
#define ADS_PIN_MOSI 11
#endif
#ifndef ADS_PIN_CS
#define ADS_PIN_CS 10
#endif
#ifndef ADS_PIN_DRDY
#define ADS_PIN_DRDY 4
#endif
#ifndef ADS_PIN_START
#define ADS_PIN_START 5
#endif
#ifndef ADS_PIN_RESET
#define ADS_PIN_RESET 6
#endif
#ifndef ADS_PIN_PWDN
#define ADS_PIN_PWDN 7
#endif
#else
#ifndef ADS_PIN_SCLK
#define ADS_PIN_SCLK 18
#endif
#ifndef ADS_PIN_MISO
#define ADS_PIN_MISO 19
#endif
#ifndef ADS_PIN_MOSI
#define ADS_PIN_MOSI 23
#endif
#ifndef ADS_PIN_CS
#define ADS_PIN_CS 5
#endif
#ifndef ADS_PIN_DRDY
#define ADS_PIN_DRDY 27
#endif
#ifndef ADS_PIN_START
#define ADS_PIN_START 25
#endif
#ifndef ADS_PIN_RESET
#define ADS_PIN_RESET 26
#endif
#ifndef ADS_PIN_PWDN
#define ADS_PIN_PWDN 32
#endif
#endif

static const uint16_t RING_PACKETS = 512;
static const char *DEVICE_NAME = "ADS1299-EEG";

ADS129X ads(ADS_PIN_DRDY, ADS_PIN_CS, ADS_PIN_RESET, ADS_PIN_PWDN, ADS_PIN_START);
WebServer server(80);
WiFiUDP ssdp;
WiFiUDP udpOut;
WiFiClient tcpClient;

static bool adsOn = false;
static bool wantUsb = false;
static bool wantSpp = false;
static bool wantBle = false;
static bool wantWifi = false;
static bool tcpWanted = false;
static bool udpWanted = false;
static bool jsonOutput = false;
static bool useDelimiter = true;
static bool udpRedundant = false;
static int streamLatencyUs = 10000;
static IPAddress streamHost;
static uint16_t streamPort = 0;
static bool staMode = false;

#if defined(ADS_HAS_SPP)
namespace {
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
BluetoothSerial SerialBT;
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
bool sppWasUp = false;
}  // namespace
#endif

#if defined(ADS_HAS_BLE)
static const char *BLE_SERVICE = "6E400001-B5A3-F4F5-A9E5-9E4000000000";
static const char *BLE_RX = "6E400002-B5A3-F4F5-A9E5-9E4000000000";
static const char *BLE_TX = "6E400003-B5A3-F4F5-A9E5-9E4000000000";
static BLECharacteristic *bleTx = nullptr;
static BLEServer *bleServer = nullptr;
static bool bleConnected = false;
static const uint8_t BLE_CMD_CAP = 64;
static volatile uint8_t bleCmd[BLE_CMD_CAP];
static volatile uint8_t bleCmdHead = 0;
static volatile uint8_t bleCmdTail = 0;

static void pushBleByte(uint8_t value) {
    uint8_t next = (uint8_t)((bleCmdHead + 1) % BLE_CMD_CAP);
    if (next == bleCmdTail) {
        return;
    }
    bleCmd[bleCmdHead] = value;
    bleCmdHead = next;
}

class BleRxCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *characteristic) override {
        String value = characteristic->getValue();
        for (unsigned i = 0; i < value.length(); i++) {
            pushBleByte((uint8_t)value[i]);
        }
    }
};

class BleServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *server) override {
        bleConnected = true;
        (void)server;
    }
    void onDisconnect(BLEServer *server) override {
        bleConnected = false;
        wantBle = false;
        server->startAdvertising();
    }
};
#endif

static void startAds() {
    if (adsOn) {
        return;
    }
    bool flag = false;
    ads.handleHostByte('b', Serial, flag);
    adsOn = true;
}

static void stopAdsIfIdle() {
    if (wantUsb || wantSpp || wantBle || wantWifi) {
        return;
    }
    if (!adsOn) {
        return;
    }
    bool flag = true;
    ads.handleHostByte('s', Serial, flag);
    adsOn = false;
}

static void feedBytes(const uint8_t *data, size_t count, Print &reply, bool &want) {
    for (size_t i = 0; i < count; i++) {
        uint8_t c = data[i];
        if (c == 'b') {
            want = true;
            startAds();
            continue;
        }
        if (c == 's') {
            want = false;
            stopAdsIfIdle();
            continue;
        }
        bool before = adsOn;
        ads.handleHostByte(c, reply, adsOn);
        if (before && !adsOn) {
            wantUsb = false;
            wantSpp = false;
            wantBle = false;
            wantWifi = false;
        }
    }
}

static void feedStream(Stream &io, bool &want) {
    while (io.available() > 0) {
        uint8_t c = (uint8_t)io.read();
        feedBytes(&c, 1, io, want);
    }
}

static bool jsonSpan(const String &body, const char *key, int &begin, int &end) {
    String pattern = String("\"") + key + "\"";
    int found = body.indexOf(pattern);
    if (found < 0) {
        return false;
    }
    int colon = body.indexOf(':', found + pattern.length());
    if (colon < 0) {
        return false;
    }
    int i = colon + 1;
    while (i < (int)body.length() && isspace((unsigned char)body[i])) {
        i++;
    }
    if (i >= (int)body.length()) {
        return false;
    }
    if (body[i] == '"') {
        int close = body.indexOf('"', i + 1);
        if (close < 0) {
            return false;
        }
        begin = i + 1;
        end = close;
        return true;
    }
    int j = i;
    while (j < (int)body.length() && body[j] != ',' && body[j] != '}' &&
           !isspace((unsigned char)body[j])) {
        j++;
    }
    begin = i;
    end = j;
    return begin < end;
}

static bool jsonString(const String &body, const char *key, String &out) {
    int begin = 0;
    int end = 0;
    if (!jsonSpan(body, key, begin, end)) {
        return false;
    }
    out = body.substring(begin, end);
    return true;
}

static bool jsonInt(const String &body, const char *key, int &out) {
    String text;
    if (!jsonString(body, key, text)) {
        return false;
    }
    out = text.toInt();
    return true;
}

static bool jsonBool(const String &body, const char *key, bool &out) {
    String text;
    if (!jsonString(body, key, text)) {
        return false;
    }
    out = text == "true" || text == "1";
    return true;
}

static void sendJson(int code, const String &body) {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(code, "application/json", body);
}

static void sendOptions() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    server.send(204);
}

static String gainsJson() {
    String out = "[";
    for (uint8_t ch = 1; ch <= 8; ch++) {
        if (ch > 1) {
            out += ',';
        }
        out += String(ADS129X::gainMultiplier(ads.channelGainCode(ch)));
    }
    out += ']';
    return out;
}

static String boardJson() {
    bool connected = ads.idValid();
    String out = "{\"board_connected\":";
    out += connected ? "true" : "false";
    out += ",\"board_type\":\"";
    out += connected ? "cyton" : "none";
    out += "\",\"num_channels\":8,\"gains\":";
    out += gainsJson();
    out += '}';
    return out;
}

static String tcpJson(bool connected) {
    String out = "{\"connected\":";
    out += connected ? "true" : "false";
    out += ",\"delimiter\":";
    out += useDelimiter ? "true" : "false";
    out += ",\"ip\":\"";
    out += streamHost.toString();
    out += "\",\"output\":\"";
    out += jsonOutput ? "json" : "raw";
    out += "\",\"port\":";
    out += String(streamPort);
    out += ",\"latency\":";
    out += String(streamLatencyUs);
    out += '}';
    return out;
}

static void applyStreamBody(const String &body) {
    String ipText;
    if (jsonString(body, "ip", ipText)) {
        streamHost.fromString(ipText);
    }
    int port = 0;
    if (jsonInt(body, "port", port) && port > 0 && port < 65536) {
        streamPort = (uint16_t)port;
    }
    String output;
    if (jsonString(body, "output", output)) {
        jsonOutput = output == "json";
    }
    bool delimiter = useDelimiter;
    if (jsonBool(body, "delimiter", delimiter)) {
        useDelimiter = delimiter;
    }
    int latency = streamLatencyUs;
    if (jsonInt(body, "latency", latency) && latency > 0) {
        streamLatencyUs = latency;
    }
    bool redundant = false;
    if (jsonBool(body, "redundancy", redundant)) {
        udpRedundant = redundant;
    }
}

static void handleTcpPost() {
    String body = server.arg("plain");
    applyStreamBody(body);
    if (streamPort == 0 || streamHost == IPAddress(0, 0, 0, 0)) {
        sendJson(400, "{\"connected\":false,\"error\":\"ip and port are required\"}");
        return;
    }
    tcpClient.stop();
    udpWanted = false;
    bool ok = tcpClient.connect(streamHost, streamPort);
    if (ok) {
        tcpClient.setNoDelay(true);
    }
    tcpWanted = ok;
    sendJson(ok ? 200 : 504, tcpJson(ok));
}

static void handleUdpPost() {
    String body = server.arg("plain");
    applyStreamBody(body);
    if (streamPort == 0 || streamHost == IPAddress(0, 0, 0, 0)) {
        sendJson(400, "{\"connected\":false,\"error\":\"ip and port are required\"}");
        return;
    }
    tcpClient.stop();
    tcpWanted = false;
    udpWanted = true;
    if (udpOut.begin(0) == 0) {
        // begin() may already be open; a zero result is still usable on some cores.
    }
    sendJson(200, tcpJson(true));
}

static void handleCommand() {
    String body = server.arg("plain");
    String command;
    if (!jsonString(body, "command", command) || command.length() == 0) {
        sendJson(400, "{\"error\":\"missing command\"}");
        return;
    }
    String reply;
    class Capture : public Print {
    public:
        String text;
        size_t write(uint8_t c) override {
            text += (char)c;
            return 1;
        }
        size_t write(const uint8_t *buffer, size_t size) override {
            for (size_t i = 0; i < size; i++) {
                text += (char)buffer[i];
            }
            return size;
        }
    } capture;
    feedBytes((const uint8_t *)command.c_str(), command.length(), capture, wantWifi);
    String out = "{\"ok\":true,\"response\":\"";
    for (unsigned i = 0; i < capture.text.length(); i++) {
        char c = capture.text[i];
        if (c == '"' || c == '\\') {
            out += '\\';
        }
        if (c == '\n' || c == '\r') {
            continue;
        }
        out += c;
    }
    out += "\"}";
    sendJson(200, out);
}

static int32_t packetChannel(const uint8_t *packet, uint8_t channel) {
    uint8_t i = (uint8_t)(2 + channel * 3);
    uint32_t raw = ((uint32_t)packet[i] << 16) | ((uint32_t)packet[i + 1] << 8) | packet[i + 2];
    return ADS129X::signExtend24(raw);
}

static int64_t countsToNanovolts(int32_t code, uint8_t gainCode) {
    uint8_t gain = ADS129X::gainMultiplier(gainCode);
    if (gain == 0) {
        return 0;
    }
    return ((int64_t)code * 9000000000LL) / ((int64_t)gain * 16777216LL);
}

static void writeJsonSample(Print &out, const uint8_t *packet) {
    out.print("{\"chunk\":[{\"data\":[");
    for (uint8_t ch = 0; ch < 8; ch++) {
        if (ch) {
            out.print(',');
        }
        int64_t nv = countsToNanovolts(packetChannel(packet, ch), ads.channelGainCode(ch + 1));
        out.print((long long)nv);
    }
    out.print("],\"timestamp\":");
    out.print((unsigned long)micros());
    out.print("}]}");
    if (useDelimiter) {
        out.print("\r\n");
    }
}

static void writeRawOrJson(Print &out, const uint8_t *packet) {
    if (jsonOutput) {
        writeJsonSample(out, packet);
    } else {
        out.write(packet, ADS1299_OPENBCI_BYTES);
    }
}

class ChunkPrint : public Print {
public:
    uint8_t data[480];
    size_t length = 0;
    size_t write(uint8_t c) override {
        if (length < sizeof(data)) {
            data[length++] = c;
        }
        return 1;
    }
    size_t write(const uint8_t *buffer, size_t size) override {
        for (size_t i = 0; i < size; i++) {
            write(buffer[i]);
        }
        return size;
    }
};

static void sendUdp(const uint8_t *packet) {
    ChunkPrint chunk;
    if (jsonOutput) {
        writeJsonSample(chunk, packet);
    }
    int copies = udpRedundant ? 3 : 1;
    for (int copy = 0; copy < copies; copy++) {
        if (!udpOut.beginPacket(streamHost, streamPort)) {
            continue;
        }
        if (jsonOutput) {
            udpOut.write(chunk.data, chunk.length);
        } else {
            udpOut.write(packet, ADS1299_OPENBCI_BYTES);
        }
        udpOut.endPacket();
    }
}

#if defined(ADS_HAS_BLE)
static void bleNotify(const uint8_t *packet) {
    if (!bleConnected || bleTx == nullptr) {
        return;
    }
    // 20-byte notifications fit the default ATT MTU (23). The client
    // reassembles them into the 33-byte Cyton packet.
    const uint16_t chunk = 20;
    uint16_t offset = 0;
    while (offset < ADS1299_OPENBCI_BYTES) {
        uint16_t n = (uint16_t)(ADS1299_OPENBCI_BYTES - offset);
        if (n > chunk) {
            n = chunk;
        }
        bleTx->setValue(packet + offset, n);
        bleTx->notify();
        offset = (uint16_t)(offset + n);
    }
}

static void startBle() {
    BLEDevice::init(DEVICE_NAME);
    BLEDevice::setMTU(185);
    bleServer = BLEDevice::createServer();
    bleServer->setCallbacks(new BleServerCallbacks());
    BLEService *service = bleServer->createService(BLE_SERVICE);
    bleTx = service->createCharacteristic(BLE_TX, BLECharacteristic::PROPERTY_NOTIFY);
    BLECharacteristic *rx = service->createCharacteristic(
        BLE_RX, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
    rx->setCallbacks(new BleRxCallbacks());
    service->start();
    BLEAdvertising *advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(BLE_SERVICE);
    advertising->start();
}
#endif

static void pollSsdp() {
    int packetSize = ssdp.parsePacket();
    if (packetSize <= 0) {
        return;
    }
    char buf[256];
    int len = ssdp.read(buf, sizeof(buf) - 1);
    if (len <= 0) {
        return;
    }
    buf[len] = '\0';
    if (strstr(buf, "M-SEARCH") == nullptr) {
        return;
    }
    IPAddress ip = staMode ? WiFi.localIP() : WiFi.softAPIP();
    char response[320];
    snprintf(response, sizeof(response),
             "HTTP/1.1 200 OK\r\n"
             "CACHE-CONTROL: max-age=1800\r\n"
             "LOCATION: http://%s\r\n"
             "ST: urn:schemas-upnp-org:device:Basic:1\r\n"
             "USN: uuid:ads1299-eeg::urn:schemas-upnp-org:device:Basic:1\r\n"
             "\r\n",
             ip.toString().c_str());
    ssdp.beginPacket(ssdp.remoteIP(), ssdp.remotePort());
    ssdp.write((const uint8_t *)response, strlen(response));
    ssdp.endPacket();
}

static void drainBleCommands() {
#if defined(ADS_HAS_BLE)
    while (bleCmdTail != bleCmdHead) {
        uint8_t c = bleCmd[bleCmdTail];
        bleCmdTail = (uint8_t)((bleCmdTail + 1) % BLE_CMD_CAP);
        feedBytes(&c, 1, Serial, wantBle);
    }
#endif
}

static void pumpPackets() {
    if (!adsOn) {
        return;
    }
    uint8_t packet[ADS1299_OPENBCI_BYTES];
    for (int n = 0; n < 48; n++) {
        if (!ads.popOpenBciPacket(packet)) {
            return;
        }
        if (wantWifi && tcpWanted && tcpClient.connected()) {
            writeRawOrJson(tcpClient, packet);
        }
        if (wantWifi && udpWanted) {
            sendUdp(packet);
        }
#if defined(ADS_HAS_SPP)
        if (wantSpp && SerialBT.hasClient()) {
            SerialBT.write(packet, ADS1299_OPENBCI_BYTES);
        }
#endif
#if defined(ADS_HAS_BLE)
        if (wantBle && bleConnected) {
            bleNotify(packet);
        }
#endif
        if (wantUsb) {
            Serial.write(packet, ADS1299_OPENBCI_BYTES);
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println();
    Serial.println("# ADS1299 OpenBCI on ESP32");
    Serial.print("# pins SCLK=");
    Serial.print(ADS_PIN_SCLK);
    Serial.print(" MISO=");
    Serial.print(ADS_PIN_MISO);
    Serial.print(" MOSI=");
    Serial.print(ADS_PIN_MOSI);
    Serial.print(" CS=");
    Serial.print(ADS_PIN_CS);
    Serial.print(" DRDY=");
    Serial.print(ADS_PIN_DRDY);
    Serial.print(" START=");
    Serial.print(ADS_PIN_START);
    Serial.print(" RESET=");
    Serial.print(ADS_PIN_RESET);
    Serial.print(" PWDN=");
    Serial.println(ADS_PIN_PWDN);

    if (!ads.enableSampleRing(RING_PACKETS)) {
        Serial.println("# sample ring allocation failed");
    }
    ads.setSpiPins(ADS_PIN_SCLK, ADS_PIN_MISO, ADS_PIN_MOSI);
    ads.begin();
    ads.configureReferential(ADS1299_GAIN_24, ADS1299_SAMPLERATE_250);
    ads.printBanner(Serial);

    if (STA_SSID[0] != '\0') {
        WiFi.mode(WIFI_STA);
        WiFi.begin(STA_SSID, STA_PASSWORD);
        Serial.print("# joining ");
        Serial.println(STA_SSID);
        uint32_t started = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - started < 12000) {
            delay(250);
            Serial.print('.');
        }
        Serial.println();
        staMode = WiFi.status() == WL_CONNECTED;
    }
    if (!staMode) {
        WiFi.mode(WIFI_AP);
        IPAddress ap(192, 168, 4, 1);
        WiFi.softAPConfig(ap, ap, IPAddress(255, 255, 255, 0));
        WiFi.softAP(AP_SSID, AP_PASSWORD);
        Serial.print("# AP ");
        Serial.print(AP_SSID);
        Serial.print(" password ");
        Serial.println(AP_PASSWORD);
    }
    IPAddress ip = staMode ? WiFi.localIP() : WiFi.softAPIP();
    Serial.print("# HTTP http://");
    Serial.println(ip);

    ssdp.beginMulticast(IPAddress(239, 255, 255, 250), 1900);

    server.on("/board", HTTP_GET, []() { sendJson(200, boardJson()); });
    server.on("/board", HTTP_OPTIONS, sendOptions);
    server.on("/all", HTTP_GET, []() {
        String body = boardJson();
        body.remove(body.length() - 1);
        body += ",\"ip\":\"";
        body += (staMode ? WiFi.localIP() : WiFi.softAPIP()).toString();
        body += "\",\"name\":\"";
        body += DEVICE_NAME;
        body += "\",\"version\":\"ADS129X-2.0.0\",\"latency\":";
        body += String(streamLatencyUs);
        body += ",\"ring_dropped\":";
        body += String(ads.ringDropped());
        body += '}';
        sendJson(200, body);
    });
    server.on("/all", HTTP_OPTIONS, sendOptions);
    server.on("/version", HTTP_GET, []() {
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(200, "text/plain", "ADS129X-2.0.0");
    });
    server.on("/version", HTTP_OPTIONS, sendOptions);
    server.on("/tcp", HTTP_GET, []() { sendJson(200, tcpJson(tcpClient.connected())); });
    server.on("/tcp", HTTP_POST, handleTcpPost);
    server.on("/tcp", HTTP_DELETE, []() {
        tcpClient.stop();
        tcpWanted = false;
        sendJson(200, tcpJson(false));
    });
    server.on("/tcp", HTTP_OPTIONS, sendOptions);
    server.on("/udp", HTTP_POST, handleUdpPost);
    server.on("/udp", HTTP_DELETE, []() {
        udpWanted = false;
        sendJson(200, tcpJson(false));
    });
    server.on("/udp", HTTP_OPTIONS, sendOptions);
    server.on("/command", HTTP_POST, handleCommand);
    server.on("/command", HTTP_OPTIONS, sendOptions);
    server.on("/stream/start", HTTP_GET, []() {
        wantWifi = true;
        startAds();
        sendJson(200, "{\"ok\":true}");
    });
    server.on("/stream/start", HTTP_OPTIONS, sendOptions);
    server.on("/stream/stop", HTTP_GET, []() {
        wantWifi = false;
        stopAdsIfIdle();
        sendJson(200, "{\"ok\":true}");
    });
    server.on("/stream/stop", HTTP_OPTIONS, sendOptions);
    server.on("/", HTTP_GET, []() {
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(200, "text/plain", "ADS1299 OpenBCI. GET /board");
    });
    server.begin();

#if defined(ADS_HAS_SPP)
    if (!SerialBT.begin(DEVICE_NAME)) {
        Serial.println("# Classic SPP failed to start");
    } else {
        Serial.println("# SPP name ADS1299-EEG");
    }
#endif
#if defined(ADS_HAS_BLE)
    startBle();
    Serial.println("# BLE Nordic UART name ADS1299-EEG");
#endif
    Serial.println("# send b to stream on USB, or use WiFi /stream/start");
}

void loop() {
    server.handleClient();
    pollSsdp();
    feedStream(Serial, wantUsb);
#if defined(ADS_HAS_SPP)
    bool sppUp = SerialBT.hasClient();
    if (sppUp && !sppWasUp) {
        ads.printBanner(SerialBT);
    }
    sppWasUp = sppUp;
    if (sppUp) {
        feedStream(SerialBT, wantSpp);
    } else if (wantSpp) {
        wantSpp = false;
        stopAdsIfIdle();
    }
#endif
    drainBleCommands();
    if (tcpWanted && !tcpClient.connected()) {
        tcpWanted = false;
    }
    pumpPackets();

    static uint32_t lastNote = 0;
    if (!wantUsb && millis() - lastNote > 2000) {
        lastNote = millis();
        Serial.print("# sps=");
        Serial.print(ads.sampleRateHz());
        Serial.print(" ring_dropped=");
        Serial.print(ads.ringDropped());
        Serial.print(" drdy_missed=");
        Serial.print(ads.missedSamples());
        Serial.print(" streaming=");
        Serial.println(adsOn ? 1 : 0);
    }
}
