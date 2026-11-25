# XIAO-AD7771 8-channel shield, 4-layer, A2-4L (guard rework of A1-4L)

**Board:** 20.0 × 25.8 mm, the same outline, XIAO socket positions and 2×7 header position as the ADS1299 R2. OSH Park 4-layer, about $8 for three boards.

**DRC:** clean, apart from one cosmetic note (the U1 label overlaps a top-side pad opening). Clearance 0.13 mm, traces 0.13–0.25 mm, vias 0.5/0.254 mm, all within OSH Park 4-layer rules.

**Input protection:** there is none on this board. Whatever plugs into J3 must carry it, for example 2.2 kΩ in series with each electrode.

## What changed from A1-4L: the input guard

In A1-4L the inputs ran on In2 with guard only beside them. The plane above them was ground, and the layer below them (B, 0.17 mm away) was the chip and ordinary ground. In A2 every input trace is enclosed in guard on all four sides:

| Layer | Over the input lanes |
|---|---|
| F (XIAO side) | **Guard pour** over the side strips and the whole band down to the header, 0.17 mm above the lanes |
| In1 | **The eight input lanes**, at 0.91 mm or wider pitch, with a **guard finger between each pair of lanes** and guard around the bundle |
| In2 | **Guard pour** under all of the lanes. A ground island remains only under the chip centre, away from the lanes. |
| B (chip side) | AD7771, op-amp block, digital fan-out. Shielded from the lanes by the In2 guard. |

- **Guard fingers:** each finger between two lanes is enclosed at both ends, so each one has its own guard via. There are 27 guard vias in all. No floating copper is left on In1.
- **Vias in the guard area:** no ground, digital or supply vias pass through the In1 guard area. The router was blocked from placing vias there, and the only vias near the lanes are the inputs' own vias, two SRB vias and one AVDD via in a small ground window above the lanes.
- **Lane path:** each input leaves its AINx+ pin on B, drops through a via straight into In1, and stays there until it reaches its header pin.
- **SRB link:** the left-to-right SRB link now runs on In2, 1.2 mm below the lanes, instead of on F. In A1 the link ran 0.17 mm from the inputs.

## Circuit changes from A1-4L

- **R1 (1 MΩ) goes from BIASINV to GUARD** instead of to the buffer output. GUARD sits behind R3 (4.7 kΩ), so the difference is 0.5 % and does not matter. This let the whole op-amp block be routed on B with no vias through the input layer.
- **Op-amp sections:** the same as A1-4L.
  - B is the bias integrator: +INB = AD7771 VCM, −INB = BIASINV, OUTB → R2 (100 kΩ) → BIASOUT.
  - A is the guard buffer: +INA = SRB, OUTA → R3 → GUARD.
- **U2 supply:** a single AVDD via in the ground window, then under the MSOP body to V+.
- **Unchanged:**
  - Reference: internal; select REF_MUX_CTRL = 01 and enable the reference buffer.
  - Clock: from XIAO D10 through R7. Y1 + JP3 remain the optional oscillator.
  - No status LED.

## Output header J3 (2×7): same pinout as R2

| Column | Inner row | Outer row |
|---|---|---|
| 1 | 1 IN1 (AIN1) | 2 IN2 (AIN0) |
| 2 | 3 IN3 (AIN2) | 4 IN4 (AIN3) |
| 3 | 5 IN5 (AIN7) | 6 IN6 (AIN6) |
| 4 | 7 IN7 (AIN5) | 8 IN8 (AIN4) |
| 5 | 9 SRB | 10 NC |
| 6 | 11 BIASOUT | 12 GUARD |
| 7 | 13 BIASINV | 14 GUARD |

**Channel order:** this order keeps the fans free of crossings. Reorder in firmware: `IN = [1, 0, 2, 3, 7, 6, 5, 4]`. This differs from A1-4L, where IN3 and IN4 were swapped.

## XIAO pins (same as A1-4L)

| XIAO | Signal |
|---|---|
| D0 | MISO (SDO) |
| D1 | MOSI (SDI) |
| D2 | SCK |
| D3 | CS, board 1 (JP1 to D3) |
| D4 | CS, board 2 (JP1 to D4) |
| D10 | MCLK |
| D9 | DRDY (JP2) |
| D8 | RESET |

## Solder jumpers (all ship open)

| Jumper | Side | Bridge | Effect |
|---|---|---|---|
| JP1 | bottom | centre to D3 | CS on D3 (single board or board 1) |
| JP1 | bottom | centre to D4 | CS on D4 (board 2) |
| JP2 | bottom | bridged | DRDY to D9 (board 1 only) |
| JP3 | top | bridged | Y1 onto MCLK (only with Y1 fitted) |

## Two boards in a stack

- **Board 2:** JP1 to D4, JP2 open, and leave off R1, R2 and C13.
- **Wiring:** link pins 13 and 14 between the boards, and wire both SRB pins to the same electrode.

## Assembly

- **Bottom:** U1, U2 and all other passives.
- **Top:** C4, C5, C6, C9 and C11 (100 nF, behind U1), plus the optional Y1 and JP3.
- **Pick-and-place files:** `_BOM_JLC.csv`, `_CPL_JLC.csv`.
- **`_layers.png`:** each copper layer coloured by net. Guard is green, inputs red, SRB blue, ground grey.
