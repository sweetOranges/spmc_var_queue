#include "SPMCVarQueue.h"
#include <stdio.h>
#include <unistd.h>
#include <thread>
#include <sys/time.h>

struct str {
 int size;
 int id;
 char data[];
};

int main()
{
	std::thread *t1 = new std::thread([](){
			int id = 1;
			spmc_var_queue *q = spmc_var_queue_init_shm("test", 100);
			struct timeval tv;
			printf("shm init\n");
			while(true) {
			spmc_var_queue_block *header = spmc_var_queue_alloc(q, sizeof(str) + 3);
			str *m = (str *)header++;
			m->size = 3;
			m->id = id++;
			memcpy(m->data, "hel", 3);
			//gettimeofday(&tv, NULL);
			//m->tsc = tv.tv_usec;
			spmc_var_queue_push(q);
			sleep(1);
			} 
			});
	sleep(2);
	std::thread *t2 = new std::thread([](){
			spmc_var_queue *q = spmc_var_queue_connect_shm("test");
			printf("shm connect\n");
			spmc_var_queue_reader *reader = spmc_var_queue_get_reader(q);
			struct timeval tv;
			while(true) {
			spmc_var_queue_block *msg2 = spmc_var_queue_read(reader);
			if (msg2 == nullptr) continue;
			str *m2 = (str *) msg2++;
			//gettimeofday(&tv, NULL);
			//printf("recv %d %ld %d\n", m2->id, m2->tsc, tv.tv_usec - m2->tsc);
			printf("recv %d %s\n", m2->id, m2->data);
			spmc_var_queue_pop(reader);
			} 
			});

  	t1->join();
}
