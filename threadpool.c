#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<pthread.h>

#define LIST_INSERT(item, list) do {	\
	item->prev = NULL;					\
	item->next = list;					\
	if ((list) != NULL) (list)->prev = item; \
	(list) = item;						\
} while(0)


#define LIST_REMOVE(item, list) do {	\
	if (item->prev != NULL) item->prev->next = item->next; \
	if (item->next != NULL) item->next->prev = item->prev; \
	if (list == item) list = item->next; 					\
	item->prev = item->next = NULL;							\
} while(0)

struct nTask{
	void (*task_func)(void *arg); //任务的功能
	void *user_data;			 //任务相应的参数

	struct nTask* prev;
	struct nTask* next;
};
struct nWorker{
	pthread_t threadid;        //线程id
	struct nManager *manager;  //管理组件 方便从任务上直接操纵线程池
	int terminate;			   //线程函数执行任务循环的退出的标志

	struct nWorker* prev;	
	struct nWorker* next;
};

typedef struct nManager{
	struct nTask* tasks;      //任务队列
	struct nWorker* workers;  //执行队列

	pthread_mutex_t mutex;		//互斥锁
	pthread_cond_t cond;	 //条件变量
}ThreadPool;

// 回调函数callback != task 
// 每一个任务的回调函数都相同
static void *nThreadPoolCallback(void *arg) {
	struct nWorker* worker = (struct nWorker*)arg;
	while(1){
		//在临界区上加锁
		pthread_mutex_lock(&worker->manager->mutex);
		//任务队列没有任务，就一直等待
		while(worker->manager->tasks == NULL){
			if(worker->terminate)break;
			pthread_cond_wait(&worker->manager->cond,&worker->manager->mutex);
		}
		//退出的标志
		if(worker->terminate){
			pthread_mutex_unlock(&worker->manager->mutex);
			break;
		}
		//移除链表的第一个结点
		struct nTask *task = worker->manager->tasks;
		LIST_REMOVE(task,worker->manager->tasks);
		pthread_mutex_unlock(&worker->manager->mutex);

		task->task_func(task->user_data);
	}
	free(worker);
	return NULL;
}

//线程池创建  传出参数 成员变量初始化
int nThreadPoolCreate(ThreadPool* pool,int numWorkers){
	if(pool == NULL) return -1;
	if(numWorkers < 1) numWorkers = 1;
	memset(pool,0,sizeof(ThreadPool));

	pthread_cond_t blank_cond = PTHREAD_COND_INITIALIZER;
	memcpy(&pool->cond, &blank_cond, sizeof(pthread_cond_t));

	pthread_mutex_init(&pool->mutex, NULL);

	//插入numWorkers个任务到任务队列pool->workers中
	int i=0;
	for(i=0; i<numWorkers; ++i){
		struct nWorker* worker = (struct nWorker*)malloc(sizeof(struct nWorker));
		if(worker == NULL){
			perror("malloc");
			return -2;
		}
		memset(worker,0,sizeof(struct nWorker));

		worker->manager = pool;
		worker->terminate = 0;

		int ret=pthread_create(&worker->threadid,NULL,nThreadPoolCallback,worker);
		if(ret){
			perror("pthread_create");
			free(worker);
			worker = NULL;
			return -3;
		}

		LIST_INSERT(worker,pool->workers);
	}

	//success
	return 0;
}

int nThreadPoolDestory(ThreadPool* pool,int nWorker){
	struct nWorker *worker = NULL;
	for(worker = pool->workers;worker != NULL;worker = worker->next){
		worker->terminate = 1;
	}

	//与回调函数里的锁要是同一把锁，否则会出现死锁现象
	pthread_mutex_lock(&pool->mutex);
	//API的作用是唤醒所有阻塞在条件变量pool->cond上的全部线程
	pthread_cond_broadcast(&pool->cond);

	pthread_mutex_unlock(&pool->mutex);

	pool->tasks = NULL;
	pool->workers = NULL;
	return 0;
}

int nThreadPoolPushTask(ThreadPool* pool,struct nTask* task){
	pthread_mutex_lock(&pool->mutex);	

	LIST_INSERT(task,pool->tasks);
	//API的作用是唤醒一个阻塞在条件变量pool->cond上的线程
	pthread_cond_signal(&pool->cond);

	pthread_mutex_unlock(&pool->mutex);

	return 0;
}

int main(int argc, char const *argv[])
{
	/* code */
	return 0;
}
