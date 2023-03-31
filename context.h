#pragma once
#include "chunkList.h"
#include <pthread.h>

const unsigned int MAX_PATH_SIZE = 1024;
struct UserParams
{
	long long a;
	long long m;
	long long c;
	long long X0;
	char path_in [MAX_PATH_SIZE];
	char path_out [MAX_PATH_SIZE];
};

struct KeyParams
{
	long long a;
	long long m;
	long long c;
	long long X0;
	size_t len;
};

void keyParamsInit(UserParams* p, KeyParams* init, int len);

struct WorkerContext{
	ChunkList* key;
	ChunkList* input;
	pthread_barrier_t* barrier;
	ChunkList* output;
};

void* worker(void* contexts);
