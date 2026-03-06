#ifndef AVOID_H
#define AVOID_H

// 超声波传感器引脚
#define TRIG 17
#define ECHO 27
// 舵机引脚
#define SERVO_PIN 22
#define DATA_TYPE_DIS 5

void initHc_sr04();
void initServo();
void *tcp_send_dis_thread(void *);
void avoid();
void stopAvoid();
#endif