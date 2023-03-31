#pragma once
const unsigned int MAX_DATA_SIZE = 2048;
struct Chunk{
	char data[MAX_DATA_SIZE];
	size_t size;
	Chunk* next;
};
struct ChunkList{
	Chunk* head;
	Chunk* tail;
	//Length in chunks
	size_t len;
};

//Clean data, doesn't delete container itself
void cleanChunks(ChunkList* list);
