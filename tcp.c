#include "tcp.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <opencv2/opencv.hpp>
#include <pthread.h>

int video_socket = -1;   // 视频数据 socket
int data_socket = -1;    // 其他数据 socket // TCP 客户端 socket（初始为-1，未连接）
int dis_socket = -1;   
int running = 1;         // TCP 运行标志（1：运行，0：停止）
int tcp_connect(int socket_fd, const char* host, uint16_t port);

int tcp_send_packet(int socket_fd, unsigned data_type, uint32_t data_len, const void* data) {
    // 参数合法性检查
    if (socket_fd < 0 || data == NULL || data_len == 0) {
        fprintf(stderr, "发送参数无效(socket:%d,数据长度：%u)\n", socket_fd, data_len);
        return -1;
    }

    // 计算数据包总大小：1字节类型 + 4字节长度 + 实际数据长度
    size_t packet_size = 1 + 4 + data_len;
    uchar* packet = (uchar*)malloc(packet_size);
    if (!packet) {
        perror("数据包内存分配失败");
        return -1;
    }

    // 构建数据包
    packet[0] = data_type;  // 数据类型
    uint32_t net_len = htonl(data_len);  // 转换长度为网络字节序
    memcpy(packet + 1, &net_len, 4);     // 复制长度字段
    memcpy(packet + 5, data, data_len);  // 复制实际数据

    // 发送数据包
    ssize_t send_len = send(socket_fd, packet, packet_size, 0);
    if (send_len != (ssize_t)packet_size) {
        perror("数据包发送失败");
        free(packet);
        return -1;
    }

    // 打印发送成功信息
    //printf("数据包发送成功 - 类型：%hhu，数据长度：%u字节，总包长：%zu字节\n",
      //     data_type, data_len, packet_size);
    
    // 释放内存
    free(packet);
    return 0;
}
//外部函数实现
int tcp_init() {
    // 先清理可能存在的旧连接
    tcp_cleanup();

    // 创建视频socket
    video_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (video_socket == -1) {
        perror("创建视频socket失败");
        return -1;
    }

    // 创建数据socket
    data_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (data_socket == -1) {
        perror("创建数据socket失败");
        close(video_socket);
        video_socket = -1;
        return -1;
    }
    dis_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (dis_socket == -1) {
        perror("创建距离socket失败");
        close(video_socket);
        video_socket = -1;
        close(data_socket);
        data_socket = -1;
        return -1;
    }
    
    // 连接视频端口
    if (tcp_connect(video_socket, HOST, VIDEO_PORT) == -1) {
        perror("连接视频端口失败");
        tcp_cleanup();
        return -1;
    }

    // 连接数据端口
    if (tcp_connect(data_socket, HOST, DATA_PORT) == -1) {
        perror("连接数据端口失败");
        tcp_cleanup();
        return -1;
    }
      if (tcp_connect(dis_socket, HOST, DIS_PORT) == -1) {
        perror("连接距离端口失败");
        tcp_cleanup();
        return -1;
    }

    printf("已连接到 Qt 端 - 视频端口：%s:%d，数据端口：%s:%d，距离端口：%s:%d\n", 
           HOST, VIDEO_PORT, HOST, DATA_PORT, HOST, DIS_PORT);
    running = 1;
    return 0;
}

int tcp_receive_commands(void (*command_handler)(const char*)) {
    // 检查参数：命令处理回调函数必须有效
    if (!command_handler) {
        fprintf(stderr, "命令处理回调函数未设置\n");
        return -1;
    }

    uint32_t cmd_len;  // 命令长度（网络字节序接收，主机字节序处理）
    char* cmd_buf = NULL;

    // 循环接收命令
    while (running) {
        // 1. 接收命令长度（前4字节）
        ssize_t recv_len = recv(data_socket, &cmd_len, 4, 0);
        if (recv_len != 4) {
            fprintf(stderr, "接收命令长度失败（实际接收：%zd字节）\n", recv_len);
            //running = 0;
            //stop();
            break;
        }
        cmd_len = ntohl(cmd_len);  // 转为主机字节序

        // 2. 分配命令缓冲区（+1 用于字符串结束符）
        cmd_buf = (char*)realloc(cmd_buf, cmd_len + 1);
        if (!cmd_buf) {
            perror("命令缓冲区内存分配失败");
            continue;
        }

        // 3. 接收命令内容（处理粘包：确保接收完整）
        ssize_t total_recv = 0;
        while (total_recv < (ssize_t)cmd_len) {
            recv_len = recv(data_socket, cmd_buf + total_recv, 
                           cmd_len - total_recv, 0);
            if (recv_len <= 0) {
                fprintf(stderr, "接收命令内容失败（实际接收：%zd字节）\n", recv_len);
                //running = 0;
                //stop();
                cmd_buf = NULL;
                goto exit_loop;  // 连接异常，退出循环
            }
            total_recv += recv_len;
        }

        // 4. 处理命令（添加结束符，调用回调函数）
        cmd_buf[cmd_len] = '\0';
        printf("收到命令：%s(长度：%u字节)\n", cmd_buf, cmd_len);
        handleCommand(cmd_buf);  // 调用main.c中的命令处理函数
        free(cmd_buf);
        cmd_buf = NULL;
    }

exit_loop:
    // 清理资源
    free(cmd_buf);
    cmd_buf = NULL;  // 防止重复释放
    printf("命令接收线程已结束\n");
    return running ? -1 : 0;  // 正常退出返回0，异常退出返回-1
}

void tcp_cleanup() {
    // 1. 设置停止标志
    running = 0;

    // 2. 关闭socket
    if (video_socket >= 0) {
        close(video_socket);
        video_socket = -1;
        printf("视频TCP socket已关闭\n");
    }
    if (data_socket >= 0) {
        close(data_socket);
        data_socket = -1;
        printf("数据TCP socket已关闭\n");
    }
    if (dis_socket >= 0) {
        close(dis_socket);
        dis_socket = -1;
        printf("距离TCP socket已关闭\n");
    }
}

int tcp_connect(int socket_fd, const char* host, uint16_t port) {
    if (socket_fd < 0 || host == NULL || port == 0) {
        fprintf(stderr, "连接参数无效(socket:%d, 主机:%s, 端口:%u)\n", 
                socket_fd, host, port);
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);  // 转换端口号为网络字节序

    // 转换IP地址为网络字节序
    if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
        perror("无效的IP地址");
        return -1;
    }

    // 连接到目标主机和端口
    if (connect(socket_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("连接失败");
        return -1;
    }

    return 0;
}