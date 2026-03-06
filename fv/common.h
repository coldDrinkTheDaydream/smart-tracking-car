#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <sys/socket.h>

extern int running;        // TCP 运行标志（控制线程启停）
extern int video_socket;   // 视频数据 socket
extern int data_socket;    // 其他数据 socket // TCP 客户端 socket（初始为-1，未连接）
extern int dis_socket;  

void handleCommand(const char* command);
// 发送视频数据包
int tcp_send_packet(int socket_fd, unsigned data_type, uint32_t data_len, const void* data);
int tcp_init();
int tcp_receive_commands(void (*command_handler)(const char*));
void tcp_cleanup();
#endif