# LOLIN ADS1299 board, 8 channels, R7.1 (18-pin flex inputs)

**R7.1** is the 18-pin flex board with every via moved off component pads. Earlier revisions: R7. Board-to-board pinouts, parts and placement are unchanged.

## R7.1 changes: vias off pads
A via on or touching a small pad can wick solder away from the joint. On 0.5 mm-pitch pins its ring can also bridge to the next pin.

- **New positions:** each via now sits beside its pad, joined by a short straight or 45° trace.
- **Re-joined traces:** traces that met the via are re-routed from their previous corner, straight and at 45°.
- **Checks:** the design-rule report matches the original board's line for line, nothing is unconnected, and `tools/check_via_in_pad.py` finds no via in any part pad.
- **"Drill clear" rows** in the table: the hole is off the pad and the ring still touches the pad edge. Vias are tented, so no solder reaches the hole.

| Pad | Net | Via moved | Distance | Result |
|---|---|---|---|---|
| C8.2 | GND | (15.50, 15.20) → (14.46, 14.60) | 1.20 mm | clear |
| JP6.1 | GND | (17.85, 8.05) → (17.70, 8.05) | 0.15 mm | clear |
| C5.2 | GND | (9.90, 11.60) → (10.58, 11.78) | 0.70 mm | clear |
| U1.32 | GND | (7.10, 11.60) → (7.09, 11.55) | 0.05 mm | clear |

`plots/*_via_check.png` shows pads and vias, before and after.

## Inputs, aux header and guard
See [`../FLEX18.md`](../FLEX18.md): 18-pin Hirose FH12 flex input, 2×3 1.27 mm aux header, and closed guard on all four layers.

## Files
| File | Contents |
|---|---|
| `LOLIN_ADS1299_R7.1.kicad_pcb` / `.kicad_pro` | KiCad 7 board and rules |
| `gerber/` | Gerbers and drill file (4 layers) |
| `LOLIN_ADS1299_R7.1_BOM.csv` | Bill of materials |
| `plots/` | Top/bottom view, copper by net, via check |
