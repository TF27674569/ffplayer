//
// Created by ubt on 2019/4/17.
//

#ifndef FFPLAYER_PLAYER_H
#define FFPLAYER_PLAYER_H

#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <pthread.h>
#include "queue.h"

extern "C"{
//封装视频格式
#include "libavformat/avformat.h"
// 解码库
#include "libavcodec/avcodec.h"
// 缩放
#include "libswscale/swscale.h"
// yuv->rgb
#include "libyuv.h"

#include "libswresample/swresample.h"
}


#define PACKET_QUEUE_SIZE 50
#define MAX_AUDIO_FRME_SIZE (48000<<2)

#define MIN_SLEEP_TIME_US 1000ll

typedef struct _JNIInfo{
    jobject             audioTrack;
    jmethodID           audioTrackPlayMethodId;
    JavaVM*             javaVM;
}JNIInfo;

typedef struct _VideoInfo{
    int                 videoStreamIndex;
    AVCodecContext*      videoCodecCtx;
    ANativeWindow*       nativeWindow;
    Queue*               videoQueue;
}VideoInfo;

typedef struct _AudioInfo{
    int                    audioStreamIndex;
    AVCodecContext*         audioCodecCtx;
    Queue*                  audioQueue;

    /*音频采样相关*/
    SwrContext*             swr_ctx;
    //输入的采样格式
    enum AVSampleFormat     in_sample_fmt;
    //输出采样格式16bit PCM
    enum AVSampleFormat     out_sample_fmt;
    //输入采样率
    int                     in_sample_rate;
    //输出采样率
    int                     out_sample_rate;
    //输出的声道个数
    int                     out_channel_nb;

    int64_t                 audioClock;
}AudioInfo;

typedef struct _ThreadInfo{
    pthread_t                readStreamId; // 生产线程Id
    pthread_mutex_t          mutex;//互斥锁
    pthread_cond_t           cond;//条件变量
    pthread_t                consumeIds[2];//条件变量

}ThreadInfo;

typedef struct _PlayerInfo{

    AVFormatContext*     avFormatContext;
    JNIInfo*             jniInfo;
    VideoInfo*           videoInfo;
    AudioInfo*           audioInfo;
    ThreadInfo*          threadInfo;

}PlayerInfo;

typedef struct _DecoderData{
    PlayerInfo*          playerInfo;
    int                  streamIndex;
}DecoderData;

extern "C"{

    PlayerInfo * mallocPlayerInfo();

    int initAVFormatContextAndFindStream(const char* playPath);

    int openCodecContext(int streamIndex);

    int prepareVideoAndAudio(JNIEnv* env,jobject surface);

    /**
     * 使用java AudioTrack 播放声音
     * @param env java环境
     * @param instance 这里写死了的在FFmpegPlayer里面创建AudioTrack函数
     */
    void findAudioTrackAndJVM(JNIEnv* env,jobject instance);

    void produceFormStream();
    void consumeFormQueue();


    void freePlayerInfo();

}

#endif //FFPLAYER_PLAYER_H
