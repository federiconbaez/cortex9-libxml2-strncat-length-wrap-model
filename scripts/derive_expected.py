#!/usr/bin/env python3
"""Independent derivation of fixtures/expected.tsv.

Written from the libxml2 sources (xmlstring.c at 8f30bdf, 8fbbf55 and
febeb5c), not from the C model. Python integers do not overflow, so every
int expression is evaluated exactly and then checked against the int range:
a result outside [-2^31, 2^31) is undefined behaviour in C. The "wrap32"
column continues with the two's-complement wrap as an explicit variant; the
"c11" column stops there with UB_STOP.
"""
import sys
from pathlib import Path

INT_MIN, INT_MAX = -(2**31), 2**31 - 1
HEAP_MAX = 2**47          # model: allocations larger than this fail
SIZE_T = 2**64


class Path_:
    def __init__(self):
        self.ub = False

    def intop(self, exact):
        """Result of a C int expression; flag UB, return the wrapped value."""
        if not INT_MIN <= exact <= INT_MAX:
            self.ub = True
        w = exact % 2**32
        return w - 2**32 if w >= 2**31 else w


def xml_strlen(p, version, n):
    if version == "master":           # size_t strlen; > INT_MAX -> 0
        return 0 if n > INT_MAX else n
    # int len; len++ per byte: overflow once n > INT_MAX
    return p.intop(n)


def fits(cap, off, n):
    return off >= 0 and off <= cap and n <= cap - off


def strncat(version, l1, l2, length):
    p = Path_()
    out = dict(size=0, req=0, res=0, oob=0)
    if length == 0:
        return "OK", p, dict(out, res=l1)
    if length < 0:
        return "NULL_RET", p, out
    size = xml_strlen(p, version, l1)
    out["size"] = size
    if version == "post" and size < 0:
        return "NULL_RET", p, out
    if version == "master":
        if size < 0 or size > INT_MAX - length:
            return "NULL_RET", p, out
        req = size + length + 1                 # (size_t) size + len + 1
    else:
        req = p.intop(p.intop(size + length) + 1) % SIZE_T
    out["req"] = req
    if req == 0:
        return "ALLOC_ZERO", p, out
    if req > HEAP_MAX:
        return ("NULL_RET" if version == "master" else "CUR_RET"), p, out
    if not fits(req, size, length):
        return "OOB_WRITE", p, dict(out, oob=size)
    term = size + length if version == "master" else p.intop(size + length)
    if not fits(req, term, 1):
        return "OOB_WRITE", p, dict(out, oob=term)
    out["res"] = term
    return ("OK" if term == l1 + length else "LOSSY"), p, out


def strncatnew(version, l1, l2, length):
    p = Path_()
    out = dict(size=0, req=0, res=0, oob=0)
    want = l1 + (l2 if length < 0 else length)
    if length < 0:
        length = xml_strlen(p, version, l2)
        if version != "pre" and length < 0:
            return "NULL_RET", p, out
    if length == 0:
        return "DUP_STR1", p, dict(out, res=l1)
    size = xml_strlen(p, version, l1)
    out["size"] = size
    if version == "master":
        if size < 0 or size > INT_MAX - length:
            return "NULL_RET", p, out
        req = size + length + 1
    else:
        if version == "post" and size < 0:
            return "NULL_RET", p, out
        req = p.intop(p.intop(size + length) + 1) % SIZE_T
    out["req"] = req
    if req == 0:
        return "ALLOC_ZERO", p, out
    if req > HEAP_MAX:
        if version == "master":
            return "NULL_RET", p, out
        if size < 0:                       # xmlStrndup(str1, size<0): NULL
            return "NULL_RET", p, out
        return "NDUP_FALLBACK", p, dict(out, res=size)
    if not fits(req, 0, size % SIZE_T):
        return "OOB_WRITE", p, dict(out, oob=0)
    if not fits(req, size, length % SIZE_T):
        return "OOB_WRITE", p, dict(out, oob=size)
    term = size + length if version == "master" else p.intop(size + length)
    if not fits(req, term, 1):
        return "OOB_WRITE", p, dict(out, oob=term)
    out["res"] = term
    return ("OK" if term == want else "LOSSY"), p, out


def main():
    root = Path(__file__).resolve().parent.parent
    rows = ["id\tversion\tlabel_c11\tlabel_wrap32\trequest\tresult_len\toob_offset\tub"]
    for line in (root / "fixtures" / "cases.tsv").read_text().splitlines():
        if not line or line.startswith("#"):
            continue
        cid, fn, l1, l2, length = line.split("\t")
        l1, l2, length = int(l1), int(l2), int(length)
        f = strncat if fn == "strncat" else strncatnew
        for version in ("pre", "post", "master"):
            label, p, o = f(version, l1, l2, length)
            c11 = "UB_STOP" if p.ub else label
            rows.append("\t".join(str(x) for x in (
                cid, version, c11, label, o["req"], o["res"], o["oob"], int(p.ub))))
    sys.stdout.write("\n".join(rows) + "\n")


if __name__ == "__main__":
    main()
