"""Every Job with the C code (jobrun) and the Rust core (refdump --job) on
each song; the two results are compared as playback plans (the Rust core
plays both), the same messages at the same times.

    python compare_jobs.py <jobrun> <refdump> <work dir> <song>..."""
import os
import subprocess
import sys
from collections import Counter

JOBS = ["transpose", "velocity", "gate", "shift", "sort", "separate", "control", "bend", "thin",
        "quantize", "stretch", "create", "delete", "chord"]


def plan(ref, f):
    out = subprocess.run([ref, f], capture_output=True, check=True).stdout.decode()
    rows = Counter()
    for ln in out.splitlines():
        p = ln.split()
        if not p:
            continue
        b = [int(x, 16) for x in p[1:]]
        if len(b) == 3 and b[0] & 0xF0 == 0x80:
            b = [0x90 | (b[0] & 0x0F), b[1], 0]
        rows[(int(p[0]) // 4, tuple(b))] += 1   # times within 4 us
    return rows


def main():
    jobrun, ref, work, songs = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4:]
    bad = 0
    for song in songs:
        line = []
        for j in JOBS:
            c, r = os.path.join(work, "c_" + j + ".mid"), os.path.join(work, "r_" + j + ".mid")
            subprocess.run([jobrun, j, song, c], check=True)
            subprocess.run([ref, "--job", j, song, r], check=True)
            a, b = plan(ref, c), plan(ref, r)
            if a == b:
                line.append(j)
            else:
                bad += 1
                d1, d2 = a - b, b - a
                line.append(f"{j}:DIFF(+{sum(d1.values())}/-{sum(d2.values())})")
                if os.environ.get("SHOW"):
                    print("   C only:", list(d1.items())[:4])
                    print("   R only:", list(d2.items())[:4])
        print(os.path.basename(song) + ": " + " ".join(line))
    sys.exit(1 if bad else 0)


main()
