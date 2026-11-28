# Hardware index

Each folder holds one board line at its latest revision. Earlier revisions are in the Git history: `git log -- hardware/<folder>` lists them, and each commit message says what changed.

| Folder | Board | Chip | Layers | Latest | Inputs | Status |
|---|---|---|---|---|---|---|
| [`esp32_devkitc`](esp32_devkitc) | ESP32-DevKitC carrier | ADS1299 | 2 | (see its README) | electrode/5 V column | Not ready to fabricate |
| [`xiao_ads1299_main`](xiao_ads1299_main) | XIAO main board, 8 ch | ADS1299 | 4 | R5.2 (history: R5) | 18-pin 0.5 mm flex + 2×3 aux | Current |
| [`xiao_ads1299_addon`](xiao_ads1299_addon) | XIAO add-on, ch 9–16 | ADS1299 | 4 | R6.2 (history: R6) | 18-pin flex + 2×3 aux | Current |
| [`xiao_ads1298_main`](xiao_ads1298_main) | XIAO main board, 8 ch | ADS1298 | 4 | R5.2 (history: R5) | 18-pin flex, WCT on pin 17 | Current |
| [`xiao_ads1298_addon`](xiao_ads1298_addon) | XIAO add-on | ADS1298 | 4 | R6.2 (history: R6) | 18-pin flex | Current |
| [`lolin_ads1299`](lolin_ads1299) | LOLIN board, 8 ch | ADS1299 | 4 | R7.1 (history: R7) | 18-pin flex + 2×3 aux | Current |
| [`lolin_ads1298`](lolin_ads1298) | LOLIN board, 8 ch | ADS1298 | 4 | R7.1 (history: R7) | 18-pin flex + 2×3 aux | Current |
| [`xiao_ads1299_2L`](xiao_ads1299_2L) | XIAO shield, 8 ch, stackable | ADS1299 | 2 | R2.3 (history: R1, R2, R2.1, R2.2) | 2×7 right-angle 2.54 mm | Current |
| [`xiao_ad7771_4L`](xiao_ad7771_4L) | XIAO shield, 8 ch | AD7771 | 4 | A2 (history: A1) | 2×7 (R2 pinout) | Current |
| [`xiao_ad7771_2L`](xiao_ad7771_2L) | XIAO board, 8 ch | AD7771 | 2 | A1.2 (history: A1, A1.1) | 2×7 (R2 pinout) | Weak guard; rework only if needed |

The 18-pin flex, aux-header and guard design shared by the main, add-on and LOLIN boards is described in [`FLEX18.md`](FLEX18.md).

Every folder has the KiCad board (`.kicad_pcb`, `.kicad_pro`), its README, the BOM (and JLC BOM/CPL where made), `gerber/` and `plots/`.

**Not yet in this repository:** main board R1–R4 and add-on R5 of the 4-layer line, the touch-proof jack board (paused; must carry the input protection resistors), and any board still only on your drive. Add them with one commit per revision, oldest first.

## XIAO pin maps

| XIAO | nRF54L15 | ADS1299/1298 main R5.2 | Add-on R6.2 | ADS1299 2L R2.3 | AD7771 A1/A2-4L | AD7771 2L A1 |
|---|---|---|---|---|---|---|
| D0 | P1.04 | DRDY | — | DRDY (JP4) | MISO | MOSI |
| D1 | P1.05 | CLK pass-through | CLK pass-through | shared ADS clock (JP2) | MOSI | CS1 |
| D2 | P1.06 | PPG_INT | — | free | SCK | SCK |
| D3 | P1.07 | CS | — | CS1 (JP1) | CS1 | CS2 |
| D4 | P1.10 | SDA | — | free (I²C) | CS2 | MISO |
| D5 | P1.11 | SCL | — | free (I²C) | free | free |
| D6 | P2.08 | free | — | free | free | free |
| D7 | P2.07 | free | CS2 | CS2 (JP5) | free | MCLK |
| D8 | P2.01 | SCK | SCK | SCK | RESET | RESET |
| D9 | P2.04 | MISO | MISO | MISO | DRDY (JP2) | free |
| D10 | P2.02 | MOSI | MOSI | MOSI | MCLK | DRDY |

- **Main R5.2 / add-on R6.2:** standard XIAO SPI on D8–D10, so they work on the XIAO ESP32-S3, nRF52840 and nRF54L15. On the nRF54L15 these are SPIM00's fixed high-speed pins (P2.01/P2.04/P2.02), which ESP32 boards can map anywhere but the nRF54L15 can't. DRDY is on P1, which supports interrupts. The ADS clock passes between boards on D1 (P1.05), which has no default peripheral, so D6/D7 stay free for the XIAO UART. With an add-on fitted, D7 is CS2, so disable `uart21` then.
- **2-layer R2.3:** same pin map as main R5.2 / add-on R6.2: SPI on D8–D10 (SPIM00 on the nRF54L15), DRDY D0, clock D1, CS D3/D7. R2–R2.2 used D0/D2/D9 for SPI, which splits across nRF54L15 ports.
- **AD7771 4-layer:** on Nordic XIAOs, fit Y1, bridge JP3 and leave D10 as an input. The 8 MHz MCLK pin is on an nRF52840 low-frequency-only pin.
- **ESP32-C3:** D0, D8 and D9 are boot-strapping pins.

nRF54L15 pin names are from [Seeed's XIAO nRF54L15 wiki](https://wiki.seeedstudio.com/xiao_nrf54l15_sense_getting_started/). The AD7771 2L A1 map was reconstructed from notes; check it against its README.

## Shared output header on the 2×7 boards

R2–R2.3 and all AD7771 boards: pins 1–8 IN1–IN8, 9 SRB, 10 NC, 11 BIASOUT, 12 GUARD, 13 BIASINV, 14 GUARD. It carries no power or I2C; the sensor board takes those from the XIAO stack headers.

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
- [ ] Consider the R2.1 guard filter (100 k + 2.2 µF) on the main R5.2 guard source (R7, currently 4.7 k with no filter capacitor).
- [ ] Resume the touch-proof jack board, with the input protection resistors on it.
- [ ] Add the missing revisions listed above from your drive.
