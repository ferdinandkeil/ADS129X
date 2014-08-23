#define ADS129X_POLLING

#include <LowPower_Teensy3.h>
#include <Adafruit_BLE_UART.h>
#include <ADS129X.h>
#include <SPI.h>


/* ADS129X pins */
const int PSU_NEG = 1;
const int PSU_POS = 0;
const int ADS_PWDN = 2;
const int ADS_RESET  = 3;
const int ADS_START  = 4;
const int ADS_DRDY = 5;
const int ADS_CS = 10;

/* LEDs */
const int LED1 = 16;
const int LED2 = 17;
const int LED3 = 18;

/* Button */
const int BUTTON = 21;

/* nRF8001 pins */
const int NRF_REQN = 9;
const int NRF_RDYN = 8;
const int NRF_RST = 7;

/* Power saving */
const int notconnectedTimeout = 30000;

ADS129X ADS = ADS129X(ADS_DRDY, ADS_CS);
Adafruit_BLE_UART BTLEserial = Adafruit_BLE_UART(NRF_REQN, NRF_RDYN, NRF_RST);
TEENSY3_LP LP = TEENSY3_LP();

void setup() {
  pinMode(PSU_NEG, OUTPUT);
  pinMode(PSU_POS, OUTPUT);
  pinMode(ADS_RESET, OUTPUT);
  pinMode(ADS_PWDN, OUTPUT);
  pinMode(ADS_START, OUTPUT);
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
  pinMode(BUTTON, INPUT);
  
  digitalWrite(LED1, HIGH);
  digitalWrite(PSU_POS, HIGH);
  digitalWrite(PSU_NEG, HIGH);
  delay(100); // wait for PSUs to come up
  
  digitalWrite(ADS_START, LOW);
  digitalWrite(ADS_PWDN, HIGH); // turn off power down mode
  digitalWrite(ADS_RESET, HIGH);
  delay(100); // delay for power-on-reset (Datasheet, pg. 48)
  // reset pulse
  digitalWrite(ADS_RESET, LOW);
  digitalWrite(ADS_RESET, HIGH);
  delay(1); // Wait for 18 tCLKs AKA 9 microseconds, we use 1 millisec
  
  ADS.SDATAC(); // device wakes up in RDATAC mode, so send stop signal
  ADS.WREG(ADS129X_REG_GPIO, 0x00);
  ADS.WREG(ADS129X_REG_CONFIG3, (1<<ADS129X_BIT_PD_REFBUF) | (1<<6)); // enable internal reference
  ADS.WREG(ADS129X_REG_CONFIG2, 0x10); // test signal generation

  // setup channels
  ADS.configChannel(1, false, ADS129X_GAIN_1X, ADS129X_MUX_TEST);
  for (int i = 2; i <= 8; i++) {
    ADS.configChannel(i, false, ADS129X_GAIN_1X, ADS129X_MUX_SHORT);
  }
  
  Serial.begin(0); // always at 12MBit/s
  
  // start BTLE serial
  BTLEserial.begin();
}

void spiSettingAds() {
  delayMicroseconds(5);
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE1);
}

void spiSettingNrf() {
  delayMicroseconds(5);
  SPI.setBitOrder(LSBFIRST);
  SPI.setDataMode(SPI_MODE0);
}

void shutdown() {
  digitalWrite(ADS_START, LOW);
  delay(1);
  digitalWrite(ADS_PWDN, LOW);
  delay(1);
  digitalWrite(PSU_NEG, LOW);
  digitalWrite(PSU_POS, LOW);
  
  pinMode(PSU_NEG, INPUT);
  pinMode(PSU_POS, INPUT);
  pinMode(ADS_RESET, INPUT);
  pinMode(ADS_PWDN, INPUT);
  pinMode(ADS_START, INPUT);
  pinMode(LED1, INPUT);
  pinMode(LED2, INPUT);
  pinMode(LED3, INPUT);
  
  delay(1);
  spiSettingNrf();
  BTLEserial.sleep();
  
  LP.Hibernate(GPIO_WAKE, PIN_21);
}

void loop() {
  static aci_evt_opcode_t laststatus = ACI_EVT_DISCONNECTED;
  static long lastConnected = 0;
  
  // tell the nRF8001 to do whatever it should be working on.
  spiSettingNrf();
  BTLEserial.pollACI();
  if (digitalRead(NRF_REQN) == LOW)
    return;
  
  // Ask what is our current status
  aci_evt_opcode_t status = BTLEserial.getState();
  // If the status changed....
  if (status != laststatus) {
    if (status == ACI_EVT_DEVICE_STARTED) {
        digitalWrite(LED2, LOW);
        lastConnected = millis();
        // end continous read mode
        spiSettingAds();
        ADS.STOP();
        ADS.SDATAC();
    }
    if (status == ACI_EVT_CONNECTED) {
        digitalWrite(LED2, HIGH);
        // start continous read mode
        while (digitalRead(ADS_DRDY) == LOW) ;
        delayMicroseconds(2);
        spiSettingAds();
        ADS.RDATAC();
        ADS.START();
    }
    if (status == ACI_EVT_DISCONNECTED) {
        /*digitalWrite(LED2, LOW);
        lastConnected = millis();
        // end continous read mode
        spiSettingAds();
        ADS.SDATAC();
        digitalWrite(ADS_START, LOW);*/
    }
    // OK set the last status change to this one
    laststatus = status;
  }
  
  static long blinkLed;
  if (millis() - blinkLed >= 500) {
    if (digitalRead(LED3) == HIGH) {
      digitalWrite(LED3, LOW);
    } else {
      digitalWrite(LED3, HIGH);
    }
    blinkLed = millis();
  }
  
  if (status != ACI_EVT_CONNECTED) {
    if (millis() - lastConnected > notconnectedTimeout) {
      shutdown();
    }
  }
  
  if (status == ACI_EVT_CONNECTED) {    
    // try to receive data
    long buffer[9];
    spiSettingAds();
    if (ADS.getData(buffer)) {
      // write data
      byte sendBuffer[20];
      if (buffer[1] > 8388607) {
        buffer[1] -= 16777215;
      }
      // send raw value with LSB first
      for (byte i = 0; i < 4; i++) {
        sendBuffer[i] = (byte) (buffer[1] >> i*8);
      }
      Serial.println(buffer[1], DEC);
      spiSettingNrf();
      BTLEserial.write(sendBuffer, 4);
    }
  }
}
