"""List the placed objects in Croc 2 level files (original/crocii/*.WAD).

Each level file holds one table of 64-byte placement records (16 little-endian int32s):
  [8..10] x, y, z position     [12] object type (offset of the object's definition)
  [13] number of saved-state slots   [14] offset of the object's saved state in the level (-1 = none)
The table is found by scanning for the longest run of records whose always-zero fields (4, 7, 11) are zero.
(Field 5 is usually zero too, but not always: requiring it cut one level's table short.)
"""
import glob
import os
import struct
import sys


def _record(d, q):
    w = struct.unpack_from('<16i', d, q)
    if (w[4] == 0 and w[7] == 0 and w[11] == 0 and 100000 < w[12] < 3000000
            and abs(w[8]) < 4_000_000 and abs(w[9]) < 1_000_000 and abs(w[10]) < 4_000_000):
        return w
    return None


def placements(path):
    d = open(path, 'rb').read()
    best = (0, 0)
    for align in range(4):
        q = align
        while q + 64 <= len(d):
            if _record(d, q):
                s = q
                while q + 64 <= len(d) and _record(d, q):
                    q += 64
                n = (q - s) // 64
                if n > best[1]:
                    best = (s, n)
            else:
                q += 4
    s, n = best
    return [struct.unpack_from('<16i', d, s + 64 * k) for k in range(n)]


if __name__ == '__main__':
    root = sys.argv[1]
    for path in sorted(glob.glob(os.path.join(root, '*.WAD'))):
        for r in placements(path):
            print(os.path.basename(path)[:-4], r[12], r[8], r[9], r[10], r[13], r[14], sep='\t')
