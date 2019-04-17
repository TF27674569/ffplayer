//
// Created by tianfeng on 2019/4/17.
//

#ifndef FFPLAYER_QUEUE_H
#define FFPLAYER_QUEUE_H

#include <pthread.h>

typedef /**
 * 队列，这里主要用于存放AVPacket的指针
 * 这里，使用生产者消费模式来使用队列，至少需要2个队列实例，分别用来存储音频AVPacket和视频AVPacket
 *  1.生产者：read_stream线程负责不断的读取视频文件中AVPacket，分别放入两个队列中
	2.消费者：
	1）视频解码，从视频AVPacket Queue中获取元素，解码，绘制
	2）音频解码，从音频AVPacket Queue中获取元素，解码，播放
 */
struct _Queue{
    //长度
    int size;

    //任意类型的指针数组，这里每一个元素都是AVPacket指针，总共有size个
    //AVPacket **packets;
    void** tab;

    //push或者pop元素时需要按照先后顺序，依次进行
    int next_to_write;
    int next_to_read;

    int *ready;
}Queue;

//分配队列元素内存的函数
typedef void* (*queue_fill_func)();
//释放队列中元素所占用的内存
typedef void* (*queue_free_func)(void* elem);

extern "C" {
/**
 * 初始化队列
 */
Queue *queueInit(int size, queue_fill_func fill_func);

/**
 * 销毁队列
 */
void queueFree(Queue *queue, queue_free_func free_func);

/**
 * 获取下一个索引位置
 */
int queueGetNext(Queue *queue, int current);

/**
 * 队列压人元素
 */
void *queuePush(Queue *queue, pthread_mutex_t *mutex, pthread_cond_t *cond);

/**
 * 弹出元素
 */
void *queuePop(Queue *queue, pthread_mutex_t *mutex, pthread_cond_t *cond);

}


#endif //FFPLAYER_QUEUE_H
