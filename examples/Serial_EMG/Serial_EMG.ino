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
const int BUTTON = 15;

/* nRF8001 pins */
const int NRF_RST = 7;

int fourbfiveb[] = {
	0b11110, // Hex data 0
	0b01001, // Hex data 1
	0b10100, // Hex data 2
	0b10101, // Hex data 3
	0b01010, // Hex data 4
	0b01011, // Hex data 5
	0b01110, // Hex data 6
	0b01111, // Hex data 7
	0b10010, // Hex data 8
	0b10011, // Hex data 9
	0b10110, // Hex data A
	0b10111, // Hex data B
	0b11010, // Hex data C
	0b11011, // Hex data D
	0b11100, // Hex data E
	0b11101  // Hex data F
};

#define encode4B5B(x) (fourbfiveb[x&0x0F])
#define HIGH_NIBBLE(x) (x>>4)
#define LOW_NIBBLE(x) (x&0x0F)

/*uint32_t fourbfiveb[256] = {
  0x1e1e, 0x1e09, 0x1e14, 0x1e15, 0x1e0a, 0x1e0b, 0x1e0e, 0x1e0f,
  0x1e12, 0x1e13, 0x1e16, 0x1e17, 0x1e1a, 0x1e1b, 0x1e1c, 0x1e1d,
  0x091e, 0x0909, 0x0914, 0x0915, 0x090a, 0x090b, 0x090e, 0x090f,
  0x0912, 0x0913, 0x0916, 0x0917, 0x091a, 0x091b, 0x091c, 0x091d,
  0x141e, 0x1409, 0x1414, 0x1415, 0x140a, 0x140b, 0x140e, 0x140f,
  0x1412, 0x1413, 0x1416, 0x1417, 0x141a, 0x141b, 0x141c, 0x141d,
  0x151e, 0x1509, 0x1514, 0x1515, 0x150a, 0x150b, 0x150e, 0x150f,
  0x1512, 0x1513, 0x1516, 0x1517, 0x151a, 0x151b, 0x151c, 0x151d,
  0x0a1e, 0x0a09, 0x0a14, 0x0a15, 0x0a0a, 0x0a0b, 0x0a0e, 0x0a0f,
  0x0a12, 0x0a13, 0x0a16, 0x0a17, 0x0a1a, 0x0a1b, 0x0a1c, 0x0a1d,
  0x0b1e, 0x0b09, 0x0b14, 0x0b15, 0x0b0a, 0x0b0b, 0x0b0e, 0x0b0f,
  0x0b12, 0x0b13, 0x0b16, 0x0b17, 0x0b1a, 0x0b1b, 0x0b1c, 0x0b1d,
  0x0e1e, 0x0e09, 0x0e14, 0x0e15, 0x0e0a, 0x0e0b, 0x0e0e, 0x0e0f,
  0x0e12, 0x0e13, 0x0e16, 0x0e17, 0x0e1a, 0x0e1b, 0x0e1c, 0x0e1d,
  0x0f1e, 0x0f09, 0x0f14, 0x0f15, 0x0f0a, 0x0f0b, 0x0f0e, 0x0f0f,
  0x0f12, 0x0f13, 0x0f16, 0x0f17, 0x0f1a, 0x0f1b, 0x0f1c, 0x0f1d,
  0x121e, 0x1209, 0x1214, 0x1215, 0x120a, 0x120b, 0x120e, 0x120f,
  0x1212, 0x1213, 0x1216, 0x1217, 0x121a, 0x121b, 0x121c, 0x121d,
  0x131e, 0x1309, 0x1314, 0x1315, 0x130a, 0x130b, 0x130e, 0x130f,
  0x1312, 0x1313, 0x1316, 0x1317, 0x131a, 0x131b, 0x131c, 0x131d,
  0x161e, 0x1609, 0x1614, 0x1615, 0x160a, 0x160b, 0x160e, 0x160f,
  0x1612, 0x1613, 0x1616, 0x1617, 0x161a, 0x161b, 0x161c, 0x161d,
  0x171e, 0x1709, 0x1714, 0x1715, 0x170a, 0x170b, 0x170e, 0x170f,
  0x1712, 0x1713, 0x1716, 0x1717, 0x171a, 0x171b, 0x171c, 0x171d,
  0x1a1e, 0x1a09, 0x1a14, 0x1a15, 0x1a0a, 0x1a0b, 0x1a0e, 0x1a0f,
  0x1a12, 0x1a13, 0x1a16, 0x1a17, 0x1a1a, 0x1a1b, 0x1a1c, 0x1a1d,
  0x1b1e, 0x1b09, 0x1b14, 0x1b15, 0x1b0a, 0x1b0b, 0x1b0e, 0x1b0f,
  0x1b12, 0x1b13, 0x1b16, 0x1b17, 0x1b1a, 0x1b1b, 0x1b1c, 0x1b1d,
  0x1c1e, 0x1c09, 0x1c14, 0x1c15, 0x1c0a, 0x1c0b, 0x1c0e, 0x1c0f,
  0x1c12, 0x1c13, 0x1c16, 0x1c17, 0x1c1a, 0x1c1b, 0x1c1c, 0x1c1d,
  0x1d1e, 0x1d09, 0x1d14, 0x1d15, 0x1d0a, 0x1d0b, 0x1d0e, 0x1d0f,
  0x1d12, 0x1d13, 0x1d16, 0x1d17, 0x1d1a, 0x1d1b, 0x1d1c, 0x1d1d
};*/

//#define encode4B5B(x) (fourbfiveb[x&0xFF])

ADS129X ADS = ADS129X(ADS_DRDY, ADS_CS);

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
  pinMode(NRF_RST, OUTPUT);

  digitalWrite(NRF_RST, LOW);
  digitalWrite(LED1, HIGH);
  digitalWrite(PSU_POS, HIGH);
  digitalWrite(PSU_NEG, HIGH);
  delay(100); // wait for PSUs to come UP

  digitalWrite(ADS_START, LOW);
  digitalWrite(ADS_PWDN, HIGH); // turn off power down mode
  digitalWrite(ADS_RESET, HIGH);
  delay(100); // delay for power-on-reset (Datasheet, pg. 48)
  // reset pulse
  digitalWrite(ADS_RESET, LOW);
  digitalWrite(ADS_RESET, HIGH);
  delay(1); // Wait for 18 tCLKs AKA 9 microseconds, we use 1 millisec

  ADS.SDATAC(); // device wakes up in RDATAC mode, so send stop signal
  ADS.WREG(ADS129X_REG_CONFIG1, ADS129X_SAMPLERATE_32); // enable 8kHz sample-rate
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

  Serial.begin(0); // always at 12Mbit/s
  digitalWrite(LED2, HIGH);
  digitalWrite(LED3, HIGH);
}

void loop() {
  long buffer[9];
  static unsigned long tLast;
  if (millis()-tLast > 500) {
    digitalWrite(LED3, !digitalRead(LED3));
    tLast = millis();
  }
  if (ADS.getData(buffer)) {
    unsigned long tStart = micros();
    for (int channel = 1; channel < 9; channel++) {
      /*if (channel == 8) {
        buffer[channel] = micros()-tStart;
      }*/
      byte value[3];
      byte packet[5];
      byte encoded[7];
      // convert long to bytes
      value[0] = (byte) (buffer[channel]>>16);
      value[1] = (byte) (buffer[channel]>>8);
      value[2] = (byte) (buffer[channel]);
      // encode bytes with 4b5b
      encoded[0] = encode4B5B( LOW_NIBBLE(channel));
      encoded[1] = encode4B5B(HIGH_NIBBLE(value[0]));
      encoded[2] = encode4B5B( LOW_NIBBLE(value[0]));
      encoded[3] = encode4B5B(HIGH_NIBBLE(value[1]));
      encoded[4] = encode4B5B( LOW_NIBBLE(value[1]));
      encoded[5] = encode4B5B(HIGH_NIBBLE(value[2]));
      encoded[6] = encode4B5B( LOW_NIBBLE(value[2]));
      /*
      assemble packet
      format:
         ---- -000 | 0011 1112 | 2222 3333 | 3444 4455 | 5556 6666
      dashes are zeros
      */
      packet[0] = 0 | (encoded[0]>>2);
      packet[1] = (encoded[0]<<6) | (encoded[1]<<1) | (encoded[2]>>4);
      packet[2] = (encoded[2]<<4) | (encoded[3]>>1);
      packet[3] = (encoded[3]<<7) | (encoded[4]<<2) | (encoded[5]>>3);
      packet[4] = (encoded[5]<<5) | (encoded[6]);
      // send packet
      Serial.write(packet, 5);
    }
  }
}

