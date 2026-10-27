"""Count TMC driver reset events (tkr17536 lines) per toolhead in a Prusa XL log.txt.

usage: python3 count_17536.py log.txt

Turn on "Save Logs To File" on the printer (it is reset at power-off) and give this
script the log.txt from the USB stick.

- The counter n in each line is cumulative since the toolhead (Dwarf) started; it
  restarts when the Dwarf reboots (n goes down). Events = sum of the last n of every run.
- skip = events not logged since the previous line (at most one line per second is
  logged), so there can be fewer lines than events.
- One log.txt can contain several sessions ("Started logging to file", or the main
  board time restarts after a reboot); they are reported separately.
"""
import re
import sys
from collections import defaultdict

LINE = re.compile(r'^([0-9.]+)s \[\w+\s*- Dwarf_(\d):\d+\] ([0-9.]+)s .*?tkr17536 GSTAT=0x(\w+) DRV=0x(\w+) '
                  r'24V=(-?\d+)mV n=(\d+) skip=(\d+) de=(\d+) r=(\d+)')
BUDDY_T = re.compile(r'^([0-9.]+)s ')
FWNAME = re.compile(r'FIRMWARE_NAME:\S+ (\S+)')
STOP = re.compile(r'tkr17536 drv_err x\d+ -> stop')
BAD = re.compile(r'tkr17536 badread')


def new_session(t):
    return dict(versions=set(), runs=defaultdict(list), lines=defaultdict(int), r=defaultdict(set),
                v=defaultdict(list), stops=0, badreads=0, t0=t, t1=t)


def main(path):
    sessions = []
    cur = None
    last_t = None
    for raw in open(path, encoding='utf-8', errors='replace'):
        line = raw.rstrip('\n')
        mt = BUDDY_T.match(line)
        if mt:
            t = float(mt.group(1))
            if cur is None or (last_t is not None and t + 5 < last_t) or 'Started logging to file' in line:
                cur = new_session(t)
                sessions.append(cur)
            last_t = t
            cur['t1'] = t
        if cur is None:
            continue
        fw = FWNAME.search(line)
        if fw:
            cur['versions'].add(fw.group(1))
        if STOP.search(line):
            cur['stops'] += 1
        if BAD.search(line):
            cur['badreads'] += 1
        m = LINE.match(line)
        if not m:
            continue
        dw = int(m.group(2))
        n = int(m.group(7))
        runs = cur['runs'][dw]
        if not runs or n < runs[-1]:
            runs.append(n)  # new Dwarf run (counter restarted)
        else:
            runs[-1] = n
        cur['lines'][dw] += 1
        cur['r'][dw].add(int(m.group(10)))
        cur['v'][dw].append(int(m.group(6)))
    if not sessions:
        print('no log lines found')
    for i, s in enumerate(sessions, 1):
        fw = ', '.join(sorted(s['versions'])) or '(no FIRMWARE_NAME line)'
        print(f'== session {i}: main board time {s["t0"]:.0f}s .. {s["t1"]:.0f}s  firmware: {fw}')
        if not s['lines']:
            print('   no tkr17536 event lines (0 driver resets)')
        for dw in sorted(s['lines']):
            ev = sum(s['runs'][dw])
            v = s['v'][dw]
            print(f'   Dwarf_{dw} (tool {dw}): events={ev} lines={s["lines"][dw]} '
                  f'r={sorted(s["r"][dw])} 24V={min(v)}..{max(v)}mV')
        if s['stops'] or s['badreads']:
            print(f'   stop lines={s["stops"]} badread lines={s["badreads"]}')


if __name__ == '__main__':
    main(sys.argv[1])
