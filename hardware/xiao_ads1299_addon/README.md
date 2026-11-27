# XIAO ADS1299 add-on, channels 9–16, R6.2 (18-pin flex inputs)

**R6.2** is the 18-pin flex board with every via moved off component pads. Earlier revisions: R6. Parts and placement are unchanged. **One pinout change:** the ADS clock between stacked boards moved from D6 to D1, and the silkscreen is relabelled to match. Main and add-on boards of the same revision must be used together.

## R6.2 changes: vias off pads
A via on or touching a small pad can wick solder away from the joint. On 0.5 mm-pitch pins its ring can also bridge to the next pin.

- **New positions:** each via now sits beside its pad, joined by a short straight or 45° trace.
- **Re-joined traces:** traces that met the via are re-routed from their previous corner, straight and at 45°.
- **Checks:** the design-rule report matches the original board's line for line, nothing is unconnected, and `tools/check_via_in_pad.py` finds no via in any part pad.
- **"Drill clear" rows** in the table: the hole is off the pad and the ring still touches the pad edge. Vias are tented, so no solder reaches the hole.

| Pad | Net | Via moved | Distance | Result |
|---|---|---|---|---|
| C5.2 | GND | (-3.00, -3.60) → (-2.92, -3.47) | 0.15 mm | clear |
| C8.2 | GND | (2.20, -3.20) → (1.89, -2.75) | 0.55 mm | clear |
| C9.2 | GND | (2.60, -4.20) → (1.92, -4.38) | 0.70 mm | clear |
| C4.2 | GND | (-1.60, -1.60) → (-0.69, -2.02) | 1.00 mm | clear |
| C3.2 | GND | (-2.40, 0.40) → (-2.11, 0.06) | 0.45 mm | clear |
| C5.1 | VCAP2 | (-4.65, -4.93) → (-4.49, -5.12) | 0.25 mm | clear |
| C9.1 | DVDD | (3.60, -4.20) → (3.52, -3.91) | 0.30 mm | clear |

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
| `XIAO_ADS1299_addon_R6.2.kicad_pcb` / `.kicad_pro` | KiCad 7 board and rules |
| `gerber/` | Gerbers and drill file (4 layers) |
| `XIAO_ADS1299_addon_R6.2_BOM.csv` | Bill of materials |
| `plots/` | Top/bottom view, copper by net, via check |
