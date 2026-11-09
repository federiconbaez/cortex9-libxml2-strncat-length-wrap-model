#include "xmlstr_model.h"

const char *cx9_label_name(cx9_label l) {
  static const char *n[] = {"OK",       "LOSSY",         "NULL_RET",
                            "CUR_RET",  "DUP_STR1",      "NDUP_FALLBACK",
                            "OOB_WRITE", "ALLOC_ZERO",   "BAD_INPUT",
                            "UB_STOP"};
  if ((int)l < 0 || (int)l >= (int)(sizeof n / sizeof n[0])) return "UNKNOWN";
  return n[l];
}
const char *cx9_version_name(cx9_version v) {
  static const char *n[] = {"pre", "post", "master"};
  if ((int)v < 0 || (int)v >= (int)(sizeof n / sizeof n[0])) return "unknown";
  return n[v];
}

/* Two's-complement wrap of an int result. Signed overflow is undefined in
 * C; the model assumes the wrap that unoptimised x86-64 code produces and
 * always reports it through the ub flag. */
int64_t cx9_wrap32(int64_t v) {
  uint64_t lo = (uint64_t)v & 0xFFFFFFFFu;
  return lo >= 0x80000000u ? (int64_t)lo - ((int64_t)1 << 32) : (int64_t)lo;
}

static int64_t int_add(int64_t a, int64_t b, int *ub) {
  int64_t s = a + b;
  if (s > INT32_MAX || s < INT32_MIN) *ub = 1;
  return cx9_wrap32(s);
}

/* (size_t)(int value) on a 64-bit target */
static uint64_t to_size_t(int64_t v) { return (uint64_t)v; }

int64_t cx9_xmlstrlen(cx9_version v, uint64_t true_len, int *ub) {
  if (v == CX9_MASTER)                 /* master 405-408: strlen, saturate */
    return true_len > INT32_MAX ? 0 : (int64_t)true_len;
  if (true_len > INT32_MAX) *ub = 1;   /* pre 422-431: int len; len++ */
  return cx9_wrap32((int64_t)(true_len & 0xFFFFFFFFu));
}

/* memcpy(&ret[off], src, n) and ret[term] = 0 against an allocation */
static int write_ok(uint64_t cap, int64_t off, uint64_t n, int64_t *bad) {
  if (off < 0) { *bad = off; return 0; }
  if ((uint64_t)off > cap || n > cap - (uint64_t)off) { *bad = off; return 0; }
  return 1;
}

static cx9_result strncat_model(cx9_version v, const cx9_input *in) {
  cx9_result r = {0};
  int64_t len = in->len, size, term;
  uint64_t req;
  r.eff_len = len;
  if (len == 0) {                          /* 452-453 */
    r.label = CX9_OK;
    r.result_len = in->l1;
    return r;
  }
  if (len < 0) {                           /* 454-455; master 430-434 */
    r.label = CX9_NULL_RET;
    return r;
  }
  size = cx9_xmlstrlen(v, in->l1, &r.ub);  /* 459 */
  r.size = size;
  if (v == CX9_POST && size < 0) {         /* 8fbbf55: if (size < 0) */
    r.label = CX9_NULL_RET;
    return r;
  }
  if (v == CX9_MASTER && (size < 0 || size > INT32_MAX - len)) { /* 440 */
    r.label = CX9_NULL_RET;
    return r;
  }
  if (v == CX9_MASTER)
    req = (uint64_t)size + (uint64_t)len + 1;            /* 444 */
  else
    req = to_size_t(int_add(int_add(size, len, &r.ub), 1, &r.ub)); /* 460 */
  r.request = req;
  if (req == 0) { r.label = CX9_ALLOC_ZERO; return r; }
  if (req > CX9_HEAP_MAX) {                /* xmlRealloc fails */
    r.label = v == CX9_MASTER ? CX9_NULL_RET : CX9_CUR_RET; /* 461-464 */
    return r;
  }
  if (!write_ok(req, size, (uint64_t)len, &r.oob_offset)) { /* 465 */
    r.label = CX9_OOB_WRITE;
    return r;
  }
  term = v == CX9_MASTER ? size + len : int_add(size, len, &r.ub); /* 466 */
  if (!write_ok(req, term, 1, &r.oob_offset)) {
    r.label = CX9_OOB_WRITE;
    return r;
  }
  r.result_len = (uint64_t)term;
  r.label = r.result_len == in->l1 + (uint64_t)len ? CX9_OK : CX9_LOSSY;
  return r;
}

static cx9_result strncatnew_model(cx9_version v, const cx9_input *in) {
  cx9_result r = {0};
  int64_t len = in->len, size, term;
  uint64_t req, want;
  if (len < 0) {                                       /* 487-488 */
    len = cx9_xmlstrlen(v, in->l2, &r.ub);
    if (v != CX9_PRE && len < 0) {                     /* 8fbbf55 */
      r.eff_len = len;
      r.label = CX9_NULL_RET;
      return r;
    }
  }
  r.eff_len = len;
  want = in->l1 + (in->len < 0 ? in->l2 : (uint64_t)in->len);
  if (len == 0) {                                      /* 489-490 */
    r.label = CX9_DUP_STR1;
    r.result_len = in->l1;  /* xmlStrdup itself is not modelled */
    return r;
  }
  size = cx9_xmlstrlen(v, in->l1, &r.ub);              /* 494 */
  r.size = size;
  if (v == CX9_POST && size < 0) {                     /* 8fbbf55: if (size < 0) */
    r.label = CX9_NULL_RET;
    return r;
  }
  if (v == CX9_MASTER && (size < 0 || size > INT32_MAX - len)) { /* 480 */
    r.label = CX9_NULL_RET;
    return r;
  }
  if (v == CX9_MASTER)
    req = (uint64_t)size + (uint64_t)len + 1;          /* 482 */
  else
    req = to_size_t(int_add(int_add(size, len, &r.ub), 1, &r.ub)); /* 495 */
  r.request = req;
  if (req == 0) { r.label = CX9_ALLOC_ZERO; return r; }
  if (req > CX9_HEAP_MAX) {
    if (v == CX9_MASTER) { r.label = CX9_NULL_RET; return r; }
    /* 496-498: xmlStrndup(str1, size): NULL if size < 0 */
    r.label = size < 0 ? CX9_NULL_RET : CX9_NDUP_FALLBACK;
    r.result_len = size < 0 ? 0 : (uint64_t)size;
    return r;
  }
  if (!write_ok(req, 0, to_size_t(size), &r.oob_offset)) {   /* 500 */
    r.label = CX9_OOB_WRITE;
    return r;
  }
  if (!write_ok(req, size, to_size_t(len), &r.oob_offset)) { /* 501 */
    r.label = CX9_OOB_WRITE;
    return r;
  }
  term = v == CX9_MASTER ? size + len : int_add(size, len, &r.ub); /* 502 */
  if (!write_ok(req, term, 1, &r.oob_offset)) {
    r.label = CX9_OOB_WRITE;
    return r;
  }
  r.result_len = (uint64_t)term;
  r.label = r.result_len == want ? CX9_OK : CX9_LOSSY;
  return r;
}

int cx9_valid_input(cx9_version v, const cx9_input *in) {
  if (!in) return 0;
  if ((int)v < (int)CX9_PRE || (int)v > (int)CX9_MASTER) return 0;
  if (in->func != CX9_STRNCAT && in->func != CX9_STRNCATNEW) return 0;
  if (in->l1 > CX9_LEN_MAX || in->l2 > CX9_LEN_MAX) return 0;
  if (in->len < INT32_MIN || in->len > INT32_MAX) return 0;
  if (in->len > 0 && (uint64_t)in->len > in->l2) return 0;
  return 1;
}

cx9_result cx9_call(cx9_version v, const cx9_input *in) {
  if (!cx9_valid_input(v, in)) {
    cx9_result r = {0};
    r.label = CX9_BAD_INPUT;
    return r;
  }
  return in->func == CX9_STRNCAT ? strncat_model(v, in)
                                 : strncatnew_model(v, in);
}

cx9_result cx9_call_c11(cx9_version v, const cx9_input *in) {
  cx9_result r = cx9_call(v, in);
  if (r.ub) {
    r.label = CX9_UB_STOP;
    r.result_len = 0;
    r.oob_offset = 0;
  }
  return r;
}

#include "sweep.h"
#include <stddef.h>

const uint64_t cx9_lengths[CX9_NLEN] = {
    0, 1, 100, 2147483636u, 2147483637u, 2147483647u, 2147483648u,
    2147483658u, 4294967295u, 4294967296ull, 4294967301ull,
    6442450954ull, 8589934592ull};
static const int64_t kLens[] = {-1, 0, 1, 10, 2147483637, 2147483647};

void cx9_run_sweep(cx9_version v, cx9_func f, cx9_sweep *out) {
  size_t a, b;
  *out = (cx9_sweep){{0}, 0, 0, 0};
  for (a = 0; a < CX9_NLEN; ++a) {
    if (f == CX9_STRNCAT) {
      for (b = 0; b < sizeof kLens / sizeof kLens[0]; ++b) {
        /* add is exactly len bytes long (len > 0), else empty */
        cx9_input in = {f, cx9_lengths[a], kLens[b] > 0 ? (uint64_t)kLens[b] : 0,
                        kLens[b]};
        cx9_result r = cx9_call(v, &in);
        out->cases++;
        out->count[r.label]++;
        out->ub += r.ub;
        if (r.label == CX9_OOB_WRITE && !r.ub) out->oob_without_ub++;
      }
    } else {
      for (b = 0; b < CX9_NLEN; ++b) {
        int k;
        for (k = 0; k < 2; ++k) {
          cx9_input in = {f, cx9_lengths[a], cx9_lengths[b], -1};
          cx9_result r;
          if (k == 1) {
            if (cx9_lengths[b] > INT32_MAX) continue; /* len must fit int */
            in.len = (int64_t)cx9_lengths[b];
          }
          r = cx9_call(v, &in);
          out->cases++;
          out->count[r.label]++;
          out->ub += r.ub;
          if (r.label == CX9_OOB_WRITE && !r.ub) out->oob_without_ub++;
        }
      }
    }
  }
}
