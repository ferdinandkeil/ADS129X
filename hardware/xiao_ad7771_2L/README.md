# XIAO-AD7771 8-channel shield, 2-layer, A1

## Revision A1.2: C12 via stub at 45°

A1.2 redoes the A1.1 fix so the short AVDD trace to C12's via runs straight. The via now sits 0.6 mm from C12 pin 1 on a straight stub, at (4.8, 17.7). Nothing else changed, and the design-rule report is identical to A1.

## Revision A1.1: vias moved off component pads

A1.1 changes only vias that sat on or touched component pads in A1. Solder can wick down such a via and starve the joint, and on 0.5 mm-pitch pins the via ring can bridge to the next pin.

No circuit, part or placement changes, and the Gerbers are regenerated. The design-rule report is identical to the previous revision's: no new errors and nothing unconnected. `plots/*_via_check.png` shows pads and vias as outlines, before and after.

- **"Clear":** the via now sits fully off the pad, joined by a short trace.
- **"Drill clear":** the drill hole is fully off the pad, and the via ring still overlaps the pad slightly where there was no room for more. Vias are tented, so no solder reaches the hole.
- **Not changed:** vias in J3's large mounting tabs are kept on purpose.

| Pad | Net | Change |
|---|---|---|
| C12 pin 1 | AVDD | The op-amp supply via sat in C12's pad. Now clear, moved 0.6 mm. |

**Board**
- 20.3 × 41.5 mm, 2 layers.
- OSH Park: about 1.31 sq in, so roughly $6.50 for three boards.

**Layout**
- The AD7771 sits on the bottom side, below the XIAO pin rows. That is what makes 2 layers possible: the chip's pins can fan out sideways instead of squeezing between the XIAO rows.
- Every square trace corner is chamfered with a small 45°.

**Design rules**
- DRC is clean.
- Clearance 0.15 mm.
- Vias 0.5/0.254 mm (0.254 mm meets OSH Park's 10 mil minimum drill).
- Stitching vias 0.6/0.3 mm.
- Minimum silkscreen text 0.6 mm.

**Input protection:** there are no input protection resistors on this board. Whatever plugs into J3 must carry them, for example 2.2 kΩ in series with each electrode on the sensor or jack board. The AD7771 inputs are only rated −0.3 V to 3.6 V.

## Circuit

- **ADC:** AD7771 in SPI control mode (FORMAT0 and FORMAT1 tied high).
  - It runs entirely from 3.3 V.
  - AVDD is the XIAO 3V3 through FB1 (600 Ω ferrite) and C1 (10 µF). Every AVDD pin has its own 100 nF cap.
  - IOVDD comes straight from 3V3, with 100 nF.
  - DREGCAP, AREG1CAP and AREG2CAP each get 1 µF.
- **Reference:** the internal 2.5 V reference.
  - REF_OUT is decoupled with C8 (100 nF). REF1± and REF2± are tied to ground.
  - Firmware must select the internal reference in ADC_MUX_CONFIG: REF_MUX_CTRL, bits 7:6 = 01.
  - Firmware must also enable the internal reference buffer, which is off at power-up.
- **Inputs:** all eight AINx− pins tie to SRB, a single common reference. AINx+ go to the header.
- **Clock:** CLK_SEL is low, so the clock is an external CMOS clock on XTAL2/MCLK.
  - The clock comes from XIAO D7 (LEDC, 8.000 MHz) through R7 (47 Ω).
  - XTAL1 is grounded.
  - Y1 is an optional oscillator footprint (DNP). Bridge JP3 to use it.
- **Bias drive, U2A (OPA2333):** an integrator, +INA = AD7771 VCM (1.65 V).
  - SRB is sensed through the guard buffer (GBUF) and R1 (1 MΩ). C13 (270 pF C0G) is the feedback capacitor, so crossover is about 590 Hz.
  - The output drives BIASOUT through R2 (100 kΩ), which limits current to about 16 µA worst case.
  - The loop holds SRB at mid-supply.
- **Guard, U2B:** a unity buffer of SRB that drives the guard pours and header pins 12 and 14 through R3 (4.7 kΩ).
  - The guard copper is the same scheme as R2: pours on both layers, around and between the input traces and the SRB traces, from the chip pins to the header.
- **Other pins:**
  - START is pulled up (R5).
  - SYNC_OUT is tied to SYNC_IN.
  - RESET is pulled up (R6) and driven from XIAO D8.
  - CS is pulled up (R4).
  - The status LED is on AD7771 GPIO0 (MODE0).
  - Pins tied to ground: CONVST_SAR, MODE1, MODE2, AUXAIN±.
  - Not connected: DOUT0–3 and DCLK.

## Output header J3 (2×7, 2.54 mm, right angle)

The pinout is the same as the ADS1299 R2 board, except that pin 10 is not connected (the AD7771 has one reference input, SRB). The jack board fits both.

| Column | Inner row | Outer row |
|---|---|---|
| 1 | 1 IN1 (AIN2) | 2 IN2 (AIN3) |
| 2 | 3 IN3 (AIN0) | 4 IN4 (AIN1) |
| 3 | 5 IN5 (AIN4) | 6 IN6 (AIN5) |
| 4 | 7 IN7 (AIN6) | 8 IN8 (AIN7) |
| 5 | 9 SRB | 10 NC |
| 6 | 11 BIASOUT | 12 GUARD |
| 7 | 13 BIASINV | 14 GUARD |

- **Channel order:** IN1–IN4 map to the ADC channels in the order AIN2, AIN3, AIN0, AIN1, so that the left-side input traces need no crossings. Reorder in firmware: `IN = [2, 3, 0, 1, 4, 5, 6, 7]`.
- **Silkscreen:** inner-row names are on the top side, outer-row names on the bottom side. Pin 1 has a dot on the bottom side.

## XIAO pins

| XIAO pin | ESP32-S3 GPIO | Signal |
|---|---|---|
| D0 | GPIO1 | MOSI (SDI) |
| D1 | GPIO2 | CS, board 1 (JP1 to D1) |
| D2 | GPIO3 | SCK (max 30 MHz) |
| D3 | GPIO4 | CS, board 2 (JP1 to D3) |
| D4 | GPIO5 | MISO (SDO) |
| D7 | GPIO44 | MCLK, 8 MHz LEDC output (shared by a stack) |
| D8 | GPIO7 | RESET (shared) |
| D10 | GPIO9 | DRDY (JP2) |
| 3V3, GND | | power |

- **Free:** D5, D6 and D9.
- **Clock moved from D4 to D7:** on this layout the AD7771's MCLK pin faces the right-hand XIAO row, so D7 gives a short clock trace that crosses nothing.
- **Reset sequence:** start the clock first, then pulse RESET low.

## Solder jumpers (bottom side, all ship open)

| Jumper | Silkscreen | Bridge | Effect |
|---|---|---|---|
| JP1 (3-pad) | D1 / CS / D3 | centre to D1 | CS on D1 (single board, or board 1) |
| | | centre to D3 | CS on D3 (board 2) |
| JP2 | DRDY | bridge | DRDY to D10. Board 1 or a single board only |
| JP3 | OSC | bridge | Y1 output onto the MCLK line. Only with Y1 fitted, and the XIAO pin D7 left as an input |

## Two boards in a stack

- **Board 2 jumpers:** JP1 to D3, JP2 open.
- **Board 2 parts:** leave off R1, R2 and C13. Only board 1 then closes the bias loop, and board 2's integrator output is left unconnected.
- **Wiring:** link pins 13 (BIASINV) and 14 (GUARD) between the boards. Wire both SRB pins to the same reference electrode.
- **Shared lines:** both boards share MCLK and RESET.
- **Starting both boards together:** pull both CS lines low and issue the SPI sync (SPI_SYNC in GENERAL_USER_CONFIG_2). Board 1's DRDY then times both.

## Assembly

- **Bottom side (chip side):** all parts except five caps. That includes U1, U2, LED1, the jumpers and J3.
- **Top side:** C4, C5, C6, C9 and C11 (100 nF each). They sit right behind the AD7771 and decouple AVDD1A, AVDD1B, AVDD4, AVDD2A and AVDD2B.
  - That makes JLC assembly two-sided. They can also be hand-soldered after reflow.
  - J1 and J2 are the XIAO sockets on the top side.
- **Files:**
  - JLC: `_BOM_JLC.csv` and `_CPL_JLC.csv`. The LCSC numbers are left blank for you to pick.
  - US sources: Digi-Key or Mouser for AD7771BCPZ and OPA2333AIDGKR.
- **Exposed pad:** U1's exposed pad has nine 0.6 mm vias to the top-side ground.
