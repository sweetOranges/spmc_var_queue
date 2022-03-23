#include "SPMCVarQueue.h"
#include <stdio.h>
#include <unistd.h>
#include <thread>
#include <sys/time.h>

struct str {
 int64_t id;
 int64_t id2;
 int64_t id3;
//  char data[];
};

int main()
{
	spmc_var_queue *q = spmc_var_queue_init_shm("test", 10245);
	//spmc_var_queue *q1 = q;
	spmc_var_queue *q1 = spmc_var_queue_connect_shm("test");
	std::thread *t2 = new std::thread([q1](){
		printf("shm start read\n");
		spmc_var_queue_reader *reader = spmc_var_queue_get_reader(q1);
		struct timeval tv;
		int64_t last = 0;
		while(true) {
			spmc_var_queue_block *msg2 = spmc_var_queue_read(reader);
			if (msg2 == nullptr) continue;
			str *m2 = (str *) (msg2);
			//gettimeofday(&tv, NULL);
			//printf("recv %d %ld %d\n", m2->id, m2->tsc, tv.tv_usec - m2->tsc);
			if (m2->id - last != 1) {
				printf("recv %d\n", m2->id);
			}
			//printf("recv %d %d\n", m2->id, reader->read_idx);
			last = m2->id;
			spmc_var_queue_pop(reader);
		} 
	});
	int64_t id = 1;
	struct timeval tv;
	printf("shm start write\n");
	while(true) {
		spmc_var_queue_block *header = spmc_var_queue_alloc(q, sizeof(str));
		str *m = (str *)(header);
		m->id = id++;
		// memcpy(m->data, "hel", 3);
		//gettimeofday(&tv, NULL);
		//m->tsc = tv.tv_usec;
		spmc_var_queue_push(q);
		if (id % 100 == 0) {
			usleep(100);
			//printf("ssss %ld\n", q->write_idx);
		}
	} 
}
