"""
Generate the KiCad project (symbols, footprints, schematic, unrouted PCB)
from design.py. Run with KiCad's python:

  "%LOCALAPPDATA%\\Programs\\KiCad\\10.0\\bin\\python.exe" gen_project.py
"""
import copy
import json
import math
import os
import sys
import uuid

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from sexpr import parse, dump, find, find_all, Sym  # noqa: E402
import design as D  # noqa: E402

OUT = os.path.normpath(os.path.join(HERE, '..', D.PROJECT))
KICAD = os.path.join(os.environ['LOCALAPPDATA'], 'Programs', 'KiCad', '10.0', 'share', 'kicad')
SYMLIB = os.path.join(KICAD, 'symbols')
FPLIB = os.path.join(KICAD, 'footprints')
NS = uuid.UUID('6b1c3f0e-2d5a-4c1b-9e1f-0a5d9850f00d')
ROOT_UUID = str(uuid.uuid5(NS, 'root'))
FONT = [Sym('effects'), [Sym('font'), [Sym('size'), 1.27, 1.27]]]


def uid(*parts):
    return str(uuid.uuid5(NS, '/'.join(str(p) for p in parts)))


def S(x):
    return Sym(x)


# ============================================================ custom symbols
def make_symbol(name, ref, left, right, footprint, desc, half_w=10.16):
    n = max(len(left), len(right))
    top = math.ceil((n - 1) / 2.0) * 2.54
    body = [S('rectangle'), [S('start'), -half_w, top + 2.54], [S('end'), half_w, top - n * 2.54],
            [S('stroke'), [S('width'), 0.254], [S('type'), S('default')]], [S('fill'), [S('type'), S('background')]]]
    pins = []
    for side, lst in ((0, left), (1, right)):
        for i, p in enumerate(lst):
            if p is None:
                continue
            num, pname = p
            x = -(half_w + 2.54) if side == 0 else (half_w + 2.54)
            ang = 0 if side == 0 else 180
            pins.append([S('pin'), S('passive'), S('line'), [S('at'), round(x, 4), round(top - i * 2.54, 4), ang],
                         [S('length'), 2.54],
                         [S('name'), pname, FONT], [S('number'), str(num), FONT]])

    def prop(k, v, x, y, hide=False):
        e = [S('property'), k, v, [S('at'), x, y, 0]]
        if hide:
            e.append([S('hide'), S('yes')])
        e.append(FONT)
        return e

    return [S('symbol'), name,
            [S('pin_names'), [S('offset'), 1.016]],
            [S('exclude_from_sim'), S('no')], [S('in_bom'), S('yes')], [S('on_board'), S('yes')],
            prop('Reference', ref, 0, top + 5.08), prop('Value', name, 0, top + 3.81),
            prop('Footprint', footprint, 0, 0, True), prop('Datasheet', '', 0, 0, True),
            prop('Description', desc, 0, 0, True),
            [S('symbol'), name + '_0_1', body],
            [S('symbol'), name + '_1_1'] + pins,
            [S('embedded_fonts'), S('no')]]


def custom_symbols():
    bp_left = [(str(i + 1), n) for i, n in enumerate(D.BP_H1)]
    bp_right = [(str(i + 21), n) for i, n in enumerate(D.BP_H2)]
    ad_left = [(str(i), D.AD_NAMES[str(i)]) for i in range(1, 11)]
    ad_right = [(str(i), D.AD_NAMES[str(i)]) for i in range(11, 21)]
    return {
        'BluePill': make_symbol('BluePill', 'U', bp_left, bp_right, D.KLIB + ':BluePill_Socket',
                                'STM32F103C8 Blue Pill board, header 1 = pins 1-20, header 2 = pins 21-40'),
        'AD9850_Module': make_symbol('AD9850_Module', 'U', ad_left, ad_right, D.KLIB + ':AD9850_Module_Socket',
                                     'AD9850 DDS module HC-SR08, control row = 1-10, data row = 11-20'),
        'A0515S-2WR3': make_symbol('A0515S-2WR3', 'PS', [('1', 'Vin'), ('2', 'GND')],
                                   [('6', '+Vo'), ('5', '0V'), ('4', '-Vo')], D.KLIB + ':Mornsun_SIP6_A_S-2WR3',
                                   'Mornsun 2 W isolated DC-DC, 5 V in, +-15 V out', half_w=7.62),
    }


# ======================================================= library symbol load
_libcache = {}


def lib_symbol(lib_id, customs):
    nick, name = lib_id.split(':')
    if nick == D.KLIB:
        sym = copy.deepcopy(customs[name])
    else:
        if nick not in _libcache:
            _libcache[nick] = parse(open(os.path.join(SYMLIB, nick + '.kicad_sym'), encoding='utf-8').read())
        lib = _libcache[nick]
        syms = {s[1]: s for s in find_all(lib, 'symbol')}
        sym = copy.deepcopy(syms[name])
        ext = find(sym, 'extends')
        if ext:
            parent = copy.deepcopy(syms[ext[1]])
            child_props = {p[1]: p for p in find_all(sym, 'property')}
            new = [S('symbol'), name]
            for item in parent[2:]:
                if isinstance(item, list) and item and item[0] == 'property' and item[1] in child_props:
                    new.append(child_props.pop(item[1]))
                elif isinstance(item, list) and item and item[0] == 'symbol':
                    sub = copy.deepcopy(item)
                    sub[1] = name + sub[1][len(ext[1]):]
                    new.append(sub)
                else:
                    new.append(item)
            # extra child-only properties go after the others
            idx = max(i for i, it in enumerate(new) if isinstance(it, list) and it and it[0] == 'property') + 1
            for p in child_props.values():
                new.insert(idx, p)
                idx += 1
            sym = new
    full = copy.deepcopy(sym)
    full[1] = lib_id
    return sym, full


def symbol_pins(sym, unit):
    """List of (number, x, y, angle, type) for a unit (lib coordinates, y up)."""
    out = []
    name = sym[1]
    for sub in find_all(sym, 'symbol'):
        parts = sub[1][len(name) + 1:].split('_')
        u = int(parts[0])
        if u not in (0, unit):
            continue
        for pin in find_all(sub, 'pin'):
            at = find(pin, 'at')
            out.append((find(pin, 'number')[1], float(at[1]), float(at[2]), int(float(at[3])), str(pin[1])))
    return out


def symbol_units(sym):
    name = sym[1]
    units = set()
    for sub in find_all(sym, 'symbol'):
        u = int(sub[1][len(name) + 1:].split('_')[0])
        if u:
            units.add(u)
    return sorted(units) or [1]


# ================================================================ schematic
def r127(v):
    return round(round(v / 1.27) * 1.27, 4)


def build_schematic(customs):
    lib_syms = {}
    body = []
    pos_of = {}     # ref -> uuid (unit 1) for the PCB
    fields_of = {}  # ref -> Description / Datasheet copied to the footprint
    flag_n = 0
    for part in D.PARTS:
        sym, full = lib_symbol(part['lib'], customs)
        lib_syms[part['lib']] = full
        units = symbol_units(sym)
        sch = part['sch']
        if not isinstance(sch, dict):
            sch = {units[0]: sch}
        is_power = find(sym, 'power') is not None
        ref = part['ref']
        if is_power:
            flag_n += 1
            ref = '#FLG%02d' % flag_n
        sym_uuid = uid('sym', part['ref'])
        pos_of[part['ref']] = sym_uuid
        lp = {p[1]: p[2] for p in find_all(sym, 'property')}
        fields_of[part['ref']] = {'Description': lp.get('Description', ''), 'Datasheet': lp.get('Datasheet', ''),
                                  'pin_names': {}}
        for sub in find_all(sym, 'symbol'):
            for pin in find_all(sub, 'pin'):
                fields_of[part['ref']]['pin_names'][find(pin, 'number')[1]] = find(pin, 'name')[1]
        for unit in units:
            X, Y = sch[unit]
            X, Y = r127(X), r127(Y)
            u = sym_uuid if unit == units[0] else uid('sym', part['ref'], unit)
            if unit != units[0]:
                pass
            inst = [S('symbol'), [S('lib_id'), part['lib']], [S('at'), X, Y, 0], [S('unit'), unit],
                    [S('exclude_from_sim'), S('no')], [S('in_bom'), S('no') if is_power or part['ref'].startswith('H') else S('yes')],
                    [S('on_board'), S('no') if is_power or not part['fp'] else S('yes')],
                    [S('dnp'), S('no')], [S('uuid'), sym_uuid if unit == units[0] else sym_uuid]]
            # one uuid per unit is required: KiCad identifies units of the same part by reference
            inst[-1] = [S('uuid'), u]
            props = {p[1]: p for p in find_all(sym, 'property')}
            for key in ('Reference', 'Value', 'Footprint', 'Datasheet', 'Description'):
                lp = props.get(key)
                at = find(lp, 'at') if lp else None
                px, py, prot = (float(at[1]), float(at[2]), at[3] if len(at) > 3 else 0) if at else (0.0, 0.0, 0)
                val = {'Reference': ref, 'Value': part['value'], 'Footprint': part['fp'] or ''}.get(key)
                if val is None:
                    val = lp[2] if lp else ''
                e = [S('property'), key, val, [S('at'), round(X + px, 4), round(Y - py, 4), prot]]
                hidden = key in ('Footprint', 'Datasheet', 'Description') or (is_power and key == 'Reference')
                if hidden:
                    e.append([S('hide'), S('yes')])
                eff = find(lp, 'effects') if lp else None
                e.append(copy.deepcopy(eff) if eff else FONT)
                inst.append(e)
            pins = symbol_pins(sym, unit)
            for num, *_ in pins:
                inst.append([S('pin'), num, [S('uuid'), uid('pin', part['ref'], num)]])
            inst.append([S('instances'), [S('project'), D.PROJECT,
                                          [S('path'), '/' + ROOT_UUID, [S('reference'), ref], [S('unit'), unit]]]])
            body.append(inst)
            # labels / no-connects at each pin end
            for num, px, py, ang, ptype in pins:
                x, y = round(X + px, 4), round(Y - py, 4)
                net = part['pins'].get(num)
                if net:
                    la = (ang + 180) % 360
                    just = 'left' if la in (0, 90) else 'right'
                    body.append([S('global_label'), net, [S('shape'), S('passive')], [S('at'), x, y, la],
                                 [S('fields_autoplaced'), S('yes')],
                                 [S('effects'), [S('font'), [S('size'), 1.27, 1.27]], [S('justify'), S(just)]],
                                 [S('uuid'), uid('label', part['ref'], num)],
                                 [S('property'), 'Intersheetrefs', '${INTERSHEET_REFS}', [S('at'), x, y, 0],
                                  [S('hide'), S('yes')], [S('effects'), [S('font'), [S('size'), 1.27, 1.27]]]]])
                else:
                    body.append([S('no_connect'), [S('at'), x, y], [S('uuid'), uid('nc', part['ref'], num)]])
    for (x, y), txt in D.SCH_NOTES:
        body.append([S('text'), txt, [S('exclude_from_sim'), S('no')], [S('at'), x, y, 0],
                     [S('effects'), [S('font'), [S('size'), 1.524, 1.524]], [S('justify'), S('left'), S('bottom')]],
                     [S('uuid'), uid('note', txt)]])
    sch = [S('kicad_sch'), [S('version'), 20250610], [S('generator'), 'eeschema'], [S('generator_version'), '10.0'],
           [S('uuid'), ROOT_UUID], [S('paper'), 'A3'],
           [S('title_block'), [S('title'), 'AD9850 DDS Function Generator - carrier board'],
            [S('date'), '2026-11-12'], [S('rev'), '1.0'],
            [S('comment'), 1, 'Blue Pill STM32F103C8 + AD9850 module + LM7171 output amplifier'],
            [S('comment'), 2, 'Generated by Hardware/tools/gen_project.py from design.py']],
           [S('lib_symbols')] + list(lib_syms.values())] + body + [
           [S('sheet_instances'), [S('path'), '/', [S('page'), '1']]],
           [S('embedded_fonts'), S('no')]]
    with open(os.path.join(OUT, D.PROJECT + '.kicad_sch'), 'w', encoding='utf-8', newline='\n') as f:
        f.write(dump(sch) + '\n')
    return pos_of, fields_of


# =============================================================== footprints
def fp_text(txt, x, y, rot=0, layer='F.SilkS', size=0.8, th=0.12):
    return [S('fp_text'), S('user'), txt, [S('at'), x, y, rot], [S('layer'), layer],
            [S('effects'), [S('font'), [S('size'), size, size], [S('thickness'), th]]],
            [S('uuid'), uid('fptext', txt, x, y, layer)]]


def fp_rect(x1, y1, x2, y2, layer, w):
    return [S('fp_rect'), [S('start'), x1, y1], [S('end'), x2, y2],
            [S('stroke'), [S('width'), w], [S('type'), S('solid')]], [S('fill'), S('no')], [S('layer'), layer],
            [S('uuid'), uid('fprect', x1, y1, x2, y2, layer)]]


def fp_pad(num, x, y, shape='circle', size=1.7, drill=1.0):
    return [S('pad'), num, S('thru_hole'), S(shape), [S('at'), x, y], [S('size'), size, size], [S('drill'), drill],
            [S('layers'), '*.Cu', '*.Mask'], [S('remove_unused_layers'), S('no')], [S('uuid'), uid('pad', num, x, y)]]


def footprint(name, descr, items, ref_at, val_at):
    return [S('footprint'), name, [S('version'), 20241229], [S('generator'), 'pcbnew'],
            [S('generator_version'), '10.0'], [S('layer'), 'F.Cu'], [S('descr'), descr], [S('tags'), name],
            [S('property'), 'Reference', 'REF**', [S('at'), ref_at[0], ref_at[1], 0], [S('layer'), 'F.SilkS'],
             [S('uuid'), uid('ref', name)], [S('effects'), [S('font'), [S('size'), 1, 1], [S('thickness'), 0.15]]]],
            [S('property'), 'Value', name, [S('at'), val_at[0], val_at[1], 0], [S('layer'), 'F.Fab'],
             [S('uuid'), uid('val', name)], [S('effects'), [S('font'), [S('size'), 1, 1], [S('thickness'), 0.15]]]],
            [S('attr'), S('through_hole')]] + items + [[S('embedded_fonts'), S('no')]]


def custom_footprints():
    fps = {}
    # ---- Blue Pill (USB on the left, header 1 on top)
    items = []
    for k in range(1, 21):
        x = round((20 - k) * 2.54, 4)
        items.append(fp_pad(str(k), x, 0.0, 'rect' if k == 1 else 'circle'))
        items.append(fp_pad(str(20 + k), x, D.BLUEPILL_ROW_SPACING))
        items.append(fp_text(D.BP_H1[k - 1], x, 3.5, 90))
        items.append(fp_text(D.BP_H2[k - 1], x, round(D.BLUEPILL_ROW_SPACING - 3.5, 4), 90))
    items += [fp_rect(-2.54, -3.81, 50.8, 19.05, 'F.SilkS', 0.12), fp_rect(-2.54, -3.81, 50.8, 19.05, 'F.Fab', 0.1),
              fp_rect(-3.1, -4.3, 51.3, 19.6, 'F.CrtYd', 0.05),
              fp_text('USB', -1.3, 7.62, 90, size=1.0, th=0.15), fp_text('SWD', 49.6, 7.62, 90, size=1.0, th=0.15),
              fp_text('BLUE PILL', 24.13, 7.62, 0, size=1.5, th=0.2)]
    fps['BluePill_Socket'] = footprint('BluePill_Socket', 'Socket (2x 1x20 female header, 15.24 mm rows) for STM32F103C8 Blue Pill, USB to the left',
                                       items, (24.13, -5.2), (24.13, 20.5))
    # ---- AD9850 module (data row on top, control row at the bottom)
    S_ = D.AD9850_ROW_SPACING
    items = []
    for k in range(1, 11):
        x = round((k - 1) * 2.54, 4)
        items.append(fp_pad(str(k), x, S_, 'rect' if k == 1 else 'circle'))
        items.append(fp_pad(str(10 + k), x, 0.0))
        items.append(fp_text(D.AD_NAMES[str(k)], x, round(S_ - 3.6, 4), 90))
        items.append(fp_text(D.AD_NAMES[str(10 + k)], x, 3.4, 90))
    m = (26.1 - S_) / 2
    items += [fp_rect(-13.0, -m, 32.1, S_ + m, 'F.SilkS', 0.12), fp_rect(-13.0, -m, 32.1, S_ + m, 'F.Fab', 0.1),
              fp_rect(-13.5, -m - 0.5, 32.6, S_ + m + 0.5, 'F.CrtYd', 0.05),
              fp_text('AD9850 MODULE', -6.5, S_ / 2, 90, size=1.2, th=0.18),
              fp_text('XTAL/LED side', -10.5, S_ / 2, 90, size=0.8)]
    fps['AD9850_Module_Socket'] = footprint('AD9850_Module_Socket', 'Socket (2x 1x10 female header) for AD9850 DDS module HC-SR08',
                                            items, (9.0, -m - 1.5), (9.0, S_ + m + 1.5))
    # ---- Mornsun A_S-2WR3 SIP (pins 1,2,4,5,6)
    items = []
    for num, name in sorted(D.PS_NAMES.items()):
        x = round((int(num) - 1) * 2.54, 4)
        items.append(fp_pad(num, x, 0.0, 'rect' if num == '1' else 'circle', size=1.6, drill=1.0))
        items.append(fp_text(name, x, 2.6, 90, size=0.8, th=0.12))
    items += [fp_rect(-2.21, -3.0, 17.44, 4.05, 'F.SilkS', 0.12), fp_rect(-2.21, -3.0, 17.44, 4.05, 'F.Fab', 0.1),
              fp_rect(-2.7, -3.5, 17.95, 4.55, 'F.CrtYd', 0.05)]
    fps['Mornsun_SIP6_A_S-2WR3'] = footprint('Mornsun_SIP6_A_S-2WR3', 'Mornsun A_S-2WR3 dual output SIP (pins 1,2,4,5,6)',
                                             items, (7.6, -4.3), (7.6, 5.5))
    return fps


# ================================================================ libraries
def write_libs(customs, fps):
    lib = [S('kicad_symbol_lib'), [S('version'), 20251024], [S('generator'), 'dds_gen'], [S('generator_version'), '10.0']]
    lib += list(customs.values())
    with open(os.path.join(OUT, D.KLIB + '.kicad_sym'), 'w', encoding='utf-8', newline='\n') as f:
        f.write(dump(lib) + '\n')
    pdir = os.path.join(OUT, D.KLIB + '.pretty')
    os.makedirs(pdir, exist_ok=True)
    for name, fp in fps.items():
        with open(os.path.join(pdir, name + '.kicad_mod'), 'w', encoding='utf-8', newline='\n') as f:
            f.write(dump(fp) + '\n')
    with open(os.path.join(OUT, 'sym-lib-table'), 'w', newline='\n') as f:
        f.write('(sym_lib_table\n\t(version 7)\n\t(lib (name "%s")(type "KiCad")(uri "${KIPRJMOD}/%s.kicad_sym")(options "")(descr "Project symbols"))\n)\n' % (D.KLIB, D.KLIB))
    with open(os.path.join(OUT, 'fp-lib-table'), 'w', newline='\n') as f:
        f.write('(fp_lib_table\n\t(version 7)\n\t(lib (name "%s")(type "KiCad")(uri "${KIPRJMOD}/%s.pretty")(options "")(descr "Project footprints"))\n)\n' % (D.KLIB, D.KLIB))


def write_project():
    def nc(name, track, clear=0.2, via=0.8, drill=0.4, prio=0):
        return {'name': name, 'clearance': clear, 'track_width': track, 'via_diameter': via, 'via_drill': drill,
                'microvia_diameter': 0.3, 'microvia_drill': 0.1, 'diff_pair_width': 0.2, 'diff_pair_gap': 0.25,
                'diff_pair_via_gap': 0.25, 'wire_width': 6, 'bus_width': 12, 'line_style': 0,
                'schematic_color': 'rgba(0, 0, 0, 0.000)', 'pcb_color': 'rgba(0, 0, 0, 0.000)', 'priority': prio}
    pro = {
        'board': {'design_settings': {'defaults': {}, 'rules': {'min_clearance': 0.2, 'min_track_width': 0.2,
                                                                 'min_via_diameter': 0.6, 'min_through_hole_diameter': 0.3,
                                                                 'min_copper_edge_clearance': 0.5, 'min_hole_clearance': 0.25,
                                                                 'min_silk_clearance': 0.0},
                                      'track_widths': [0.0, 0.3, 0.6, 1.0], 'via_dimensions': [{'diameter': 0.0, 'drill': 0.0}, {'diameter': 0.8, 'drill': 0.4}]}},
        'meta': {'filename': D.PROJECT + '.kicad_pro', 'version': 3},
        'net_settings': {
            'classes': [nc('Default', 0.3, prio=2147483647), nc('Power', 0.6, prio=0), nc('Analog', 0.4, prio=1)],
            'meta': {'version': 4},
            'netclass_patterns': [{'netclass': 'Power', 'pattern': n} for n in D.POWER_NETS] +
                                 [{'netclass': 'Analog', 'pattern': n} for n in
                                  ('SINE_RAW', 'SINE_AC', 'AMP_WIPER', 'AMP_INP', 'AMP_FB', 'AMP_OUT', 'SINE_OUT', 'SQ_OUT', 'SYNC_OUT')],
        },
        'schematic': {'legacy_lib_dir': '', 'legacy_lib_list': []},
        'sheets': [[ROOT_UUID, 'Root']],
    }
    with open(os.path.join(OUT, D.PROJECT + '.kicad_pro'), 'w', newline='\n') as f:
        json.dump(pro, f, indent=2)


# ===================================================================== PCB
def build_pcb(sym_uuids, fields):
    import pcbnew
    mm = pcbnew.FromMM
    OX, OY = 50.0, 50.0

    def V(x, y):
        return pcbnew.VECTOR2I(mm(x + OX), mm(y + OY))

    path = os.path.join(OUT, D.PROJECT + '.kicad_pcb')
    if os.path.exists(path):
        os.remove(path)
    board = pcbnew.NewBoard(path)
    nets = {}

    def net(name):
        if name not in nets:
            ni = pcbnew.NETINFO_ITEM(board, name)
            board.Add(ni)
            nets[name] = ni
        return nets[name]

    for name in sorted({n for p in D.PARTS for n in p['pins'].values() if n}):
        net(name)

    for part in D.PARTS:
        if not part['fp']:
            continue
        nick, name = part['fp'].split(':')
        libdir = os.path.join(OUT, nick + '.pretty') if nick == D.KLIB else os.path.join(FPLIB, nick + '.pretty')
        fp = pcbnew.FootprintLoad(libdir, name)
        if fp is None:
            raise RuntimeError('footprint not found: ' + part['fp'])
        fp.SetFPID(pcbnew.LIB_ID(nick, name))
        fp.SetReference(part['ref'])
        fp.SetValue(part['value'])
        board.Add(fp)
        x, y, rot = part['pcb']
        fp.SetPosition(V(x, y))
        fp.SetOrientationDegrees(rot)
        fp.SetPath(pcbnew.KIID_PATH('/' + sym_uuids[part['ref']]))
        fp.SetLibDescription(fields[part['ref']]['Description'])
        fp.GetField('Description').SetText(fields[part['ref']]['Description'])
        fp.GetField('Datasheet').SetText(fields[part['ref']]['Datasheet'])
        if part['ref'].startswith('H'):
            fp.Reference().SetVisible(False)
        if part['ref'] in D.REF_POS:
            fp.Reference().SetPosition(V(*D.REF_POS[part['ref']]))
            fp.Reference().SetTextAngleDegrees(0)
        pin_names = fields[part['ref']]['pin_names']
        for pad in fp.Pads():
            num = pad.GetNumber()
            n = part['pins'].get(num)
            if not n and num in pin_names:
                pn = pin_names[num]
                # same name KiCad gives an unconnected schematic pin
                n = 'unconnected-(%s-%sPad%s)' % (part['ref'], pn + '-' if pn and pn != '~' else '', num)
            if n:
                pad.SetNet(net(n))

    # outline
    pts = [(0, 0), (D.BOARD_W, 0), (D.BOARD_W, D.BOARD_H), (0, D.BOARD_H)]
    for i in range(4):
        s = pcbnew.PCB_SHAPE(board)
        s.SetShape(pcbnew.SHAPE_T_SEGMENT)
        s.SetStart(V(*pts[i]))
        s.SetEnd(V(*pts[(i + 1) % 4]))
        s.SetLayer(pcbnew.Edge_Cuts)
        s.SetWidth(mm(0.1))
        board.Add(s)

    def text(t, x, y, size=1.2, rot=0, layer=pcbnew.F_SilkS):
        tx = pcbnew.PCB_TEXT(board)
        tx.SetText(t)
        tx.SetPosition(V(x, y))
        tx.SetLayer(layer)
        tx.SetTextSize(pcbnew.VECTOR2I(mm(size), mm(size)))
        tx.SetTextThickness(mm(size * 0.15))
        tx.SetTextAngleDegrees(rot)
        board.Add(tx)

    for t, x, y, size, rot in D.PCB_TEXT:
        text(t, x, y, size, rot)

    board.Save(path)
    return path


def main():
    os.makedirs(OUT, exist_ok=True)
    customs = custom_symbols()
    fps = custom_footprints()
    write_libs(customs, fps)
    write_project()
    sym_uuids, fields = build_schematic(customs)
    if '--sch-only' in sys.argv:
        print('schematic and libraries written (PCB left untouched)')
        return
    path = build_pcb(sym_uuids, fields)
    print('written', path)


if __name__ == '__main__':
    main()
