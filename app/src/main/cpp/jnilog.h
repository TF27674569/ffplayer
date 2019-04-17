//
// Created by ubt on 2019/4/17.
//

#ifndef FFPLAYER_JNILOG_H
#define FFPLAYER_JNILOG_H
#include <android/log.h>

#define LOG_TAG "ffmpeg"
#define LOGE(FORMAT,...) __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,FORMAT,##__VA_ARGS__)

#endif //FFPLAYER_JNILOG_H
