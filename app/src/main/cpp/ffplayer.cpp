#include <jni.h>
#include <string>
extern "C"{
#include "player.h"
}


extern "C"
JNIEXPORT void JNICALL
Java_com_demo_ffplayer_player_FFmpegPlayer_play(JNIEnv *env, jobject instance, jobject surface,jstring videoPath_) {
    const char *videoPath = env->GetStringUTFChars(videoPath_, 0);

    PlayerInfo* playerInfo = mallocPlayerInfo();

    initAVFormatContextAndFindStream(videoPath);

    // 打开解码器
    openCodecContext(playerInfo->videoInfo->videoStreamIndex);
    openCodecContext(playerInfo->audioInfo->audioStreamIndex);

    prepareVideoAndAudio(env,surface);

    findAudioTrackAndJVM(env,instance);

    produceFormStream();

    consumeFormQueue();

//    env->ReleaseStringUTFChars(videoPath_, videoPath);
}