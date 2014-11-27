#include <assert.h>
#include <stdbool.h>
#include "spasm.h"

void print_vec_(int *Aj, spasm_GFp * Ax, int p, int q, int n) {
    spasm_GFp x[n];
    int i, k;

    for (i = 0; i < n; i++) {
        x[i] = 0;
    }

    for (k = p; k < q; k++) {
        x[Aj[k]] = Ax[k];
    }

    printf("[ ");
    for (i = 0; i < n; i++) {
        printf("%d ", x[i]);
    }
    printf("]");
}

void print_vec(const spasm * A, int i) {
    print_vec_(A->j, A->x, A->p[i], A->p[i + 1], A->m);
}

/*
 * compute a (P)L(QU) decomposition.
 * 
 * L is always square of size n * n U has the same size as A
 * 
 * pinv[j] = i if the pivot on column j is on row i. -1 if no pivot (yet) found
 * on column j.
 * 
 */
spasm_lu *spasm_LU(const spasm * A) {
    spasm *L, *U;
    spasm_lu *N;
    spasm_GFp *Lx, *Ux, *x;
    int *Lp, *Lj, *Up, *Uj, *pinv, *xi;
    int n, m, ipiv, i, j, top, p, lnz, unz, prime;

    /* check inputs */
    assert(A != NULL);

    n = A->n;
    m = A->m;
    prime = A->prime;

    printf("[DEBUG LU] input matrix is %d x %d modulo %d\n", n, m, prime);

    /* educated guess of the size of L,U */
    lnz = 4 * spasm_nnz(A) + n;
    unz = 4 * spasm_nnz(A) + n;

    /* get GFp workspace */
    x = spasm_malloc(n * sizeof(spasm_GFp));

    /* get int workspace */
    xi = spasm_malloc(2 * n * sizeof(int));

    /* allocate result */
    N = spasm_malloc(sizeof(spasm_lu));

    /* allocate result L */
    N->L = L = spasm_csr_alloc(n, n, lnz, prime, true);

    /* allocate result U */
    N->U = U = spasm_csr_alloc(n, m, unz, prime, true);

    /* allocate result pinv */
    N->pinv = pinv = spasm_malloc(m * sizeof(int));

    Lp = L->p;
    Up = U->p;

    /* clear workspace */
    for (i = 0; i < n; i++) {
        x[i] = 0;
    }

    /* no rows pivotal yet */
    for (i = 0; i < m; i++) {
        pinv[i] = -1;
    }

    /* no rows of U yet */
    for (i = 0; i <= n; i++) {
        Up[i] = 0;
    }
    lnz = unz = 0;

    /* compute L[i] and U[i] */
    for (i = 0; i < n; i++) {
        printf("[DEBUG LU] i = %d. A[i] = ", i);
        print_vec(A, i);
        printf("\n");

        /* --- Triangular solve --------------------------------------------- */
        Lp[i] = lnz;            /* L[i] starts here */
        Up[i] = unz;            /* U[i] starts here */

        /* not enough room in L/U ? realloc twice the size */
        if (lnz + n > L->nzmax) {
            spasm_csr_realloc(L, 2 * L->nzmax + n);
        }
        if (unz + n > U->nzmax) {
            spasm_csr_realloc(U, 2 * U->nzmax + n);
        }
        Lj = L->j;
        Lx = L->x;
        Uj = U->j;
        Ux = U->x;
        //permutation des lignes ?

        /* Solve x * U = A[i] */
	for (p = 0; p < i; p++) {
            printf("U[%d] = ", p);
            print_vec(U, p);
            printf("\n");
        }
        top = spasm_sparse_forwardsolve(U, A, i, xi, x, pinv);

	/* check resolution */
	spasm_GFp y[n];
	for(p = 0; p < n; p++) {
	  y[p] = 0;
	}
	for(p = top; p < n; p++) {
	  j = xi[p];
	  if (pinv[j] == -1) {
	    y[j] = (y[j] + x[j]) % A->prime;
	  } else {
	    spasm_scatter(Uj, Ux, Up[ pinv[j] ], Up[pinv[j] + 1], x[j], y, A->prime);
	  }
	}
	    
        printf("[DEBUG LU] x = [");
        for (int r = 0; r < n; r++) {
            printf("%d ", x[r]);
        }
        printf("]\n");
        printf("[DEBUG LU] y = [");
        for (int r = 0; r < n; r++) {
            printf("%d ", y[r]);
        }
        printf("]\n");




        /*
         * --- Find pivot and dispatch coeffs into L and U
         * ------------------------
         */
        ipiv = m + 1;
        /* index of best pivot so far.*/
	/* attention, il faut que le pivot soit le premier de la ligne... */

	for (p = top; p < n; p++) {
            /* x[j] is (generically) nonzero */
            j = xi[p];

            /* if x[j] == 0 (numerical cancelation), we just ignore it */
            if (x[j] == 0) {
                continue;
            }

            if (pinv[j] < 0) {
                /* column j is not yet pivotal ? */

                /* have found the pivot on row i yet ? */
                if (j < ipiv) {
                    ipiv = j;
                }
            } else {
                /* column j is pivotal */
                /* x[j] is the entry L[i, pinv[j] ] */
                Lj[lnz] = pinv[j];
                Lx[lnz] = x[j];
                lnz++;
            }
        }

        /* pivot found */
        if (ipiv != m + 1) {

	  /* L[i,i] <--- 1 */
	  Lj[lnz] = i;
	  Lx[lnz] = 1;
	  lnz++;
	  pinv[ ipiv ] = i;

	  /* pivot must be the first entry in U[i] */
	  Uj[unz] = ipiv;
	  Ux[unz] = x[ ipiv ];
	  unz++;
        }

	/* send remaining non-pivot coefficients into U */
	for (p = top; p < n; p++) {
	  j = xi[p];

	  if (pinv[j] < 0) {
	    Uj[unz] = j;
	    Ux[unz] = x[j];
	    unz++;
	  }
	  /* clean x for the next iteration */
	  x[j] = 0;
	}

	printf("[DEBUG LU] pivot choisi colonne %d\n", ipiv);
        printf("-----------------------------------------\n");
    }

    /* --- Finalize L and U ------------------------------------------------- */
    Lp[n] = lnz;
    Up[n] = unz;

    /* remove extra space from L and U */
    spasm_csr_realloc(L, -1);
    spasm_csr_realloc(U, -1);
    free(x);
    free(xi);

    return N;
}

void spasm_free_LU(spasm_lu *X) {
  assert( X != NULL );
  spasm_csr_free(X->L);
  spasm_csr_free(X->U);
  free(X->pinv);
  free(X);
}