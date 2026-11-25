# XIAO-ADS1299 8-channel shield, 2-layer, R1

A single-board ADS1299 shield for a XIAO ESP32-S3. The XIAO plugs into J1/J2 on the top side. The ADS1299, the input traces and the 2×6 right-angle output header are on the bottom side.

- **Board:** 20.0 × 25.8 mm (0.787 × 1.016 in = 0.80 sq in), 2 layers, 1.6 mm. XIAO pin labels are 0.6 mm text, rotated, in the strips beside the sockets.
- **Fab rules used:** 0.15 mm clearance, 0.15–0.3 mm tracks, 0.6/0.3 mm vias. DRC is clean.

## Output header J3 (2×6, 2.54 mm, right angle, body overhanging the bottom edge)

Pin n carries signal n. Odd pins are on the inner row and even pins on the outer row. Pin 1 is marked with a dot on the ADS side.

| Column | Inner row | Outer row |
|---|---|---|
| 1 | 1 IN1P | 2 IN2P |
| 2 | 3 IN3P | 4 IN4P |
| 3 | 5 IN5P | 6 IN6P |
| 4 | 7 IN7P | 8 IN8P |
| 5 | 9 SRB1 | 10 BIASOUT |
| 6 | 11 GUARD | 12 SRB2 |

The header carries no power, ground or GPIO.

## Inputs, reference, guard

- **Negative inputs:** all eight negative inputs (IN1N–IN8N) are tied to SRB1 on the board, by a bus under the chip body. Every channel therefore reads INxP − SRB1, whatever the SRB1 bit in MISC1 is set to.
- **Input traces:** the positive inputs run on the bottom layer, straight from the pin row to the header with one 45° jog and no vias. Even channels pass between the inner-row pads to reach the outer row.
- **SRB2:** it crosses under the input fan on the top layer, inside the guard pour, to pin 12. This is the only signal that crosses the inputs, and only through the board's thickness, which couples about 1 fF per crossing.
- **Guard pour:** both layers carry it from under the chip's input pins all the way to the board edge past the header, including a continuous strip below the outer row. It is stitched between layers with 27 vias.
- **Guard drive:** the guard is fed from BIASOUT through R3 (4.7 kΩ). There is no capacitor on the guard.
- **Ground:** both layers carry it everywhere else, stitched with 25 vias plus routed vias.

## Bias

- **Feedback:** R2 (1 MΩ) in parallel with C9 (1.5 nF C0G) sits between BIASINV and BIASOUT, on the top side next to the chip.
- **BIASOUT:** it goes straight to header pin 10. There is no current limiter on this board; the JFET limiter belongs on the sensor/electrode side as planned.

## XIAO pin use (ESP32-S3)

SPI is on pins that give crossing-free routing. The ESP32's SPI pins can be assigned to any GPIO in firmware.

| XIAO pin | ESP32-S3 GPIO | Signal |
|---|---|---|
| D0 | GPIO1 | MOSI (ADS DIN) |
| D1 | GPIO2 | CS (10 kΩ pull-up, R1) |
| D2 | GPIO3 | SCK |
| D9 | GPIO8 | MISO (ADS DOUT) |
| D10 | GPIO9 | DRDY |
| 5V | n/a | AVDD (ADS analog 5 V) |
| 3V3 | n/a | DVDD |
| GND | n/a | GND |

Arduino setup: `SPI.begin(D2 /*SCK*/, D9 /*MISO*/, D0 /*MOSI*/, D1 /*CS*/)`, mode 1.

**Power note:** AVDD comes from the XIAO's 5V pin. That pin is live only on USB, or when 5 V is fed into it; it is not powered from a battery on the XIAO's battery pads.

## ADS1299 fixed pins

| Pin | Connection |
|---|---|
| CLKSEL | high (internal oscillator); CLK pin left open |
| START | low (use the START command) |
| RESET, PWDN | high (use the RESET command) |
| DAISY_IN, RESV1 | ground |
| GPIO1 | LED D1 through R4 (3.3 kΩ); set GPIO1 as an output, high = on |
| GPIO2–4 | open; set them as outputs in firmware so they don't float |
| BIASREF, BIASIN, NC pins | open |

**ADS1299-6:** it fits the same footprint. Header pins 7 and 8 are then unused. The datasheet asks for unused inputs to go to AVDD; on this board IN7N/IN8N are tied to SRB1 instead, and IN7P/IN8P are open at the header.

## Silkscreen

- **Top (XIAO side):** XIAO outline with the USB end, pin functions beside each socket (MOSI, CS, SCK, 5V, GND, 3V3, DRDY, MISO; unused pins by D number) and part references.
- **Bottom (ADS side):** XIAO D numbers beside each socket, ADS1299 and U1, D1, and header pin names (IN1–IN8, SR1/SR2 = SRB1/SRB2, BIA = BIASOUT, GRD = GUARD) with a pin-1 dot.
- **Silkscreen text:** 0.8 mm where it fits, 0.6 mm (0.12 mm stroke) for the pin labels and the tightest part references. Every part and XIAO pin is labeled.

## Assembly

- **Top side (XIAO side):** C1–C9, R1–R3. All passives are 0603 except C1/C3 (0805) and C4 (1206), which suits hot-plate reflow.
- **Bottom side (ADS side):** U1, D1, R4, then J3.
- **Sockets:** the XIAO sockets J1/J2 go on last.

## Files

| File | Contents |
|---|---|
| `XIAO_ADS1299_8ch_2L_R1.kicad_pcb` | KiCad 7 board |
| `XIAO_ADS1299_8ch_2L_R1_gerbers.zip` | Gerbers and drill file |
| `XIAO_ADS1299_8ch_2L_R1_BOM.csv` | Bill of materials |
| `*_copper.png` | Copper and silkscreen views |
| `*_copper.png` | Guard copper only |
| `*_assembly.png` | Assembly drawing |
| `*_silkscreen.png` | Silkscreen, both sides |
