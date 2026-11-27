# XIAO ADS1298 main board, 8 channels, R5.2 (18-pin flex inputs)

**R5.2** is the 18-pin flex board with every via moved off component pads. Earlier revisions: R5. Parts and placement are unchanged. **One pinout change:** the ADS clock between stacked boards moved from D6 to D1, and the silkscreen is relabelled to match. Main and add-on boards of the same revision must be used together.

## R5.2 changes: vias off pads
A via on or touching a small pad can wick solder away from the joint. On 0.5 mm-pitch pins its ring can also bridge to the next pin.

- **New positions:** each via now sits beside its pad, joined by a short straight or 45° trace.
- **Re-joined traces:** traces that met the via are re-routed from their previous corner, straight and at 45°.
- **Checks:** the design-rule report matches the original board's line for line, nothing is unconnected, and `tools/check_via_in_pad.py` finds no via in any part pad.
- **"Drill clear" rows** in the table: the hole is off the pad and the ring still touches the pad edge. Vias are tented, so no solder reaches the hole.
- **U1 pin 25:** the only spot was 1.5 mm out past the pin tip, between the pin row and J1 pin 4. Its stub can't be straight or 45° there with 0.15 mm clearance, so it's one short angled run on the bottom.
- **U1 pin 62 (AVDD):** the via sits just past the pin tip with its drill clear. The ring still touches the tip, because neighbouring traces leave no room further out. Its top-side trace to the decoupling cap is rerouted at 45°, where it used to run at an angle.

| Pad | Net | Via moved | Distance | Result |
|---|---|---|---|---|
| U1.62 | AVDD | (4.85, 2.75) → (4.75, 2.75) | 0.10 mm | drill clear |
| U1.33 | GND | (-4.20, -5.66) → (-4.25, -5.66) | 0.05 mm | clear |
| C4.2 | GND | (-2.50, -2.00) → (-2.56, -2.35) | 0.35 mm | clear |
| C7.2 | GND | (2.85, 1.60) → (3.35, 1.60) | 0.50 mm | drill clear |
| D1.1 | GND | (1.25, -8.55) → (1.31, -7.85) | 0.70 mm | clear |
| C3.2 | GND | (-2.90, 0.40) → (-2.13, 0.04) | 0.85 mm | clear |
| C8.2 | GND | (2.70, -2.40) → (2.74, -1.90) | 0.50 mm | clear |
| U1.25 | GND | (-5.66, -0.25) → (-6.89, -1.11) | 1.50 mm | clear |
| C4.2 | GND | (-1.90, -1.20) → (-2.56, -1.66) | 0.80 mm | clear |
| C11.2 | GND | (1.30, -1.40) → (1.85, -0.22) | 1.30 mm | clear |
| R6.2 | GND | (0.46, 0.00) → (0.45, -0.10) | 0.10 mm | drill clear |
| U1.32 | GND | (-5.50, -3.80) → (-5.82, -4.25) | 0.55 mm | clear |
| R7.1 | GUARD | (4.50, 10.00) → (4.49, 9.90) | 0.10 mm | clear |

`plots/*_via_check.png` shows pads and vias, before and after.

## XIAO nRF54L15

The XIAO pins match every XIAO, including the nRF54L15, and SPI sits on that board's high-speed SPI:

| XIAO pin | nRF54L15 | Use here |
|---|---|---|
| D8 / D9 / D10 | P2.01 / P2.04 / P2.02 | SCK / MISO / MOSI on **SPIM00** (Zephyr `xiao_spi` = `&spi00`) |
| D0 | P1.04 | DRDY (main). P1 supports pin interrupts; P2 GPIOs don't |
| D3 | P1.07 | CS (main) |
| D7 | P2.07 | CS2 (add-on) |
| D4 / D5 | P1.10 / P1.11 | I²C (`i2c22`), main board only |
| D1 | P1.05 | ADS clock pass-through between stacked boards (moved from D6 in this revision) |

- **D6 is free now.** The clock used to share D6 with the UART TX that Zephyr enables by default (`uart21`), so it moved to D1. With an add-on fitted, D7 is CS2, so disable `uart21` then: `&uart21 { status = "disabled"; };`.
- **USB power only:** AVDD comes from the XIAO 5 V pin through FB1, and that pin carries no usable 5 V when the XIAO runs on battery alone.

## Inputs, aux header and guard
See [`../FLEX18.md`](../FLEX18.md): 18-pin Hirose FH12 flex input, 2×3 1.27 mm aux header, and closed guard on all four layers.

## Files
| File | Contents |
|---|---|
| `XIAO_ADS1298_8ch_R5.2.kicad_pcb` / `.kicad_pro` | KiCad 7 board and rules |
| `gerber/` | Gerbers and drill file (4 layers) |
| `XIAO_ADS1298_8ch_R5.2_BOM.csv` | Bill of materials |
| `plots/` | Top/bottom view, copper by net, via check |
