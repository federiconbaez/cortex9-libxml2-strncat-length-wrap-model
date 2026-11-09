/* Arithmetic model of libxml2 xmlStrncat / xmlStrncatNew (xmlstring.c) at
 * 8f30bdf (pre), 8fbbf55 (fix for CVE-2016-1834) and febeb5c (master).
 * Strings are lengths and allocations are capacities: no string of 2 GiB
 * is built and no byte is copied. */
#ifndef CX9_XMLSTR_MODEL_H
#define CX9_XMLSTR_MODEL_H
#include <stdint.h>

#define CX9_HEAP_MAX ((uint64_t)1 << 47) /* model: larger requests fail */

typedef enum { CX9_PRE = 0, CX9_POST = 1, CX9_MASTER = 2 } cx9_version;
typedef enum { CX9_STRNCAT = 0, CX9_STRNCATNEW = 1 } cx9_func;

typedef enum {
  CX9_OK = 0,        /* result holds every byte of both inputs */
  CX9_LOSSY,         /* memory-safe, but the result is shorter */
  CX9_NULL_RET,      /* returned NULL (API error path) */
  CX9_CUR_RET,       /* allocation failed, cur returned unchanged */
  CX9_DUP_STR1,      /* xmlStrdup(str1) path (len == 0 after xmlStrlen) */
  CX9_NDUP_FALLBACK, /* malloc failed, xmlStrndup(str1, size) path */
  CX9_OOB_WRITE,     /* a memcpy or the terminator leaves the allocation */
  CX9_ALLOC_ZERO,    /* request of 0 bytes: implementation-defined, stop */
  CX9_BAD_INPUT,     /* outside the model's API domain: nothing computed */
  CX9_UB_STOP        /* C11 semantics: signed int overflow, no outcome */
} cx9_label;

/* Input domain: l1, l2 <= CX9_LEN_MAX; len in [INT32_MIN, INT32_MAX];
 * len > 0 implies len <= l2 (the caller's contract); known version/func. */
#define CX9_LEN_MAX CX9_HEAP_MAX

typedef struct {
  cx9_func func;
  uint64_t l1;  /* true length of cur / str1 (non-NULL) */
  uint64_t l2;  /* true length of add / str2 (non-NULL) */
  int64_t len;  /* the int len argument, in [INT32_MIN, INT32_MAX] */
} cx9_input;

typedef struct {
  cx9_label label;
  int64_t size;         /* xmlStrlen(cur/str1) as returned (int) */
  int64_t eff_len;      /* len after the len < 0 branch (StrncatNew) */
  uint64_t request;     /* bytes passed to xmlRealloc / xmlMalloc */
  uint64_t result_len;  /* length of the returned string, when defined */
  int64_t oob_offset;   /* first offending offset (may be negative) */
  int ub;               /* signed int overflow on the path (UB in C) */
} cx9_result;

int64_t cx9_wrap32(int64_t v);
int64_t cx9_xmlstrlen(cx9_version v, uint64_t true_len, int *ub);
/* WRAP32 VARIANT. Signed int overflow is undefined behaviour in C; this
 * variant continues with the two's-complement wrap an unoptimised x86-64
 * build typically shows, and sets ub = 1 whenever it does so. It is a
 * model choice, not a statement about what a compiler emits. */
cx9_result cx9_call(cx9_version v, const cx9_input *in);
/* C11 SEMANTICS. Same as cx9_call, but any path with signed overflow is
 * labelled CX9_UB_STOP: the standard gives that execution no meaning. */
cx9_result cx9_call_c11(cx9_version v, const cx9_input *in);
int cx9_valid_input(cx9_version v, const cx9_input *in);
const char *cx9_label_name(cx9_label l);
const char *cx9_version_name(cx9_version v);
#endif
