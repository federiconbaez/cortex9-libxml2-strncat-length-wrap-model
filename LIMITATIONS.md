# Limitations

- **Lengths only.** No string is built and no memory is touched. A
  "write" is a comparison of an offset and a length against a capacity.
- **Undefined behaviour.** Signed `int` overflow is UB in C. `label_c11`
  stops there (`UB_STOP`). `label_wrap32` is an explicit variant that
  assumes a two's-complement wrap; a real compiler may optimise the path
  differently. No wrap result is presented as what libxml2 does.
- **Allocator.** Requests above 2^47 bytes are modelled as failing; below
  that they succeed. A request of 0 bytes stops as `ALLOC_ZERO`
  (implementation-defined `realloc`/`malloc`).
- **Not modelled:** `xmlStrndup`/`xmlStrdup` internals, NULL `cur`/`add`
  pointers, encodings, callers of these functions, 32-bit targets.
- **8fbbf55.** The model shows that `post` still evaluates `size + len + 1`
  in `int` (case `x07`). This is an **unverified observation** about the
  model, not a finding: it was not reproduced against a libxml2 build and
  later commits up to febeb5c add the `INT_MAX - len` check.
- **Input domain:** l1, l2 <= 2^47; `len` in the `int` range; `len > 0`
  requires `len <= l2`. Anything else is `BAD_INPUT`. The CLI uses checked
  `strtoull`/`strtoll` (digits only).
- **Sweep:** `results/sweep.csv` is a fixed grid, not a proof.
- This is not a finding. Nothing is attributed beyond the commits listed in
  SOURCES.md and the public CVE.
