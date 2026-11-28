# ADS1299 carrier for the ESP32-DevKitC-32

Carrier for an ADS1299 (the built board is stuffed as an ADS1299-6) and a HiLetgo / Espressif ESP32-DevKitC-32 (ESP32-WROOM-32, 38-pin, 2×19). It replaces the Teensy 4.0 carrier. The DevKit plugs into two 1×11 sockets. Its USB port is the serial link, so the old RX/TX/GND/5V header is gone and the electrode side is a single 0.1 inch column.

The layout is KiCad 8.0.9 (`kicad_pcb` version 20240108). KiCad 8 opens it. KiCad 7 does not.

**Do not send this revision to a fab.** The SPI and control nets are routed. The analog supply, the reference caps, and two of the electrode nets are still open, and DRC is not clean. Details are below.

## Size

|  | Teensy carrier | This board |
| --- | --- | --- |
| Outline | 36.83 × 46.99 mm | 31.3 × 32.0 mm bounding box |
| Copper area | about 1731 mm² | about 886 mm² |

The outline is the DevKit width plus a 2.9 mm tab. The main rectangle is 28.4 mm wide. The tab (x = 28.4 to 31.3 mm, y = 0 to 15.6 mm) holds J1. The antenna end of the DevKit lines up with the top edge of this board and overhangs it; that overhang is clear of copper.

## What changed from the Teensy board

- Teensy 4.0 sockets (two rows, 15.24 mm apart, 14 pins) are replaced by DevKitC sockets (two rows, **25.40 mm** apart, 11 pins of the 19).
- The 1×4 RX/TX/GND/5V header is removed. USB on the DevKit is the serial port.
- The 2×14 electrode breakout is a 1×6 column: the three-snap wiring plus analog 5 V and ground.
- No 3.3 V regulator on the carrier, same as the Teensy board. DVDD comes from the DevKit 3V3 pin.
- Analog supply stays a single unipolar 5 V rail. No input filters were added.

## ESP32 wiring

Socket pin 1 is the antenna end. Only pins 1–11 are on this board. Pins 12–19 and the USB connector hang off the bottom.

These match `examples/ESP32_OpenBCI/ESP32_OpenBCI.ino` for the ESP32 Dev Module.

| DevKit pin | GPIO | ADS1299 |
| --- | --- | --- |
| J2.1 | 3V3 | DVDD |
| J2.7 | 32 | PWDN |
| J2.9 | 25 | START |
| J2.10 | 26 | RESET |
| J2.11 | 27 | DRDY |
| J3.1 and J3.7 | GND | GND |
| J3.2 | 23 | DIN (MOSI) |
| J3.8 | 19 | DOUT (MISO) |
| J3.9 | 18 | SCLK |
| J3.10 | 5 | CS |

J2 EN, VP, VN, IO34, IO35, IO33 and J3 IO22, TX, RX, IO21, IO17 are drilled so the DevKit can plug in. They are not connected. TX and RX are not wired; use the DevKit USB connector.

Power the DevKit from its USB port. Do not connect the DevKit 5V pin to the analog 5 V rail. The two grounds are common.

## J1, bottom to top

| Pin | Net | Three-snap use |
| --- | --- | --- |
| 1 | IN1P | signal (EEG or EMG+) |
| 2 | IN1N | EMG−. Leave open for referential EEG |
| 3 | SRB1 | EEG reference |
| 4 | BIASOUT | bias electrode |
| 5 | GND | boost-module ground |
| 6 | AVDD | analog 5 V from the LiPo boost |

Referential EEG: signal on pin 1, reference on pin 3, bias on pin 4. Differential EMG: pins 1, 2, and 4.

## Analog circuit

Unipolar, ADS1299 datasheet Figure 77 style:

- AVDD = 5 V from J1 pin 6 only. AVSS, AVSS1, DGND, VREFN, and RESV1 are the one ground net.
- DVDD = 3.3 V from J2 pin 1.
- CLK is tied to ground. CLKSEL is pulled to DVDD with R2 (10 kΩ) so the internal 2.048 MHz oscillator runs.
- DAISY_IN is tied to ground.
- GPIO1–GPIO4 each have a 10 kΩ pulldown. The firmware does not use them.
- IN7 and IN8 (pins 1–4) are tied to AVDD because the ADS1299-6 has no channels 7 and 8. Cut those ties to stuff an ADS1299-8. Channels 2–6 are left open, which is the same as an unplugged header pin.
- BIASOUT to BIASINV is R1 1 MΩ in parallel with C13 10 nF. That network is an assumption: the original silkscreen values were only partly readable. BIASIN and BIASREF are not connected. SRB2 is not connected.
- Bypass: C1 10 µF and C2 100 nF on AVDD, C3 1 µF on AVDD1, C4 1 µF and C5 100 nF on DVDD, C6 10 µF and C7 100 nF on VREFP, C8 100 µF on VCAP1, C9 1 µF on VCAP2, C10 1 µF and C11 100 nF on VCAP3, C12 1 µF on VCAP4. All returns go to ground.

The PAG package on this footprint has no exposed pad. Ground is the AVSS, AVSS1, DGND, and RESV1 pins, each via-stitched to the bottom plane where a hole fits.

## Ground plane

One bottom-copper zone covers the outline, pulled in 0.30 mm from the edge. Nothing is placed under the 10 mm package body, and no track is routed under that body on either layer. The exposed-pad area does not exist on this package; the ground pins drop to the plane on short top-side traces and vias just outside the body.

SPI (DRDY, START, RESET, PWDN, DOUT, SCLK, CS) crosses the board on the bottom only north of the package, then drops beside the DevKit sockets. Those bottom runs are kept out of the analog input and reference region. A few other bottom runs (electrode drops in the tab, and unfinished VCAP stubs beside the package) are outside the package body. They are a compromise so those nets can reach a header without crossing the top-side fanout, and they are subtracted from the pour.

Digital return current stays on the top fanout and on those northern bottom jumpers, not across the input pins.

## DRC (KiCad 8.0.9)

`kicad-cli pcb drc --severity-error` on this board:

| | Count |
| --- | --- |
| Shorts | 0 |
| Clearance | 20 |
| Unconnected items | 32 |
| Hole-to-hole | 3 |
| Hole clearance | 1 |
| Solder-mask bridge | 1 |

Nets that are one copper piece today: DRDY, START, RESET, PWDN, DIN, DOUT, SCLK, CS, and IN1N.

Still more than one piece: DVDD, AVDD, AVDD1, VREFP, VCAP1–VCAP4, IN1P, SRB1, BIASOUT, BIASINV, CLKSEL. Many of the GND “unconnected” DRC hits are SMD pads that do not yet have a via into the plane. The pour ties the plated holes.

Rules used: 0.15 mm clearance, 0.20 mm tracks (0.15 mm in the 0.5 mm fanout), 0.25 mm copper-to-edge, 0.45 mm hole-edge to hole-edge. The solder-mask minimum web is set to 0 because a 0.5 mm QFP cannot keep a mask web between pads.

## Check before any order

1. Caliper the DevKit you actually have. This board uses the Espressif DevKitC V4 drawing: **25.40 mm** row spacing, 27.94 × 48.26 mm PCB, pin 1 1.30 mm from the antenna edge, antenna metal 6.04 mm past that edge. A “breadboard friendly” WROOM-32 clone is often 22.86 mm (0.9 in) between rows and will not plug in. HiLetgo’s listing does not print the row spacing.
2. Do not order this revision until the open analog nets above are finished and DRC is re-run.
3. Confirm the bias network (1 MΩ || 10 nF) against the board you are copying. BIASREF and BIASIN are unconnected here.
4. Analog 5 V is only J1 pin 6. The boost module ground is J1 pin 5. Do not use the DevKit 5V pin for the ADS1299.
5. The file is KiCad 8. Opening it in KiCad 7 will fail.

## Files

- `ads1299_esp32_devkitc.kicad_pro`, `.kicad_sch`, `.kicad_pcb`
- `build_board.py` regenerates the PCB with KiCad 8’s `pcbnew` module
- `gerber/` — F.Cu, B.Cu, F/B silk, F/B mask, Edge.Cuts, Excellon drill in millimetres
- `plots/ads1299_esp32_devkitc.png` — front copper, back copper, front silk, and the board edge
