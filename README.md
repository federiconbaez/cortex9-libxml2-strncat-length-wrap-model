# cortex9-libxml2-strncat-length-wrap-model

Length-arithmetic model of **CVE-2016-1834**, the heap buffer overflow in
libxml2's `xmlStrncat` / `xmlStrncatNew` (`xmlstring.c`), at the parent
8f30bdf (`pre`), the fix 8fbbf55 (`post`) and master febeb5c (`master`).
Educational, offline, deterministic. Strings are **lengths** and
allocations are **capacities**: no 2 GiB string is built, nothing is
copied, no libxml2 code runs.

Spanish: [README.es.md](README.es.md). Exercises: [PRACTICE.md](PRACTICE.md).

## Two semantics, always side by side

Signed `int` overflow is **undefined behaviour** in C. The model never
assumes a wrap silently:

- `label_c11` (`cx9_call_c11`): any path that needs a signed overflow ends
  as `UB_STOP`. This is what the C standard supports.
- `label_wrap32` (`cx9_call`): an **explicit variant** that continues with
  the two's-complement wrap an unoptimised x86-64 build typically shows,
  and flags it with `ub = 1`. It is a model choice, not a claim about what
  a given compiler emits.

## What it shows

| Case | pre | post | master |
|---|---|---|---|
| `x05` cur > 2 GiB, len = INT_MAX | wrap32: `OOB_WRITE` at a negative offset; c11: `UB_STOP` | `NULL_RET` (wrap32), `UB_STOP` (c11) | `LOSSY`: strlen saturates to 0, memory-safe |
| `x07` size + len > INT_MAX, no strlen wrap | wrap32: `CUR_RET`; c11: `UB_STOP` | same as pre | `OK`, checked with `size > INT_MAX - len` |

**8fbbf55, unverified observation:** in the model, `post` still computes
`size + len + 1` in `int` (case `x07`: `UB_STOP` in C11, `CUR_RET` in
wrap32). This is an **unverified observation** about the model, not a
finding: it was not reproduced against a libxml2 build, and master adds
the `size > INT_MAX - len` check. See [LIMITATIONS.md](LIMITATIONS.md).

On a boundary grid (`tests/test_model.c`): every `OOB_WRITE` needs a signed
overflow first; post and master have no `OOB_WRITE` even in the wrap
variant; master has no signed overflow on any path.

## Run

```sh
sh scripts/build.sh test              # -Wall -Wextra -Werror, ASan + UBSan
python3 scripts/derive_expected.py | diff - fixtures/expected.tsv
sh scripts/build.sh results /tmp/r && diff -r results /tmp/r
./build/xmlstr case pre strncat 2147483658 2147483647 2147483647
```

`fixtures/expected.tsv` is derived independently in Python with exact
integers from the libxml2 sources; the C tests compare against it.

See [LIMITATIONS.md](LIMITATIONS.md) and [SOURCES.md](SOURCES.md).
License: Apache-2.0.
