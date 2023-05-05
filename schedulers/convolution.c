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

#define NUM_ITER 20

// int algo1_4[] = {3, 1, 0, 1, 0, 1, 1, 1, 2, 2, 0, 0, 2, 2, 2, 2, 0, 0, 0, 2, 2, 1, 0, 1, 2, 0, 2, 2, 0, 0, 0, 0, 2, 1, 1, 1, 0, 1, 1, 2, 2, 2, 1, 1, 1, 0, 0, 1, 2, 2, 1, 0, 2, 0, 0, 1, 1};
// int algo1_8[] = {7, 6, 5, 4, 5, 4, 1, 6, 6, 0, 0, 3, 3, 2, 4, 1, 6, 5, 4, 4, 1, 3, 5, 3, 4, 4, 6, 6, 0, 5, 5, 5, 3, 3, 3, 2, 2, 2, 6, 2, 1, 1, 0, 0, 0, 0, 1, 2, 6, 0, 5, 3, 2, 2, 4, 1, 1};

// int algo2_4[] = {3, 1, 1, 2, 1, 0, 2, 1, 2, 0, 1, 0, 2, 0, 1, 1, 2, 1, 0, 2, 1, 2, 0, 1, 0, 2, 0, 2, 1, 0, 2, 1, 2, 1, 0, 2, 2, 2, 0, 1, 2, 0, 0, 2, 1, 0, 0, 2, 2, 0, 1, 0, 2, 1, 1, 1, 0};
// int algo2_8[] = {7, 6, 6, 5, 3, 4, 0, 1, 0, 2, 3, 4, 1, 2, 6, 6, 5, 3, 4, 0, 1, 0, 2, 3, 4, 1, 2, 5, 6, 5, 0, 1, 5, 6, 5, 0, 1, 0, 2, 3, 0, 2, 4, 1, 3, 4, 2, 5, 1, 2, 3, 4, 5, 3, 6, 6, 4};

// int algo3_4[] = {3, 2, 2, 0, 2, 1, 0, 2, 0, 1, 2, 1, 2, 1, 2, 2, 0, 2, 1, 0, 2, 0, 1, 2, 1, 2, 1, 0, 2, 1, 0, 2, 0, 2, 1, 0, 2, 0, 1, 2, 0, 1, 1, 2, 2, 1, 1, 0, 2, 1, 2, 1, 0, 2, 2, 2, 1};
// int algo3_8[] = {7, 0, 0, 1, 5, 6, 2, 3, 2, 4, 5, 6, 3, 4, 0, 0, 1, 5, 6, 2, 3, 2, 4, 5, 6, 3, 4, 1, 5, 6, 2, 3, 1, 5, 6, 2, 3, 2, 4, 5, 2, 4, 6, 3, 5, 6, 4, 1, 3, 4, 5, 6, 1, 5, 0, 0, 6};

int base_4[] = {3, 0, 2, 0, 1, 1, 1, 0, 2, 1, 0, 2, 0, 2, 1, 1, 1, 1, 1, 1, 1, 2, 0, 2, 1, 1, 2, 0, 1, 2, 0, 0, 1, 2, 0, 0, 0, 2, 2, 0, 0, 2, 2, 2, 2, 0, 0, 2, 1, 1, 2, 0, 1, 0, 0, 2, 2};
int base_8[] = {7, 3, 5, 2, 3, 4, 1, 6, 4, 0, 0, 1, 3, 2, 1, 6, 3, 0, 1, 4, 2, 1, 1, 0, 4, 5, 3, 3, 6, 2, 0, 1, 2, 4, 0, 5, 5, 5, 3, 4, 2, 4, 6, 0, 2, 6, 6, 5, 3, 0, 1, 2, 5, 6, 4, 5, 6};

// Core assignment
int assign_thread_to_core(int core_id) {
   cpu_set_t cpuset;
   CPU_ZERO(&cpuset);
   CPU_SET(core_id, &cpuset);

   pthread_t current_thread = pthread_self();    
   return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
}


void *matrixConvolution(void *threadid){
	/*
		Parallel multiply given input matrices and return resultant matrix
	*/

	long id = (long)threadid;
    assign_thread_to_core(base_4[id]);

	int partition = DIMENSION / THREAD_COUNT;
	int start = partition * id;
	int end = partition * (id + 1) - 1;

    for (long a = 0; a < 10000; a++) {
        for (int i = start; i < end; i++) {
            for(int j=0; j < DIMENSION; j++) {
                int down = i - 1;
                int left = j - 1;
                int right = j + 1;
                int up = i + 1;
                int sum = 0;
                int count = 0;
                if (i != 0) {
                    if (j != 0) {
                        sum += matrixA[down][left];
                        count++;
                    }
                    sum += matrixA[down][j];
                    count++;
                    if (j != DIMENSION - 1) {
                        sum += matrixA[down][right];
                        count++;
                    }
                }
                if (j != 0) {
                    sum += matrixA[i][left];
                    count++;
                }
                sum += matrixA[i][j];
                count++;
                if (j != DIMENSION - 1) {
                    sum += matrixA[i][right];
                    count++;
                }
                if (i != DIMENSION - 1) {
                    if (j != 0) {
                        sum += matrixA[up][left];
                        count++;
                    }
                    sum += matrixA[up][j];
                    count++;
                    if (j != DIMENSION - 1) {
                        sum += matrixA[up][right];
                        count++;
                    }
                }

                
                matrixA[i][j] = (sum * sum) / count;
            }
        }
    }
	return 0;
}

int main(){

    int dimension = DIMENSION;

    double opmLatency;

	matrixA = randomSquareMatrix(dimension);

	long i;

	pthread_t threads[THREAD_COUNT];  // thread data structure

	struct timeval t0, t1;
	gettimeofday(&t0, 0);

    for (int itr = 0; itr < NUM_ITER; itr++) {

    // Spwan Threads to run Convolution
    for(i = 0; i < THREAD_COUNT; i++) {
        pthread_create(&threads[i], NULL, matrixConvolution, (void *)i);
    }

	// wait for children threads to exit
	void *status;
	for (i=0; i < THREAD_COUNT; i++ ) {
		pthread_join(threads[i], &status);
	}

    }

	gettimeofday(&t1, 0);
	double elapsed = (t1.tv_sec-t0.tv_sec) * 1.0f + (t1.tv_usec - t0.tv_usec) / 1000000.0f;

	// opmLatency = parallelMultiply(matrixA, matrixB, matrixResult, dimension);

    printf("Latency: %lf\n", elapsed);

	free(matrixA);

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