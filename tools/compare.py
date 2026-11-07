"""Compare XGPlay DUMP (C, as on the Amiga) with the Rust core's playback plan.

    python compare.py <xgplay exe> <refdump exe> <file.mid>...

Note-offs are compared as 9n nn 00 (the Rust core writes them so); times may
differ by 2 microseconds (rounding). Exit code 1 when a file differs."""
import subprocess
import sys


def lines(cmd):
    out = subprocess.run(cmd, capture_output=True, check=True).stdout.decode()
    rows = []
    for ln in out.splitlines():
        p = ln.split()
        if not p:
            continue
        b = [int(x, 16) for x in p[1:]]
        if len(b) == 3 and b[0] & 0xF0 == 0x80:
            b = [0x90 | (b[0] & 0x0F), b[1], 0]
        rows.append((int(p[0]), tuple(b)))
    return rows


def main():
    xgplay, ref, files = sys.argv[1], sys.argv[2], sys.argv[3:]
    bad = 0
    for f in files:
        a = sorted(lines([xgplay, f, "DUMP"]), key=lambda r: (r[1], r[0]))
        b = sorted(lines([ref, f]), key=lambda r: (r[1], r[0]))
        # pair the same messages in time order; count what is left over or off in time
        from collections import defaultdict
        da, db = defaultdict(list), defaultdict(list)
        for t, m in a:
            da[m].append(t)
        for t, m in b:
            db[m].append(t)
        missing = extra = late = 0
        worst = 0
        for m in set(da) | set(db):
            ta, tb = sorted(da[m]), sorted(db[m])
            for x, y in zip(ta, tb):
                worst = max(worst, abs(x - y))
                late += abs(x - y) > 2
            extra += max(0, len(ta) - len(tb))
            missing += max(0, len(tb) - len(ta))
        ok = not (missing or extra or late)
        bad += not ok
        print(f"{'OK  ' if ok else 'DIFF'} {f}: {len(a)} messages (Rust {len(b)}), "
              f"{missing} missing, {extra} extra, {late} off in time (worst {worst} us)")
    sys.exit(1 if bad else 0)


main()
