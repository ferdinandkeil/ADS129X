# XIAO ADS1299 add-on, Rev 4

Changes from Rev 3:
- grouped sensor connector, analog first;
- no J4 link header;
- guard on all three analog-side layers, set through a GUARD pin.

## Sensor connector J3 (2×11, 1.27 mm, right-angle)
Columns run 1 → 11 from left to right, looking at the XIAO side with the USB end up.

| Col | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Near row | IN1P | IN1N | IN2P | IN2N | IN3P | IN3N | IN4P | IN4N | IN5P | IN5N | IN6P |
| Far row | **GUARD** | GPIO2 | GND | BIASINV | SRB2 | SRB1 | IN8N | IN8P | IN7N | IN7P | IN6N |

These are channels 9–16 of the 16-channel system.

## Guard: set from the main board
- **No divider on this board.** R6/R7 are not fitted; their pads are kept only for standalone use.
- **C11 (4.7 uF tantalum)** filters the guard locally. Its + pad goes to the guard.
- **J3 col 1 (GUARD)** is the guard net. It connects solidly to the guard copper on the ADS side, inner 2 and inner 1.
- **The sensor board must tie this GUARD pin to the main board's GUARD pin.** The add-on's guard then follows the main board's choice:

| Main board fitted | Add-on guard |
|---|---|
| R8 (divider) | AVDD/2 |
| R7 (BIASOUT) | BIASOUT through 4.7 k. C11 then makes it a filtered BIASOUT (about 7 Hz), so the bias amplifier never drives the guard capacitance directly |

Left unconnected, the guard floats. That's harmless, but it gives no benefit.

## Link to the main board: no J4
| Signal | Path |
|---|---|
| CLK | Main chip's CLK → XIAO **D6** pass-through pin → this chip's CLK. Firmware keeps GPIO43 (D6) as an input |
| Data | This chip's DOUT → **MISO** (D9). It is high-impedance while CS2 (D7) is high |
| CS2 | XIAO **D7**, with a 10 k pull-up (R5) |

The two chips are read one after the other: CS (D3) and then CS2 (D7), each 3 + 3 × channels bytes per DRDY.

## Layer use in the analog region (below the ADS input pins)
| Layer | Contents |
|---|---|
| ADS side | Electrode inputs and SRB only; every other fill is guard |
| Inner 2 | Guard plane. Only SRB2 and BIASINV pass through (both analog, both guarded) |
| Inner 1 | Unbroken guard plane |
| XIAO side | Ground pour; CLK, CS2, GPIO2, SCK and supplies |

CLK, CS2, SCK and GPIO2 leave the ADS upward through vias into the digital area.

## Other changes
- **C6 removed.** Pins 64, 62 (BIASIN) and 45 (GPIO3) are left open. The main board handles sensor-board ID with its own GPIO2/3.

## Check
- KiCad check: 0 unconnected pads, 0 clearance errors.
- The remaining notices are small text, silkscreen near the edge, and the C4/C5 courtyard touch.

## Rev note
C5 moved 0.3 mm up so its courtyard no longer overlaps C4 (VCAP1/VCAP2 pads now 0.575 mm apart).
Silkscreen: all labels kept, strokes raised to 0.15 mm (printable) and each label row enlarged as far as the space allows (0.40-0.60 mm).
