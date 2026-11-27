# XIAO-ADS1299 8-channel shield, 2-layer, R2.2 (stackable to two boards)

**R2.2 changes from R2.1:** no via sits in any pad.
- **C8** is now a **1 µF X5R 0402** (was 0603), moved to (2.6, −3.65) on the top side. Its DVDD pad joins the existing DVDD trace with a short stub.
- **The GND via that was in C8's ground pad** moved to (3.1, −2.9), clear of the pad. It still ties C8's ground, U1 pin 53 and the ground copper under the chip together. A short trace links it to C8.
- **CLKSEL (top)** moved down to y = −2.33 to make room for the via. It still has 0.15 mm or more clearance to C6.

`hardware/tools/check_via_in_pad.py` reports no via in any part pad. DRC is identical to R2.1, with library-footprint notes only. The rest of the routing is unchanged.

**R2.1 changes from R2:** guard filter only. Copper routing is unchanged.
- **R3** is now **100 kΩ** (was 4.7 kΩ).
- **C10 (new):** 2.2 µF X7R 0603 from GUARD to GND, on the top side at the edge of the guard pour.

Together, R3 and C10 form a low-pass filter with its corner at about 0.7 Hz. The guard keeps the body's DC level but no longer carries the bias drive's correction signal, which moves opposite to the inputs. At AC it now behaves like a quiet shield. The capacitor sits on the guard side of R3, so the ADS1299 bias amplifier never sees it directly. After power-up or an electrode reconnect, the guard takes a few seconds to settle.

DRC is clean. There are no unconnected pads, and the only notes are library-footprint notes.

R2 is the R1 board plus four open solder jumpers, so two boards can share one XIAO with a common clock, and a 2×7 output header that adds BIASINV and a second GUARD pin. The board is still 20.0 × 25.8 mm, 2 layers. Every square trace corner is chamfered with a small 45°; the only remaining right angles are T-junctions and trace entries into pads or vias. DRC is clean (0.15 mm clearance, 0.6/0.3 mm vias, 0.6 mm minimum silkscreen text).

## Output header J3 (2×7, 2.54 mm, right angle)

Odd pins are on the inner row and even pins on the outer row. Pin 1 is marked with a dot on the ADS side.

| Column | Inner row | Outer row |
|---|---|---|
| 1 | 1 IN1P | 2 IN2P |
| 2 | 3 IN3P | 4 IN4P |
| 3 | 5 IN5P | 6 IN6P |
| 4 | 7 IN7P | 8 IN8P |
| 5 | 9 SRB1 | 10 SRB2 |
| 6 | 11 BIASOUT | 12 GUARD |
| 7 | 13 BIASINV | 14 GUARD |

- **Pins 9–12 changed from R1.** SRB2 moved to pin 10, BIASOUT to pin 11 and GUARD to pin 12. Pins 13 (BIASINV) and 14 (GUARD) are the new outer column.
- **Splitting the header:** a 2×6 right-angle header on pins 1–12 plus a 1×2 right-angle on pins 13/14 fits the same holes. The 1×2 is the board-to-board link: BIASINV and GUARD.
- **Silkscreen names (ADS side):** IN1–IN8; SR1/SR2 = SRB1/SRB2; BIA = BIASOUT; BI- = BIASINV; GRD = GUARD.

## Solder jumpers (top side, all ship open)

| Jumper | Silkscreen | Bridge | Effect |
|---|---|---|---|
| JP1 (3-pad) | 2 / CS / 1 | centre to 1 | chip select on D1 (board 1, or a single board) |
| | | centre to 2 | chip select on D3 (board 2) |
| JP4 | DRDY | bridge | DRDY to D10. Board 1 or a single board only; leave open on board 2 |
| JP2 | CLK | bridge | ADS1299 CLK pin to D4, the shared clock line. Bridge on both boards of a stack; leave open on a single board |
| JP3 | EXT | bridge | CLKSEL to ground: external clock. Board 2 only. R5 (10 kΩ) holds CLKSEL high otherwise |

| Setup | JP1 | JP4 | JP2 | JP3 |
|---|---|---|---|---|
| Single board | to 1 (D1) | bridged | open | open |
| Stack, board 1 (clock master) | to 1 (D1) | bridged | bridged | open |
| Stack, board 2 | to 2 (D3) | open | bridged | bridged |

- **Board 1 firmware:** set CLK_EN in CONFIG1 so board 1 drives its 2.048 MHz clock onto D4.
- **Starting both boards together:** pull both CS lines low and send START (or SYNC). The two boards then convert in lockstep, and board 1's DRDY times both. Read board 1, then board 2, on the shared SPI bus.
- **Don't drive D4 from the XIAO:** leave it as an input.

## Second board in a stack

- **Leave off R2, C9 and R3.** C10 is optional on board 2: fitted, it just adds to the shared guard capacitance. Then only board 1's feedback network is on the shared BIASINV line, and board 2's guard comes only from board 1 over pin 14.
- **Firmware:** power down board 2's bias amplifier (PD_BIAS = 0 in CONFIG3). Its BIAS_SENSP/N channels still sum into board 1's amplifier through the shared BIASINV, as the ADS1299 datasheet shows for multiple devices.
- **Wiring:** link pins 13 and 14 between the boards, and make sure both boards' SRB1 lines reach the same reference electrode.
- **Stacking headers:** J1/J2 on the lower board need long-tail stacking headers so the XIAO pins pass through.

## XIAO pin use (ESP32-S3)

| XIAO pin | ESP32-S3 GPIO | Signal |
|---|---|---|
| D0 | GPIO1 | MOSI (ADS DIN) |
| D1 | GPIO2 | CS, board 1 (JP1 to 1) |
| D2 | GPIO3 | SCK |
| D3 | GPIO4 | CS, board 2 (JP1 to 2) |
| D4 | GPIO5 | shared ADS clock (JP2) |
| D9 | GPIO8 | MISO (ADS DOUT) |
| D10 | GPIO9 | DRDY (JP4) |
| 5V | n/a | AVDD (ADS analog 5 V) |
| 3V3 | n/a | DVDD |
| GND | n/a | GND |

- **CS pull-up:** R1 (10 kΩ) sits on the ADS side of JP1, so an unbridged chip stays deselected.
- **Arduino setup:** `SPI.begin(D2 /*SCK*/, D9 /*MISO*/, D0 /*MOSI*/)`, mode 1, with CS on D1 and D3 handled in code.
- **Power note:** AVDD comes from the XIAO's 5V pin. That pin is live only on USB, or when 5 V is fed into it.

## Inputs, reference, guard

These are unchanged from R1, apart from routing.

- **Negative inputs:** all eight are tied to SRB1 on the board.
- **Positive inputs:** they run on the bottom layer with one 45° jog and no vias, under guard copper on both layers.
- **SRB2:** it runs on the top layer inside the guard pour, between the header rows, to pin 10.
- **BIASINV:** it runs on the top layer inside the guard pour to pin 13.
- **Guard:** it is fed from BIASOUT through R3 (100 kΩ), filtered by C10 (2.2 µF) to ground, and stitched with 16 vias.
- **Ground:** it is stitched with 17 vias.

## Bias

R2 (1 MΩ) in parallel with C9 (1.5 nF C0G) sits between BIASINV and BIASOUT. The corner is about 106 Hz.

## ADS1299 fixed pins

| Pin | Connection |
|---|---|
| CLKSEL | R5 (10 kΩ) to DVDD; JP3 to ground for an external clock |
| CLK | JP2 to D4 |
| START | low (use the START command) |
| RESET, PWDN | high |
| DAISY_IN, RESV1 | ground |
| GPIO1 | LED D1 through R4 (3.3 kΩ) |

## Assembly

- **Top side (XIAO side):** C1–C10, R1–R3, R5. Bridge the jumpers for your setup (see the table above).
- **Bottom side (ADS side):** U1, D1, R4, then J3.
- **Sockets:** J1/J2 go on last, as long-tail stacking headers on the lower board of a stack.

## Files

| File | Contents |
|---|---|
| `XIAO_ADS1299_8ch_2L_R2.2.kicad_pcb` / `.kicad_pro` | KiCad 7 board and rules |
| `XIAO_ADS1299_8ch_2L_R2.2` gerber/ | Gerbers and drill file |
| `XIAO_ADS1299_8ch_2L_R2.2_BOM.csv` | Bill of materials, with notes for the 2nd board |
| `plots/*_top_bottom.png`, `*_layers.png`, `*_via_check.png` | Both sides, copper by net, via check |
