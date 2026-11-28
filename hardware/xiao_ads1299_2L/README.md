# XIAO-ADS1299 8-channel shield, 2-layer, R2.3 (stackable to two boards)

**R2.3** re-lays the top half so SPI uses the XIAO's standard SPI pins (D8–D10). On the XIAO nRF54L15 these are the fixed pins of SPIM00, its high-speed SPI; ESP32 boards can remap SPI, but the nRF54L15 can't. D4/D5 (I²C) and D2 stay free and pass straight through the stacking headers to a sensor board.

| XIAO pin | nRF54L15 | R2.3 | R2.2 |
|---|---|---|---|
| D0 | P1.04 | DRDY (JP4) | MOSI |
| D1 | P1.05 | shared ADS clock (JP2) | CS, board 1 |
| D2 | P1.06 | free (PPG_INT on a sensor board) | SCK |
| D3 | P1.07 | CS, board 1 (JP1) | CS, board 2 |
| D4 / D5 | P1.10 / P1.11 | free: I²C pass-through | shared clock / free |
| D6 | P2.08 | free | free |
| D7 | P2.07 | CS, board 2 (JP5) | free |
| D8 | P2.01 | SCK | free |
| D9 | P2.04 | MISO | MISO |
| D10 | P2.02 | MOSI | DRDY |

This is the same pin map as the 4-layer main R5.2 / add-on R6.2 boards, so one firmware build serves both.

**Other R2.3 changes:**
- **CS jumpers:** the 3-pad JP1 is now two 2-pad jumpers in the top corners: JP1 (marked 1) sends CS to D3 and JP5 (marked 2) sends it to D7. CS runs along the top edge to both.
- **Jumper positions:** JP2 (CLK) and JP4 (DRDY) sit next to D1/D0.
- **R1 (CS pull-up)** is on the bottom side, top-left; R4 (LED resistor) is on the top side.
- **LED:** D1 is driven by **GPIO4** (pin 46) and is **active-low**: its anode is on DVDD, so writing GPIO4 low turns it on. GPIO1's pin sits between SCK and MISO with no room for a via.
- **Power:** AVDD comes from the 5 V pin on the bottom layer through the empty corner beside the chip, then down the inside of J2. DVDD comes straight from the 3V3 pin into pin 50, as before.
- **SRB2** no longer crosses the input area. On the top side it runs from its via beside pin 18 straight across at the via's level, then in one 45° diagonal down to J3 between pins 9 and 11, and into pin 10. The top-side guard under the input header pins is one solid pour.
- **Ground fill:** every ground pocket cut off by the SPI, CS and power traces (around 3V3/MOSI/MISO/SCK/CS2 on top, D7–D9 on the bottom, between DRDY and MOSI at the top) is kept and stitched to ground on the other layer with its own via, 7 in all. Only slivers under 2 mm² stay empty, where no via fits.
- **Decoupling, under-chip fan-out and inputs:** unchanged from R2.2, apart from C4 moving 0.4 mm left and C9 0.05 mm up.

The design-rule report has no clearance errors and nothing unconnected, `tools/check_via_in_pad.py` finds no via in any part pad, and every trace is straight or at 45°. The only notes are library-footprint notes and the solder-jumper outlines trimmed at their own pads.

**XIAO nRF54L15 firmware:** use `&spi00` (Zephyr's `xiao_spi`) on D8–D10, DRDY on D0 as an interrupt (P1 supports it; P2 GPIOs don't), and disable `uart21` when a second board uses D7 as CS: `&uart21 { status = "disabled"; };`.

**R2.2 changes from R2.1:** C8 became 0402 and its ground via moved out of its pad.

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
- **Silkscreen names (ADS side, above row 1 and below row 2):** IN1–IN8; SR1/SR2 = SRB1/SRB2; BIA = BIASOUT; BI- = BIASINV; GRD = GUARD.

## Solder jumpers (top side, all ship open)

| Jumper | Silkscreen | Bridge | Effect |
|---|---|---|---|
| JP1 | 1 (top-left corner) | bridge | chip select on D3 (board 1, or a single board) |
| JP5 | 2 (top-right corner) | bridge | chip select on D7 (board 2) |
| JP4 | DRDY | bridge | DRDY to D0. Board 1 or a single board only; leave open on board 2 |
| JP2 | CLK | bridge | ADS1299 CLK pin to D1, the shared clock line. Bridge on both boards of a stack; leave open on a single board |
| JP3 | EXT | bridge | CLKSEL to ground: external clock. Board 2 only. R5 (10 kΩ) holds CLKSEL high otherwise |

| Setup | JP1 | JP5 | JP4 | JP2 | JP3 |
|---|---|---|---|---|---|
| Single board | bridged (D3) | open | bridged | open | open |
| Stack, board 1 (clock master) | bridged (D3) | open | bridged | bridged | open |
| Stack, board 2 | open | bridged (D7) | open | bridged | bridged |

- **Board 1 firmware:** set CLK_EN in CONFIG1 so board 1 drives its 2.048 MHz clock onto D1.
- **Starting both boards together:** pull both CS lines low and send START (or SYNC). The two boards then convert in lockstep, and board 1's DRDY times both. Read board 1, then board 2, on the shared SPI bus.
- **Don't drive D1 from the XIAO:** leave it as an input.

## Second board in a stack

- **Leave off R2, C9 and R3.** C10 is optional on board 2: fitted, it just adds to the shared guard capacitance. Then only board 1's feedback network is on the shared BIASINV line, and board 2's guard comes only from board 1 over pin 14.
- **Firmware:** power down board 2's bias amplifier (PD_BIAS = 0 in CONFIG3). Its BIAS_SENSP/N channels still sum into board 1's amplifier through the shared BIASINV, as the ADS1299 datasheet shows for multiple devices.
- **Wiring:** link pins 13 and 14 between the boards, and make sure both boards' SRB1 lines reach the same reference electrode.
- **Stacking headers:** J1/J2 on the lower board need long-tail stacking headers so the XIAO pins pass through.

## XIAO pin use

| XIAO pin | ESP32-S3 GPIO | nRF54L15 | Signal |
|---|---|---|---|
| D0 | GPIO1 | P1.04 | DRDY (JP4) |
| D1 | GPIO2 | P1.05 | shared ADS clock (JP2) |
| D3 | GPIO4 | P1.07 | CS, board 1 (JP1) |
| D7 | GPIO44 | P2.07 | CS, board 2 (JP5) |
| D8 | GPIO7 | P2.01 | SCK |
| D9 | GPIO8 | P2.04 | MISO (ADS DOUT) |
| D10 | GPIO9 | P2.02 | MOSI (ADS DIN) |
| D2, D4, D5, D6 | | | not used; pass through the stacking headers (D4/D5 = I²C) |
| 5V | n/a | n/a | AVDD (ADS analog 5 V) |
| 3V3 | n/a | n/a | DVDD |
| GND | n/a | n/a | GND |

- **CS pull-up:** R1 (10 kΩ) sits on the ADS side of the CS jumpers, so an unbridged chip stays deselected.
- **Arduino setup:** `SPI.begin(D8 /*SCK*/, D9 /*MISO*/, D10 /*MOSI*/)`, mode 1, with CS on D3 and D7 handled in code.
- **Power note:** AVDD comes from the XIAO's 5V pin. That pin is live only on USB, or when 5 V is fed into it.

## Inputs, reference, guard

These are unchanged from R1, apart from routing.

- **Negative inputs:** all eight are tied to SRB1 on the board.
- **Positive inputs:** they run on the bottom layer with one 45° jog and no vias, under guard copper on both layers.
- **SRB2:** it runs on the top layer from a via beside pin 18, in one 45° diagonal to J3 between pins 9 and 11, and into pin 10.
- **BIASINV:** it runs on the top layer inside the guard pour to pin 13.
- **Guard:** it is fed from BIASOUT through R3 (100 kΩ), filtered by C10 (2.2 µF) to ground, and stitched with 16 vias.
- **Ground:** it is stitched with 17 vias.

## Bias

R2 (1 MΩ) in parallel with C9 (1.5 nF C0G) sits between BIASINV and BIASOUT. The corner is about 106 Hz.

## ADS1299 fixed pins

| Pin | Connection |
|---|---|
| CLKSEL | R5 (10 kΩ) to DVDD; JP3 to ground for an external clock |
| CLK | JP2 to D1 |
| START | low (use the START command) |
| RESET, PWDN | high |
| DAISY_IN, RESV1 | ground |
| GPIO4 | LED D1 through R4 (3.3 kΩ), active-low (anode on DVDD) |
| GPIO1–3 | not connected |

## Assembly

- **Top side (XIAO side):** C1–C10, R2–R5. Bridge the jumpers for your setup (see the table above).
- **Bottom side (ADS side):** U1, D1, R1, then J3.
- **Sockets:** J1/J2 go on last, as long-tail stacking headers on the lower board of a stack.

## Files

| File | Contents |
|---|---|
| `XIAO_ADS1299_8ch_2L_R2.3.kicad_pcb` / `.kicad_pro` | KiCad 7 board and rules |
| `XIAO_ADS1299_8ch_2L_R2.3` gerber/ | Gerbers and drill file |
| `XIAO_ADS1299_8ch_2L_R2.3_BOM.csv` | Bill of materials, with notes for the 2nd board |
| `plots/*_top_bottom.png`, `*_layers.png`, `*_via_check.png` | Both sides, copper by net, via check |
