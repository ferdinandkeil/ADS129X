#include <ADS129X.h>
#include <SPI.h>

/* ADS129X pins */
const int ADS_RESET  = 3;
const int ADS_DRDY = 5;
const int ADS_CS = 10;

ADS129X ADS = ADS129X(ADS_DRDY, ADS_CS);

void setup() {
  pinMode(ADS_RESET, OUTPUT);

  digitalWrite(ADS_RESET, HIGH);
  delay(100); // delay for power-on-reset (Datasheet, pg. 48)
  // reset pulse
  digitalWrite(ADS_RESET, LOW);
  digitalWrite(ADS_RESET, HIGH);
  delay(1); // Wait for 18 tCLKs AKA 9 microseconds, we use 1 millisec

  ADS.SDATAC(); // device wakes up in RDATAC mode, so send stop signal
  ADS.WREG(ADS129X_REG_CONFIG1, ADS129X_SAMPLERATE_1024); // enable 8kHz sample-rate
  ADS.WREG(ADS129X_REG_CONFIG3, (1<<ADS129X_BIT_PD_REFBUF) | (1<<6)); // enable internal reference
  //ADS.WREG(ADS129X_REG_CONFIG2, (1<<ADS129X_BIT_INT_TEST) | ADS129X_TEST_FREQ_2HZ);

  // setup channels
  ADS.configChannel(1, false, ADS129X_GAIN_12X, ADS129X_MUX_NORMAL);
  ADS.configChannel(2, false, ADS129X_GAIN_12X, ADS129X_MUX_NORMAL);
  for (int i = 3; i <= 8; i++) {
    ADS.configChannel(i, false, ADS129X_GAIN_1X, ADS129X_MUX_SHORT);
  }

  delay(1);
  ADS.RDATAC();
  ADS.START();

  Serial.begin(1000000); // always at 12Mbit/s
  Serial.println("Firmware v0.0.1");
}

void loop() {
  long buffer[9];
  static unsigned long tLast;
  if (millis()-tLast > 500) {
    // digitalWrite(LED3, !digitalRead(LED3));
    tLast = millis();
  }
  if (ADS.getData(buffer)) {
    unsigned long tStart = micros();
    for (int channel = 1; channel < 9; channel++) {
      /*if (channel == 8) {
        buffer[channel] = micros()-tStart;
      }*/
      byte value[3];

      // convert long to bytes
      value[0] = (byte) (buffer[channel]>>16);
      value[1] = (byte) (buffer[channel]>>8);
      value[2] = (byte) (buffer[channel]);

      Serial.write(value[0]);
      Serial.write(value[1]);
      Serial.write(value[2]);
      
    }
  }
}

