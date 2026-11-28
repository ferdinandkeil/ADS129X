#!/usr/bin/env python3
"""ADS1299 carrier for a HiLetgo / Espressif ESP32-DevKitC-32 (25.40 mm row spacing).

KiCad 7 pcbnew. Bottom copper is a single unbroken GND zone. No traces are
routed on the bottom, and nothing is placed under the ADS1299 body.
"""
import faulthandler
import heapq
import math
import os
import sys
import pcbnew

faulthandler.enable()
print("start", flush=True)

OUT = "/workspace/hardware/esp32_devkitc/ads1299_esp32_devkitc.kicad_pcb"
IU = pcbnew.FromMM

# Geometry (mm). Antenna / pin-1 end is +Y.
# Width is the DevKit (27.94 mm) plus one 0.1" column for the electrode
# and 5 V pins. The old Teensy RX/TX/GND/5V header is not fitted: USB on
# the DevKit is the serial port, so that column can sit in a narrow tab.
BOARD_TOP = 32.0
MAIN_BOTTOM = 2.4
MAIN_RIGHT = 28.4
TAB_RIGHT = 31.3
TAB_TOP = 15.6
ELEC_X = 29.95

# DevKitC V4 drawing: pin 1 is 1.30 mm below the antenna edge, pitch 2.54,
# rows 25.40 mm apart. Our top edge lines up with the DevKit top edge.
PIN1_Y = BOARD_TOP - 1.30
LEFT_X = 1.55
RIGHT_X = LEFT_X + 25.40  # 26.95
N_PINS = 11  # pins 1..11; 12..19 overhang with the USB end

CHIP = (14.2, 15.2)  # package center

# Sacred analog rectangle: router may enter it only for analog nets.
# Chip body itself is always blocked.
ANALOG_BOX = (7.5, 0.0, 31.3, 12.2)  # x0,y0,x1,y1


def mm(v):
    return int(IU(v))


def xy(x, y):
    return pcbnew.VECTOR2I(mm(x), mm(y))


board = pcbnew.BOARD()
ds = board.GetDesignSettings()
ds.m_MinClearance = mm(0.15)
ds.m_TrackMinWidth = mm(0.15)
ds.m_ViasMinSize = mm(0.45)
ds.m_ViasMinDrill = mm(0.25)
ds.m_MicroViasMinSize = mm(0.45)
ds.m_MicroViasMinDrill = mm(0.25)
ds.m_MinThroughDrill = mm(0.25)
ds.m_CopperEdgeClearance = mm(0.25)
ds.m_HoleToHoleMin = mm(0.45)
ds.m_HoleClearance = mm(0.2)
ds.m_SilkClearance = mm(0.1)
ds.m_MinSilkTextHeight = mm(0.5)
ds.m_MinSilkTextThickness = mm(0.08)
ds.m_SolderMaskToCopperClearance = mm(0.05)
ds.m_SolderMaskMinWidth = mm(0.0)
ds.SetCopperLayerCount(2)

# Nets
_net_code = 1
NETS = {}


def net(name):
    global _net_code
    if name not in NETS:
        item = pcbnew.NETINFO_ITEM(board, name, _net_code)
        board.Add(item)
        NETS[name] = item
        _net_code += 1
    return NETS[name]


# Outline
OUTLINE = [
    (0, MAIN_BOTTOM),
    (MAIN_RIGHT, MAIN_BOTTOM),
    (MAIN_RIGHT, 0),
    (TAB_RIGHT, 0),
    (TAB_RIGHT, TAB_TOP),
    (MAIN_RIGHT, TAB_TOP),
    (MAIN_RIGHT, BOARD_TOP),
    (0, BOARD_TOP),
]


def add_edge():
    pts = OUTLINE + [OUTLINE[0]]
    for a, b in zip(pts, pts[1:]):
        s = pcbnew.PCB_SHAPE(board)
        s.SetShape(pcbnew.SHAPE_T_SEGMENT)
        s.SetStart(xy(*a))
        s.SetEnd(xy(*b))
        s.SetLayer(pcbnew.Edge_Cuts)
        s.SetWidth(mm(0.1))
        board.Add(s)


add_edge()

# DevKit keep-out note on the drawing layer (antenna is off the board).
dwg = pcbnew.PCB_TEXT(board)
dwg.SetText("ESP32 ANTENNA OVERHANGS PAST THIS EDGE")
dwg.SetPosition(xy(14.2, 31.15))
dwg.SetLayer(pcbnew.F_SilkS)
dwg.SetTextSize(xy(0.7, 0.7))
dwg.SetTextThickness(mm(0.12))
board.Add(dwg)

# ---------- footprints ----------
PADS = []  # dicts: x,y,w,h,shape,net,fp,num  (mm, axis-aligned)


def new_fp(ref, value, at):
    fp = pcbnew.FOOTPRINT(board)
    fp.SetPosition(xy(*at))
    fp.SetReference(ref)
    fp.SetValue(value)
    fp.Reference().SetLayer(pcbnew.F_SilkS)
    fp.Reference().SetTextSize(xy(0.6, 0.6))
    fp.Reference().SetTextThickness(mm(0.1))
    fp.Value().SetLayer(pcbnew.F_Fab)
    fp.Value().SetTextSize(xy(0.5, 0.5))
    fp.Value().SetTextThickness(mm(0.08))
    board.Add(fp)
    return fp


def _place_pad(fp, pad, x, y):
    # x, y are absolute millimetres. KiCad 8 stores the pad position in the
    # footprint's local frame via SetPos; SetPosition keeps the board frame.
    local = pcbnew.VECTOR2I(mm(x) - fp.GetPosition().x, mm(y) - fp.GetPosition().y)
    if hasattr(pad, "SetPos0"):
        pad.SetPos0(local)
    else:
        pad.SetPos(local)
    pad.SetPosition(xy(x, y))
    fp.Add(pad)


def add_smd(fp, number, x, y, w, h, netname, rot_deg=0):
    pad = pcbnew.PAD(fp)
    pad.SetNumber(str(number))
    pad.SetAttribute(pcbnew.PAD_ATTRIB_SMD)
    pad.SetShape(pcbnew.PAD_SHAPE_RECT)
    pad.SetSize(xy(w, h))
    pad.SetLayerSet(pad.SMDMask())
    pad.SetNet(net(netname))
    if rot_deg:
        pad.SetOrientationDegrees(rot_deg)
    _place_pad(fp, pad, x, y)
    # axis-aligned size after rotation
    if rot_deg % 180:
        aw, ah = h, w
    else:
        aw, ah = w, h
    PADS.append({"x": x, "y": y, "w": aw, "h": ah, "net": netname, "round": False})
    return pad


def add_pth(fp, number, x, y, drill, pad_d, netname):
    pad = pcbnew.PAD(fp)
    pad.SetNumber(str(number))
    pad.SetAttribute(pcbnew.PAD_ATTRIB_PTH)
    pad.SetShape(pcbnew.PAD_SHAPE_CIRCLE)
    pad.SetSize(xy(pad_d, pad_d))
    pad.SetDrillSize(xy(drill, drill))
    pad.SetLayerSet(pad.PTHMask())
    pad.SetNet(net(netname))
    _place_pad(fp, pad, x, y)
    PADS.append({"x": x, "y": y, "w": pad_d, "h": pad_d, "net": netname, "round": True})
    return pad


def _fp_shape(fp):
    # KiCad 8 footprint graphics are PCB_SHAPE parented to the footprint.
    return pcbnew.PCB_SHAPE(fp)


def courtyard(fp, x0, y0, x1, y1):
    # Courtyard graphics are omitted. KiCad 8 stores footprint shapes in a
    # local frame and the absolute coordinates used here landed far from the
    # pads, which made every courtyard overlap. Fab does not require them.
    return


def silk_rect(fp, x0, y0, x1, y1):
    ox = fp.GetPosition().x
    oy = fp.GetPosition().y
    corners = [(x0, y0), (x1, y0), (x1, y1), (x0, y1), (x0, y0)]
    for a, b in zip(corners, corners[1:]):
        s = _fp_shape(fp)
        s.SetShape(pcbnew.SHAPE_T_SEGMENT)
        s.SetLayer(pcbnew.F_SilkS)
        s.SetWidth(mm(0.12))
        s.SetStart(pcbnew.VECTOR2I(mm(a[0]) - ox, mm(a[1]) - oy))
        s.SetEnd(pcbnew.VECTOR2I(mm(b[0]) - ox, mm(b[1]) - oy))
        fp.Add(s)


def ref_at(fp, x, y):
    fp.Reference().SetPosition(xy(x, y))
    fp.Value().SetPosition(xy(x, y - 0.9))


# Chip pin geometry. Datasheet PAG, then rotate 90 deg CCW so inputs face -Y
# (toward the electrode tab) and the SPI/control side faces +Y.
NAMES = {
    1: "IN8N", 2: "IN8P", 3: "IN7N", 4: "IN7P", 5: "IN6N", 6: "IN6P",
    7: "IN5N", 8: "IN5P", 9: "IN4N", 10: "IN4P", 11: "IN3N", 12: "IN3P",
    13: "IN2N", 14: "IN2P", 15: "IN1N", 16: "IN1P",
    17: "SRB1", 18: "SRB2", 19: "AVDD", 20: "AVSS", 21: "AVDD", 22: "AVDD",
    23: "AVSS", 24: "VREFP", 25: "VREFN", 26: "VCAP4", 27: "NC", 28: "VCAP1",
    29: "NC", 30: "VCAP2", 31: "RESV1", 32: "AVSS",
    33: "DGND", 34: "DIN", 35: "PWDN", 36: "RESET", 37: "CLK", 38: "START",
    39: "CS", 40: "SCLK", 41: "DAISY_IN", 42: "GPIO1", 43: "DOUT", 44: "GPIO2",
    45: "GPIO3", 46: "GPIO4", 47: "DRDY", 48: "DVDD",
    49: "DGND", 50: "DVDD", 51: "DGND", 52: "CLKSEL", 53: "AVSS1", 54: "AVDD1",
    55: "VCAP3", 56: "AVDD", 57: "AVSS", 58: "AVSS", 59: "AVDD", 60: "BIASREF",
    61: "BIASINV", 62: "BIASIN", 63: "BIASOUT", 64: "RESERVED",
}

# Net assigned to each package pin. GND is the single unipolar return.
PIN_NET = {i: NAMES[i] for i in range(1, 65)}
for i in (20, 23, 32, 33, 49, 51, 53, 57, 58, 31):
    PIN_NET[i] = "GND"
PIN_NET[25] = "GND"  # VREFN tied to AVSS
PIN_NET[37] = "GND"  # CLK tied low; internal oscillator
PIN_NET[41] = "GND"  # DAISY_IN
# IN7/IN8 do not exist on the ADS1299-6; datasheet: unused inputs to AVDD.
for i in (1, 2, 3, 4):
    PIN_NET[i] = "AVDD"
PIN_NET[27] = "NC27"
PIN_NET[29] = "NC29"
PIN_NET[64] = "NC64"
# GPIO pulldowns are separate nets on the pin, resistor to GND.
for i, name in ((42, "GPIO1"), (44, "GPIO2"), (45, "GPIO3"), (46, "GPIO4")):
    PIN_NET[i] = name


def chip_local(n):
    i = n - 1
    side = i // 16
    k = i % 16
    along = 3.75 - k * 0.5  # +3.75 .. -3.75
    row = 5.55
    if side == 0:
        x, y = -row, along
        rot = 90  # pad long axis radial (horizontal)
    elif side == 1:
        x, y = -along, -row
        rot = 0
    elif side == 2:
        x, y = row, -along
        rot = 90
    else:
        x, y = along, row
        rot = 0
    # CCW 90: (x,y) -> (-y, x)
    return -y, x, rot


print("building chip", flush=True)
fp = new_fp("U1", "ADS1299", CHIP)
CHIP_PINS = {}
for n in range(1, 65):
    lx, ly, rot = chip_local(n)
    x, y = CHIP[0] + lx, CHIP[1] + ly
    # Long axis is radial. Top/bottom rows are pitched in X; left/right in Y.
    if abs(ly) > abs(lx):
        w, h = 0.25, 0.95
    else:
        w, h = 0.95, 0.25
    add_smd(fp, n, x, y, w, h, PIN_NET[n], 0)
    CHIP_PINS[n] = (x, y, lx, ly)
# pin 1 mark
cx, cy = CHIP
p1 = CHIP_PINS[1]
s = _fp_shape(fp)
s.SetShape(pcbnew.SHAPE_T_CIRCLE)
s.SetLayer(pcbnew.F_SilkS)
s.SetWidth(mm(0.12))
s.SetCenter(pcbnew.VECTOR2I(mm(p1[0] - 0.85) - fp.GetPosition().x, mm(p1[1] - 0.85) - fp.GetPosition().y))
s.SetEnd(pcbnew.VECTOR2I(mm(p1[0] - 0.55) - fp.GetPosition().x, mm(p1[1] - 0.85) - fp.GetPosition().y))
fp.Add(s)
print("chip pads done", flush=True)
courtyard(fp, cx - 7.2, cy - 7.2, cx + 7.2, cy + 7.2)
print("courtyard ok", flush=True)
silk_rect(fp, cx - 5.05, cy - 5.05, cx + 5.05, cy + 5.05)
ref_at(fp, cx, cy + 8.3)
fp.Reference().SetText("U1 ADS1299")

# Headers: two 1x11 female sockets, DevKitC pin 1 at the antenna end.
J2 = [
    "DVDD", "NC_EN", "NC_VP", "NC_VN", "NC_IO34", "NC_IO35",
    "PWDN", "NC_IO33", "START", "RESET", "DRDY",
]
J2_LABEL = ["3V3", "EN", "VP", "VN", "IO34", "IO35", "IO32", "IO33", "IO25", "IO26", "IO27"]
J3 = [
    "GND", "DIN", "NC_IO22", "NC_TX", "NC_RX", "NC_IO21",
    "GND", "DOUT", "SCLK", "CS", "NC_IO17",
]
J3_LABEL = ["GND", "IO23", "IO22", "TX", "RX", "IO21", "GND", "IO19", "IO18", "IO5", "IO17"]

fpL = new_fp("J2", "ESP32_J2_1x11", (LEFT_X, PIN1_Y - 5 * 2.54))
fpR = new_fp("J3", "ESP32_J3_1x11", (RIGHT_X, PIN1_Y - 5 * 2.54))
for i in range(N_PINS):
    y = PIN1_Y - i * 2.54
    add_pth(fpL, i + 1, LEFT_X, y, 1.0, 1.7, J2[i])
    add_pth(fpR, i + 1, RIGHT_X, y, 1.0, 1.7, J3[i])
courtyard(fpL, LEFT_X - 1.3, PIN1_Y - 10 * 2.54 - 1.4, LEFT_X + 1.3, PIN1_Y + 1.4)
courtyard(fpR, RIGHT_X - 1.3, PIN1_Y - 10 * 2.54 - 1.4, RIGHT_X + 1.3, PIN1_Y + 1.4)
ref_at(fpL, LEFT_X + 2.2, PIN1_Y + 0.2)
ref_at(fpR, RIGHT_X - 3.4, PIN1_Y + 0.2)
fpL.Reference().SetText("J2")
fpR.Reference().SetText("J3")

# Silk pin numbers for the connected pins only, inward.
def silk_text(text, x, y, size=0.55):
    t = pcbnew.PCB_TEXT(board)
    t.SetText(text)
    t.SetPosition(xy(x, y))
    t.SetLayer(pcbnew.F_SilkS)
    t.SetTextSize(xy(size, size))
    t.SetTextThickness(mm(0.1))
    board.Add(t)


for i, lab in enumerate(J2_LABEL):
    if J2[i].startswith("NC"):
        continue
    y = PIN1_Y - i * 2.54
    silk_text(lab, LEFT_X + 2.3, y, 0.5)
for i, lab in enumerate(J3_LABEL):
    if J3[i].startswith("NC"):
        continue
    y = PIN1_Y - i * 2.54
    silk_text(lab, RIGHT_X - 2.6, y, 0.5)

# One 1x6 column on the narrow tab, clear of the DevKit PCB edge (~28.2 mm).
# Bottom to top: electrode snaps, then the LiPo-boost 5 V input.
# The Teensy-era RX/TX header is omitted; DevKit USB is the serial port.
fpE = new_fp("J1", "Electrode_5V_1x6", (ELEC_X, 8.0))
ELEC = [
    ("IN1P", 1.40),
    ("IN1N", 3.94),
    ("SRB1", 6.48),
    ("BIASOUT", 9.02),
    ("GND", 11.56),
    ("AVDD", 14.10),
]
ELEC_SILK = ["1P", "1N", "SRB1", "BIAS", "GND", "5V"]
for i, (nm, y) in enumerate(ELEC):
    add_pth(fpE, i + 1, ELEC_X, y, 1.0, 1.7, nm)
    silk_text(ELEC_SILK[i], 27.35, y, 0.55)
courtyard(fpE, ELEC_X - 1.3, 0.4, ELEC_X + 1.3, 15.1)
ref_at(fpE, 26.2, 15.9)
fpE.Reference().SetText("J1")

# Passives. 0603 pads 0.7 x 0.75, centers 1.5 mm apart.
# 0805 centers 1.9 mm. Tantalum case C centers 3.6 mm, pads 1.5 x 2.0.


def add_two(ref, value, x, y, dx, dy, pad_w, pad_h, net_a, net_b, courtyard_extra=0.6):
    fp = new_fp(ref, value, (x, y))
    add_smd(fp, 1, x - dx / 2, y - dy / 2, pad_w, pad_h, net_a)
    add_smd(fp, 2, x + dx / 2, y + dy / 2, pad_w, pad_h, net_b)
    span = math.hypot(dx, dy)
    if abs(dx) >= abs(dy):
        courtyard(fp, x - span / 2 - courtyard_extra, y - 1.0, x + span / 2 + courtyard_extra, y + 1.0)
        silk_rect(fp, x - span / 2 - 0.3, y - 0.55, x + span / 2 + 0.3, y + 0.55)
    else:
        courtyard(fp, x - 1.0, y - span / 2 - courtyard_extra, x + 1.0, y + span / 2 + courtyard_extra)
        silk_rect(fp, x - 0.55, y - span / 2 - 0.3, x + 0.55, y + span / 2 + 0.3)
    ref_at(fp, x, y + span / 2 + 1.3)
    fp.Reference().SetText(ref)
    fp.Value().SetText(value)
    return fp


# Passives stay outside the 10 mm package (body is CHIP ± 5.0).
# Left corridor x≈5.2, right corridor x≈23.2, nothing under the body.

# Left column, vertical 0603. Lower pad is net_a.
add_two("C13", "10nF", 6.0, 10.2, 0, 1.55, 0.75, 0.7, "BIASOUT", "BIASINV")
add_two("R1", "1M", 6.0, 12.8, 0, 1.55, 0.75, 0.7, "BIASOUT", "BIASINV")
add_two("R2", "10k", 6.0, 16.6, 0, 1.55, 0.75, 0.7, "CLKSEL", "DVDD")
add_two("C4", "1uF", 6.0, 19.2, 0, 1.55, 0.75, 0.7, "DVDD", "GND")
# VCAP3 caps sit below the package, left of the other reservoir caps.
add_two("C11", "100nF", 5.6, 4.05, 1.55, 0, 0.7, 0.75, "VCAP3", "GND")
add_two("C10", "1uF", 8.5, 4.05, 1.55, 0, 0.7, 0.75, "VCAP3", "GND")
# DVDD bulk near the ESP32 3V3 pin, above the digital fanout.
add_two("C5", "100nF", 4.8, 29.85, 1.55, 0, 0.7, 0.75, "GND", "DVDD")
# GPIO pulldowns on their fanout lanes. Right pad is the GPIO net.
add_two("R6", "10k", 8.8, 23.20, 1.55, 0, 0.7, 0.75, "GND", "GPIO4")
add_two("R5", "10k", 8.8, 24.20, 1.55, 0, 0.7, 0.75, "GND", "GPIO3")
add_two("R4", "10k", 8.8, 25.20, 1.55, 0, 0.7, 0.75, "GND", "GPIO2")
add_two("R3", "10k", 8.8, 26.20, 1.55, 0, 0.7, 0.75, "GND", "GPIO1")

add_two("C1", "10uF", 22.7, 18.2, 1.9, 0, 1.0, 1.15, "AVDD", "GND", 0.8)
add_two("C2", "100nF", 22.7, 15.8, 1.55, 0, 0.7, 0.75, "AVDD", "GND")
add_two("C3", "1uF", 22.7, 13.4, 1.55, 0, 0.7, 0.75, "AVDD1", "GND")
add_two("C6", "10uF", 22.7, 10.6, 1.9, 0, 1.0, 1.15, "VREFP", "GND", 0.8)
add_two("C7", "100nF", 22.7, 8.0, 1.55, 0, 0.7, 0.75, "VREFP", "GND")
# Below the input pins, clear of the package body.
add_two("C9", "1uF", 11.6, 4.05, 1.55, 0, 0.7, 0.75, "VCAP2", "GND")
add_two("C12", "1uF", 16.4, 4.05, 1.55, 0, 0.7, 0.75, "VCAP4", "GND")
add_two("C8", "100uF", 21.2, 4.05, 3.2, 0, 1.4, 1.6, "VCAP1", "GND", 1.2)

# ---------- tracks, vias, router ----------
TRACKS = []  # (x1,y1,x2,y2,width,net) mm
VIAS = []  # (x,y,net)


def add_track(x1, y1, x2, y2, netname, width=0.2, layer="F.Cu"):
    if math.hypot(x2 - x1, y2 - y1) < 0.01:
        return
    t = pcbnew.PCB_TRACK(board)
    t.SetStart(xy(x1, y1))
    t.SetEnd(xy(x2, y2))
    t.SetWidth(mm(width))
    t.SetLayer(pcbnew.F_Cu if layer == "F.Cu" else pcbnew.B_Cu)
    t.SetNet(net(netname))
    board.Add(t)
    TRACKS.append((x1, y1, x2, y2, width, netname, layer))


def add_via(x, y, netname, diam=0.55, drill=0.30):
    v = pcbnew.PCB_VIA(board)
    v.SetPosition(xy(x, y))
    v.SetWidth(mm(diam))
    v.SetDrill(mm(drill))
    v.SetNet(net(netname))
    v.SetViaType(pcbnew.VIATYPE_THROUGH)
    board.Add(v)
    VIAS.append((x, y, diam, netname, drill))


# Grid router. 0.15 mm. Block foreign pads and the chip body.
GS = 0.15
OX, OY = -1.0, -1.0
GW = int((TAB_RIGHT + 2) / GS) + 4
GH = int((BOARD_TOP + 2) / GS) + 4


def gcell(x, y):
    return int(round((x - OX) / GS)), int(round((y - OY) / GS))


def gxy(i, j):
    return OX + i * GS, OY + j * GS


blocked = [[False] * GH for _ in range(GW)]


def block_disk(x, y, r):
    i0, j0 = gcell(x - r, y - r)
    i1, j1 = gcell(x + r, y + r)
    for i in range(max(0, i0), min(GW, i1 + 1)):
        for j in range(max(0, j0), min(GH, j1 + 1)):
            gx, gy = gxy(i, j)
            if (gx - x) ** 2 + (gy - y) ** 2 <= r * r:
                blocked[i][j] = True


def block_rect(x, y, w, h, extra):
    # sample the rectangle expanded by extra
    block_disk(x, y, 0)  # keep defined
    x0, x1 = x - w / 2 - extra, x + w / 2 + extra
    y0, y1 = y - h / 2 - extra, y + h / 2 + extra
    i0, j0 = gcell(x0, y0)
    i1, j1 = gcell(x1, y1)
    for i in range(max(0, i0), min(GW, i1 + 1)):
        for j in range(max(0, j0), min(GH, j1 + 1)):
            blocked[i][j] = True


# Chip body: no copper tracks under the package.
for x in (CHIP[0] - 5.15, CHIP[0] + 5.15):
    pass
i0, j0 = gcell(CHIP[0] - 5.15, CHIP[1] - 5.15)
i1, j1 = gcell(CHIP[0] + 5.15, CHIP[1] + 5.15)
for i in range(i0, i1 + 1):
    for j in range(j0, j1 + 1):
        if 0 <= i < GW and 0 <= j < GH:
            blocked[i][j] = True

# Pads block, expanded by clearance + half track. Keep the expansion
# smaller than half the 0.5 mm TQFP gap so a radial escape still exists.
for p in PADS:
    extra = 0.16
    if p["round"]:
        block_disk(p["x"], p["y"], p["w"] / 2 + extra)
    else:
        block_rect(p["x"], p["y"], p["w"], p["h"], extra)

# Board exterior (rough): block outside the outline with a margin so tracks
# stay 0.35 mm inside.
def inside_board(x, y, m=0.25):
    """True when (x, y) is at least m mm inside the outline."""
    if y > BOARD_TOP - m or y < m or x < m:
        return False
    if y > TAB_TOP - m:
        return x <= MAIN_RIGHT - m
    if y >= MAIN_BOTTOM + m:
        return x <= TAB_RIGHT - m
    # Below the main bottom edge only the side tab exists.
    return (MAIN_RIGHT + m) <= x <= (TAB_RIGHT - m)


for i in range(GW):
    for j in range(GH):
        gx, gy = gxy(i, j)
        if not inside_board(gx, gy):
            blocked[i][j] = True


def cell_open(i, j):
    if not (0 <= i < GW and 0 <= j < GH):
        return False
    if blocked[i][j]:
        return False
    x, y = gxy(i, j)
    return inside_board(x, y, 0.35)


def nearest_free(x, y, prefer):
    """Step away from a pad until the track is clear of the pad halo.

    The first free cell is often a one-cell sliver beside a pad. Require a
    short run of open cells so the escape can actually leave.
    """
    dirs = [prefer, (prefer[0], 0), (0, prefer[1]), (1, 0), (-1, 0), (0, 1), (0, -1)]
    for dx, dy in dirs:
        norm = math.hypot(dx, dy) or 1
        px, py = dx / norm, dy / norm
        run = []
        for step_i in range(2, 40):
            qx, qy = x + px * (step_i * 0.15), y + py * (step_i * 0.15)
            i, j = gcell(qx, qy)
            if cell_open(i, j):
                run.append((qx, qy))
                if len(run) >= 4 and step_i * 0.15 >= 0.9:
                    return run[-1]
            else:
                run.clear()
    return None


def astar(start, goal, analog_ok):
    si, sj = gcell(*start)
    gi, gj = gcell(*goal)
    if not (0 <= si < GW and 0 <= sj < GH and 0 <= gi < GW and 0 <= gj < GH):
        return None
    # unblock the endpoints
    blocked[si][sj] = False
    blocked[gi][gj] = False

    def cost_cell(i, j):
        x, y = gxy(i, j)
        c = 1.0
        x0, y0, x1, y1 = ANALOG_BOX
        if (not analog_ok) and (x0 <= x <= x1 and y0 <= y <= y1):
            c += 8.0
        return c

    opens = [(0.0, 0.0, si, sj)]
    came = {}
    gscore = {(si, sj): 0.0}
    seen = set()
    while opens:
        _, g, i, j = heapq.heappop(opens)
        if (i, j) in seen:
            continue
        seen.add((i, j))
        if (i, j) == (gi, gj):
            path = [(i, j)]
            while path[-1] in came:
                path.append(came[path[-1]])
            path.reverse()
            return [gxy(a, b) for a, b in path]
        for di in (-1, 0, 1):
            for dj in (-1, 0, 1):
                if di == 0 and dj == 0:
                    continue
                ni, nj = i + di, j + dj
                if not (0 <= ni < GW and 0 <= nj < GH):
                    continue
                if blocked[ni][nj] and (ni, nj) != (gi, gj):
                    continue
                step = 1.4142 if di and dj else 1.0
                ng = g + cost_cell(ni, nj) * step
                if ng < gscore.get((ni, nj), 1e18):
                    gscore[(ni, nj)] = ng
                    came[(ni, nj)] = (i, j)
                    h = abs(ni - gi) + abs(nj - gj)
                    heapq.heappush(opens, (ng + h, ng, ni, nj))
    nbr = []
    for di in (-1, 0, 1):
        for dj in (-1, 0, 1):
            if di == 0 and dj == 0:
                continue
            ni, nj = si + di, sj + dj
            nbr.append("." if cell_open(ni, nj) else "#")
    print("  astar fail", "".join(nbr), "visited", len(seen), "start", gxy(si, sj))
    return None


def simplify(path):
    if not path or len(path) < 3:
        return path
    out = [path[0]]
    for k in range(1, len(path) - 1):
        x0, y0 = out[-1]
        x1, y1 = path[k]
        x2, y2 = path[k + 1]
        # keep if direction changes
        if abs((x1 - x0) * (y2 - y1) - (y1 - y0) * (x2 - x1)) > 1e-6:
            out.append(path[k])
    out.append(path[-1])
    return out


def paint_path(path, width=0.20):
    extra = width / 2 + 0.12
    for x, y in path:
        block_disk(x, y, extra)


ANALOG_NETS = {
    "IN1P", "IN1N", "IN2P", "IN2N", "IN3P", "IN3N", "IN4P", "IN4N",
    "IN5P", "IN5N", "IN6P", "IN6N", "SRB1", "SRB2", "BIASOUT", "BIASINV",
    "BIASIN", "BIASREF", "VREFP", "VCAP1", "VCAP2", "VCAP3", "VCAP4",
    "AVDD", "AVDD1",
}


BODY = (9.05, 10.05, 19.40, 20.45)


def _hits_body(x1, y1, x2, y2):
    bx0, by0, bx1, by1 = BODY
    if abs(y1 - y2) <= 1e-6:
        y = y1
        if by0 - 0.05 <= y <= by1 + 0.05:
            xa, xb = sorted((x1, x2))
            return not (xb < bx0 or xa > bx1)
        return False
    if abs(x1 - x2) <= 1e-6:
        x = x1
        if bx0 - 0.05 <= x <= bx1 + 0.05:
            ya, yb = sorted((y1, y2))
            return not (yb < by0 or ya > by1)
        return False
    return True


LAST = [""]


CLR = 0.10


def _gap_seg_rect(x1, y1, x2, y2, cx, cy, w, h):
    """Distance from an axis-aligned segment to an axis-aligned rectangle."""
    left, right = cx - w / 2.0, cx + w / 2.0
    bot, top = cy - h / 2.0, cy + h / 2.0
    if abs(y1 - y2) <= 1e-9:
        y = y1
        xa, xb = (x1, x2) if x1 <= x2 else (x2, x1)
        dy = 0.0 if bot <= y <= top else (y - top if y > top else bot - y)
        dx = 0.0 if not (xb < left or xa > right) else (left - xb if xb < left else xa - right)
    else:
        x = x1
        ya, yb = (y1, y2) if y1 <= y2 else (y2, y1)
        dx = 0.0 if left <= x <= right else (x - right if x > right else left - x)
        dy = 0.0 if not (yb < bot or ya > top) else (bot - yb if yb < bot else ya - top)
    return math.hypot(dx, dy)


def _gap_segs(x1, y1, x2, y2, a, b, c, d):
    """Distance between two axis-aligned segments. 0 if they cross or overlap."""
    h1 = abs(y1 - y2) <= 1e-9
    h2 = abs(b - d) <= 1e-9
    if h1 and h2:
        ya, yb = y1, b
        xa0, xa1 = sorted((x1, x2))
        xb0, xb1 = sorted((a, c))
        if xa1 < xb0:
            dx = xb0 - xa1
        elif xb1 < xa0:
            dx = xa0 - xb1
        else:
            dx = 0.0
        return math.hypot(dx, abs(ya - yb))
    if (not h1) and (not h2):
        xa, xb = x1, a
        ya0, ya1 = sorted((y1, y2))
        yb0, yb1 = sorted((b, d))
        if ya1 < yb0:
            dy = yb0 - ya1
        elif yb1 < ya0:
            dy = ya0 - yb1
        else:
            dy = 0.0
        return math.hypot(abs(xa - xb), dy)
    if h1:
        hx0, hx1, hy = min(x1, x2), max(x1, x2), y1
        vx, vy0, vy1 = a, min(b, d), max(b, d)
    else:
        hx0, hx1, hy = min(a, c), max(a, c), b
        vx, vy0, vy1 = x1, min(y1, y2), max(y1, y2)
    if hx0 <= vx <= hx1 and vy0 <= hy <= vy1:
        return 0.0
    if hx0 <= vx <= hx1:
        dy = vy0 - hy if hy < vy0 else hy - vy1
        return abs(dy)
    if vy0 <= hy <= vy1:
        return vx - hx1 if vx > hx1 else hx0 - vx
    dx = hx0 - vx if vx < hx0 else vx - hx1
    dy = vy0 - hy if hy < vy0 else hy - vy1
    return math.hypot(dx, dy)


def _seg_clear(x1, y1, x2, y2, netname, width=0.20, layer="F.Cu"):
    """True when an axis-aligned segment keeps CLR to other copper."""
    if abs(x1 - x2) > 1e-6 and abs(y1 - y2) > 1e-6:
        LAST[0] = "diag"
        return False
    half = width / 2.0
    inset = 0.25 + half
    for x, y in ((x1, y1), (x2, y2)):
        if not inside_board(x, y, inset):
            LAST[0] = "edge %s %s" % (round(x, 2), round(y, 2))
            return False
    # Package body is a keep-out on both copper layers.
    if _gap_seg_rect(x1, y1, x2, y2, CHIP[0], CHIP[1], 10.15, 10.15) < half:
        LAST[0] = "body"
        return False
    for p in PADS:
        if p["net"] == netname:
            continue
        # SMD pads are on the top copper only. Bottom tracks clear them.
        if layer == "B.Cu" and not p["round"]:
            continue
        need = half + CLR
        gap = _gap_seg_rect(x1, y1, x2, y2, p["x"], p["y"], p["w"], p["h"])
        if gap < need:
            LAST[0] = "pad %s %.3f" % (p["net"], gap)
            return False
    for a, b, c, d, w, n, *rest in TRACKS:
        if n == netname:
            continue
        other = rest[0] if rest else "F.Cu"
        if other != layer:
            continue
        need = half + w / 2.0 + CLR
        gap = _gap_segs(x1, y1, x2, y2, a, b, c, d)
        if gap < need - 1e-6:
            LAST[0] = "trk %s %.3f" % (n, gap)
            return False
    for vx, vy, vd, vn, *_r in VIAS:
        if vn == netname:
            continue
        # Gap is already measured to the via pad edge.
        need = half + CLR
        gap = _gap_seg_rect(x1, y1, x2, y2, vx, vy, vd, vd)
        if gap < need:
            LAST[0] = "via %s %.3f" % (vn, gap)
            return False
    return True


def _path_ok(pts, netname):
    for a, b in zip(pts, pts[1:]):
        if not _seg_clear(a[0], a[1], b[0], b[1], netname):
            return False
    return True


def _dedup(pts):
    out = []
    for p in pts:
        if not out or abs(p[0] - out[-1][0]) > 0.02 or abs(p[1] - out[-1][1]) > 0.02:
            out.append(p)
    return out


def connect_pts(a, b, netname):
    """Orthogonal path around the package, using the open channels."""
    xs = (3.55, 7.35, 21.15, 25.45, 29.2)
    ys = (4.35, 28.7)
    cands = [
        [a, (b[0], a[1]), b],
        [a, (a[0], b[1]), b],
    ]
    for x in xs:
        for y in ys:
            cands.append([a, (x, a[1]), (x, y), (b[0], y), b])
            cands.append([a, (a[0], y), (b[0], y), b])
            cands.append([a, (x, a[1]), (x, b[1]), b])
    for c in cands:
        simp = _dedup(c)
        if len(simp) < 2:
            continue
        if _path_ok(simp, netname):
            return simp
    return None


def escape_of(x, y):
    """Step off an ADS1299 pad along its radial axis before turning."""
    dx, dy = x - CHIP[0], y - CHIP[1]
    radius = math.hypot(dx, dy)
    if not (5.0 < radius < 7.2):
        return (x, y)
    if abs(dx) >= abs(dy):
        return (x + (1.35 if dx > 0 else -1.35), y)
    return (x, y + (1.35 if dy > 0 else -1.35))


def route_net(netname, terminals, prefers):
    if len(terminals) < 2:
        return True
    escaped = []
    for x, y in terminals:
        e = escape_of(x, y)
        if abs(e[0] - x) > 0.01 or abs(e[1] - y) > 0.01:
            add_track(x, y, e[0], e[1], netname, 0.20)
        escaped.append(e)
    linked = [escaped[0]]
    ok = True
    for term in escaped[1:]:
        best = None
        best_len = 1e9
        for prev in linked:
            path = connect_pts(prev, term, netname)
            if path is None:
                continue
            length = sum(math.hypot(p[0] - q[0], p[1] - q[1]) for p, q in zip(path, path[1:]))
            if length < best_len:
                best, best_len = path, length
        if best is None:
            print("NO PATH", netname, term, LAST[0])
            ok = False
            continue
        for p, q in zip(best, best[1:]):
            add_track(p[0], p[1], q[0], q[1], netname, 0.20)
        linked.append(term)
    return ok


def outward_from_chip(x, y):
    return (x - CHIP[0], y - CHIP[1])


# Group SMD pads by net (chip + passives). PTH terminals included too.
from collections import defaultdict
by_net = defaultdict(list)
for p in PADS:
    by_net[p["net"]].append(p)

# Ground pins share a via when they are 0.5 mm apart (drill-to-drill rule).
# The via sits outside the package body. Nothing is routed under the body.
def gnd_via(x, y):
    add_via(x, y, "GND", 0.55, 0.30)


_LAST_NET = [""]


def wire(netname, pts, width=0.20, layer="F.Cu"):
    _LAST_NET[0] = netname
    for a, b in zip(pts, pts[1:]):
        if not _seg_clear(a[0], a[1], b[0], b[1], netname, width, layer):
            print("SKIP", netname, LAST[0], (round(a[0], 2), round(a[1], 2)), (round(b[0], 2), round(b[1], 2)), layer)
            continue
        add_track(a[0], a[1], b[0], b[1], netname, width, layer)


def svia(x, y, netname=None):
    """Signal via, 0.46 mm pad / 0.25 mm drill, so it clears a 0.50 mm fanout."""
    add_via(x, y, netname or _LAST_NET[0], 0.46, 0.25)


BUS = 28.80  # vertical in the tab, left of the 1x6 column
# Header Y for the socketed DevKit pins (pin 1 at PIN1_Y).
def hy(index):
    return PIN1_Y - index * 2.54


# --- ground ties (outside the package) ---
wire("GND", [(8.65, 18.95), (7.45, 18.95)])
svia(7.45, 18.95)
wire("GND", [(8.65, 17.95), (7.45, 17.95)])
svia(7.45, 17.95)
wire("GND", [(8.65, 16.95), (7.45, 16.95)])
svia(7.45, 16.95)
wire("GND", [(8.65, 14.95), (7.45, 14.95), (7.45, 14.45)])
svia(7.45, 14.45)
wire("GND", [(19.75, 18.95), (20.50, 18.95), (20.50, 18.45), (19.75, 18.45)])
svia(20.50, 18.70)
wire("GND", [(19.75, 15.45), (20.50, 15.45)])
svia(20.50, 15.45)
wire("GND", [(19.75, 14.45), (20.50, 14.45)])
svia(20.50, 14.45)
wire("GND", [(19.75, 12.95), (20.50, 12.95)])
svia(20.50, 12.95)
# Top-row ground pins. Via is above the pad, between the 0.15 mm fanout tracks.
for gx in (13.95, 15.95, 17.95):
    wire("GND", [(gx, 20.75), (gx, 21.55)], width=0.15)
    svia(gx, 21.55)
# Stitch the pour around the package, off the escape lanes.
for vx, vy in ((8.3, 9.7), (8.3, 20.55), (20.1, 9.7), (23.6, 20.6)):
    gnd_via(vx, vy)

# --- digital fanout ---
# Left-header nets ride the bottom copper north of the package, then drop
# in the corridor beside J2. Higher lane, further-left drop, so the bottom
# verticals never cross a lower lane. Header stubs are on the top.
# DRDY is the lowest lane and the rightmost drop.
wire("DRDY", [(10.95, 20.75), (10.95, 22.70)], width=0.15)
svia(10.95, 22.70)
wire("DRDY", [(10.95, 22.70), (4.90, 22.70), (4.90, hy(10))], layer="B.Cu")
svia(4.90, hy(10))
wire("DRDY", [(4.90, hy(10)), (LEFT_X, hy(10))])

wire("START", [(15.45, 20.75), (15.45, 26.95)], width=0.15)
svia(15.45, 26.95)
wire("START", [(15.45, 26.95), (4.30, 26.95), (4.30, hy(8))], layer="B.Cu")
svia(4.30, hy(8))
wire("START", [(4.30, hy(8)), (LEFT_X, hy(8))])

wire("RESET", [(16.45, 20.75), (16.45, 27.50)], width=0.15)
svia(16.45, 27.50)
wire("RESET", [(16.45, 27.50), (3.70, 27.50), (3.70, hy(9))], layer="B.Cu")
svia(3.70, hy(9))
wire("RESET", [(3.70, hy(9)), (LEFT_X, hy(9))])

wire("PWDN", [(16.95, 20.75), (16.95, 28.05)], width=0.15)
svia(16.95, 28.05)
wire("PWDN", [(16.95, 28.05), (3.10, 28.05), (3.10, hy(6))], layer="B.Cu")
svia(3.10, hy(6))
wire("PWDN", [(3.10, hy(6)), (LEFT_X, hy(6))])

# DOUT / SCLK / CS jump the bottom north of the chip and drop beside J3.
# Lowest header (CS) has the rightmost drop.
wire("DOUT", [(12.95, 20.75), (12.95, 23.50)], width=0.15)
svia(12.95, 23.50)
wire("DOUT", [(12.95, 23.50), (24.60, 23.50), (24.60, hy(7))], layer="B.Cu")
svia(24.60, hy(7))
wire("DOUT", [(24.60, hy(7)), (RIGHT_X, hy(7))])

wire("SCLK", [(14.45, 20.75), (14.45, 24.40)], width=0.15)
svia(14.45, 24.40)
wire("SCLK", [(14.45, 24.40), (25.15, 24.40), (25.15, hy(8))], layer="B.Cu")
svia(25.15, hy(8))
wire("SCLK", [(25.15, hy(8)), (RIGHT_X, hy(8))])

wire("CS", [(14.95, 20.75), (14.95, 25.30)], width=0.15)
svia(14.95, 25.30)
wire("CS", [(14.95, 25.30), (25.70, 25.30), (25.70, hy(9))], layer="B.Cu")
svia(25.70, hy(9))
wire("CS", [(25.70, hy(9)), (RIGHT_X, hy(9))])

wire("DIN", [(17.45, 20.75), (17.45, 28.65), (25.10, 28.65), (25.10, hy(1)), (RIGHT_X, hy(1))], width=0.15)

# GPIO pulldowns. Right pad of each resistor is the GPIO net (x=9.575).
wire("GPIO4", [(11.45, 20.75), (11.45, 23.20), (9.575, 23.20)], width=0.15)
wire("GPIO3", [(11.95, 20.75), (11.95, 24.20), (9.575, 24.20)], width=0.15)
wire("GPIO2", [(12.45, 20.75), (12.45, 25.20), (9.575, 25.20)], width=0.15)
wire("GPIO1", [(13.45, 20.75), (13.45, 26.20), (9.575, 26.20)], width=0.15)

# DVDD: both package pins, C4, C5, and J2 3V3. Not the DevKit 5V pin.
wire("DVDD", [(10.45, 20.75), (10.45, 22.05), (6.85, 22.05), (6.85, 18.425), (6.00, 18.425)], width=0.20)
wire("DVDD", [(8.65, 18.45), (6.85, 18.45)])
wire("DVDD", [(LEFT_X, hy(0)), (5.575, hy(0)), (5.575, 29.85)])
wire("DVDD", [(5.575, 29.85), (5.575, 22.05), (6.85, 22.05)])

# --- bias and CLKSEL, left of the package ---
wire("CLKSEL", [(8.65, 17.45), (6.70, 17.45), (6.70, 15.825), (6.00, 15.825)], width=0.15)
wire("BIASINV", [(8.65, 12.95), (7.40, 12.95), (7.40, 13.575), (6.00, 13.575)])
wire("BIASINV", [(6.00, 10.975), (7.40, 10.975)])
wire("BIASOUT", [(8.65, 11.95), (7.15, 11.95), (7.15, 12.025), (6.00, 12.025)], width=0.15)
wire("BIASOUT", [(6.00, 9.425), (7.15, 9.425)], width=0.15)
wire("BIASOUT", [(7.15, 9.425), (7.15, 9.11), (16.40, 9.11)], width=0.15)
svia(16.40, 9.11)
wire("BIASOUT", [(16.40, 9.11), (28.50, 9.11), (28.50, 9.02)], layer="B.Cu", width=0.15)
svia(28.50, 9.02)
wire("BIASOUT", [(28.50, 9.02), (ELEC_X, 9.02)], width=0.15)

# VCAP3 drops on the bottom, left of the package, to the caps under the inputs.
wire("VCAP3", [(8.65, 15.95), (7.20, 15.95)])
svia(7.20, 15.95)
wire("VCAP3", [(7.20, 15.95), (7.20, 4.70)], layer="B.Cu")
svia(7.20, 4.70)
wire("VCAP3", [(7.20, 4.70), (7.725, 4.70), (7.725, 4.05)])
wire("VCAP3", [(7.20, 4.70), (4.825, 4.70), (4.825, 4.05)])

# AVDD1 crosses above the digital fanout on the bottom, then drops left of C3.
wire("AVDD1", [(8.65, 16.45), (7.90, 16.45), (7.90, 15.10), (2.75, 15.10)], width=0.15)
svia(2.75, 15.10)
wire("AVDD1", [(2.75, 15.10), (2.75, 29.15), (15.20, 29.15)], layer="B.Cu", width=0.15)
svia(15.20, 29.15)
wire("AVDD1", [(15.20, 29.15), (15.20, 16.60)], width=0.15)
svia(15.20, 16.60)
wire("AVDD1", [(15.20, 16.60), (15.20, 12.20), (21.925, 12.20), (21.925, 13.40)], width=0.15)

# Right-hand AVDD pins to C1, C2, and the 5 V tab pin.
wire("AVDD", [(19.75, 13.95), (20.40, 13.95), (20.40, 14.20), (21.70, 14.20), (21.70, 18.20), (21.75, 18.20)], width=0.15)
wire("AVDD", [(19.75, 13.45), (20.40, 13.45)])
wire("AVDD", [(19.75, 12.45), (20.40, 12.45), (20.40, 14.05), (ELEC_X, 14.05), (ELEC_X, 14.10)], width=0.15)
wire("AVDD", [(21.70, 15.80), (21.925, 15.80)], width=0.15)
# Left AVDD pins and the IN7/IN8 ties (ADS1299-6 has no channels 7-8).
wire("AVDD", [(8.65, 15.45), (8.05, 15.45), (8.05, 8.40)], width=0.15)
wire("AVDD", [(8.65, 13.95), (8.05, 13.95)], width=0.15)
wire("AVDD", [(10.45, 9.65), (10.45, 8.40)], width=0.15)
wire("AVDD", [(10.95, 9.65), (10.95, 8.40)], width=0.15)
wire("AVDD", [(11.45, 9.65), (11.45, 8.40)], width=0.15)
wire("AVDD", [(11.95, 9.65), (11.95, 8.40), (8.05, 8.40)], width=0.15)
wire("AVDD", [(8.05, 8.40), (5.25, 8.40)], width=0.15)
svia(5.25, 8.40)
wire("AVDD", [(5.25, 8.40), (5.25, 22.15), (24.20, 22.15)], layer="B.Cu", width=0.15)
svia(24.20, 22.15)
wire("AVDD", [(24.20, 22.15), (24.20, 14.05), (21.70, 14.05)], width=0.15)

# VREFP to C6 and C7, between the ground vias and the bypass caps.
wire("VREFP", [(19.75, 14.95), (22.55, 14.95), (22.55, 11.40), (21.75, 11.40), (21.75, 10.60)], width=0.15)
wire("VREFP", [(22.55, 8.00), (21.925, 8.00)], width=0.15)

# VCAP1/2/4 use the bottom under the bypass caps (outside the package body).
wire("VCAP1", [(19.75, 16.95), (22.40, 16.95)], width=0.15)
svia(22.40, 16.95)
wire("VCAP1", [(22.40, 16.95), (22.40, 4.55)], layer="B.Cu", width=0.15)
svia(22.40, 4.55)
wire("VCAP1", [(22.40, 4.55), (19.60, 4.55), (19.60, 4.05)], width=0.15)

wire("VCAP4", [(19.75, 15.95), (20.35, 15.95), (20.35, 16.40), (23.05, 16.40)], width=0.15)
svia(23.05, 16.40)
wire("VCAP4", [(23.05, 16.40), (23.05, 5.20)], layer="B.Cu", width=0.15)
svia(23.05, 5.20)
wire("VCAP4", [(23.05, 5.20), (15.625, 5.20), (15.625, 4.05)], width=0.15)

wire("VCAP2", [(19.75, 17.95), (20.35, 17.95), (20.35, 17.20), (23.70, 17.20)], width=0.15)
svia(23.70, 17.20)
wire("VCAP2", [(23.70, 17.20), (23.70, 5.70)], layer="B.Cu", width=0.15)
svia(23.70, 5.70)
wire("VCAP2", [(23.70, 5.70), (10.825, 5.70), (10.825, 4.05)], width=0.15)

# Electrodes cross the J3 row in the pad gaps. IN1P and SRB1 drop to the
# tab pins on the bottom so they do not cut the top-side highways.
wire("IN1N", [(17.45, 9.65), (17.45, 6.57), (28.15, 6.57), (28.15, 3.94), (ELEC_X, 3.94)])
wire("IN1P", [(17.95, 9.65), (17.95, 6.20), (16.80, 6.20), (16.80, 6.57)])
svia(16.80, 6.57)
wire("IN1P", [(16.80, 6.57), (28.90, 6.57), (28.90, 1.40)], layer="B.Cu")
svia(28.90, 1.40)
wire("IN1P", [(28.90, 1.40), (ELEC_X, 1.40)])
wire("SRB1", [(19.75, 11.45), (20.50, 11.45), (20.50, 11.60), (28.55, 11.60)])
svia(28.55, 11.60)
wire("SRB1", [(28.55, 11.60), (28.55, 6.48)], layer="B.Cu")
svia(28.55, 6.48)
wire("SRB1", [(28.55, 6.48), (ELEC_X, 6.48)])


def _hole_ok(x, y, drill=0.25):
    if not inside_board(x, y, 0.6):
        return False
    for vx, vy, _vd, _vn, vdrill in VIAS:
        if math.hypot(vx - x, vy - y) < drill / 2 + vdrill / 2 + 0.50:
            return False
    for p in PADS:
        if not p["round"]:
            continue
        if math.hypot(p["x"] - x, p["y"] - y) < drill / 2 + 0.50 + 0.50:
            return False
    return True


# Every SMD ground pad needs its own via into the bottom plane. PTH pads
# already connect. Skip a via that would violate the hole-to-hole rule.
# SMD ground pads are tied with an in-pad via only when that hole is clear
# of every other hole. Pads under a bottom-side VCAP run are left for a
# manual via so the via does not short that run.
for p in list(PADS):
    if p["net"] != "GND" or p["round"]:
        continue
    blocked = False
    for x1, y1, x2, y2, width, netname, *rest in TRACKS:
        if netname == "GND":
            continue
        if rest and rest[0] != "B.Cu":
            continue
        if _gap_seg_rect(x1, y1, x2, y2, p["x"], p["y"], 0.50, 0.50) < 0.20:
            blocked = True
            break
    if blocked or not _hole_ok(p["x"], p["y"]):
        print("NO GND VIA", round(p["x"], 2), round(p["y"], 2))
        continue
    add_via(p["x"], p["y"], "GND", 0.46, 0.25)

ds = board.GetDesignSettings()
ds.m_MinClearance = mm(0.15)
ds.m_TrackMinWidth = mm(0.15)
ds.m_CopperEdgeClearance = mm(0.25)
ds.m_SilkClearance = mm(0.05)
ds.m_SolderMaskMinWidth = mm(0.0)
ds.m_SolderMaskExpansion = mm(0.04)
try:
    nc = ds.m_NetSettings.m_DefaultNetClass
    nc.SetClearance(mm(0.15))
    nc.SetTrackWidth(mm(0.20))
    nc.SetViaDiameter(mm(0.55))
    nc.SetViaDrill(mm(0.30))
except Exception as exc:
    print("netclass", exc)
print("manual wires", len(TRACKS))


def snap_track_ends():
    """Pull track ends onto pad centers when rounding left a 1 nm gap."""
    pads = []
    for fp in board.GetFootprints():
        for p in fp.Pads():
            pads.append(p.GetPosition())
    for t in board.GetTracks():
        if t.GetClass() == "PCB_VIA":
            continue
        for getter, setter in ((t.GetStart, t.SetStart), (t.GetEnd, t.SetEnd)):
            pt = getter()
            best = None
            bestd = 20000
            for p in pads:
                d = abs(p.x - pt.x) + abs(p.y - pt.y)
                if d < bestd:
                    bestd = d
                    best = p
            if best is not None:
                setter(pcbnew.VECTOR2I(best.x, best.y))


snap_track_ends()
print("tracks", len(TRACKS), "vias", len(VIAS), "pads", len(PADS))

# Continuity of the routes that were actually placed (pad centers + track ends).
parent = {}


def find(a):
    parent.setdefault(a, a)
    while parent[a] != a:
        parent[a] = parent[parent[a]]
        a = parent[a]
    return a


def union(a, b):
    ra, rb = find(a), find(b)
    if ra != rb:
        parent[rb] = ra


def key_of(x, y):
    return (round(x, 2), round(y, 2))


for p in PADS:
    find(key_of(p["x"], p["y"]))
for x1, y1, x2, y2, _w, _n, *_r in TRACKS:
    union(key_of(x1, y1), key_of(x2, y2))
for vx, vy, _d, _n, *_r in VIAS:
    # A via ties the copper at its coordinate into one node for this report.
    find(key_of(vx, vy))
wanted = (
    "DRDY", "START", "RESET", "PWDN", "DIN", "DOUT", "SCLK", "CS", "DVDD",
    "AVDD", "AVDD1", "VREFP", "VCAP1", "VCAP2", "VCAP3", "VCAP4",
    "IN1P", "IN1N", "SRB1", "BIASOUT", "BIASINV", "CLKSEL", "GND",
)
print("continuity (placed copper, lower is better):")
for name in wanted:
    roots = set()
    n = 0
    for p in PADS:
        if p["net"] != name:
            continue
        n += 1
        roots.add(find(key_of(p["x"], p["y"])))
    print(" ", name, "pads", n, "pieces", len(roots))

# Bottom GND zone. pcbnew's ZONE_FILLER segfaults in this KiCad 7.0.11 build,
# so the fill is constructed directly: one polygon, deflated from the outline,
# with clearance holes only around non-ground through-holes. No bottom traces,
# no slots, and nothing is cut out under the ADS1299 body.
zone = pcbnew.ZONE(board)
zone.SetLayer(pcbnew.B_Cu)
zone.SetNet(net("GND"))
zone.SetFillMode(pcbnew.ZONE_FILL_MODE_POLYGONS)
zone.SetHatchStyle(pcbnew.ZONE_BORDER_DISPLAY_STYLE_NO_HATCH)
zone.SetBorderHatchPitch(mm(0.5))
zone.SetPadConnection(pcbnew.ZONE_CONNECTION_FULL)
zone.SetLocalClearance(mm(0.15))
zone.SetMinThickness(mm(0.20))
zone.SetThermalReliefGap(mm(0.25))
zone.SetThermalReliefSpokeWidth(mm(0.30))
zone.SetAssignedPriority(1)
outline = zone.Outline()
outline.NewOutline()
for x, y in OUTLINE:
    outline.Append(mm(x), mm(y))
board.Add(zone)

pour = pcbnew.SHAPE_POLY_SET()
pour.NewOutline()
for x, y in OUTLINE:
    pour.Append(mm(x), mm(y))
pour.Deflate(mm(0.30), pcbnew.CORNER_STRATEGY_CHAMFER_ALL_CORNERS, mm(0.05))

clearance = pcbnew.SHAPE_POLY_SET()
n_holes = 0
for p in PADS:
    if p["net"] == "GND" or not p["round"]:
        continue
    clearance.NewOutline()
    radius = p["w"] / 2 + 0.22
    for k in range(16):
        ang = math.radians((k + 0.5) * (360 / 16))
        clearance.Append(mm(p["x"] + radius * math.cos(ang)), mm(p["y"] + radius * math.sin(ang)))
    n_holes += 1
for rec in TRACKS:
    if len(rec) < 7 or rec[6] != "B.Cu":
        continue
    x1, y1, x2, y2, width, _n, _layer = rec
    clearance.NewOutline()
    # Axis-aligned keep-out around the bottom jumper.
    extra = width / 2 + 0.20
    xa, xb = sorted((x1, x2))
    ya, yb = sorted((y1, y2))
    for x, y in (
        (xa - extra, ya - extra),
        (xb + extra, ya - extra),
        (xb + extra, yb + extra),
        (xa - extra, yb + extra),
    ):
        clearance.Append(mm(x), mm(y))
    n_holes += 1
for vx, vy, vd, vn, *_r in VIAS:
    if vn == "GND":
        continue
    clearance.NewOutline()
    radius = vd / 2 + 0.20
    for k in range(12):
        ang = math.radians((k + 0.5) * 30)
        clearance.Append(mm(vx + radius * math.cos(ang)), mm(vy + radius * math.sin(ang)))
    n_holes += 1
if n_holes:
    pour.BooleanSubtract(clearance, pcbnew.SHAPE_POLY_SET.PM_FAST)
pour.Fracture(pcbnew.SHAPE_POLY_SET.PM_FAST)
zone.SetFilledPolysList(pcbnew.B_Cu, pour)
zone.SetIsFilled(True)
zone.SetNeedRefill(False)
print("pour outlines", pour.OutlineCount(), "pth holes", n_holes)

# Title silk
silk_text("ADS1299  ESP32", 14.0, 31.55, 0.7)
silk_text("J1  1P 1N SRB BIAS GND 5V", 16.5, 2.85, 0.55)

board.Save(OUT)
print("saved", OUT, os.path.getsize(OUT))
