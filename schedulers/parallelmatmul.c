#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <omp.h>
#include <math.h>

typedef double TYPE;
#define MAX_DIM 2000*2000
#define MAX_VAL 10
#define MIN_VAL 1

#define THREAD_COUNT 56  // you can change this value
#define DIMENSION 112  // Should be multiple of thread_count for easy math

// Method signatures
TYPE** randomSquareMatrix(int dimension);
TYPE** zeroSquareMatrix(int dimension);
void convert(TYPE** matrixA, TYPE** matrixB, int dimension);

// Matrix multiplication methods
double parallelMultiply(TYPE** matrixA, TYPE** matrixB, TYPE** matrixC, int dimension);
double optimizedParallelMultiply(TYPE** matrixA, TYPE** matrixB, TYPE** matrixC, int dimension);

// 1 Dimensional matrix on stack
TYPE flatA[MAX_DIM];
TYPE flatB[MAX_DIM];

// Dimension
TYPE** matrixA;
TYPE** matrixB;
TYPE** matrixResult;

#define NUM_ITER 10000
// core assignments
// int algo1[] = {3, 1, 0, 2, 2, 2, 1, 0, 2, 1, 0, 2, 1, 0, 2, 1, 0, 2, 1, 0, 2, 1, 0, 2, 1, 0, 2, 1, 0, 2, 1, 0, 2, 1, 0, 2, 1, 0, 2, 1, 0, 2, 1, 0, 2, 1, 0, 2, 1, 0, 2, 1, 0, 2, 1, 0, 1};
// int algo1_8[]= {7, 1, 0, 3, 2, 6, 5, 4, 2, 3, 1, 0, 6, 5, 4, 2, 3, 1, 0, 6, 5, 4, 2, 3, 1, 0, 6, 5, 4, 2, 3, 1, 6, 5, 4, 2, 3, 6, 5, 4, 2, 3, 6, 5, 4, 6, 5, 4, 6, 5, 4, 6, 5, 4, 6, 5, 4};
// int algo1_8_200[] = {7, 1, 0, 3, 2, 6, 5, 4, 2, 3, 1, 0, 6, 5, 4, 2, 3, 1, 0, 6, 5, 4, 2, 3, 1, 0, 6, 5, 4, 2, 3, 1, 6, 5, 4, 2, 3, 6, 5, 4, 2, 3, 6, 5, 4, 6, 5, 4, 6, 5, 4, 6, 5, 4, 6, 5, 4};
// int algo1_8_600[] = {7, 1, 0, 3, 2, 5, 4, 0, 2, 3, 1, 6, 5, 4, 0, 2, 3, 1, 6, 5, 4, 0, 2, 3, 1, 6, 5, 4, 0, 2, 3, 1, 6, 5, 4, 2, 3, 6, 5, 4, 2, 3, 6, 5, 4, 6, 5, 4, 6, 5, 4, 6, 5, 4, 6, 5, 4};

// int algo2_4[] = {3, 1, 0, 2, 2, 1, 0, 2, 2, 1, 1, 0, 0, 2, 2, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 0, 0, 1, 1, 1};
// int algo2_8[] = {7, 0, 1, 2, 3, 0, 1, 2, 3, 0, 0, 1, 1, 2, 2, 3, 3, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6};

// int algo3_8[] = {7, 5, 6, 3, 4, 5, 6, 3, 4, 5, 5, 6, 6, 3, 3, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 3, 3, 3, 3, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2};
// int algo3_4[] = {3, 0, 1, 2, 2, 0, 1, 2, 2, 0, 0, 1, 1, 2, 2, 2, 2, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2};

int base_4[] = {3, 1, 0, 2, 2, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 1, 2, 1, 2, 2};
int base_8[] = {7, 1, 0, 3, 2, 3, 0, 1, 6, 5, 4, 2, 3, 1, 6, 5, 4, 2, 3, 1, 6, 5, 4, 2, 3, 6, 5, 4, 2, 3, 6, 5, 4, 2, 3, 6, 5, 4, 2, 3, 6, 5, 4, 2, 3, 6, 5, 4, 6, 5, 4, 6, 5, 4, 6, 5, 4};

// Core assignment
int assign_thread_to_core(int core_id) {
   cpu_set_t cpuset;
   CPU_ZERO(&cpuset);
   CPU_SET(core_id, &cpuset);

   pthread_t current_thread = pthread_self();    
   return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
}

void *matrixParallel(void *threadid){
	/*
		Parallel multiply given input matrices and return resultant matrix
	*/

	long id = (long)threadid;

	assign_thread_to_core(base_4[id]);

	int partition = DIMENSION / THREAD_COUNT;
	int start = partition * id;
	int end = partition * (id + 1) - 1;

	for (int i = start; i < end; i++){
		for(int j=0; j < DIMENSION; j++){
			for(int k=0; k < DIMENSION; k++){
				matrixResult[i][j] += matrixA[i][k] * matrixB[k][j];
			}
		}
	}

	return 0;
}

int main(){

    int dimension = DIMENSION;

    double opmLatency;
	matrixA = randomSquareMatrix(dimension);
	matrixB = randomSquareMatrix(dimension);
    matrixResult = zeroSquareMatrix(dimension);

	long i;

	struct timeval t0, t1;
	gettimeofday(&t0, 0);

	for (int j=0; j < NUM_ITER; j++) {
	
	printf("Iteration: %d\n", j);

	pthread_t threads[THREAD_COUNT];  // thread data structure

	// spawn thread to run matrixParallel
	for(i = 0; i < THREAD_COUNT; i++ ) {
		int tid = reorder[i];
		pthread_create(&threads[tid], NULL, matrixParallel, (void *)tid);
	}

	// wait for children threads to exit
	void *status;
	for (i=0; i < THREAD_COUNT; i++ ) {
		int tid = reorder[i];
		pthread_join(threads[tid], &status);
	}


	// opmLatency = parallelMultiply(matrixA, matrixB, matrixResult, dimension);

	}
	gettimeofday(&t1, 0);
	double elapsed = (t1.tv_sec-t0.tv_sec) * 1.0f + (t1.tv_usec - t0.tv_usec) / 1000000.0f;

	printf("Latency: %lf\n", elapsed);

	free(matrixResult);
	free(matrixA);
	free(matrixB);

	return 0;
}

TYPE** randomSquareMatrix(int dimension){
	/*
		Generate 2 dimensional random TYPE matrix.
	*/

	TYPE** matrix = malloc(dimension * sizeof(TYPE*));

	for(int i=0; i<dimension; i++){
		matrix[i] = malloc(dimension * sizeof(TYPE));
	}

	//Random seed
	srandom(time(0)+clock()+random());

	#pragma omp parallel for
	for(int i=0; i<dimension; i++){
		for(int j=0; j<dimension; j++){
			matrix[i][j] = rand() % MAX_VAL + MIN_VAL;
		}
	}

	return matrix;
}

TYPE** zeroSquareMatrix(int dimension){
	/*
		Generate 2 dimensional zero TYPE matrix.
	*/

	TYPE** matrix = malloc(dimension * sizeof(TYPE*));

	for(int i=0; i<dimension; i++){
		matrix[i] = malloc(dimension * sizeof(TYPE));
	}

	//Random seed
	srandom(time(0)+clock()+random());
	for(int i=0; i<dimension; i++){
		for(int j=0; j<dimension; j++){
			matrix[i][j] = 0;
		}
	}

	return matrix;
}
