#include <cstdlib>
#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
#include "chunkList.h"
#include "context.h"
#include <pthread.h>

void* keyGen(void* p){
	KeyParams* params = (KeyParams*)p;
	ChunkList* key = (ChunkList*)malloc(sizeof(ChunkList));
	key->head = (Chunk*)malloc(sizeof(Chunk));

	long long start_x = params->X0;
	Chunk* t = key->head;
	size_t count = 0;
	size_t internal_count = 0;
	key->len = 0;
	while(count != params->len){
		if(internal_count <= MAX_DATA_SIZE - sizeof(long long)){
			memcpy((void*)(t->data + internal_count), (void*)&start_x, sizeof(long long));
			internal_count += sizeof(long long);
			count += sizeof(long long);
			start_x = (params->a * start_x + params->c) % params->m;
			printf("X_n = %lli, count = %d, internal_count = %d\n", start_x, count, internal_count);
		}
		else{
			t->next = (Chunk*)malloc(sizeof(Chunk));
			t->size = internal_count;
			internal_count = 0;
			t = t->next;
			key->len += 1;
		}
	}
	t->next = NULL;
	t->size = internal_count;
	key->tail = t;
	key->len += 1;
	pthread_exit(key);
}

void cleanChunks(ChunkList* list){
	Chunk* t = list->head;
	Chunk* next = NULL;
	for (size_t i = 0; i < list->len; ++i){
		next = t->next;
		free(t);
		t = next;
	}
}

void* worker(void* context){
	WorkerContext* locContext = (WorkerContext*)context;
	Chunk* t_data = locContext->input->head;
	Chunk* t_key = locContext->key->head;
	Chunk* t_out;
	for (size_t i = 0; i < locContext->key->len; ++i){
		if(locContext->output->len == 0){
			locContext->output->head = (Chunk*)malloc(sizeof(Chunk));
			t_out = locContext->output->head;
		}
		else{
			t_out->next = (Chunk*)malloc(sizeof(Chunk));
			t_out = t_out->next;
		}
		for (size_t j = 0; j < t_data->size; ++j){
			t_out->data[j] = t_data->data[j] ^ t_key->data[j];
		}
		t_out->size = t_data->size;
		locContext->output->len += 1;

		t_key = t_key->next;
		t_data = t_data->next;
	}
	locContext->output->tail = t_out;
	printf("Worker with context %p is going to be set in waiting\n", (void*)context);
	pthread_barrier_wait(locContext->barrier);
	printf("Worker with context %p has woke up\n", (void*)context);
	pthread_exit((void*)locContext->output);
}

void keyParamsInit(UserParams* p, KeyParams* init, size_t len){
	init->a = p->a;
	init->c = p->c;
	init->X0 = p->X0;
	init->m = p->m;
	init->len = len;
}

int main(int argc, char *argv[]) {
	//Arguments reading
	int opt = 0;
	UserParams params;
	while ((opt = getopt(argc, argv, "i:o:x:a:c:m:")) != -1) {
	    switch (opt) {
	    case 'i':
	      memcpy(params.path_in, optarg, strlen(optarg) + 1);
	      break;
	    case 'o':
	      memcpy(params.path_out, optarg, strlen(optarg) + 1);
	      break;
	    case 'x': {
	      params.X0 = atoll(optarg);
	      break;
	    }
	    case 'a':
	      params.a = atoll(optarg);
	      break;
	    case 'c':
	      params.c = atoll(optarg);
	      break;
	    case 'm':
	      params.m = atoll(optarg);
	      break;
	    case '?':
	      if (optopt == 'i' || optopt == 'o' || optopt == 'x' || optopt == 'a' ||
	          optopt == 'c' || optopt == 'm')
	        fprintf(stderr, "Option -%c requires an argument.\n", optopt);
	      else
	        fprintf(stderr, "Unknown option `-%c'.\n", optopt);
	      exit(EXIT_FAILURE);
	    default:
	      abort();
	    }
	}

	//Data reading
	FILE *fd_i;
	printf("%s\n", params.path_in);
	fd_i = fopen(params.path_in, "rb");
	if (fd_i == NULL) {
	    fprintf(stderr, "Error opening file.\n");
	    return 1;
	}

	struct stat sb;
	if (stat(params.path_in, &sb) < 0) {
	    fprintf(stderr, "Error opening file.\n");
	    return 1;
	}
	ChunkList input_data_list;
	input_data_list.head = (Chunk*)malloc(sizeof(Chunk));
	Chunk* t = input_data_list.head;
	//quantity of chunks
	printf("st_size is %d bytes\n", sb.st_size);
	input_data_list.len = sb.st_size / MAX_DATA_SIZE;

	if (sb.st_size % MAX_DATA_SIZE != 0){
		input_data_list.len += 1;
	}
	printf("chunk q is %d\n", input_data_list.len);
	size_t i = 0;
	while(i < input_data_list.len){
		if (i == (input_data_list.len - 1)){
			if (sb.st_size % MAX_DATA_SIZE != 0){
				fread(t->data, sizeof(char), sb.st_size % MAX_DATA_SIZE, fd_i);
				t->size = sb.st_size % MAX_DATA_SIZE;
			}
			else{
				fread(t->data, sizeof(char), MAX_DATA_SIZE, fd_i);
				t->size = MAX_DATA_SIZE;
			}
			t->next = NULL;
		}
		else{
			fread(t->data, sizeof(char), MAX_DATA_SIZE, fd_i);
			t->size = MAX_DATA_SIZE;
			t->next = (Chunk*)malloc(sizeof(Chunk));
			t = t->next;
		}
		++i;
	}
	input_data_list.tail = t;
	fclose(fd_i);

	//Linear key generation
	KeyParams key_params;
	keyParamsInit(&params, &key_params, (input_data_list.len - 1) * MAX_DATA_SIZE + input_data_list.tail->size);
	pthread_t keygenThread;
	ChunkList* key;
	pthread_create(&keygenThread, NULL, &keyGen, (void*)&key_params);
	pthread_join(keygenThread, ((void**)&key));
	printf("Key has %d chunks and %d bytes of data\n", key->len, key->tail->size + (key->len - 1) * MAX_DATA_SIZE);

	//Create the barrier
	pthread_barrier_t barrier;
	long N = sysconf(_SC_NPROCESSORS_ONLN);
	printf("There is %li available threads\n", N);
	if (N / key->len){
		N = key->len;
	}
	pthread_barrier_init(&barrier, NULL, N + 1);
	printf("%li threads will start\n", N);
	//Init & start workers
	size_t chunksPerWorker = (input_data_list.len / N);
	Chunk* data_head = input_data_list.head;
	Chunk* key_head = key->head;
	WorkerContext** contexts = (WorkerContext**)malloc(sizeof(WorkerContext*) * N);
	pthread_t** workers = (pthread_t**)malloc(sizeof(pthread_t*) * N);
	for (long i = 0; i < N; ++i){
		if (i == N - 1 && input_data_list.len % N != 0){
			chunksPerWorker = input_data_list.len % N;
		}
		ChunkList* workerData = (ChunkList*)malloc(sizeof(ChunkList));
		ChunkList* workerKey = (ChunkList*)malloc(sizeof(ChunkList));
		workerData->head = data_head;
		workerKey->head = key_head;
		Chunk *t_data = workerData->head;
		Chunk *t_key = workerKey->head;
		for (size_t j = 0; j < chunksPerWorker - 1; ++j){
			t_data = t_data->next;
			t_key = t_key->next;
		}
		workerData->tail = t_data;
		workerData->len = chunksPerWorker;
		workerKey->tail = t_key;
		workerKey->len = chunksPerWorker;

		if (workerData->tail)
			data_head = workerData->tail->next;

		if (workerKey->tail)
			key_head = workerKey->tail->next;

		WorkerContext* context = (WorkerContext*)malloc(sizeof(WorkerContext));
		context->barrier = &barrier;
		context->input = workerData;
		context->key = workerKey;
		context->output = (ChunkList*)malloc(sizeof(ChunkList));
		context->output->len = 0;

		contexts[i] = context;
		pthread_t* workerThread = (pthread_t*)malloc(sizeof(pthread_t));
		workers[i] = workerThread;
		pthread_create(workerThread, NULL, &worker, (void*)context);
	}
	//Stop main thread till workers end
	pthread_barrier_wait(&barrier);
	FILE *fd_o = fopen(params.path_out, "wb");
	if (fd_o == NULL) {
		fprintf(stderr, "Error opening file %s.\n", params.path_out);
		return 1;
	}
	//Write down output
	for (long j = 0; j < N; ++j){
		ChunkList* chunks = contexts[j]->output;
		Chunk* t = chunks->head;
		for (size_t k = 0; k < chunks->len; ++k){
			if(fwrite(t->data, sizeof(char), t->size, fd_o) != t->size * sizeof(char)){
				fprintf(stderr, "Error in writing %s.\n", params.path_out);
				return 1;
			}
			t = t->next;
		}
	}
	fclose(fd_o);

	//Clear memory on the heap
	cleanChunks(&input_data_list);
	cleanChunks(key);
	free(key);
	for (size_t j = 0; j < N; ++j){
		cleanChunks(contexts[j]->output);
		free(contexts[j]);
		free(workers[j]);
	}
	free(contexts);
	free(workers);
	return EXIT_SUCCESS;
}
