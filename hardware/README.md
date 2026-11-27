# Hardware index

Each folder holds one board line at its latest revision. Earlier revisions are in the Git history: `git log -- hardware/<folder>` lists them, and each commit message says what changed.

| Folder | Board | Chip | Layers | Latest | Inputs | Status |
|---|---|---|---|---|---|---|
| [`esp32_devkitc`](esp32_devkitc) | ESP32-DevKitC carrier | ADS1299 | 2 | (see its README) | electrode/5 V column | Not ready to fabricate |
| [`xiao_ads1299_main`](xiao_ads1299_main) | XIAO main board, 8 ch | ADS1299 | 4 | R5.1 (history: R5) | 22-pin 0.5 mm FPC + 1×4 aux | Current |
| [`xiao_ads1299_addon`](xiao_ads1299_addon) | XIAO add-on, ch 9–16 | ADS1299 | 4 | R6.1 (history: R4, R6) | 22-pin 0.5 mm FPC + 1×4 aux | Current |
| [`xiao_ads1298_main`](xiao_ads1298_main) | XIAO main board, 8 ch | ADS1298 | 4 | R5.1 (history: R5) | 22-pin FPC, WCT on pin 3 | Current |
| [`xiao_ads1298_addon`](xiao_ads1298_addon) | XIAO add-on | ADS1298 | 4 | R6.1 (history: R6) | 22-pin FPC | Current |
| [`xiao_ads1299_2L`](xiao_ads1299_2L) | XIAO shield, 8 ch, stackable | ADS1299 | 2 | R2.1 (history: R1, R2) | 2×7 right-angle 2.54 mm | Current |
| [`xiao_ad7771_4L`](xiao_ad7771_4L) | XIAO shield, 8 ch | AD7771 | 4 | A2 (history: A1) | 2×7 (R2 pinout) | Current |
| [`xiao_ad7771_2L`](xiao_ad7771_2L) | XIAO board, 8 ch | AD7771 | 2 | A1.1 (history: A1) | 2×7 (R2 pinout) | Weak guard; rework only if needed |

Every folder has the KiCad board (`.kicad_pcb`, `.kicad_pro`), its README, the BOM (and JLC BOM/CPL where made), `gerber/` and `plots/`.

**Not yet in this repository:** main board R1–R4 and add-on R5 of the 4-layer line, the touch-proof jack board (paused; must carry the input protection resistors), and any board still only on your drive. Add them with one commit per revision, oldest first.

## XIAO pin maps

| XIAO | nRF54L15 | ADS1299/1298 main R5 | Add-on R6 | ADS1299 R2 / R2.1 | AD7771 A1/A2-4L | AD7771 2L A1 |
|---|---|---|---|---|---|---|
| D0 | P1.04 | DRDY | — | MOSI | MISO | MOSI |
| D1 | P1.05 | free | — | CS1 (JP1) | MOSI | CS1 |
| D2 | P1.06 | PPG_INT | — | SCK | SCK | SCK |
| D3 | P1.07 | CS | — | CS2 (JP1) | CS1 | CS2 |
| D4 | P1.10 | SDA | — | shared ADS clock (JP2) | CS2 | MISO |
| D5 | P1.11 | SCL | — | free | free | free |
| D6 | P2.08 | CLK pass-through | CLK pass-through | free | free | free |
| D7 | P2.07 | free | CS2 | free | free | MCLK |
| D8 | P2.01 | SCK | SCK | free | RESET | RESET |
| D9 | P2.04 | MISO | MISO | MISO | DRDY (JP2) | free |
| D10 | P2.02 | MOSI | MOSI | DRDY (JP4) | MCLK | DRDY |

- **Main R5 / add-on R6:** standard XIAO SPI on D8–D10, so they work on the XIAO ESP32-S3, nRF52840 and nRF54L15. On the nRF54L15, SPI is entirely on port 2, its fast SPI port.
- **R2 / R2.1:** on the nRF54L15, SPI splits across ports 1 and 2, which may block hardware SPI. Check it with a Devicetree overlay, or move SPI to D8–D10 in a revision.
- **AD7771 4-layer:** on Nordic XIAOs, fit Y1, bridge JP3 and leave D10 as an input. The 8 MHz MCLK pin is on an nRF52840 low-frequency-only pin.
- **ESP32-C3:** D0, D8 and D9 are boot-strapping pins.

nRF54L15 pin names are from [Seeed's XIAO nRF54L15 wiki](https://wiki.seeedstudio.com/xiao_nrf54l15_sense_getting_started/). The AD7771 2L A1 map was reconstructed from notes; check it against its README.

## Shared output header on the 2×7 boards

R2, R2.1 and all AD7771 boards: pins 1–8 IN1–IN8, 9 SRB, 10 NC, 11 BIASOUT, 12 GUARD, 13 BIASINV, 14 GUARD. It carries no power or I2C; the sensor board takes those from the XIAO stack headers.

## Firmware channel remaps

| Board | `IN[]` |
|---|---|
| AD7771 A2-4L | `[1, 0, 2, 3, 7, 6, 5, 4]` |
| AD7771 A1-4L | `[1, 0, 3, 2, 7, 6, 5, 4]` |
| AD7771 2L A1 | `[2, 3, 0, 1, 4, 5, 6, 7]` |

## Checking for vias on pads

`tools/check_via_in_pad.py <board.kicad_pcb>` lists every via whose drill or ring touches a component pad, classed as exposed pad (expected), mounting tab, or part pad. Run it before ordering. KiCad's own design-rule check does not flag a via on a pad of the same net.

## Open items

- [ ] Check R2's SPI pins on the nRF54L15 with a Devicetree overlay, or revise R2 to use D8–D10.
- [ ] Measure the XIAO nRF54L15 stack's idle current when powered through USB (its SAMD11 programming chip stays on).
- [ ] Check AVDD at the ADS1299 under load when the 5 V comes from the boost converter (needs 4.75–5.25 V), and look for switching spurs.
- [ ] Consider the R2.1 guard filter (100 k + 2.2 µF) on the main R5 guard source (R7, currently 4.7 k with no filter capacitor).
- [ ] Resume the touch-proof jack board, with the input protection resistors on it.
- [ ] Add the missing revisions listed above from your drive.
- [ ] R2.x: reroute CLKSEL near C8 so C8's ground via can leave its pad (see the R2.1 README).
