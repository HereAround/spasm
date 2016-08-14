#include <sys/time.h>
#include <err.h>
#include "spasm.h"

#ifdef _OPENMP
int spasm_get_num_threads()
{
	return omp_get_num_threads();
}

int spasm_get_max_threads()
{
	return omp_get_max_threads();
}


int spasm_get_thread_num()
{
	return omp_get_thread_num();
}
#else
int spasm_get_num_threads()
{
	return 1;
}

int spasm_get_max_threads()
{
	return 1;	
}

int spasm_get_thread_num()
{
	return 0;
}
#endif

#ifdef HAVE_NUMA
#include <numa.h>
int spasm_numa_num_nodes() {
	return numa_num_configured_nodes();
}

int spasm_numa_get_node() {
	int result = -1;
	struct bitmask *bm = numa_get_run_node_mask();
	int nodes = numa_num_configured_nodes();
	if (numa_bitmask_weight(bm) != 1) {
		/*fprintf(stderr, "[numa] WARNING, CPUs available to thread %d: ", spasm_get_thread_num());
		for (int j = 0; j < nodes; j++)
			if (numa_bitmask_isbitset(bm, j))
				fprintf(stderr, "%d ", j);
		fprintf(stderr, "\n");*/
		result = -1;
	} else {
		for (int j = 0; j < nodes; j++)
			if (numa_bitmask_isbitset(bm, j))
				result = j;
		
	}
	numa_bitmask_free(bm);
	return result;
}

void spasm_numa_node_leaders(int *leaders) {
	int nodes = spasm_numa_num_nodes();
	for(int i = 0; i < nodes; i++)
		leaders[i] = -1;
	#pragma omp parallel
	{
		int tid = spasm_get_thread_num();
		int node = spasm_numa_get_node();
		if (node >= 0) {
			#pragma omp critical
			if (leaders[node] < 0)
				leaders[node] = tid;
			else
				leaders[node] = spasm_min(leaders[node], tid);
		}
	}
	int OK = 0;
	for(int i = 0; i < nodes; i++)
		OK |= (leaders[i] >= 0);
	if (!OK)
		fprintf(stderr, "[numa] Unable to determine NUMA node leaders. Make sure that OMP_PROC_BIND=true\n");
}


void spasm_numa_extra_verbose()
{
	struct bitmask *bm;

	if (numa_available() < 0)
		errx(1, "The system does not support the NUMA API.\n");
	
	int nodes = numa_num_configured_nodes();
	int cpus = numa_num_configured_cpus();


	fprintf(stderr, "[numa] %d-CPU, %d-node machine\n", cpus, nodes);
	bm = numa_allocate_cpumask();
	for (int i = 0; i < nodes; i++) {
		numa_node_to_cpus(i, bm);
		fprintf(stderr, "[numa] CPUs in node %d: ", i);
		for (int j = 0; j < cpus; j++)
			if (numa_bitmask_isbitset(bm, j))
				fprintf(stderr, "%d ", j);
		fprintf(stderr, "\n");
	}
	numa_bitmask_free(bm);

	for (int i = 0; i < nodes; i++)
		for (int j = i + 1; j < nodes; j++)
			fprintf(stderr, "[numa] distance %d <--> %d: %d\n", i, j, numa_distance(i, j));

	fprintf(stderr, "[numa] preferred node: %d\n", numa_preferred());
	fprintf(stderr, "[numa] CPUs available: ");
	for (int i = 0; i < cpus; i++)
		if (numa_bitmask_isbitset(numa_all_cpus_ptr , i))
			fprintf(stderr, "%d ", i);
	fprintf(stderr, "\n");

	int pagesize = numa_pagesize();
	fprintf(stderr, "[numa] page size: %d bytes\n", pagesize);

	bm = numa_get_interleave_mask();
	if (numa_bitmask_weight(bm)) {
		fprintf(stderr, "[numa] pages interleaved on nodes: ");
		for (int i = 0; i < nodes; i++)
			if (numa_bitmask_isbitset(bm, i))
				fprintf(stderr, "%d ", i);
		fprintf(stderr, "\n");
	} else {
		fprintf(stderr, "[numa] page interleaving disabled\n");		
	}
	numa_bitmask_free(bm);

	/* perform a test */
	int interleaving = 0;
	int non_local = 0;
	int non_bound = 0;
	#pragma omp parallel
	{
		int node = spasm_numa_get_node();
		if (node < 0)
			#pragma omp atomic write
			non_bound = 1;

		int *test = malloc(13000);
		int *aligned = test;
		for(int i = 0; i < 13000/4; i++) {
			test[i] = i;
		}
		while (((uint64_t) aligned) % pagesize)
			aligned++;	
	 	int status[2];
	 	int ret;
	 	ret = numa_move_pages(0, 2, &aligned, NULL, status, 0);
	 	if (ret != 0)
	 		err(1, "numa_move_pages: ");
	 	if (status[0] != status[1]) {
	 		#pragma omp atomic write
	 		interleaving = 1;
	 	} else if (node >= 0 && status[0] != node) {
	 		#pragma omp atomic write
	 		non_local = 1;
	 	}
		free(test);
	}
	if (interleaving)
		fprintf(stderr, "[numa] malloc is interleaving pages\n");
	if (non_local)
		fprintf(stderr, "[numa] malloc is allocating pages non-locally\n");
	if (non_bound)
		fprintf(stderr, "[numa] unable to perform locality test\n");
}

void spasm_numa_info() 
{
	struct bitmask *bm;

	if (numa_available() < 0)
		errx(1, "The system does not support the NUMA API.\n");
	
	int nodes = numa_num_configured_nodes();
	int cpus = numa_num_configured_cpus();
	fprintf(stderr, "[numa] %d-node, %d-CPU machine\n", nodes, cpus);
	fprintf(stderr, "[numa] running %d thread\n", spasm_get_max_threads());

	int *threads_on_node = calloc(sizeof(int), nodes);
	#pragma omp parallel
	{
		int i = spasm_get_thread_num();
		int j = spasm_numa_get_node();
		if (j >= 0)
			#pragma omp atomic
			threads_on_node[j]++;
	}	
	for(int i=0; i < nodes; i++)
		fprintf(stderr, "[numa] %d threads running on node %d\n", threads_on_node[i], i);
	free(threads_on_node);
}
#else
int spasm_numa_num_nodes() {
	return 1;
}

int spasm_numa_get_node() {
	return 0;
}

void spasm_numa_node_leaders(int *leaders) {
	leaders[0] = 0;
}

void spasm_numa_info() 
{
}

void spasm_numa_extra_verbose()
{
}
#endif

double spasm_wtime() {
	struct timeval ts;
	gettimeofday(&ts, NULL);
	return (double)ts.tv_sec + ts.tv_usec / 1E6;
}


int spasm_nnz(const spasm * A) {
	return A->p[A->n];
}

/* return a string representing n in 4 bytes */
void spasm_human_format(int64_t n, char *target) {
	if (n < 1000) {
		sprintf(target, "%lld", n);
		return;
	}
	if (n < 1000000) {
		sprintf(target, "%.1fk", n / 1e3);
		return;
	}
	if (n < 1000000000) {
		sprintf(target, "%.1fm", n / 1e6);
		return;
	}
	if (n < 1000000000000ll) {
		sprintf(target, "%.1fg", n / 1e9);
		return;
	}
	if (n < 1000000000000000ll) {
		sprintf(target, "%.1ft", n / 1e12);
		return;
	}
}

void *spasm_malloc(size_t size) {
	void *x = malloc(size);
	if (x == NULL)
		err(1, "malloc failed");
	return x;
}

void *spasm_calloc(size_t count, size_t size) {
	void *x = calloc(count, size);
	if (x == NULL)
		err(1, "calloc failed");
	return x;
}

void *spasm_realloc(void *ptr, size_t size) {
	void *x = realloc(ptr, size);
	if (ptr != NULL && x == NULL && size != 0)
		err(1, "realloc failed");
	return x;
}

/* allocate a sparse matrix (compressed-row form) */
spasm *spasm_csr_alloc(int n, int m, int nzmax, int prime, int with_values) {
	spasm *A;

	if (prime > 46337) {
		prime = 46337;
		fprintf(stderr, "WARNING: modulus has been set to 46337.\n");
	}
	A = spasm_malloc(sizeof(spasm));	/* allocate the cs struct */
	A->m = m;		/* define dimensions and nzmax */
	A->n = n;
	A->nzmax = nzmax;
	A->prime = prime;
	A->p = spasm_malloc((n + 1) * sizeof(int));
	A->j = spasm_malloc(nzmax * sizeof(int));
	A->x = with_values ? spasm_malloc(nzmax * sizeof(spasm_GFp)) : NULL;
	return A;

}

/* allocate a sparse matrix (triplet form) */
spasm_triplet *spasm_triplet_alloc(int n, int m, int nzmax, int prime, int with_values) {
	spasm_triplet *A;

	A = spasm_malloc(sizeof(spasm_triplet));
	A->m = m;
	A->n = n;
	A->nzmax = nzmax;
	A->prime = prime;
	A->nz = 0;
	A->i = spasm_malloc(nzmax * sizeof(int));
	A->j = spasm_malloc(nzmax * sizeof(int));
	A->x = (with_values ? spasm_malloc(nzmax * sizeof(spasm_GFp)) : NULL);
	return A;
}

/*
 * change the max # of entries in a sparse matrix. If nzmax < 0, then the
 * matrix is trimmed to its current nnz.
 */
void spasm_csr_realloc(spasm * A, int nzmax) {
	if (nzmax < 0)
		nzmax = spasm_nnz(A);
	A->j = spasm_realloc(A->j, nzmax * sizeof(int));
	if (A->x != NULL)
		A->x = spasm_realloc(A->x, nzmax * sizeof(spasm_GFp));
	A->nzmax = nzmax;
}

/*
 * change the max # of entries in a sparse matrix. If nzmax < 0, then the
 * matrix is trimmed to its current nnz.
 */
void spasm_triplet_realloc(spasm_triplet * A, int nzmax) {
	if (nzmax < 0)
		nzmax = A->nz;
	A->i = spasm_realloc(A->i, nzmax * sizeof(int));
	A->j = spasm_realloc(A->j, nzmax * sizeof(int));
	if (A->x != NULL)
		A->x = spasm_realloc(A->x, nzmax * sizeof(spasm_GFp));
	A->nzmax = nzmax;
}

/* free a sparse matrix */
void spasm_csr_free(spasm * A) {
	if (A == NULL)
		return;
	free(A->p);
	free(A->j);
	free(A->x);		/* trick : free does nothing on NULL pointer */
	free(A);
}

void spasm_triplet_free(spasm_triplet * A) {
	free(A->i);
	free(A->j);
	free(A->x);		/* trick : free does nothing on NULL pointer */
	free(A);
}

void spasm_csr_resize(spasm * A, int n, int m) {
	A->m = m;
	/* in case of a column shrink, check that no entries are left outside */
	A->p = spasm_realloc(A->p, (n + 1) * sizeof(int));

	if (A->n < n) {
		int *Ap = A->p;
		for (int i = A->n; i < n + 1; i++)
			Ap[i] = Ap[A->n];
	}
	A->n = n;
}


spasm_partition *spasm_partition_alloc(int n, int m, int nr, int nc) {
	spasm_partition *P;

	P = spasm_malloc(sizeof(spasm_partition));
	P->p = spasm_malloc(n * sizeof(int));
	P->q = spasm_malloc(m * sizeof(int));
	P->rr = spasm_malloc((nr + 1) * sizeof(int));
	P->cc = spasm_malloc((nc + 1) * sizeof(int));
	P->nr = 0;
	P->nr = 0;
	return P;
}

void spasm_partition_tighten(spasm_partition * P) {
	P->rr = spasm_realloc(P->rr, (P->nr + 1) * sizeof(int));
	P->cc = spasm_realloc(P->cc, (P->nc + 1) * sizeof(int));
}


void spasm_partition_free(spasm_partition * P) {
	if (P == NULL)
		return;
	free(P->p);
	free(P->q);
	free(P->rr);
	free(P->cc);
	free(P);
}

void spasm_cc_free(spasm_cc * C) {
	if (C == NULL)
		return;
	spasm_partition_free(C->CC);
	spasm_partition_free(C->SCC);
	free(C);
}

void spasm_dm_free(spasm_dm * x) {
	if (x == NULL)
		return;
	spasm_partition_free(x->DM);
	spasm_cc_free(x->H);
	spasm_cc_free(x->S);
	spasm_cc_free(x->V);
	free(x);
}

void spasm_vector_zero(spasm_GFp * x, int n) {
	for (int i = 0; i < n; i++)
		x[i] = 0;
}

void spasm_vector_set(spasm_GFp * x, int a, int b, spasm_GFp alpha) {
	for (int i = a; i < b; i++)
		x[i] = alpha;
}
