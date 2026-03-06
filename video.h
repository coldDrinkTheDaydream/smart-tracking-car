#ifndef VIDEO_H
#define VIDEO_H

#include <stdint.h>
#include <pthread.h>
#include <semaphore.h>
#define DATA_TYPE_VIDEO 1          // 视频数据类型标识
extern pthread_t video_thread;
void* tcp_send_video_thread(void* );
#endif