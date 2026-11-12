"""
Autoroute the board with Freerouting, then add GND planes on both layers.

  python.exe route.py <path-to-freerouting.jar> <path-to-java.exe> [passes]
"""
import os
import subprocess
import sys

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import design as D  # noqa: E402

OUT = os.path.normpath(os.path.join(HERE, '..', D.PROJECT))
PCB = os.path.join(OUT, D.PROJECT + '.kicad_pcb')
mm = pcbnew.FromMM
OX, OY = 50.0, 50.0


def main():
    jar, java = sys.argv[1], sys.argv[2]
    passes = sys.argv[3] if len(sys.argv) > 3 else '40'
    board = pcbnew.LoadBoard(PCB)
    # remove old tracks / zones so the script can be re-run
    for t in list(board.GetTracks()):
        board.Remove(t)
    for z in list(board.Zones()):
        board.Remove(z)
    dsn = os.path.join(OUT, D.PROJECT + '.dsn')
    ses = os.path.join(OUT, D.PROJECT + '.ses')
    if os.path.exists(ses):
        os.remove(ses)
    if not pcbnew.ExportSpecctraDSN(board, dsn):
        raise RuntimeError('DSN export failed')
    cmd = [java, '-jar', jar, '-de', dsn, '-do', ses, '-mp', passes, '--gui.enabled=false']
    print(' '.join(cmd))
    subprocess.run(cmd, check=True, timeout=1800)
    if not os.path.exists(ses):
        raise RuntimeError('Freerouting produced no .ses file')
    if not pcbnew.ImportSpecctraSES(board, ses):
        raise RuntimeError('SES import failed')

    gnd = board.FindNet('GND')
    m = 0.4
    for layer in (pcbnew.F_Cu, pcbnew.B_Cu):
        z = pcbnew.ZONE(board)
        z.SetLayer(layer)
        z.SetNetCode(gnd.GetNetCode())
        o = z.Outline()
        o.NewOutline()
        for x, y in ((m, m), (D.BOARD_W - m, m), (D.BOARD_W - m, D.BOARD_H - m), (m, D.BOARD_H - m)):
            o.Append(mm(x + OX), mm(y + OY))
        z.SetLocalClearance(mm(0.3))
        z.SetMinThickness(mm(0.25))
        z.SetThermalReliefGap(mm(0.4))
        z.SetThermalReliefSpokeWidth(mm(0.5))
        z.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
        z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
        z.SetZoneName('GND_' + ('TOP' if layer == pcbnew.F_Cu else 'BOT'))
        board.Add(z)
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    board.Save(PCB)
    print('routed and saved', PCB)


if __name__ == '__main__':
    main()
