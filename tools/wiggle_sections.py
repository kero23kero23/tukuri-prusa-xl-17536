"""Count driver resets per section of a wiggle test (see docs/wiggle_test.md).

usage: python3 wiggle_sections.py log.txt TOOL_NUMBER

Sections are separated by changes of the nozzle target temperature of that tool
("Set hotend temperature: N" lines), which the test uses as markers. The toolhead's
own timestamps (the second time in each Dwarf_N line) are used, so a late transfer
of the toolhead log to the main board does not move events between sections.
Resets in a section = increase of the cumulative counter n (includes the events that
were not logged one by one, see skip=). If the Dwarf rebooted during the log, use
only the part after the reboot.
"""
import re
import sys


def main(path, tool):
    pat = re.compile(r'^([0-9.]+)s \[\w+\s*- Dwarf_' + tool + r':\d+\] ([0-9.]+)s (.*)$')
    marks, events = [], []
    for line in open(path, encoding='utf-8', errors='replace'):
        m = pat.match(line.rstrip('\n'))
        if not m:
            continue
        t = float(m.group(2))
        msg = m.group(3)
        mt = re.search(r'Set hotend temperature: (\d+)', msg)
        if mt:
            marks.append((t, int(mt.group(1))))
        me = re.search(r'tkr17536 GSTAT=0x\w+ DRV=0x\w+ 24V=-?\d+mV n=(\d+)', msg)
        if me:
            events.append((t, int(me.group(1))))
    marks.sort()
    events.sort()

    def n_before(t):
        best = 0
        for te, n in events:
            if te < t:
                best = n
        return best

    bounds = [(float('-inf'), None)] + marks
    print(f'tool {tool}: {len(marks)} temperature markers, {len(events)} tkr17536 lines')
    for i, (t0, temp) in enumerate(bounds):
        t1 = bounds[i + 1][0] if i + 1 < len(bounds) else float('inf')
        inside = [n for te, n in events if t0 <= te < t1]
        n0 = n_before(t0) if t0 != float('-inf') else 0
        resets = (max(inside) - n0) if inside else 0
        label = 'before the first marker' if temp is None else f'{temp} C marker'
        span = '' if t0 == float('-inf') or t1 == float('inf') else f' ({t1 - t0:.0f} s)'
        start = '-' if t0 == float('-inf') else f'{t0:.1f}s'
        print(f'  {label:24s} from {start:>10s}{span:9s} resets={resets} lines={len(inside)}')


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
