#ifndef TAH_H
#define TAH_H

#include <stdint.h>
// 定义DHT11连接的wiringPi引脚
#define DHT_PIN 24
#define DATA_TYPE_TEMP 3          // 视频数据类型标识
#define DATA_TYPE_HUM 4
void *dht11_thread(void *); 
#endif