#include "video.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <opencv2/opencv.hpp>
#include <pthread.h>
#include <semaphore.h>

pthread_t video_thread;  // 定义线程变量
void* tcp_send_video_thread(void*) {
    // 1. 初始化摄像头
    cv::VideoCapture cap(0);
    if (!cap.isOpened()) {
        fprintf(stderr, "无法打开摄像头！\n");
        return NULL;
    }
    cv::Mat frame;
    std::vector<uchar> jpeg_buffer;  // 存储JPEG编码后的视频数据

    // 2. 循环发送视频数据
    while (running) {
        // 2.1 读取摄像头帧
        cap >> frame;
        if (frame.empty()) {
            fprintf(stderr, "摄像头读取失败，重试...\n");
            usleep(100000);  // 100ms后重试
            continue;
        }

        // 2.2 编码为JPEG（质量50，平衡清晰度与速度）
        jpeg_buffer.clear();
        bool encode_ok = cv::imencode(".jpg", frame, jpeg_buffer, 
                                     std::vector<int>{cv::IMWRITE_JPEG_QUALITY, 50});
        if (!encode_ok) {
            fprintf(stderr, "视频帧JPEG编码失败\n");
            usleep(100000);
            continue;
        }

        // 2.3 发送视频数据包（类型1）
        tcp_send_packet(video_socket, DATA_TYPE_VIDEO, jpeg_buffer.size(),jpeg_buffer.data());

        // 2.4 控制帧率（500ms/帧，与原有逻辑一致）
        usleep(500000);
    }

    // 3. 线程退出清理
    cap.release();
    printf("视频发送线程已结束\n");
    return NULL;
}
