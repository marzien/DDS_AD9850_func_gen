#!/usr/bin/env python3
"""
Memory budget check for the firmware (run after a build):

    python tools/mem_check.py [Debug/DDS_AD9850_FULL.elf]

1. Flash and RAM use against the linker regions.
2. Worst-case stack depth of every task and of the interrupt (main) stack,
   estimated from the disassembly call graph, compared with the stack sizes
   in Core/Src/App.c / FreeRTOSConfig.h / the linker script.

Frame sizes come from GCC's .su files (-fstack-usage) where available, else
from the function prologue (push / sub sp). Calls through function pointers
cannot be followed automatically; the known ones are listed in INDIRECT.
The result is an estimate: confirm on the board with the USB command MEM.
"""
import glob
import os
import re
import subprocess
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
ELF = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'Debug', 'DDS_AD9850_FULL.elf')
MARGIN = 0.25          # required free share of every stack
CONTEXT = 64           # bytes a task stack holds for a context switch / exception entry
EXC_FRAME = 32         # hardware exception frame on the main stack per nested interrupt

# caller regex -> callee regexes, for calls through function pointers
INDIRECT = {
    r'^App_MemReport$': [r'^out$'],
    r'^(_svfprintf_r|_svfiprintf_r|_printf_i|_printf_common)$': [r'^__ssputs_r$'],
    r'^USBD_(?!CDC_|FS_)': [r'^USBD_CDC_', r'^CDC_.*_FS$', r'^USBD_FS_.*Descriptor$',
                                                         r'^USBD_CDC_Get.*Desc'],
    r'^USBD_CDC_': [r'^CDC_.*_FS$'],
    r'^CDC_Receive_FS$': [r'^CDC_RxCallback$'],
    r'^HAL_TIM_IRQHandler$': [r'^HAL_TIM_PeriodElapsedCallback$'],
}
# Low priority interrupts (15) can be preempted by USB (5): depths add up on the main stack
ISR_LOW = ['TIM4_IRQHandler', 'SysTick_Handler', 'PendSV_Handler']
ISR_HIGH = ['USB_LP_CAN1_RX0_IRQHandler']


def tool(name):
    pat = os.path.join('C:\\', 'ST', 'STM32CubeIDE_*', 'STM32CubeIDE', 'plugins', '*gnu-tools-for-stm32*', 'tools', 'bin',
                       name + '.exe')
    hits = sorted(glob.glob(pat))
    return hits[-1] if hits else name


def run(*args):
    return subprocess.run(args, capture_output=True, text=True, check=True).stdout


def read_su():
    su = {}
    for f in glob.glob(os.path.join(os.path.dirname(ELF), '**', '*.su'), recursive=True):
        for line in open(f, encoding='utf-8', errors='replace'):
            parts = line.rstrip('\n').split('\t')
            if len(parts) >= 3 and parts[1].isdigit():
                name = parts[0].split(':')[-1]
                su[name] = max(su.get(name, 0), int(parts[1]))
    return su


def read_disasm():
    funcs = {}
    cur = None
    for line in run(tool('arm-none-eabi-objdump'), '-d', '--no-show-raw-insn', ELF).splitlines():
        m = re.match(r'^[0-9a-f]+ <([^>]+)>:$', line)
        if m:
            cur = funcs.setdefault(m.group(1), {'calls': set(), 'frame': 0, 'indirect': False})
            continue
        if cur is None or '\t' not in line:
            continue
        ins = line.split('\t', 1)[1].strip()
        op = ins.split()[0] if ins else ''
        m = re.search(r'<([A-Za-z_][\w.]*)(\+0x[0-9a-f]+)?>', ins)
        if op.startswith(('bl', 'b.', 'b', 'cb')) and m and not m.group(2):
            cur['calls'].add(m.group(1))
        if op in ('blx', 'bx') and not m and 'lr' not in ins:
            cur['indirect'] = True
        if op == 'push' or op == 'stmdb':
            regs = re.search(r'\{(.*)\}', ins)
            if regs:
                cur['frame'] += 4 * len(regs.group(1).split(','))
        m2 = re.match(r'sub(?:\.w|w|s)?\s+sp,(?:\s*sp,)?\s*#(\d+)', ins)
        if m2:
            cur['frame'] += int(m2.group(1))
    return funcs


def main():
    size = run(tool('arm-none-eabi-size'), '-A', ELF)
    sec = {m.group(1): int(m.group(2)) for m in re.finditer(r'^(\.[\w.]+)\s+(\d+)', size, re.M)}
    syms = {}
    for line in run(tool('arm-none-eabi-nm'), '-S', ELF).splitlines():
        p = line.split()
        if len(p) == 4:
            syms[p[3]] = (int(p[0], 16), int(p[1], 16))
        elif len(p) == 3:
            syms[p[2]] = (int(p[0], 16), 0)
    flash_size, ram_size = syms['_flash_size'][0], syms['_ram_size'][0]
    flash = sum(v for k, v in sec.items() if k in ('.isr_vector', '.text', '.rodata', '.ARM.extab', '.ARM',
                                                    '.preinit_array', '.init_array', '.fini_array', '.data'))
    ram_static = sec.get('.data', 0) + sec.get('.bss', 0)
    reserve = sec.get('._user_heap_stack', 0)          # newlib heap + main stack reserved by the linker
    main_stack = syms['_Min_Stack_Size'][0]
    ok = True

    print('== Memory regions')
    for name, used, total in (('Flash', flash, flash_size), ('RAM (static + heap + stack)', ram_static + reserve, ram_size)):
        pct = 100.0 * used / total
        flag = 'ok' if pct <= 95 else 'FULL'
        ok &= pct <= 95
        print('  %-28s %6d / %6d B  %5.1f %%  free %6d  %s' % (name, used, total, pct, total - used, flag))
    print('  data+bss %d B (task stacks %d B of it), newlib heap %d B, main/interrupt stack %d B'
          % (ram_static, sum(syms[s][1] for s in syms if s.startswith('s_stack_') or s.startswith('uxIdleTaskStack')),
             reserve - main_stack, main_stack))

    su = read_su()
    funcs = read_disasm()
    for name, f in funcs.items():
        if name in su:
            f['frame'] = su[name]
    for pat, callees in INDIRECT.items():
        for name, f in funcs.items():
            if re.search(pat, name):
                for c in funcs:
                    if any(re.search(cp, c) for cp in callees) and c != name:
                        f['calls'].add(c)
                f['indirect'] = False

    memo = {}
    unresolved = set()

    def depth(name, path=()):
        if name in memo:
            return memo[name]
        f = funcs.get(name)
        if f is None or name in path:
            return 0, ()
        if f['indirect']:
            unresolved.add(name)
        best, chain = 0, ()
        for c in f['calls']:
            d, ch = depth(c, path + (name,))
            if d > best:
                best, chain = d, ch
        memo[name] = (f['frame'] + best, (name,) + chain)
        return memo[name]

    def stack_of(sym):
        return syms[sym][1]

    tasks = [('DDS', 'DDS_Task', stack_of('s_stack_dds')), ('INPUT', 'input_task', stack_of('s_stack_input')),
             ('UI', 'FG_UiTask', stack_of('s_stack_ui')), ('USB', 'UC_Task', stack_of('s_stack_usb')),
             ('IDLE', 'prvIdleTask', next(v[1] for k, v in syms.items() if k.startswith('uxIdleTaskStack')))]
    print('\n== Worst-case stack estimate (frame sizes + call graph)')
    print('  %-6s %6s %6s %6s  %s' % ('stack', 'size', 'need', 'free', 'deepest call chain'))
    rows = []
    for label, entry, size_b in tasks:
        d, chain = depth(entry)
        rows.append((label, size_b, d + CONTEXT, chain))
    low = max((depth(n) for n in ISR_LOW if n in funcs), key=lambda x: x[0])
    high = max((depth(n) for n in ISR_HIGH if n in funcs), key=lambda x: x[0])
    rows.append(('ISR', main_stack, low[0] + high[0] + 2 * EXC_FRAME, low[1][:2] + ('+',) + high[1]))
    for label, size_b, need, chain in rows:
        free = size_b - need
        good = free >= MARGIN * size_b
        ok &= good
        txt = ' > '.join(chain)
        print('  %-6s %6d %6d %6d  %s%s' % (label, size_b, need, free, '' if good else '** LOW ** ',
                                           txt if len(txt) < 150 else txt[:147] + '...'))
    if unresolved:
        print('\n  Function-pointer calls not followed in: ' + ', '.join(sorted(unresolved)))
    print('\nRESULT: ' + ('PASS' if ok else 'CHECK NEEDED') +
          '  (estimate; confirm on the board with the USB command MEM)')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
