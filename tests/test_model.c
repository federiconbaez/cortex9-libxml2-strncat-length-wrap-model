/* Tests for the xmlStrncat / xmlStrncatNew length model. Run under
 * ASan + UBSan with -fno-sanitize-recover (scripts/build.sh test). */
#include "sweep.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checks, failures;
#define CHECK(c, what)                                                    \
  do {                                                                    \
    checks++;                                                             \
    if (!(c)) {                                                           \
      failures++;                                                         \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, what);               \
    }                                                                     \
  } while (0)

#define MAXCASES 64
static char ids[MAXCASES][64];
static cx9_input ins[MAXCASES];
static int ncases;

static void load_cases(const char *dir) {
  char path[512], line[512];
  FILE *f;
  snprintf(path, sizeof path, "%s/cases.tsv", dir);
  f = fopen(path, "r");
  CHECK(f != NULL, "open cases.tsv");
  if (!f) return;
  while (fgets(line, sizeof line, f) && ncases < MAXCASES) {
    char fn[16];
    unsigned long long a, b;
    long long c;
    if (line[0] == '#' || line[0] == '\n') continue;
    if (sscanf(line, "%63s %15s %llu %llu %lld", ids[ncases], fn, &a, &b, &c) != 5) {
      CHECK(0, "cases.tsv row");
      continue;
    }
    ins[ncases].func = strcmp(fn, "strncat") == 0 ? CX9_STRNCAT : CX9_STRNCATNEW;
    ins[ncases].l1 = a;
    ins[ncases].l2 = b;
    ins[ncases].len = c;
    ncases++;
  }
  fclose(f);
}

/* fixtures/expected.tsv comes from scripts/derive_expected.py, an
 * independent Python derivation from the libxml2 sources. */
static void test_fixtures(const char *dir) {
  char path[512], line[512];
  FILE *f;
  int rows = 0, i, v;
  load_cases(dir);
  snprintf(path, sizeof path, "%s/expected.tsv", dir);
  f = fopen(path, "r");
  CHECK(f != NULL, "open expected.tsv");
  if (!f) return;
  while (fgets(line, sizeof line, f)) {
    char id[64], ver[16], c11[32], w32[32];
    unsigned long long req, res;
    long long oob;
    int ub;
    if (strncmp(line, "id\t", 3) == 0) continue;
    if (sscanf(line, "%63s %15s %31s %31s %llu %llu %lld %d", id, ver, c11, w32,
               &req, &res, &oob, &ub) != 8) {
      CHECK(0, "expected.tsv row");
      continue;
    }
    for (i = 0; i < ncases; ++i) {
      if (strcmp(ids[i], id) != 0) continue;
      for (v = 0; v < 3; ++v) {
        cx9_result r, c;
        if (strcmp(ver, cx9_version_name((cx9_version)v)) != 0) continue;
        r = cx9_call((cx9_version)v, &ins[i]);
        c = cx9_call_c11((cx9_version)v, &ins[i]);
        CHECK(strcmp(cx9_label_name(r.label), w32) == 0, id);
        CHECK(strcmp(cx9_label_name(c.label), c11) == 0, id);
        CHECK(r.request == req, id);
        CHECK(r.result_len == res, id);
        CHECK(r.oob_offset == oob, id);
        CHECK(r.ub == ub, id);
        rows++;
      }
    }
  }
  fclose(f);
  CHECK(ncases == 12 && rows == 3 * ncases, "every fixture row checked");
}

/* Boundary grid: properties that must hold for every (l1, l2, len). */
static void test_invariants(void) {
  static const uint64_t L[] = {0, 1, 2, 11, 12, 100, 2147483636u, 2147483637u,
                               2147483646u, 2147483647u, 2147483648u, 2147483649u,
                               4294967295u, 4294967296ull, 4294967297ull,
                               6442450943ull, 8589934592ull};
  static const int64_t N[] = {INT32_MIN, -2, -1, 0, 1, 11, 12, 1000,
                              2147483636, 2147483637, 2147483646, 2147483647};
  size_t a, b, k;
  int v, f;
  long cases = 0, post_oob = 0, master_ub = 0, oob_no_ub = 0, c11_ok = 1;
  for (f = 0; f < 2; ++f)
    for (v = 0; v < 3; ++v)
      for (a = 0; a < sizeof L / sizeof L[0]; ++a)
        for (b = 0; b < sizeof L / sizeof L[0]; ++b)
          for (k = 0; k < sizeof N / sizeof N[0]; ++k) {
            cx9_input in;
            cx9_result r, c;
            in.func = (cx9_func)f;
            in.l1 = L[a];
            in.l2 = L[b];
            in.len = N[k];
            if (!cx9_valid_input((cx9_version)v, &in)) continue;
            r = cx9_call((cx9_version)v, &in);
            c = cx9_call_c11((cx9_version)v, &in);
            cases++;
            if (v != CX9_PRE && r.label == CX9_OOB_WRITE) post_oob++;
            if (v == CX9_MASTER && r.ub) master_ub++;
            if (r.label == CX9_OOB_WRITE && !r.ub) oob_no_ub++;
            if ((c.label == CX9_UB_STOP) != (r.ub != 0)) c11_ok = 0;
            if (!r.ub && c.label != r.label) c11_ok = 0;
          }
  CHECK(cases > 1000, "grid size");
  CHECK(post_oob == 0, "post and master: no OOB_WRITE even in the wrap variant");
  CHECK(master_ub == 0, "master: no signed overflow on any path");
  CHECK(oob_no_ub == 0, "every pre OOB_WRITE needs a signed overflow first");
  CHECK(c11_ok, "c11 label is UB_STOP exactly when ub");
}

static void test_adversarial(void) {
  cx9_input in = {CX9_STRNCAT, 10, 5, 5};
  cx9_result r;
  int ub = 0;
  CHECK(cx9_call(CX9_PRE, NULL).label == CX9_BAD_INPUT, "NULL input");
  CHECK(cx9_call_c11(CX9_PRE, NULL).label == CX9_BAD_INPUT, "NULL input c11");
  CHECK(cx9_call((cx9_version)3, &in).label == CX9_BAD_INPUT, "version 3");
  CHECK(cx9_call((cx9_version)-1, &in).label == CX9_BAD_INPUT, "version -1");
  in.func = (cx9_func)2;
  CHECK(cx9_call(CX9_PRE, &in).label == CX9_BAD_INPUT, "func 2");
  in.func = CX9_STRNCAT;
  in.len = (int64_t)INT32_MAX + 1;
  CHECK(cx9_call(CX9_PRE, &in).label == CX9_BAD_INPUT, "len > INT_MAX");
  in.len = (int64_t)INT32_MIN - 1;
  CHECK(cx9_call(CX9_PRE, &in).label == CX9_BAD_INPUT, "len < INT_MIN");
  in.len = INT64_MAX;
  CHECK(cx9_call(CX9_PRE, &in).label == CX9_BAD_INPUT, "len INT64_MAX");
  in.len = 6;
  CHECK(cx9_call(CX9_PRE, &in).label == CX9_BAD_INPUT, "len > l2 (contract)");
  in.len = 5;
  in.l1 = CX9_LEN_MAX + 1;
  CHECK(cx9_call(CX9_MASTER, &in).label == CX9_BAD_INPUT, "l1 too large");
  in.l1 = UINT64_MAX;
  CHECK(cx9_call(CX9_MASTER, &in).label == CX9_BAD_INPUT, "l1 UINT64_MAX");
  in.l1 = 10;
  in.l2 = UINT64_MAX;
  in.func = CX9_STRNCATNEW;
  in.len = -1;
  CHECK(cx9_call(CX9_PRE, &in).label == CX9_BAD_INPUT, "l2 UINT64_MAX");
  in.l2 = CX9_LEN_MAX;  /* largest accepted: runs, no sanitizer report */
  r = cx9_call(CX9_PRE, &in);
  CHECK(r.label != CX9_BAD_INPUT, "l2 == LEN_MAX accepted");
  r = cx9_call(CX9_MASTER, &in);
  CHECK(r.label == CX9_DUP_STR1 && !r.ub, "master saturates strlen to 0");
  CHECK(cx9_wrap32(INT64_MIN) == 0 && cx9_wrap32((int64_t)1 << 31) == INT32_MIN,
        "wrap32 extremes");
  CHECK(cx9_xmlstrlen(CX9_PRE, UINT64_MAX, &ub) == -1 && ub == 1, "pre strlen 2^64-1");
  CHECK(strcmp(cx9_label_name((cx9_label)99), "UNKNOWN") == 0, "label name range");
  CHECK(strcmp(cx9_label_name((cx9_label)-1), "UNKNOWN") == 0, "label name -1");
  CHECK(strcmp(cx9_version_name((cx9_version)9), "unknown") == 0, "version name range");
}

/* The checks behind PRACTICE.md, one per level. */
static void practice(void) {
  cx9_input in = {CX9_STRNCAT, 10, 5, 5};
  cx9_result r;
  r = cx9_call(CX9_PRE, &in);
  CHECK(r.label == CX9_OK && r.request == 16, "practice_level_1");
  in.l1 = 2147483648u; in.l2 = 10; in.len = 10;
  r = cx9_call(CX9_PRE, &in);
  CHECK(r.size == INT32_MIN && r.ub, "practice_level_2");
  in.l1 = 2147483658u; in.l2 = 2147483647; in.len = 2147483647;
  r = cx9_call(CX9_PRE, &in);
  CHECK(r.label == CX9_OOB_WRITE && r.request == 10 && r.oob_offset < 0, "practice_level_3");
  CHECK(cx9_call_c11(CX9_PRE, &in).label == CX9_UB_STOP, "practice_level_4");
  r = cx9_call(CX9_POST, &in);
  CHECK(r.label == CX9_NULL_RET, "practice_level_5");
  in.l1 = 2147483637u; in.l2 = 10; in.len = 10;
  r = cx9_call(CX9_POST, &in);
  CHECK(r.label == CX9_CUR_RET && r.ub && r.size == 2147483637, "practice_level_6");
  r = cx9_call(CX9_MASTER, &in);
  CHECK(r.label == CX9_OK && !r.ub && r.request == 2147483648u, "practice_level_7");
  in.l1 = 4294967301ull;
  r = cx9_call(CX9_MASTER, &in);
  CHECK(r.label == CX9_LOSSY && r.result_len == 10, "practice_level_8");
}

int main(int argc, char **argv) {
  test_fixtures(argc > 1 ? argv[1] : "fixtures");
  test_invariants();
  test_adversarial();
  practice();
  printf("%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
