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
	void (*task_func)(void *arg);
	void *user_data;

	struct nTask* prev;
	struct nTask* next;
};
struct nWorker{
	pthread_t threadid;
	struct nManager *manager;
	int terminate;

	struct nWorker* prev;
	struct nWorker* next;
};

typedef struct nManager{
	struct nTask* tasks;
	struct nWorker* workers;

	pthread_mutex_t mutex;
	pthread_cond_t cond;
}ThreadPool;

// 回调函数callback != task
static void *nThreadPoolCallback(void *arg) {
	struct nWorker* worker = (struct nWorker*)arg;
	while(1){
		pthread_mutex_lock(&worker->manager->mutex);
		//任务队列没有任务，就一直等待
		while(worker->manager->tasks == NULL){
			if(worker->terminate)break;
			pthread_cond_wait(&worker->manager->cond,&worker->manager->mutex);
		}
		if(worker->terminate){
			pthread_mutex_unlock(&worker->manager->mutex);
			break;
		}
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

	int i=0;
	for(i=0;i<numWorkers;++i){
		struct nWorker* worker = (struct nWorker*)malloc(sizeof(struct nWorker));
		if(worker == NULL){
			perror("malloc");
			return -2;
		}
		memset(worker,0,sizeof(struct nWorker));

		worker->manager = pool;

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
	pthread_mutex_lock(&pool->mutex);

	pthread_cond_broadcast(&pool->cond);

	pthread_mutex_unlock(&pool->mutex);

	pool->tasks = NULL;
	pool->workers = NULL;
	return 0;
}

int nThreadPoolPushTask(ThreadPool* pool,struct nTask* task){
	pthread_mutex_lock(&pool->mutex);	

	LIST_INSERT(task,pool->tasks);

	pthread_cond_signal(&pool->cond);

	pthread_mutex_unlock(&pool->mutex);

	return 0;
}

int main(int argc, char const *argv[])
{
	/* code */
	return 0;
}
