/* indent -nfbs -i4 -nip -npsl -di0 -nut .... */

#ifndef _SPASM_H
#define _SPASM_H

#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stddef.h>

/* --- primary SpaSM routines and data structures --- */

typedef int spasm_GFp;

typedef struct spasm_sparse {   /* matrix in compressed-column or triplet form */
    int nzmax;                  /* maximum number of entries */
    int m;                      /* number of rows */
    int n;                      /* number of columns */
    int *p;                     /* column pointers (size n+1) or col indices (size nzmax) */
    int *i;                     /* row indices, size nzmax */
    spasm_GFp *x;                     /* numerical values, size nzmax (optional) */
    int nz;                     /* # of entries in triplet matrix, -1 for compressed-col */
    int prime;
} spasm;

/* example (this is Matrix/t1)

    [ 4.5  0.0  3.2  0.0 ]
    [ 3.1  2.9  0.0  0.9 ]
A = [ 0.0  1.7  3.0  0.0 ]
    [ 3.5  0.4  0.0  1.0 ]

Triplet form (nz != -1) :

i = {   2,   1,   3,   0,   1,   3,   3,   1,   0,   2 }
p = {   2,   0,   3,   2,   1,   0,   1,   3,   0,   1 }
x = { 3.0, 3.1, 1.0, 3.2, 2.9, 3.5, 0.4, 0.9, 4.5, 1.7 }

the coefficients may appear in any order.

Compressed Column form :

p = {   0,             3,             6,        8,      10 }
i = {   0,   1,   3,   1,   2,   3,   0,   2,   1,   3 }
x = { 4.5, 3.1, 3.5, 2.9, 1.7, 0.4, 3.2, 3.0, 0.9, 1.0 }

In partiular, the actual number of nnz is p[n].

The numerical values are optional (useful for storing a sparse graph, or the pattern of a matrix).
*/

/* spasm_util.c */
int spasm_is_csc(const spasm *A);
int spasm_is_triplet(const spasm *A);
int spasm_nnz(const spasm *A);

void * spasm_malloc(size_t size);
void * spasm_calloc(size_t count, size_t size);
void * spasm_realloc(void *ptr, size_t size);

spasm *spasm_spalloc(int m, int n, int nzmax, int prime, int values, int triplet);
void spasm_sprealloc(spasm *A, int nzmax);
void spasm_spfree(spasm *A);

/* spasm_triplet.c */
void spasm_add_entry(spasm *T, int i, int j, spasm_GFp x);
spasm * spasm_compress(const spasm *T);

/* spasm_io.c */
spasm * spasm_load_ctf(FILE *f, int prime);
void spasm_save_ctf(FILE *f, const spasm *A);

/* spasm_permutation.c */
#define SPASM_IDENTITY_PERMUTATION NULL

void cs_pvec(const int *p, const spasm_GFp * b, spasm_GFp * x, int n);
void cs_ipvec(const int *p, const spasm_GFp * b, spasm_GFp * x, int n);
int *spasm_pinv(int const *p, int n);
spasm *spasm_permute(const spasm * A, const int *pinv, const int *q, int values);



static inline int spasm_max(int a, int b) {
  return (a > b) ? a : b;
}

static inline int spasm_min(int a, int b) {
  return (a < b) ? a : b;
}

#define CS_FLIP(i) (-(i)-2)
#define CS_UNFLIP(i) (((i) < 0) ? CS_FLIP(i) : (i))
#define CS_MARKED(w,j) (w [j] < 0)
#define CS_MARK(w,j) { w [j] = CS_FLIP (w [j]) ; }
#endif
