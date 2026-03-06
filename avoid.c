#include <wiringPi.h>
#include <softPwm.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <math.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "motor.h"
#include "tcp.h"
#include "avoid.h"
#include "common.h"

// 舵机参数 - 可能需要根据实际舵机型号调整

#define SERVO_MIN 50      // 调整最小脉冲值
#define SERVO_MAX 250     // 调整最大脉冲值
#define MID_ANGLE 90      // 舵机中间角度（通常是90度）
#define MIN_DISTANCE 20.0 

int thread_running = 0;
int flag = 0; 
pthread_t avoid_thread;
 
void setServoAngle(int angle);
void scanSurroundings();
void avoidObstacel();
float getDistance();
void *tcp_send_dis_thread(void *);
void scanSurroundings(float *leftDist, float *rightDist);

// 初始化超声波传感器
void initHc_sr04() {
    pinMode(TRIG, OUTPUT);
    pinMode(ECHO, INPUT);
    digitalWrite(TRIG, LOW);
    delay(50);
}

// 初始化舵机
void initServo() {
    // 初始化wiringPi（确保在main函数中已经初始化）
    if (wiringPiSetupGpio() == -1) {  // 使用BCM引脚编号
        printf("wiringPi初始化失败！\n");
        exit(1);
    }
    
    pinMode(SERVO_PIN, PWM_OUTPUT);
    pwmSetMode(PWM_MODE_MS);
    pwmSetRange(1024);
    pwmSetClock(192);  // 调整时钟设置，50Hz: 19.2MHz / (192 * 1024) ≈ 50Hz
    printf("舵机初始化完成，引脚：%d，范围：%d-%d\n", SERVO_PIN, SERVO_MIN, SERVO_MAX);
    
    setServoAngle(MID_ANGLE);  // 初始角度
    delay(1000);
}

float getDistance()
{
    unsigned long t1, t2;
    unsigned long timeout; 
    
    // 发出触发信号
    digitalWrite(TRIG, 1);
    usleep(10);
    digitalWrite(TRIG, 0);
    
    // 等待回应信号，增加超时机制
    timeout = micros() + 30000; // 30ms超时
    while(digitalRead(ECHO) == LOW) {
        if(micros() > timeout) {
            return -1.0;
        }
    }
    
    t1 = micros();
    
    // 等待高电平结束，增加超时机制
    timeout = micros() + 30000; // 30ms超时
    while(digitalRead(ECHO) == HIGH) {
        if(micros() > timeout) {
            return -1.0;
        }
    }
    
    t2 = micros();
    digitalWrite(TRIG, 0);
    usleep(10000);
    
    return (t2 - t1) * 0.034 / 2;
}

// 线程函数
void *tcp_send_dis_thread(void *) {
    float dis;  
    while (running) {
        dis = getDistance();
        if(dis > 0){
            tcp_send_packet(dis_socket, DATA_TYPE_DIS, sizeof(float), &dis);
            delay(800);
        }   
    }
    return NULL;
}

void* avoid_thread_func(void* ){
    thread_running = 1;
    flag = 1;
    while(flag == 1 && thread_running == 1){
        if(flag == 0 || thread_running == 0){
            break;
        }
        float dis = getDistance();

        if(dis < MIN_DISTANCE){
            avoidObstacel();
        }
    }
    flag = 0;
    thread_running = 0;
    pthread_exit(NULL);
}

void avoid(){
    if (thread_running == 1) {
        printf("已有避障线程在运行，无需重复启动\n");
        return;
    }
    
    int ret = pthread_create(&avoid_thread, NULL, avoid_thread_func, NULL);

    if (ret != 0) {
        perror("创建避障线程失败");
        thread_running = 0;
        return;
    }
    printf("避障线程创建成功，主线程继续接收命令...\n");
 }

// 避障处理逻辑
void avoidObstacel() {
    float leftDist, rightDist;
    
    stop();
    printf("检测到障碍物，开始扫描环境...\n");
    
    scanSurroundings(&leftDist, &rightDist);
    printf("距离检测: 左=%.2f, 右=%.2f (安全距离: %.2f)\n",
           leftDist, rightDist, MIN_DISTANCE);
    
    if (leftDist >= rightDist && leftDist >= MIN_DISTANCE) {
            printf("左侧空间充足，向左转向...\n");
            backward();
            delay(1000);
            turnLeft(); 
            delay(1000);
    } else if (rightDist >= MIN_DISTANCE) {
            printf("右侧空间充足，向右转向...\n");
            backward();
            delay(1000);
            turnRight(); 
            delay(1000);
    } else {
            printf("两侧空间均不足，执行后退向左转操作...\n");
            backward();
            delay(1500);
            turnLeft(); 
            delay(1000);
        }
    stop();
    
    setServoAngle(MID_ANGLE);
    delay(200);
 }

// 设置舵机角度 - 增加调试信息
void setServoAngle(int angle) {
    // 限制角度在0~180°
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;
    
    // 角度转脉冲宽度
    int pulse = SERVO_MIN + (SERVO_MAX - SERVO_MIN) * angle / 180;
    printf("设置舵机角度: %d度, 脉冲值: %d\n", angle, pulse); // 调试信息
    
    pwmWrite(SERVO_PIN, pulse);
    delay(500);  // 增加延迟时间，确保舵机有足够时间转动到位
}

// 扫描周围环境
void scanSurroundings(float *leftDist, float *rightDist) {
    // 扫描左侧（舵机150°）
    setServoAngle(150);
    *leftDist = getDistance();
    for(int i=0; i<3 && *leftDist < 0; i++){
        printf("左侧测距重试：%d次\n", i+1);
        *leftDist = getDistance();
        delay(50);
    }
    printf("左侧距离: %.2f cm\n", *leftDist);
    
    // 扫描右侧（舵机30°）
    setServoAngle(30);
    *rightDist = getDistance();
    for(int i=0; i<3 && *rightDist < 0; i++){
        printf("右侧测距重试：%d次\n", i+1);
        *rightDist = getDistance();
        delay(50);
    }
    printf("右侧距离: %.2f cm\n", *rightDist);
    
    // 舵机回正
    setServoAngle(MID_ANGLE);
}

// 停止避障
void stopAvoid() {
    if (thread_running == 0) {
        printf("当前无避障线程在运行\n");
        return;
    }
    setServoAngle(MID_ANGLE);

    flag = 0;
    thread_running = 0;
    printf("已发送停止信号，等待避障线程结束...\n");

    pthread_join(avoid_thread, NULL);
    printf("避障线程已安全结束");

    flag = 0;
    printf("已取消避障");
}
