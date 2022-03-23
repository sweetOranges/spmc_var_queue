#pragma once
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>


struct spmc_var_queue_block
{
	uint64_t size;
};

struct spmc_var_queue {
	uint64_t size;
	uint64_t block_cnt;
	alignas(64) volatile uint64_t write_idx;
	pthread_cond_t cond;
	pthread_mutex_t mutex;
	spmc_var_queue_block data[];  
};

struct spmc_var_queue_reader
{
	spmc_var_queue *q;
	alignas(64) volatile uint64_t read_idx;
};

spmc_var_queue* spmc_var_queue_construct(void *mem, int64_t len)
{
	spmc_var_queue *ptr = (spmc_var_queue *)mem;
	ptr->size = len * sizeof(spmc_var_queue_block);
	ptr->block_cnt = len;
	ptr->write_idx = 0;
	pthread_condattr_t attrcond;
	pthread_condattr_init(&attrcond);
	pthread_condattr_setpshared(&attrcond, PTHREAD_PROCESS_SHARED);
	pthread_cond_init(&ptr->cond, &attrcond);
	pthread_mutexattr_t attrmutex;
	pthread_mutexattr_init(&attrmutex);
	pthread_mutexattr_setpshared(&attrmutex, PTHREAD_PROCESS_SHARED);
	pthread_mutexattr_setrobust_np(&attrmutex, PTHREAD_MUTEX_ROBUST_NP);
	pthread_mutex_init(&ptr->mutex, &attrmutex);
	return ptr;
}


spmc_var_queue * spmc_var_queue_init(int len)
{
	void *mem = malloc(len * sizeof(spmc_var_queue_block) + sizeof(spmc_var_queue));
	return spmc_var_queue_construct(mem, len);
}

spmc_var_queue * spmc_var_queue_init_shm(const char *filename, int64_t len)
{
	int64_t size = len * sizeof(spmc_var_queue_block) + sizeof(spmc_var_queue);
	int shm_fd = shm_open(filename, O_CREAT | O_RDWR, 0666);
	if(shm_fd == -1) {
		return nullptr;
	}
	if(ftruncate(shm_fd, size)) {
		close(shm_fd);
		return nullptr;
	}
	void* mem = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
	close(shm_fd);
	if(mem == MAP_FAILED) {
		return nullptr;
	}
	return spmc_var_queue_construct(mem, len);
}

spmc_var_queue * spmc_var_queue_connect_shm(const char *filename)
{
	int shm_fd = shm_open(filename, O_CREAT | O_RDWR, 0666);
	if(shm_fd == -1) {
		return nullptr;
	}
	spmc_var_queue* q = (spmc_var_queue*)mmap(0, sizeof(spmc_var_queue), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
	int64_t size = sizeof(spmc_var_queue) + q->size;
	q = (spmc_var_queue*)mremap(q, sizeof(spmc_var_queue), size, MREMAP_MAYMOVE);
	return q;
}


spmc_var_queue_block * spmc_var_queue_alloc(spmc_var_queue *q, uint64_t size)
{
	size += sizeof(spmc_var_queue_block);
	uint64_t blk_sz = (size + sizeof(spmc_var_queue_block) - 1) / sizeof(spmc_var_queue_block);
	uint64_t padding_sz = q->block_cnt - (q->write_idx  % q->block_cnt);
	bool rewind = blk_sz > padding_sz;
	if (rewind) {
		q->data[q->write_idx % q->block_cnt].size = 0;
		__atomic_add_fetch(&q->write_idx, padding_sz, __ATOMIC_RELEASE);
	}
	spmc_var_queue_block *header = &q->data[q->write_idx % q->block_cnt];
	header->size = size;
	header++;
	return header;
}

void spmc_var_queue_push(spmc_var_queue *q)
{
	uint64_t blk_sz = (q->data[q->write_idx % q->block_cnt].size + sizeof(spmc_var_queue_block) - 1) / sizeof(spmc_var_queue_block);
	__atomic_add_fetch(&q->write_idx, blk_sz, __ATOMIC_RELEASE);
}

spmc_var_queue_reader* spmc_var_queue_get_reader(spmc_var_queue *q)
{
	spmc_var_queue_reader* reader = (spmc_var_queue_reader*)malloc(sizeof(spmc_var_queue_reader));
	reader->q = q;
	reader->read_idx = 0;
	return reader;
}

spmc_var_queue_block* spmc_var_queue_read(spmc_var_queue_reader *reader)
{
	if (reader->read_idx ==  __atomic_load_n(&reader->q->write_idx, __ATOMIC_ACQUIRE)) {
		return nullptr;
	}
	uint64_t size = reader->q->data[reader->read_idx % reader->q->block_cnt].size;
	if (size == 0) {
		reader->read_idx += reader->q->block_cnt - (reader->read_idx % reader->q->block_cnt);
		if (reader->read_idx == reader->q->write_idx) {
			return nullptr;
		}
	}
	spmc_var_queue_block *header = &reader->q->data[reader->read_idx % reader->q->block_cnt];
	header++;
	return header;
}

void spmc_var_queue_pop(spmc_var_queue_reader *reader)
{
	uint64_t blk_sz = (reader->q->data[reader->read_idx % reader->q->block_cnt].size + sizeof(spmc_var_queue_block) - 1) / sizeof(spmc_var_queue_block);
	reader->read_idx += blk_sz;
}

void spmc_queue_notify(spmc_var_queue *q)
{
	if (pthread_mutex_lock(&q->mutex) == EOWNERDEAD)
	{
		pthread_mutex_consistent_np(&q->mutex);
	}
	pthread_cond_broadcast(&q->cond);
	pthread_mutex_unlock(&q->mutex);
}

void spmc_queue_wait(spmc_var_queue_reader *reader)
{
	pthread_mutex_lock(&reader->q->mutex);
	while (reader->read_idx == reader->q->write_idx) {
		pthread_cond_wait(&reader->q->cond, &reader->q->mutex);
	}
	pthread_mutex_unlock(&reader->q->mutex);
}

