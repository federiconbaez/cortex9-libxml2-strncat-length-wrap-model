#ifndef CX9_SWEEP_H
#define CX9_SWEEP_H
#include "xmlstr_model.h"
#define CX9_LABELS 10
#define CX9_NLEN 13
typedef struct {
  long count[CX9_LABELS];
  long cases, ub, oob_without_ub;
} cx9_sweep;
extern const uint64_t cx9_lengths[CX9_NLEN];
void cx9_run_sweep(cx9_version v, cx9_func f, cx9_sweep *out);
#endif
