### 无锁变长消息队列 spmc

#### 特性
* 运行时零内存分配
* 环形缓冲区
* 变长存储
* 共享内存
* 多进程通信

### 实例
```cpp
struct str {
 int size;
 int id;
 char data[];
};

int main()
{
	spmc_var_queue *q = spmc_var_queue_init(100); // 初始化队列
	spmc_var_queue_block *header = spmc_var_queue_alloc(q, sizeof(str) + 3); //申请内存
	str *m = (str *)header++;
	m->size = 3;
	m->id = 3;
	memcpy(m->data, "hel", 3);
	spmc_var_queue_push(q);// 发布
	
	//读取数据

	spmc_var_queue_reader *reader = spmc_var_queue_get_reader(q);// 获取读者
	spmc_var_queue_block *msg2 = spmc_var_queue_read(reader); // 读取消息
	if (msg2== nullptr) {
		return 1;
	}
	str *m2 = (str *) msg2++;//解析消息
	printf("recv %d %s\n", m2->id, m2->data);
	
}
```

### 共享内存实例
```cpp
spmc_var_queue *q = spmc_var_queue_init_shm("test", 100); // 初始化
spmc_var_queue_block *header = spmc_var_queue_alloc(q, sizeof(str) + 3);
str *m = (str *)header++;
m->size = 3;
m->id = 3;
memcpy(m->data, "hel", 3);
spmc_var_queue_push(q);// 发布

// 读取
spmc_var_queue *q = spmc_var_queue_connect_shm("test");
spmc_var_queue_block *msg2 = spmc_var_queue_read(reader); // 读取消息
if (msg2== nullptr) {
	return 1;
}
str *m2 = (str *) msg2++;//解析消息
printf("recv %d %s\n", m2->id, m2->data);
```
