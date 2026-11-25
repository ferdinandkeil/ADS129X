# XIAO-AD7771 8-channel shield, 4-layer, A1-4L

**Board**
- 20.0 × 25.8 mm, the same outline as the ADS1299 R2 board.
- The XIAO sockets and the 2×7 output header are in the same places as on R2.
- OSH Park 4-layer: about 0.80 sq in, so roughly $8 for three boards.

**Design rules**
- DRC is clean, apart from one cosmetic note: the C13 label touches a solder-mask opening.
- Clearance 0.13 mm.
- Traces 0.13–0.25 mm.
- Vias 0.5/0.254 mm.
- Minimum silkscreen text 0.6 mm.
- These are within OSH Park's 4-layer rules (5/5 mil, 10 mil drill).

**Input protection:** there are no input protection resistors on this board. Whatever plugs into J3 must carry them, for example 2.2 kΩ in series with each electrode. The AD7771 inputs are only rated −0.3 V to 3.6 V.

## Layer stack

| Layer | Use |
|---|---|
| F (XIAO side) | AVDD pour behind the chip, ground pour, a few digital lines, guard pour over the output end, SRB link |
| In1 | Solid ground plane |
| In2 | The eight input traces, run inside a guard pour from the chip to the header, with a ground island under the chip |
| B (chip side) | AD7771, op-amp block, passives, digital fan-out, guard around the SRB buses and the header |

The input traces never run on an outer layer. From each AINx+ pin they drop straight into In2 through a via, cross under the chip and come up at the header. On In2 the guard pour surrounds them.

## Circuit

The circuit is the same as the 2-layer A1, with these differences:

- **Op-amp sections:** U2 is still an OPA2333, but the sections are swapped.
  - **B is the bias integrator:** +INB = AD7771 VCM, −INB = BIASINV, OUTB → R2 (100 kΩ) → BIASOUT.
  - **A is the guard buffer:** +INA = SRB, OUTA → R3 (4.7 kΩ) → GUARD.
  - As on A1, R1 (1 MΩ) runs from the buffered SRB to BIASINV, and C13 is 270 pF C0G. Crossover is about 590 Hz and bias current is at most about 16 µA.
- **Op-amp supply:** U2 gets AVDD through a short inner-layer link inside the ground island.
- **No status LED:** there was no room left for it on this size. AD7771 GPIO0 is unconnected.
- **No local op-amp bypass cap:** there is no dedicated 100 nF beside U2. It is fed from the AVDD pour, with C6 about 6 mm away. That is fine for an op-amp drawing 17 µA.
- **Reference:** internal 2.5 V. REF_OUT is decoupled with C8. REF1± and REF2± are tied to ground. Firmware must select the internal reference (ADC_MUX_CONFIG REF_MUX_CTRL = 01) and enable the internal reference buffer.
- **Clock:** CLK_SEL is low, so the clock comes from XIAO D10 through R7 (47 Ω). Y1 (DNP, top side) plus JP3 is the optional oscillator.

## Output header J3 (2×7): same pinout as R2 and A1

| Column | Inner row | Outer row |
|---|---|---|
| 1 | 1 IN1 (AIN1) | 2 IN2 (AIN0) |
| 2 | 3 IN3 (AIN3) | 4 IN4 (AIN2) |
| 3 | 5 IN5 (AIN7) | 6 IN6 (AIN6) |
| 4 | 7 IN7 (AIN5) | 8 IN8 (AIN4) |
| 5 | 9 SRB | 10 NC |
| 6 | 11 BIASOUT | 12 GUARD |
| 7 | 13 BIASINV | 14 GUARD |

- **Channel order:** the order on the inner layer keeps both input fans free of crossings. Reorder in firmware: `IN = [1, 0, 3, 2, 7, 6, 5, 4]`. Note that this differs from the 2-layer A1.

## XIAO pins (different from A1, chosen for this layout)

| XIAO pin | ESP32-S3 GPIO | Signal |
|---|---|---|
| D0 | GPIO1 | MISO (SDO) |
| D1 | GPIO2 | MOSI (SDI) |
| D2 | GPIO3 | SCK |
| D3 | GPIO4 | CS, board 1 (JP1 to D3) |
| D4 | GPIO5 | CS, board 2 (JP1 to D4) |
| D10 | GPIO9 | MCLK, 8 MHz LEDC (shared by a stack) |
| D9 | GPIO8 | DRDY (JP2) |
| D8 | GPIO7 | RESET (shared) |

- **Free:** D5, D6 and D7.

## Solder jumpers (all ship open)

| Jumper | Side | Silkscreen | Bridge | Effect |
|---|---|---|---|---|
| JP1 | bottom | D3 / CS / D4 | centre to D3 | CS on D3 (single board or board 1) |
| | | | centre to D4 | CS on D4 (board 2) |
| JP2 | bottom | DRDY | bridge | DRDY to D9 (board 1 or a single board only) |
| JP3 | top | OSC | bridge | Y1 onto MCLK (only with Y1 fitted; keep XIAO D10 as an input) |

## Two boards in a stack

- **Board 2:** JP1 to D4, JP2 open, and leave off R1, R2 and C13.
- **Wiring:** link pins 13 and 14 between the boards, and wire both SRB pins to the same electrode.
- **Starting both boards together:** pull both CS lines low and issue SPI_SYNC.

## Assembly

- **Bottom side:** U1, U2, the jumpers and all other passives.
- **Top side:** C4, C5, C6, C9 and C11 (100 nF, right behind U1), plus the optional Y1 and JP3.
  - That makes JLC assembly two-sided. Or hand-solder the five caps after reflow.
- **Files:** use `_BOM_JLC.csv` and `_CPL_JLC.csv`.
