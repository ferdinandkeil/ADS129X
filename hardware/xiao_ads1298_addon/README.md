# XIAO ADS1299 / ADS1298: main board R5 and add-on R6 (flex-cable inputs)

## Revision R6.1: vias moved off component pads

R6.1 changes only vias that sat on or touched component pads in R6. Solder can wick down such a via and starve the joint, and on 0.5 mm-pitch pins the via ring can bridge to the next pin.

No circuit, part or placement changes, and the Gerbers are regenerated. The design-rule report is identical to the previous revision's: no new errors and nothing unconnected. `plots/*_via_check.png` shows pads and vias as outlines, before and after.

- **"Clear":** the via now sits fully off the pad, joined by a short trace.
- **"Drill clear":** the drill hole is fully off the pad, and the via ring still overlaps the pad slightly where there was no room for more. Vias are tented, so no solder reaches the hole.
- **Not changed:** vias in J3's large mounting tabs are kept on purpose.

| Pad | Net | Change |
|---|---|---|
| **U1 pin 62** | AVDD | Drill was in the pin, between RLDINV and RLDOUT. Now drill clear, moved 0.10 mm. |
| C3, C4, C5, C8, C9 | GND | Clear. |
| C5 pin 1 | VCAP2 | Clear. |
| C9 pin 1 | DVDD | Clear. |
| J3 pin 22 | GUARD | Ring touched the pad. Now clear. |

These replace the 2×14 / 2×11 J3 header with a **22-pin, 0.5 mm FPC connector** for the guarded inputs. A small **1×4 header** carries the bias and GPIO lines. Power, ground and I²C now reach the sensor shield through the stack headers, so they're gone from J3.

| Board | Size | Layers | KiCad check |
|---|---|---|---|
| XIAO ADS1299 8ch main R5 | 20.05 × 26.55 mm | 4 | 0 unconnected, 0 clearance / hole / edge errors |
| XIAO ADS1299 add-on R6 | 20.25 × 26.55 mm | 4 | same; 2 short router stubs flagged as dangling (CLK, MISO; outside the guard zone, as on R5) |
| XIAO ADS1298 8ch main R5 | 20.05 × 26.55 mm | 4 | 0 unconnected, 0 clearance / hole / edge errors |
| XIAO ADS1298 add-on R6 | 20.25 × 26.55 mm | 4 | same as the ADS1299 add-on |

- **Stack headers:** J1 and J2 (XIAO / stack) are unchanged, checked pad by pad against main R4 / add-on R5.
- **Remaining notices:** small-text, silkscreen and library notices, plus courtyard touches:
  - J4 vs J3, both boards: header outline vs connector outline;
  - main ADS1298: C6 vs J2;
  - add-on: R5 vs J2.

  The parts clear each other physically.

## J3: input cable connector
- **Part:** Hirose **FH12-22S-0.5SH(55)**: 22 positions, 0.5 mm pitch, bottom contact, slide-lock ZIF.
- **Placement:** ADS side, cable entering from the bottom edge.
- **Cables:** fits the Adafruit / SparkFun 22-pin 0.5 mm cables.

**Pinout, left to right as seen from the XIAO side** (also on the silkscreen):

| Positions | ADS1299 | ADS1298 |
|---|---|---|
| 1 | GUARD | GUARD |
| 2 | SRB1 | PACE1 |
| 3 | SRB2 | **WCT** |
| 4 | GUARD | GUARD |
| 5–12 | IN1P IN1N IN2P IN2N IN3P IN3N IN4P IN4N | same |
| 13 | GUARD | GUARD |
| 14–21 | IN5P IN5N IN6P IN6N IN7P IN7N IN8P IN8N | same |
| 22 | GUARD | GUARD |

- **Silkscreen:** every J3 contact is labelled on the ADS side, between the chip and J3, in two staggered rows. Each label has a leader line ending at its contact pad.
- **Foil shield:** both outermost conductors are guard, so the copper foil can be soldered to either end lead.
- **Mounting tabs:** J3's two mounting tabs are also guard.

## J4: aux header (1×4, 1.27 mm, ADS side, right edge)

| Pin | ADS1299 | ADS1298 |
|---|---|---|
| 1 | GPIO2 | GPIO2 |
| 2 | GPIO3 | GPIO3 |
| 3 | BIASOUT | RLDOUT |
| 4 | BIASINV | RLDINV |

The pinout is the same on main and add-on, so the sensor shield can treat every board alike.

## Guard
- **Same closed guard region as R4/R5** on all four layers, from the chip's input pins down over all of J3 to the bottom edge.
- **Under the chip:** on the inner layers and the ADS side, the guard reaches up under the chip body in one straight edge, just inside the supply pins. The input pins have guard on every side, and main and add-on match in this area.
  - **Cut-outs:** one small cut-out each: a ground via on the main boards, an AVDD via on the add-ons.
- **Inner 2:** one solid guard sheet under the whole input fan.
- **Input fan (ADS side):** 16 traces with no vias. IN1P–IN4N run straight down to J3; IN5P–IN8N take a single 0.5 mm jog.
- **SRB1/SRB2:** leave the chip corner under its body and run beside IN1P to J3, inside the guard pour.
- **J3 guard pads:** each of the four has its own via into the guard planes, directly in front of the contact row.
- **Guard source** (main board only, unchanged): R7 = BIAS/RLD out through 4.7 k, R8 = divider.
  - R7 moved beside J4, on the XIAO side.
- **Add-on guard:** the add-on has no guard source. Its guard comes back from the sensor shield through its own J3 cable, so the shield must tie the guard conductors of both cables together. The stack headers are unchanged.

## Changes
- **Bias/RLD and GPIO2/3:** they leave the chip on short vias and run on **inner 1** down the right side to J4. That keeps them under the inner-2 guard sheet and away from the inputs.
- **ADS1298 WCT:**
  - from pin 64 (bottom-right corner), across under the fan on inner 1, guarded above and below, to J3 position 3;
  - on the main board, C6 (100 pF) stays on WCT, beside the chip;
  - **the add-on now brings WCT out too**, as requested; pin 18 (PACE2) is left open on both boards.
- **GPIO3 and BIASOUT/RLDOUT on the add-on:** now brought out to J4 as well.
- **Add-on R5 (CS2 pull-up):** moved beside J2's CS2 pin.
- **Gone from the inputs connector:** SDA, SCL, PPG_INT, 3V3 and GND. The sensor shield gets these from the stack headers.

## Notes for the sensor shield
- **Contact side:** J3 is bottom-contact. With an "A-B" cable whose contacts are on opposite sides at each end, the shield's connector needs to keep pin 1 on the same end. Check the cable once before laying the shield out.
- **Guard ties:** tie all J3 guard positions from every cable together on the shield, plus the foil drain.
