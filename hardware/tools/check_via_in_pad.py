"""List vias whose drill touches a component pad. Usage: python3 check_via_in_pad.py board.kicad_pcb
EXPOSED PAD rows (pads over 2 mm) are planned thermal vias; MOUNT TAB rows are connector tabs; PIN/PART PAD rows need fixing."""
import pcbnew, sys
from pcbnew import ToMM
b = pcbnew.LoadBoard(sys.argv[1])
vias = [t for t in b.GetTracks() if t.GetClass() == 'PCB_VIA']
seen=set()
for f in b.Footprints():
    for p in f.Pads():
        if p.GetAttribute() not in (pcbnew.PAD_ATTRIB_SMD, pcbnew.PAD_ATTRIB_CONN): continue
        for L in (pcbnew.F_Cu, pcbnew.B_Cu):
            if not p.IsOnLayer(L): continue
            for v in vias:
                if p.GetEffectiveShape(L).Collide(pcbnew.SHAPE_CIRCLE(v.GetPosition(), int(v.GetDrillValue()/2)), 0):
                    k=(f.GetReference(),p.GetNumber(),round(ToMM(v.GetPosition().x),2),round(ToMM(v.GetPosition().y),2))
                    if k in seen: continue
                    seen.add(k)
                    sx,sy=ToMM(p.GetSize().x),ToMM(p.GetSize().y)
                    kind='EXPOSED PAD (ok)' if min(sx,sy)>2 else ('MOUNT TAB' if p.GetNumber() in ('MP','') else 'PIN/PART PAD')
                    print(f"{kind:17s} {f.GetReference():4s} pad {p.GetNumber():3s} {p.GetNetname():8s} {sx:.2f}x{sy:.2f}  via d{ToMM(v.GetWidth()):.2f}/{ToMM(v.GetDrillValue()):.2f} at ({k[2]},{k[3]}) {'F' if L==pcbnew.F_Cu else 'B'}  {f.GetValue()}")
