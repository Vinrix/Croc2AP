"""Work out how many inline argument words each strat opcode reads, from croc2.exe.

The opcode table is at RVA 0x7F9420 (468 handlers, rcx = strat, [strat+0] = script position). A handler reads an
argument by loading the position, adding 4 and storing it back, so the total it advances is 4 x its argument count.
This follows each handler's straight-line code and tracks registers that hold "position + n".
Usage: opcode_args.py <croc2.exe> [out.json]
"""
import json
import re
import sys

import capstone
import pefile

TABLE_RVA, COUNT = 0x7F9420, 468


def analyse(pe, md, base, rva, depth=0):
    strat = {'rcx'}          # registers holding the strat pointer
    val = {}                 # register -> bytes past the original position
    consumed = 0
    data = pe.get_data(rva, 600)
    n = 0
    for ins in md.disasm(data, base + rva):
        n += 1
        m, ops = ins.mnemonic, ins.op_str
        if m == 'mov':
            dst, src = [s.strip() for s in ops.split(',', 1)]
            if src in strat and re.fullmatch(r'r\w+', dst):
                strat.add(dst); val.pop(dst, None); continue
            mm = re.fullmatch(r'qword ptr \[(\w+)\]', src)
            if mm and mm.group(1) in strat:
                val[dst] = consumed; strat.discard(dst); continue
            mm = re.fullmatch(r'qword ptr \[(\w+)\]', dst)
            if mm and mm.group(1) in strat and src in val:
                consumed = max(consumed, val[src]); continue
            if src in val and re.fullmatch(r'r\w+', dst):
                val[dst] = val[src]; strat.discard(dst); continue
            if re.fullmatch(r'r\w+', dst):
                val.pop(dst, None); strat.discard(dst)
        elif m == 'lea':
            dst, src = [s.strip() for s in ops.split(',', 1)]
            mm = re.fullmatch(r'\[(\w+) \+ (0x[0-9a-f]+|\d+)\]', src)
            if mm and mm.group(1) in val:
                val[dst] = val[mm.group(1)] + int(mm.group(2), 0); strat.discard(dst)
            else:
                val.pop(dst, None); strat.discard(dst)
        elif m == 'add':
            dst, src = [s.strip() for s in ops.split(',', 1)]
            mm = re.fullmatch(r'qword ptr \[(\w+)\]', dst)
            if mm and mm.group(1) in strat and re.fullmatch(r'0x[0-9a-f]+|\d+', src):
                consumed += int(src, 0)
            elif dst in val and re.fullmatch(r'0x[0-9a-f]+|\d+', src):
                val[dst] += int(src, 0)
        elif m == 'jmp':
            mm = re.fullmatch(r'0x([0-9a-f]+)', ops)
            if mm and n <= 12 and depth < 2 and consumed == 0:   # thunk into another handler body
                return analyse(pe, md, base, int(mm.group(1), 16) - base, depth + 1)
            break
        elif m == 'ret':
            break
    return consumed // 4


def main():
    pe = pefile.PE(sys.argv[1])
    base = pe.OPTIONAL_HEADER.ImageBase
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    out = {}
    for op in range(COUNT):
        target = pe.get_qword_at_rva(TABLE_RVA + op * 8)
        out[op] = analyse(pe, md, base, target - base) if target else None
    if len(sys.argv) > 2:
        json.dump(out, open(sys.argv[2], 'w'), indent=0)
    return out


if __name__ == '__main__':
    r = main()
    print({k: v for k, v in r.items() if v})
