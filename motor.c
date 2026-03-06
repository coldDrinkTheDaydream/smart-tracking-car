#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wiringPi.h>
#include <softPwm.h>
#include <pthread.h>
#include "motor.h"
#include "tcp.h"
#include "common.h" 
#include <arpa/inet.h> 

pthread_t accel_thread;  // 加速线程ID
int thread1_running = 0;  // 线程运行状态（0=未运行，1=运行中）
pthread_t decel_thread;  // 减速线程ID
int thread2_running = 0;  // 线程运行状态（0=未运行，1=运行中）
int speed = 20;
int max_speed = 100;       // 最大目标速度
int step = 2;           // 固定加速步长
int flag1 = 0;     
int flag2 = 0;
pthread_mutex_t speed_mutex = PTHREAD_MUTEX_INITIALIZER;  //速度互斥锁

// 初始化电机控制
int initMotors() {
    // 设置电机控制引脚为输出
    pinMode(ENA, OUTPUT);
    pinMode(ENB, OUTPUT);
    pinMode(IN1, OUTPUT);
    pinMode(IN2, OUTPUT);
    pinMode(IN3, OUTPUT);
    pinMode(IN4, OUTPUT);
    
    // 创建软PWMF
    if (softPwmCreate(ENA, 0, PWM_RANGE) != 0) {
        perror("ENA软PWM创建失败");
        exit(1);
    }
    if (softPwmCreate(ENB, 0, PWM_RANGE) != 0) {
        perror("ENB软PWM创建失败");
        exit(1);
    }
     // 初始停止状态
    stop();
    return 0; 
   
}

// 停止
void stop() {
    // 电机停转逻辑（所有控制引脚置低）
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, LOW);
    softPwmWrite(ENA, 0);
    softPwmWrite(ENB, 0);

    printf("电机已停止\n");
}
LRspeed actualspeed(int speed){
    LRspeed res;
    // 限制速度范围
    if(speed < 0) speed = 0;
    if(speed > 100) speed = 100;
    int lefts = speed + LeftPwmOffset;
    int rights =speed + RightPwmOffset;
    // 确保补偿后的速度在有效范围内
    lefts =(lefts >100)?100 : lefts;
    rights=(rights >100)?100:rights;
    lefts =(lefts<0)?0:lefts;
    rights =(rights<0)?0:rights;
    res.leftSpeed = lefts;
    res.rightSpeed =rights;
    return res;
}
// 前进
void forward() {
    LRspeed result = actualspeed(speed);

    // 电机正转逻辑
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);
    
    // 输出PWM速度信号
    softPwmWrite(ENA, result.leftSpeed);
    softPwmWrite(ENB, result.rightSpeed);
    printf("前进，左轮: %d%%, 右轮: %d%%\n", result.leftSpeed, result.rightSpeed);
}

// 后退
void backward() {
    LRspeed result = actualspeed(20);

    // 电机反转逻辑
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, HIGH);
    
    // 输出PWM速度信号
    softPwmWrite(ENA, result.leftSpeed);
    softPwmWrite(ENB, result.rightSpeed);
    printf("后退，左轮: %d%%, 右轮: %d%%\n",result.leftSpeed, result.rightSpeed);
}

// 左转
void turnLeft() {
    // 左轮减速或停止，右轮前进实现左转
    int rightSpeed = 25 + RightPwmOffset;
    // 确保补偿后的速度在有效范围内
    rightSpeed = (rightSpeed > 100) ? 100 : rightSpeed;
    rightSpeed = (rightSpeed < 0) ? 0 : rightSpeed;
    
    // 左转逻辑
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);
    
    // 输出PWM速度信号
    softPwmWrite(ENA, 0);
    softPwmWrite(ENB, rightSpeed);
    printf("左转，左轮: %d%%, 右轮: %d%%\n", 0, rightSpeed);
}

// 右转
void turnRight() {
    // 右轮减速或停止，左轮前进实现右转
    int leftSpeed = 25 + LeftPwmOffset;
    // 确保补偿后的速度在有效范围内
    leftSpeed = (leftSpeed > 100) ? 100 : leftSpeed;
    leftSpeed = (leftSpeed < 0) ? 0 : leftSpeed;

    // 右转逻辑
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);
    
    // 输出PWM速度信号
    softPwmWrite(ENA, leftSpeed);
    softPwmWrite(ENB, 0);
    printf("右转，左轮: %d%%, 右轮: %d%%\n", leftSpeed,0);
}

// 更新当前速度并实时应用到PWM
void setSpeed() {
    LRspeed result = actualspeed(speed);
    softPwmWrite(ENA,result.leftSpeed);
    softPwmWrite(ENB,result.rightSpeed);
    printf("左轮: %d%%, 右轮: %d%%\n", result.leftSpeed, result.rightSpeed);
}

void* accel_thread_func(void* ){
    thread1_running = 1;  // 标记线程开始运行
    int new_speed;
    flag1 =1 ;
    forward() ;
    // 加锁读取启动时的速度
    pthread_mutex_lock(&speed_mutex);
    printf("加速线程启动，当前speed = %d\n", speed);
    pthread_mutex_unlock(&speed_mutex);
    while(flag1 == 1 && thread1_running == 1){
        if(flag1==0|| thread1_running == 0){
            break;
        }
        pthread_mutex_lock(&speed_mutex);
        new_speed = speed + step;
        if(new_speed >= max_speed){
            new_speed = max_speed;
            printf("已达最大速度！");
            send_motor_speed(new_speed);
            pthread_mutex_unlock(&speed_mutex);
            break;
        }
        speed = new_speed;
        LRspeed result = actualspeed(speed);
        send_motor_speed(speed) ;
        pthread_mutex_unlock(&speed_mutex);
        softPwmWrite(ENA,result.leftSpeed);
        softPwmWrite(ENB,result.rightSpeed);
        printf("左轮: %d%%, 右轮: %d%%\n", result.leftSpeed, result.rightSpeed);
        // 可中断延时：2000ms内每1ms检查一次flag1（关键！）
        for(int i = 0; i < 2000 && flag1== 1; i++){
            usleep(1000);  // usleep单位是微秒，1000微秒=1毫秒
        }
    }
    flag1 =0;
    thread1_running = 0;
    pthread_exit(NULL);  // 线程退出
}
void accelerate(){
    // 启动加速：创建独立线程
    // 避免重复创建线程（若线程已在运行，直接返回）
    if (thread1_running == 1) {
        printf("已有加速线程在运行，无需重复启动\n");
        return;
    }

    // 创建加速线程（参数：线程ID、属性、任务函数、参数）
    int ret = pthread_create(&accel_thread, NULL, accel_thread_func, NULL);

    if (ret != 0) {
        perror("创建加速线程失败");
        thread1_running = 0;
        return;
    }
    printf("加速线程创建成功，主线程继续接收命令...\n");
 }

// 停止逐渐加速
void stopAccel() {
    // 若线程未运行，直接返回
    if (thread1_running == 0) {
        printf("当前无加速线程在运行\n");
        return;
    }

    // 设置状态，触发子线程循环退出
    flag1 = 0;
    thread1_running = 0;
    printf("已发送停止信号，等待加速线程结束...\n");

    // 等待线程安全退出（避免资源泄漏）
    pthread_join(accel_thread, NULL);
    printf("加速线程已安全结束");

    flag1 = 0;
    printf("已停止逐渐加速");
}

void* decel_thread_func(void* ){
    thread2_running = 1;  // 标记线程开始运行
    int new_speed;
    flag2 =1 ;
    forward() ;
    // 读取启动时的速度
    pthread_mutex_lock(&speed_mutex);
    printf("减速线程启动,当前speed = %d\n", speed);
    pthread_mutex_unlock(&speed_mutex);
    while(flag2 == 1 && thread2_running == 1){
        if(flag2 ==0|| thread2_running == 0){
            break;
        }
        new_speed = speed - step;
        if(new_speed <= 0){
            new_speed = 0;
            printf("已达最小速度！\n");
            send_motor_speed(new_speed) ;
            pthread_mutex_unlock(&speed_mutex);
            break;
        }
        speed = new_speed;
        LRspeed result = actualspeed(speed);
        send_motor_speed(speed) ;
        pthread_mutex_unlock(&speed_mutex);
        softPwmWrite(ENA,result.leftSpeed);
        softPwmWrite(ENB,result.rightSpeed);
        printf("左轮: %d%%, 右轮: %d%%\n", result.leftSpeed, result.rightSpeed);
        // 可中断延时：2000ms内每1ms检查一次flag2（关键！）
        for(int i = 0; i < 2000 && flag2 == 1; i++){
            usleep(1000);  // usleep单位是微秒，1000微秒=1毫秒
        }
    }
    flag2 =0;
    thread2_running = 0;
    pthread_exit(NULL);  // 线程退出
}
void decelerate(){
    // 启动减速：创建独立线程
    // 避免重复创建线程（若线程已在运行，直接返回）
    if (thread2_running == 1) {
        printf("已有减速线程在运行，无需重复启动\n");
        return;
    }

    // 创建减速线程（参数：线程ID、属性、任务函数、参数）
    int ret = pthread_create(&decel_thread, NULL, decel_thread_func, NULL);

    if (ret != 0) {
        perror("创建减速线程失败");
        thread2_running = 0;
        return;
    }
    printf("减速线程创建成功，主线程继续接收命令...\n");
 }

// 停止逐渐减速
void stopDecel() {
    // 若线程未运行，直接返回
    if (thread2_running == 0) {
        printf("当前无减速线程在运行\n");
        return;
    }

    // 设置状态，触发子线程循环退出
    flag2 = 0;
    thread2_running = 0;
    printf("已发送停止信号，等待减速线程结束...\n");

    // 等待线程安全退出（避免资源泄漏）
    pthread_join(decel_thread, NULL);
    printf("减速线程已安全结束");

    flag2 = 0;
    printf("已停止逐渐减速");
}

int send_motor_speed(int speed) {
    int net_speed = htonl(speed);
    int send_ret = tcp_send_packet(data_socket, DATA_TYPE_MOTOR_SPEED, sizeof(int), &net_speed);
    if (send_ret != 0) {
        fprintf(stderr, "发送speed失败(speed=%d)\n", speed);
    }
    return send_ret;
}