//
// Created by ubt on 2019/4/17.
//

#include <libavutil/time.h>
#include "player.h"
#include "jnilog.h"
#include <unistd.h>

PlayerInfo *playerInfo;

void decodeAudio(PlayerInfo *pInfo, AVPacket *pPacket);

void decodeVideo(PlayerInfo *pInfo, AVPacket *pPacket);

void initAudioInfo() {
    AVCodecContext *codec_ctx = playerInfo->audioInfo->audioCodecCtx;

    //重采样设置参数-------------start
    //输入的采样格式
    enum AVSampleFormat in_sample_fmt = codec_ctx->sample_fmt;
    //输出采样格式16bit PCM
    enum AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;
    //输入采样率
    int in_sample_rate = codec_ctx->sample_rate;
    //输出采样率
    int out_sample_rate = in_sample_rate;
    //获取输入的声道布局
    //根据声道个数获取默认的声道布局（2个声道，默认立体声stereo）
    //av_get_default_channel_layout(codecCtx->channels);
    uint64_t in_ch_layout = codec_ctx->channel_layout;
    //输出的声道布局（立体声）
    uint64_t out_ch_layout = AV_CH_LAYOUT_STEREO;

    //frame->16bit 44100 PCM 统一音频采样格式与采样率
    SwrContext *swr_ctx = swr_alloc();
    swr_alloc_set_opts(swr_ctx,
                       out_ch_layout, out_sample_fmt, out_sample_rate,
                       in_ch_layout, in_sample_fmt, in_sample_rate,
                       0, NULL);
    swr_init(swr_ctx);

    //输出的声道个数
    int out_channel_nb = av_get_channel_layout_nb_channels(out_ch_layout);

    //重采样设置参数-------------end

    playerInfo->audioInfo->in_sample_fmt = in_sample_fmt;
    playerInfo->audioInfo->out_sample_fmt = out_sample_fmt;
    playerInfo->audioInfo->in_sample_rate = in_sample_rate;
    playerInfo->audioInfo->out_sample_rate = out_sample_rate;
    playerInfo->audioInfo->out_channel_nb = out_channel_nb;
    playerInfo->audioInfo->swr_ctx = swr_ctx;
}

/**
 * 给AVPacket开辟空间，后面会将AVPacket栈内存数据拷贝至这里开辟的空间
 */
void *player_fill_packet() {
    //请参照我在vs中写的代码
    AVPacket *packet = static_cast<AVPacket *>(malloc(sizeof(AVPacket)));
    return packet;
}

PlayerInfo *mallocPlayerInfo() {
    // 结构体初始化
    playerInfo = (PlayerInfo *)(malloc(sizeof(PlayerInfo)));
    playerInfo->jniInfo = (JNIInfo *)(malloc(sizeof(JNIInfo)));
    playerInfo->videoInfo =(VideoInfo *)(malloc(sizeof(VideoInfo)));
    playerInfo->audioInfo = (AudioInfo *)(malloc(sizeof(AudioInfo)));
    playerInfo->threadInfo = (ThreadInfo *)(malloc(sizeof(ThreadInfo)));

    playerInfo->videoInfo->videoStreamIndex = -1;
    playerInfo->audioInfo->audioStreamIndex = -1;

    playerInfo->videoInfo->videoQueue = queueInit(PACKET_QUEUE_SIZE, player_fill_packet);
    playerInfo->audioInfo->audioQueue = queueInit(PACKET_QUEUE_SIZE, player_fill_packet);

    playerInfo->audioInfo->audioClock = -1;
    return playerInfo;
}

int initAVFormatContextAndFindStream(const char *playPath) {
    if (playerInfo == NULL) {
        LOGE("initAVFormatContextAndFindStream error! , playerInfo==NULL ,must be call mallocPlayerInfo");
        return -1;
    }
    av_register_all();
    // 封装格式上下文
    AVFormatContext *avFormatContext = avformat_alloc_context();
    playerInfo->avFormatContext = avFormatContext;

    if (avformat_open_input(&avFormatContext, playPath, NULL, NULL) != 0) {
        LOGE("打开视频文件失败");
        return -1;
    }

    if (avformat_find_stream_info(avFormatContext, NULL) < 0) {
        LOGE("获取文件stream流信息失败");
        return -1;
    }

    // find stream
    for (int i = 0; i < avFormatContext->nb_streams; ++i) {
        enum AVMediaType mediaType = avFormatContext->streams[i]->codec->codec_type;
        if (mediaType == AVMEDIA_TYPE_VIDEO) {
            playerInfo->videoInfo->videoStreamIndex = i;
        } else if (mediaType == AVMEDIA_TYPE_AUDIO) {
            playerInfo->audioInfo->audioStreamIndex = i;
        }
    }

    return 0;
}

int openCodecContext(int streamIndex) {
    if (playerInfo == NULL || playerInfo->avFormatContext == NULL) {
        LOGE("openCodecContext error! , playerInfo==NULL ||playerInfo->avFormatContext==NULL");
        return -1;
    }
    //解码器上下文
    AVCodecContext *pCodecCtx = playerInfo->avFormatContext->streams[streamIndex]->codec;
    // 拿到流获取解码器
    AVCodec *pCodec = avcodec_find_decoder(pCodecCtx->codec_id);

    if (pCodec == NULL) {
        LOGE("无法解码，是否加密\n");
        return -1;
    }

    // 打开解码器
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        LOGE("无法解码，打开解码器失败\n");
        return -1;
    }

    if (streamIndex == playerInfo->videoInfo->videoStreamIndex) {
        playerInfo->videoInfo->videoCodecCtx = pCodecCtx;
    } else if (streamIndex == playerInfo->audioInfo->audioStreamIndex) {
        playerInfo->audioInfo->audioCodecCtx = pCodecCtx;
    }
    return 0;
}


int prepareVideoAndAudio(JNIEnv *env, jobject surface) {
    if (playerInfo == NULL) {
        LOGE("prepareVideoAndAudio error!!!");
        return -1;
    }
    // video
    playerInfo->videoInfo->nativeWindow = ANativeWindow_fromSurface(env, surface);
    // audio
    initAudioInfo();
    return 0;
}


void findAudioTrackAndJVM(JNIEnv *env, jobject instance) {

    //JNI begin------------------
    jclass player_class = env->GetObjectClass(instance);

    //AudioTrack对象
    jmethodID create_audio_track_mid = env->GetMethodID(player_class, "createAudioTrack",
                                                        "(II)Landroid/media/AudioTrack;");
    jobject audio_track = env->CallObjectMethod(instance, create_audio_track_mid,
                                                playerInfo->audioInfo->out_sample_rate,
                                                playerInfo->audioInfo->out_channel_nb);

    //调用AudioTrack.play方法
    jclass audio_track_class = env->GetObjectClass(audio_track);
    jmethodID audio_track_play_mid = env->GetMethodID(audio_track_class, "play", "()V");
    env->CallVoidMethod(audio_track, audio_track_play_mid);

    //AudioTrack.write
    jmethodID audio_track_write_mid = env->GetMethodID(audio_track_class, "write", "([BII)I");

    //JNI end------------------
    playerInfo->jniInfo->audioTrack = env->NewGlobalRef(audio_track);
    //(*env)->DeleteGlobalRef
    playerInfo->jniInfo->audioTrackPlayMethodId = audio_track_write_mid;

    // 初始化jvm虚拟机
    env->GetJavaVM(&(playerInfo->jniInfo->javaVM));
}

/*********************************************************************************************************/
////////////////////////////////////解码packet到队列中////////////////////////////////////////////////////

void *readDataFromStream(void *args) {
    PlayerInfo *info = static_cast<PlayerInfo *>(args);
    int ret;
    //栈内存上保存一个AVPacket
    AVPacket packet, *pkt = &packet;
    for (;;) {
        ret = av_read_frame(info->avFormatContext, pkt);
        //到文件结尾了
        if (ret < 0) {
            break;
        }

        //根据AVpacket->stream_index获取对应的队列
        Queue *queue;
        if (pkt->stream_index == info->videoInfo->videoStreamIndex) {
            queue = info->videoInfo->videoQueue;
        } else if (pkt->stream_index == info->audioInfo->audioStreamIndex) {
            queue = info->audioInfo->audioQueue;
        }

        //示范队列内存释放
        //queue_free(queue,packet_free_func);
        pthread_mutex_lock(&info->threadInfo->mutex);
        //将AVPacket压入队列
        AVPacket *packet_data = static_cast<AVPacket *>(queuePush(queue, &info->threadInfo->mutex,
                                                                  &info->threadInfo->cond));
        //拷贝（间接赋值，拷贝结构体数据）
        *packet_data = packet;
        pthread_mutex_unlock(&info->threadInfo->mutex);
        LOGE("queue:%#x, packet:%#x", queue, packet);
    }
    return 0;
}

void produceFormStream() {
    pthread_create(&(playerInfo->threadInfo->readStreamId), NULL, readDataFromStream, playerInfo);
    usleep(1000);
}
/*********************************************************************************************************/
///////////////////////////////消费线程///////////////////////////////////////////////////////////////////

void *decodeData(void *args) {
    DecoderData *decoderData =(DecoderData *)(args);
    PlayerInfo *playerInfo = decoderData->playerInfo;
    int streamIndex = decoderData->streamIndex;

    //根据stream_index获取对应的AVPacket队列
    Queue *queue=NULL;
   if (streamIndex == playerInfo->videoInfo->videoStreamIndex) {
        queue = playerInfo->videoInfo->videoQueue;
    } else if (streamIndex == playerInfo->audioInfo->audioStreamIndex) {
        queue = playerInfo->audioInfo->audioQueue;
    }

    //6.一阵一阵读取压缩的视频数据AVPacket
    int video_frame_count = 0, audio_frame_count = 0;
    for (;;) {
        //消费AVPacket
        pthread_mutex_lock(&playerInfo->threadInfo->mutex);
        AVPacket *packet = (AVPacket *) queuePop(queue, &playerInfo->threadInfo->mutex,
                                                 &playerInfo->threadInfo->cond);
        pthread_mutex_unlock(&playerInfo->threadInfo->mutex);
        if (streamIndex == playerInfo->videoInfo->videoStreamIndex) {
            decodeVideo(playerInfo, packet);
            LOGE("video_frame_count:%d", video_frame_count++);
        } else if (streamIndex == playerInfo->audioInfo->audioStreamIndex) {
            decodeAudio(playerInfo, packet);
            LOGE("audio_frame_count:%d", audio_frame_count++);
        }

    }
    return 0;
}


/**
 * 延迟
 *
 * @param stream_time 当前视频时间
 * @param stream_no 流位置
 */
void playerWaitForFrame(PlayerInfo *player, int64_t video_frame_time, int stream_no) {
    pthread_mutex_lock(&player->threadInfo->mutex);
    for (;;) {
        int64_t sleep_time = video_frame_time - player->audioInfo->audioClock;
        if (sleep_time < -300000ll) {
            // 300 ms late
            int64_t new_value = player->audioInfo->audioClock - sleep_time;
            LOGE("new value is %d", new_value);
            pthread_cond_broadcast(&player->threadInfo->cond);
        }

        if (sleep_time <= MIN_SLEEP_TIME_US) {
            // We do not need to wait if time is slower then minimal sleep time
            break;
        }

        if (sleep_time > 500000ll) {
            // if sleep time is bigger then 500ms just sleep this 500ms
            // and check everything again
            sleep_time = 500000ll;
        }
        //等待指定时长
        int timeout_ret = pthread_cond_timeout_np(&player->threadInfo->cond,
                                                  &player->threadInfo->mutex, sleep_time / 1000ll);

        // just go further
        LOGE("player_wait_for_frame[%d] finish", stream_no);
    }
    pthread_mutex_unlock(&player->threadInfo->mutex);
}

void decodeVideo(PlayerInfo *pInfo, AVPacket *packet) {
    AVFormatContext *input_format_ctx = pInfo->avFormatContext;
    AVStream *stream = input_format_ctx->streams[pInfo->videoInfo->videoStreamIndex];
    //像素数据（解码数据）
    AVFrame *yuv_frame = av_frame_alloc();
    AVFrame *rgb_frame = av_frame_alloc();
    //绘制时的缓冲区
    ANativeWindow_Buffer outBuffer;
    AVCodecContext *codec_ctx = pInfo->videoInfo->videoCodecCtx;
    int got_frame;
    //解码AVPacket->AVFrameav_gettime
    avcodec_decode_video2(codec_ctx, yuv_frame, &got_frame, packet);
    //Zero if no frame could be decompressed
    //非零，正在解码
    if (got_frame) {
        //lock
        //设置缓冲区的属性（宽、高、像素格式）
        ANativeWindow_setBuffersGeometry(pInfo->videoInfo->nativeWindow, codec_ctx->width,
                                         codec_ctx->height, WINDOW_FORMAT_RGBA_8888);
        ANativeWindow_lock(pInfo->videoInfo->nativeWindow, &outBuffer, NULL);

        //设置rgb_frame的属性（像素格式、宽高）和缓冲区
        //rgb_frame缓冲区与outBuffer.bits是同一块内存
        avpicture_fill((AVPicture *) rgb_frame, (const uint8_t *) (outBuffer.bits), AV_PIX_FMT_RGBA,
                       codec_ctx->width, codec_ctx->height);

        //YUV->RGBA_8888
        libyuv::I420ToARGB(yuv_frame->data[0], yuv_frame->linesize[0],
                           yuv_frame->data[2], yuv_frame->linesize[2],
                           yuv_frame->data[1], yuv_frame->linesize[1],
                           rgb_frame->data[0], rgb_frame->linesize[0],
                           codec_ctx->width, codec_ctx->height);

        //计算延迟
        int64_t pts = av_frame_get_best_effort_timestamp(yuv_frame);
        //转换（不同时间基时间转换）
        int64_t time = av_rescale_q(pts, stream->time_base, AV_TIME_BASE_Q);

        // 等待视频帧
        playerWaitForFrame(playerInfo, time, pInfo->videoInfo->videoStreamIndex);

        //unlock
        ANativeWindow_unlockAndPost(pInfo->videoInfo->nativeWindow);
    }

    av_frame_free(&yuv_frame);
    av_frame_free(&rgb_frame);
}

void decodeAudio(PlayerInfo *pInfo, AVPacket *packet) {
    AVFormatContext *input_format_ctx = pInfo->avFormatContext;
    AVStream *stream = input_format_ctx->streams[pInfo->audioInfo->audioStreamIndex];

    AVCodecContext *codec_ctx = pInfo->audioInfo->audioCodecCtx;
    LOGE("%s", "decode_audio");
    //解压缩数据
    AVFrame *frame = av_frame_alloc();
    int got_frame;
    avcodec_decode_audio4(codec_ctx, frame, &got_frame, packet);

    //16bit 44100 PCM 数据（重采样缓冲区）
    uint8_t *out_buffer = (uint8_t *) av_malloc(MAX_AUDIO_FRME_SIZE);
    //解码一帧成功
    if (got_frame > 0) {
        swr_convert(pInfo->audioInfo->swr_ctx, &out_buffer, MAX_AUDIO_FRME_SIZE,
                    (const uint8_t **) frame->data, frame->nb_samples);
        //获取sample的size
        int out_buffer_size = av_samples_get_buffer_size(NULL, pInfo->audioInfo->out_channel_nb,
                                                         frame->nb_samples,
                                                         pInfo->audioInfo->out_sample_fmt, 1);

        int64_t pts = packet->pts;
        if (pts != AV_NOPTS_VALUE) {
            pInfo->audioInfo->audioClock = av_rescale_q(pts, stream->time_base, AV_TIME_BASE_Q);
            //				av_q2d(stream->time_base) * pts;
            LOGE("player_write_audio - read from pts");
        }

        //关联当前线程的JNIEnv
        JavaVM *javaVM = pInfo->jniInfo->javaVM;
        JNIEnv *env;
        javaVM->AttachCurrentThread(&env, NULL);

        //out_buffer缓冲区数据，转成byte数组
        jbyteArray audio_sample_array = env->NewByteArray(out_buffer_size);
        jbyte *sample_bytep = env->GetByteArrayElements(audio_sample_array, NULL);
        //out_buffer的数据复制到sampe_bytep
        memcpy(sample_bytep, out_buffer, out_buffer_size);
        //同步
        env->ReleaseByteArrayElements(audio_sample_array, sample_bytep, 0);

        //AudioTrack.write PCM数据
        env->CallIntMethod(pInfo->jniInfo->audioTrack, pInfo->jniInfo->audioTrackPlayMethodId,
                           audio_sample_array, 0, out_buffer_size);
        //释放局部引用
        env->DeleteLocalRef(audio_sample_array);

        javaVM->DetachCurrentThread();

//        usleep(1000 * 16);
    }

    av_frame_free(&frame);
}
/***************************************************************************************************************/
// //////////////////////////////////消费///////////////////////////////////////////////////
void consumeFormQueue() {
    //消费者线程
    DecoderData videoData = {playerInfo, playerInfo->videoInfo->videoStreamIndex};
    pthread_create(&(playerInfo->threadInfo->consumeIds[playerInfo->videoInfo->videoStreamIndex]),
                   NULL, decodeData, (void *) &videoData);

    DecoderData audioData = { playerInfo, playerInfo->audioInfo->audioStreamIndex};
    pthread_create(&(playerInfo->threadInfo->consumeIds[playerInfo->audioInfo->audioStreamIndex]),
                   NULL, decodeData, (void *) &audioData);

//    pthread_join(playerInfo->threadInfo->readStreamId,NULL);
//    pthread_join(playerInfo->threadInfo->consumeIds[playerInfo->videoInfo->videoStreamIndex],NULL);
//    pthread_join(playerInfo->threadInfo->consumeIds[playerInfo->audioInfo->audioStreamIndex],NULL);
}


/*********************************************************************************************************/
void freePlayerInfo() {
    free(playerInfo->audioInfo);
    free(playerInfo->videoInfo);
    free(playerInfo->jniInfo);
    free(playerInfo->threadInfo);

}
