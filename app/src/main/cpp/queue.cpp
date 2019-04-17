//
// Created by tianfeng on 2019/4/17.
//

#include "queue.h"
#include "jnilog.h"


/**
 * 初始化队列
 */
Queue *queueInit(int size, queue_fill_func fill_func){
    Queue* queue = (Queue*)malloc(sizeof(Queue));
    queue->size = size;
    queue->next_to_write = 0;
    queue->next_to_read = 0;
    //数组开辟空间
    queue->tab = static_cast<void **>(malloc(sizeof(*queue->tab) * size));
    int i;
    for(i=0; i<size; i++){
        queue->tab[i] = fill_func();
    }
    return queue;
}

/**
 * 销毁队列
 */
void queueFree(Queue *queue, queue_free_func free_func){
    for(int i=0; i<queue->size; i++){
        //销毁队列的元素，通过使用回调函数
        free_func((void*)queue->tab[i]);
    }
    free(queue->tab);
    free(queue);
}

/**
 * 获取下一个索引位置
 */
int queueGetNext(Queue *queue, int current){
    return (current + 1) % queue->size;
}

/**
 * 队列压人元素
 */
void *queuePush(Queue *queue, pthread_mutex_t *mutex, pthread_cond_t *cond){
    int current = queue->next_to_write;
    int next_to_write;
    for(;;){
        //下一个要读的位置等于下一个要写的，等我写完，在读
        //不等于，就继续
        next_to_write = queueGetNext(queue,current);
        if(next_to_write != queue->next_to_read){
            break;
        }
        //阻塞
        pthread_cond_wait(cond,mutex);
    }

    queue->next_to_write = next_to_write;
    LOGE("queue_push queue:%#x, %d",queue,current);
    //通知
    pthread_cond_broadcast(cond);

    return queue->tab[current];
}

/**
 * 弹出元素
 */
void *queuePop(Queue *queue, pthread_mutex_t *mutex, pthread_cond_t *cond){
    int current = queue->next_to_read;
    for(;;){
        if(queue->next_to_read != queue->next_to_write){
            break;
        }
        pthread_cond_wait(cond,mutex);
    }

    queue->next_to_read = queueGetNext(queue,current);
    LOGE("queue_pop queue:%#x, %d",queue,current);
    pthread_cond_broadcast(cond);
    return queue->tab[current];
}



