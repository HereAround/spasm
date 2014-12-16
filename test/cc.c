#include <stdio.h>
#include <assert.h>
#include "spasm.h"

int main(int argc, char **argv) {
  spasm_triplet *T;
  spasm *A, *B, *C;
  int n, m, test, i, j, px, k, nb;
  int *rr, *cc, *p, *x, *y, *q, *qinv, *Cp, *Cj;
  spasm_partition *P;

  assert(argc > 1);
  test = atoi(argv[1]);

  T = spasm_load_sms(stdin, 42013);
  A = spasm_compress(T);
  spasm_triplet_free(T);

  n = A->n;
  m = A->m;

  // generate random row & col permutation
  p = spasm_random_permutation(n);
  q = spasm_random_permutation(m);
  B = spasm_permute(A, p, q, SPASM_IGNORE_VALUES);

  free(p);
  free(q);
  spasm_csr_free(A);

  P = spasm_connected_components(B);
  p = P->p;
  q = P->q;
  rr = P->rr;
  cc = P->cc;
  nb = P->nr;

  /* verbosity */
  printf("p = ");
  for(i = 0; i < n; i++) {
    printf("%d ", p[i] + 1);
  }
  printf("\n rr = ");
  for(i = 0; i < nb + 1; i++) {
    printf("%d ", rr[i]);
  }
  printf("\n-----------------------\n");
  printf("q = ");
  for(j = 0; j < m; j++) {
    printf("%d ", q[j] + 1);
  }
  printf("\ncc = ");
  for(i = 0; i < nb + 1; i++) {
    printf("%d ", cc[i]);
  }
  printf("\n");

  /* --- check that p and q are actually permutations ---------------- */
  x = spasm_malloc(n * sizeof(int));
  y = spasm_malloc(m * sizeof(int));

  for(i = 0; i < n; i++) {
    x[i] = 0;
  }
  for(i = 0; i < n; i++) {
    x[ p[i] ]++;
  }
  for(i = 0; i < n; i++) {
    if (x[i] != 1) {
      printf("not ok %d - CC - p is not bijective\n", test);
      exit(0);
    }
  }

  for(j = 0; j < m; j++) {
    y[j] = 0;
  }
  for(j = 0; j < m; j++) {
    y[ q[j] ]++;
  }
  for(j = 0; j < m; j++) {
    if (y[j] != 1) {
      printf("not ok %d - CC - q is not bijective\n", test);
      exit(0);
    }
  }

  free(x);
  free(y);

  /* --- verbosity ---------------- */
  printf("# CC = %d\n", nb);

  for(k = 0; k < nb; k++) {
    printf("# CC_%d : ", k);
    for(i = rr[k]; i < rr[k + 1]; i++) {
      printf("%d ", p[i] + 1);
    }
    printf(" / ");
    for(j = cc[k]; j < cc[k + 1]; j++) {
      printf("%d ", q[j] + 1);
    }
    printf("\n");
  }

  /* --- check that decomposition is really block-upper-triangular ---------------- */
  qinv = spasm_pinv(q, m);
  C = spasm_permute(B, p, qinv, SPASM_IGNORE_VALUES);
  Cp = C->p;
  Cj = C->j;

  free(qinv);
  spasm_csr_free(B);

  for(k = 0; k < nb; k++) {
    for(i = rr[k]; i < rr[k + 1]; i++) {
      for(px = Cp[i]; px < Cp[i + 1]; px++) {
	j = Cj[px];
	if (j < cc[k] || cc[k + 1] < j) {
	  printf("not ok %d - CC - row %d (in C_%d) has entries on column %d\n", test, i + 1, k, j + 1);
	  exit(0);
	}
      }
    }
  }

  printf("ok %d - CC\n", test);

  spasm_partition_free(P);
  spasm_csr_free(C);
  return 0;
}
