#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <wiringPi.h>
#include <sys/types.h>
#include "motor.h"
#include "common.h" //包含TCP通信头文件
#include "video.h"
#include "tah.h"
#include <signal.h> // 用于发送信号
#include "avoid.h"

#define BEEP 25

// 函数声明（仅保留电机控制和命令处理）
void process_spinbox_value(int value);
void execute_auto_command();

int main() {
    pthread_t tah_thread;
    pthread_t dis_thread;

    // 初始化wiringPi
    if (wiringPiSetupGpio() < 0) {
        perror("初始化wiringPi失败");
        exit(1);
    }
    // 1. 初始化电机（原有逻辑）
    if (initMotors() != 0) {
        fprintf(stderr, "电机初始化失败，程序退出\n");
        exit(EXIT_FAILURE);
    }
    initHc_sr04();
    initServo();
    pinMode(BEEP, OUTPUT);
    digitalWrite(BEEP, 0);

    // 2. 初始化TCP（连接Qt端）
    if (tcp_init() != 0) {
        fprintf(stderr, "TCP初始化失败，程序退出\n");
        stop();  // 停止电机
        exit(EXIT_FAILURE);
    }
    send_motor_speed(speed) ;
    if (pthread_create(&tah_thread, NULL, dht11_thread, NULL) != 0) {
        perror("创建温湿度发送线程失败");
        tcp_cleanup();
        stop();
        exit(EXIT_FAILURE);
    }
    if (pthread_create(&dis_thread, NULL, tcp_send_dis_thread, NULL) != 0) {
        perror("创建距离发送线程失败");
        tcp_cleanup();
        stop();
        exit(EXIT_FAILURE);
    }
    // 3. 创建视频发送线程（调用TCP模块函数）
    if (pthread_create(&video_thread, NULL, tcp_send_video_thread, NULL) != 0) {
        perror("创建视频发送线程失败");
        tcp_cleanup();
        stop();
        exit(EXIT_FAILURE);
    }
    // 5. 主线程接收Qt命令（调用TCP模块函数，传入命令处理回调）
    tcp_receive_commands(handleCommand);

    // 6. 程序退出清理
    tcp_cleanup();                  // 释放TCP资源
    pthread_join(video_thread, NULL);  // 等待视频线程结束
    pthread_join(tah_thread, NULL);
    pthread_join(dis_thread, NULL);
    stop();                         // 停止电机
    pthread_mutex_destroy(&speed_mutex);  // 销毁速度互斥锁

    printf("程序正常退出，资源已释放\n");
    return 0;
}

// 命令处理函数（原有逻辑保留，无修改）
void handleCommand(const char* command) {
    if (strcmp(command, "forward") == 0) {
        forward();
    } else if (strcmp(command, "backward") == 0) {
        backward();
    } else if (strcmp(command, "left_turn") == 0) {
        turnLeft();
    } else if (strcmp(command, "right_turn") == 0) {
        turnRight();
    } else if (strcmp(command, "stop") == 0) {
        stop();
    } else if (strstr(command, "spinbox:") == command) {
        int value = atoi(command + strlen("spinbox:"));
        process_spinbox_value(value);
    } else if (strcmp(command, "ac") == 0) {
        accelerate();
    } else if (strcmp(command, "ac_cancel") == 0) {
        stopAccel();
    } else if (strcmp(command, "de") == 0) {
        decelerate();
    } else if (strcmp(command, "de_cancel") == 0) {
        stopDecel();
    } else if (strcmp(command, "beep")== 0){
        digitalWrite(BEEP,1);
        delay(200);                // 持续100毫秒（可根据需要调整）
        digitalWrite(BEEP,0);
    }else if (strcmp(command, "avoid") == 0) {
        avoid();
    }else if (strcmp(command, "avoid_cancel") == 0) {
        stopAvoid();
    }else if (strcmp(command, "auto") == 0) {
       execute_auto_command();
    } else if (strcmp(command, "auto_cancel") == 0) {
        system("pkill -f auto_control.py");
        initMotors; 
    }else{
        printf("未知命令: %s\n", command);
    }
}

// 处理SpinBox数值
void process_spinbox_value(int value) {
    printf("收到SpinBox数值: %d\n", value);
    pthread_mutex_lock(&speed_mutex);  // 加锁保护
    speed = value;
    pthread_mutex_unlock(&speed_mutex);  // 解锁
    setSpeed(); 
}
// 执行auto命令对应的脚本
void execute_auto_command() {
    printf("收到auto命令，开始执行自动控制脚本...\n");
    
    // 启动Python脚本，使用fork和execvp确保可以并发处理
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork失败");
        return;
    }
    
    if (pid == 0) {
        // 子进程执行Python脚本
        char *args[] = {"/home/pi/env/bin/python3",  "/home/pi/code/fv/raspberry_pi_inference.py", NULL};
        execvp(args[0], args);
        
        // 如果execvp返回，说明执行失败
        perror("执行脚本失败");
        exit(EXIT_FAILURE);
    }
}
