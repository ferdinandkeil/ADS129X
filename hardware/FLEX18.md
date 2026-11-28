# 18-pin flex boards: shared input-cable, aux-header and guard notes

These six boards share one input-cable pinout and one aux-header pinout, so a single sensor-shield design serves both the XIAO and the LOLIN stacks.

| Board | Size | Layers | KiCad check |
|---|---|---|---|
| XIAO ADS1299 8ch main R5 | 20.05 × 26.55 mm | 4 | 0 unconnected, 0 clearance / hole / edge errors |
| XIAO ADS1299 add-on R6 | 20.25 × 26.55 mm | 4 | same |
| XIAO ADS1298 8ch main R5 | 20.05 × 26.55 mm | 4 | same |
| XIAO ADS1298 add-on R6 | 20.25 × 26.55 mm | 4 | same |
| LOLIN ADS1299 R7 | 25.4 × 24.9 mm | 4 | same |
| LOLIN ADS1298 R7 | 25.4 × 24.9 mm | 4 | same |

- **Courtyard notices:** the aux header outline touches the connector outline on the XIAO boards. On the ADS1298 boards there's also C6 vs J2 (LOLIN) and R5 vs J2 (XIAO add-on). The parts clear each other physically.
- **Leftover router stubs:** a few short stubs remain outside the guard area (CLK on the XIAO add-ons, DVDD on the LOLIN ADS1299). They're harmless.
- **Other notices:** small-text, silkscreen and library notices only.
- **Stack headers:** unchanged on every board, checked pad by pad.

## Input cable connector
- **Part:** Hirose **FH12-18S-0.5SH(55)**: 18 positions, 0.5 mm pitch, bottom contact, slide-lock ZIF.
- **Placement:** on the ADS side, with cable entry at the bottom edge. It's J3 on the XIAO boards and J1 on the LOLIN boards.
- **Cable:** fits Adafruit's 18-pin 0.5 mm EYESPI cables (A-B type).

**Pinout (connector pin numbers):**

| Pin | ADS1299 | ADS1298 |
|---|---|---|
| 18 | SRB1 | PACE1 (output; leave unconnected) |
| 17 | SRB2 | WCT |
| 16 … 1 | IN1P, IN1N, IN2P … IN8P, IN8N (pin 16 = IN1P, pin 1 = IN8N) | same |

- **Straight input runs:** every input and SRB line runs straight down from its chip pin to its contact, with no jogs, no vias and no layer changes. The ADS1298's WCT is the one exception: it crosses under the input fan on inner 1, guarded on both sides.
- **No guard in the cable:** there are no guard conductors in the cable. The copper foil over the cable provides the shield, soldered to guard at the sensor-shield end.
- **Mounting tabs:** the connector's tabs are tied to guard.
- **Labels:** every contact is labelled on the ADS-side silkscreen, with a leader line to its pad.

## Aux header (2×3, 1.27 mm, ADS side)
J4 on all boards. Viewed from the XIAO / LOLIN side:

|  | Left | Right |
|---|---|---|
| Top | GUARD | GUARD |
| Middle | BIASOUT (RLDOUT) | BIASINV (RLDINV) |
| Bottom | GPIO2 | GPIO3 |

- **Pass-through rows:** the guard and bias/RLD pins pass straight through the stack to the sensor shield. **Only one board may drive BIAS/RLD**, so power down the bias/RLD amplifier on the others in firmware.
- **GPIO row:** bend the GPIO pins out 90° so each board's GPIOs stay independent.
- **Add-on guard:** on the add-ons, the guard comes in through these pins. The main board drives it, through R7 (XIAO) or JP9 (LOLIN).
- **XIAO header position:** J4 sits at the same spot on the XIAO main and add-on boards, so the pins line up through the stack.
- **XIAO map:** printed beside J4 on the XIAO side.
- **LOLIN map:** printed beside J4 on the ADS side, where it reads mirrored ("BINV BOUT", "GP3 GP2").

## Guard
- **Closed guard on all four layers**, from a straight edge under the chip down to the bottom edge.
- **Inner 2:** a solid guard sheet under the whole input fan.
- **Bias and GPIO routing:**
  - They leave the chip on short vias and run on inner 1, under the inner-2 guard sheet, to J4.
  - The last few millimetres into J4 are split across layers (inner 1, inner 2, top or bottom) right beside the header, outside the guard area.
  - On the LOLIN boards, the bias lines leave the chip outward between the CS solder jumpers, so the GPIOs can run around the outside to the bottom row.

## Assembly note
- **XIAO main boards:** fit **either R7 (4.7 k, guard from BIASOUT/RLDOUT) or R8 (0 Ω, guard from the AVDD/2 divider)**, never both. The BOM lists both footprints.

## Other moves
- **XIAO main R7 (4.7 k bias-out → guard):** under the XIAO, beside J4's lines.
- **ADS1298 C6 (100 pF on WCT):**
  - XIAO main: beside the chip;
  - LOLIN: on the LOLIN side, between JP27 and JP28.
- **ADS1298 pin 62:** tied to AVDD with a short link to C7.

## LOLIN R7: second SPI bus (bus A / bus B select)
R7 is R6 with a second SPI bus brought out on the stack headers. Each board can run on either bus, so a large LOLIN stack can be split across the ESP32-S3's two SPI controllers (for example 3 + 3 boards at 16 kSPS, or 4 + 4 at 8 kSPS).

| Line | Bus A (as before) | Bus B (new) | Jumper |
|---|---|---|---|
| SCLK | J2-7, SCK | J2-5, IO4 (SCK2) | JP31, LOLIN side, top centre |
| DOUT | J2-9, MISO | J3-12, IO16 (MISO2) | JP32, ADS side, top centre (vertical) |
| DIN | J2-11, MOSI | J2-13, IO10 (MOSI2) | JP33, ADS side, top left |

- **All three jumpers ship open, like the CS and DRDY jumpers.** A board does nothing on SPI until you bridge each centre pad to its A pad or its B pad with solder. There's no trace to cut, ever. Put all three on the same bus.
- **Silkscreen:** each jumper is marked with its A and B sides. On the ADS side the header labels now read SCK2 / MOSI2 / MISO2. On the LOLIN side they still show the GPIO numbers (IO4, IO10, IO16).
- **Pins used:** J2-5 (IO4), J2-13 (IO10) and J3-12 (IO16) were the only free GPIOs on the headers. J2-1 is EN (reset), so it can't be used.
- **Firmware:** bus A on one SPI host (SCK 12 / MISO 13 / MOSI 11), bus B on the other (SCK 4 / MISO 16 / MOSI 10). CS, DRDY, START and CLK_BUS are unchanged and shared as before.
- **Routing:** only the SPI lines, plus a few digital lines the router needed to move (DRDY_LOCAL, START, CLK / CLK_BUS, CS8, CLKSEL, GPIO1 / LED), were rerouted. The guard area, inputs, power and every other track are exactly as in R6, and every other pad is unchanged, checked pad by pad.
- **KiCad check:** 0 unconnected, 0 clearance errors on both boards, with the same harmless notices as R6.
