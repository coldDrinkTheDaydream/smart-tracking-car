#ifndef TCP_H
#define TCP_H

#include <stdint.h>
#include <sys/socket.h>

// 设置目标（Qt 接收端）的 IP 和端口
#define HOST "192.168.210.184"  // 替换为 Qt 端的 IP
#define VIDEO_PORT 12345        // 视频数据端口
#define DATA_PORT 12346         // 其他数据端口
#define DIS_PORT 12347  
#define TCP_BUFFER_SIZE 1024        // 数据缓冲区大小
#endif