#include "sweep.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Unsigned decimal, digits only (strtoull would accept "-1" and spaces). */
static int parse_u64(const char *s, uint64_t max, uint64_t *out) {
  char *e;
  unsigned long long v;
  if (!s || !(*s >= '0' && *s <= '9')) return -1;
  errno = 0;
  v = strtoull(s, &e, 10);
  if (errno || *e || v > max) return -1;
  *out = (uint64_t)v;
  return 0;
}

static int parse_i32(const char *s, int64_t *out) {
  char *e;
  long long v;
  if (!s || !(*s == '-' || (*s >= '0' && *s <= '9'))) return -1;
  errno = 0;
  v = strtoll(s, &e, 10);
  if (errno || *e || v < INT32_MIN || v > INT32_MAX) return -1;
  *out = (int64_t)v;
  return 0;
}

static int parse(const char *fn, const char *a, const char *b, const char *c,
                 cx9_input *in) {
  if (strcmp(fn, "strncat") == 0) in->func = CX9_STRNCAT;
  else if (strcmp(fn, "strncatnew") == 0) in->func = CX9_STRNCATNEW;
  else return -1;
  if (parse_u64(a, CX9_LEN_MAX, &in->l1) || parse_u64(b, CX9_LEN_MAX, &in->l2) ||
      parse_i32(c, &in->len))
    return -1;
  return cx9_valid_input(CX9_PRE, in) ? 0 : -1; /* API contract */
}

static int parse_version(const char *s) {
  if (strcmp(s, "pre") == 0) return CX9_PRE;
  if (strcmp(s, "post") == 0) return CX9_POST;
  if (strcmp(s, "master") == 0) return CX9_MASTER;
  return -1;
}

/* label_wrap32 is the explicit wrap variant; label_c11 is UB_STOP whenever
 * that path needed a signed int overflow. */
static void row(FILE *o, const char *id, cx9_version v, const cx9_input *in) {
  cx9_result r = cx9_call(v, in), c = cx9_call_c11(v, in);
  fprintf(o, "%s,%s,%s,%s,%" PRId64 ",%" PRId64 ",%" PRIu64 ",%" PRIu64 ",%" PRId64 ",%d\n",
          id, cx9_version_name(v), cx9_label_name(c.label), cx9_label_name(r.label),
          r.size, r.eff_len, r.request, r.result_len, r.oob_offset, r.ub);
}
static const char *kHead = "id,version,label_c11,label_wrap32,size_wrap32,eff_len,"
                           "request_wrap32,result_len_wrap32,oob_offset_wrap32,ub\n";

static int results(const char *cases, const char *out) {
  char path[512], line[512];
  FILE *in = fopen(cases, "r"), *o;
  int v, f, l;
  if (!in) return 1;
  snprintf(path, sizeof path, "%s/cases.csv", out);
  if (!(o = fopen(path, "w"))) { fclose(in); return 1; }
  fputs(kHead, o);
  while (fgets(line, sizeof line, in)) {
    char id[64], fn[16], a[32], b[32], c[32];
    cx9_input ci;
    if (line[0] == '#' || line[0] == '\n') continue;
    if (sscanf(line, "%63s %15s %31s %31s %31s", id, fn, a, b, c) != 5) {
      fprintf(stderr, "malformed line\n"); fclose(in); fclose(o); return 1;
    }
    if (parse(fn, a, b, c, &ci)) { fprintf(stderr, "bad case %s\n", id); fclose(in); fclose(o); return 1; }
    for (v = 0; v < 3; ++v) row(o, id, (cx9_version)v, &ci);
  }
  fclose(in);
  fclose(o);
  snprintf(path, sizeof path, "%s/sweep.csv", out);
  if (!(o = fopen(path, "w"))) return 1;
  /* wrap32 variant counts; c11_ub_stop = cases C11 leaves undefined */
  fprintf(o, "function,version,cases");
  for (l = 0; l <= CX9_ALLOC_ZERO; ++l) fprintf(o, ",%s", cx9_label_name((cx9_label)l));
  fprintf(o, ",c11_ub_stop,oob_without_ub\n");
  for (f = 0; f < 2; ++f)
    for (v = 0; v < 3; ++v) {
      cx9_sweep s;
      cx9_run_sweep((cx9_version)v, (cx9_func)f, &s);
      fprintf(o, "%s,%s,%ld", f ? "xmlStrncatNew" : "xmlStrncat",
              cx9_version_name((cx9_version)v), s.cases);
      for (l = 0; l <= CX9_ALLOC_ZERO; ++l) fprintf(o, ",%ld", s.count[l]);
      fprintf(o, ",%ld,%ld\n", s.ub, s.oob_without_ub);
    }
  fclose(o);
  return 0;
}

int main(int argc, char **argv) {
  if (argc == 7 && strcmp(argv[1], "case") == 0) {
    cx9_input in;
    int v = parse_version(argv[2]);
    if (v < 0 || parse(argv[3], argv[4], argv[5], argv[6], &in)) { fprintf(stderr, "bad arguments\n"); return 2; }
    fputs(kHead, stdout);
    row(stdout, "cli", (cx9_version)v, &in);
    return 0;
  }
  if (argc == 4 && strcmp(argv[1], "results") == 0) return results(argv[2], argv[3]);
  fprintf(stderr,
          "usage: xmlstr case pre|post|master strncat|strncatnew LEN1 LEN2 LEN_ARG\n"
          "       xmlstr results fixtures/cases.tsv OUTDIR\n"
          "Lengths only: no string is built and nothing is copied.\n");
  return 2;
}
