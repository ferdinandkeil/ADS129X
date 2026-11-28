# LOLIN ADS1298 board, 8 channels, R7.1 (18-pin flex inputs)

**R7.1** is the 18-pin flex board with every via moved off component pads. Earlier revisions: R7. Board-to-board pinouts, parts and placement are unchanged.

## R7.1 changes: vias off pads
A via on or touching a small pad can wick solder away from the joint. On 0.5 mm-pitch pins its ring can also bridge to the next pin.

- **New positions:** each via now sits beside its pad, joined by a short straight or 45° trace.
- **Re-joined traces:** traces that met the via are re-routed from their previous corner, straight and at 45°.
- **Checks:** the design-rule report matches the original board's line for line, nothing is unconnected, and `tools/check_via_in_pad.py` finds no via in any part pad.
- **"Drill clear" rows** in the table: the hole is off the pad and the ring still touches the pad edge. Vias are tented, so no solder reaches the hole.
- **U1 pin 62 (AVDD):** the via sits just past the pin tip with its drill clear. The ring still touches the tip, because neighbouring traces leave no room further out. Its top-side trace to the decoupling cap is rerouted at 45°, where it used to run at an angle.

| Pad | Net | Via moved | Distance | Result |
|---|---|---|---|---|
| U1.62 | AVDD | (17.55, 18.55) → (17.45, 18.55) | 0.10 mm | drill clear |
| C8.2 | GND | (15.50, 15.20) → (15.41, 15.44) | 0.25 mm | clear |
| D1.1 | GND | (8.30, 6.80) → (7.81, 7.14) | 0.60 mm | clear |
| C7.2 | GND | (15.10, 17.15) → (14.63, 17.32) | 0.50 mm | clear |
| JP6.1 | GND | (17.95, 8.30) → (17.66, 8.50) | 0.35 mm | clear |
| C4.2 | GND | (11.30, 13.60) → (9.95, 14.55) | 1.65 mm | clear |
| C3.2 | GND | (9.30, 16.40) → (9.05, 16.57) | 0.30 mm | clear |
| C6.1 | GND | (5.45, 24.05) → (5.19, 24.20) | 0.30 mm | clear |
| J1.17 | WCT | (8.45, 23.50) → (8.45, 23.45) | 0.05 mm | clear |

`plots/*_via_check.png` shows pads and vias, before and after.

## Inputs, aux header and guard
See [`../FLEX18.md`](../FLEX18.md): 18-pin Hirose FH12 flex input, 2×3 1.27 mm aux header, and closed guard on all four layers.

## Files
| File | Contents |
|---|---|
| `LOLIN_ADS1298_R7.1.kicad_pcb` / `.kicad_pro` | KiCad 7 board and rules |
| `gerber/` | Gerbers and drill file (4 layers) |
| `LOLIN_ADS1298_R7.1_BOM.csv` | Bill of materials |
| `plots/` | Top/bottom view, copper by net, via check |
