#ifndef MOTOR_H
#define MOTOR_H
#include <pthread.h>

// 电机引脚定义
#define ENA 16   // 左电机使能
#define ENB 12   // 右电机使能
#define IN1 19   // 左电机正转
#define IN2 13   // 左电机反转
#define IN3 26   // 右电机正转
#define IN4 20   // 右电机反转
// PWM相关定义
#define PWM_RANGE 100          // PWM范围
#define LeftPwmOffset 0        // 左电机PWM补偿
#define RightPwmOffset 0       // 右电机PWM补偿
#define DATA_TYPE_MOTOR_SPEED 2
// 定义左右轮速度结构体
typedef struct {
    int leftSpeed;
    int rightSpeed;
} LRspeed;

// 函数声明
int initMotors();
LRspeed actualspeed(int speed);
void forward();
void stop();
void backward();
void turnLeft();
void turnRight();
void setSpeed() ;
void* accel_thread_func(void* );
void accelerate();
void stopAccel();
void* decel_thread_func(void* );
void decelerate();
void stopDecel();
int send_motor_speed(int speed);
extern int speed;
extern pthread_mutex_t speed_mutex;

#endif