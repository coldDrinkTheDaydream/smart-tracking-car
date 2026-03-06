#include <wiringPi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>  // 包含 htonl()、ntohl() 等字节序转换函数的声明
#include "tah.h"
#include "common.h"

// 全局变量用于存储温湿度数据
int hum_int = 0, hum_dec = 0, temp_int = 0, temp_dec = 0;
void send_tah_data(unsigned char type,int tah_int, int tah_frac);

// 函数声明：读取DHT11一个bit的数据
uint8_t read_dht11_bit(void) {
    uint8_t count = 0;
    // 等待总线从高变低（跳过初始高电平）
    while (digitalRead(DHT_PIN) == HIGH) {
        delayMicroseconds(1);
        count++;
        if (count > 100) return 0; // 超时，返回错误
    }
    // 等待总线从低变高（数据位开始）
    while (digitalRead(DHT_PIN) == LOW) {
        delayMicroseconds(1);
        count++;
        if (count > 100) return 0; // 超时，返回错误
    }
    // 记录高电平持续时间：>50us为1，<50us为0
    delayMicroseconds(30); 
    return (digitalRead(DHT_PIN) == HIGH) ? 1 : 0;
}

// 函数声明：读取DHT11完整数据
int read_dht11_data(int *humidity_int, int *humidity_dec, 
                    int *temperature_int, int *temperature_dec) {
    uint8_t buf[5] = {0}; // 存储5个8位数据（共40位）
    uint8_t i, j;

    // 1. 主机发送请求：拉低总线至少18ms，再拉高20-40us
    pinMode(DHT_PIN, OUTPUT);
    digitalWrite(DHT_PIN, LOW);
    delay(20); // 拉低20ms（满足>18ms要求）
    digitalWrite(DHT_PIN, HIGH);
    delayMicroseconds(30); // 拉高30us
    pinMode(DHT_PIN, INPUT); // 切换为输入，等待DHT11响应

    // 2. 等待DHT11响应：先拉低80us，再拉高80us
    if (digitalRead(DHT_PIN) == LOW) {
        // 等待响应低电平结束
        while (digitalRead(DHT_PIN) == LOW);
        // 等待响应高电平结束
        while (digitalRead(DHT_PIN) == HIGH);
        
        // 3. 读取40位数据（5个字节）
        for (i = 0; i < 5; i++) {
            for (j = 0; j < 8; j++) {
                buf[i] <<= 1; // 左移1位，准备接收下一位
                buf[i] |= read_dht11_bit(); // 读取1位并写入
            }
        }

        // 4. 校验数据：前4字节之和 == 第5字节（校验和）
        if (buf[0] + buf[1] + buf[2] + buf[3] == buf[4]) {
            *humidity_int = buf[0];    // 湿度整数部分
            *humidity_dec = buf[1];    // 湿度小数部分（DHT11小数通常为0）
            *temperature_int = buf[2]; // 温度整数部分
            *temperature_dec = buf[3]; // 温度小数部分（DHT11小数通常为0）
            return 0; // 读取成功
        } else {
            printf("数据校验失败！\n");
            return -1; // 校验失败
        }
    } else {
        printf("DHT11无响应！\n");
        return -2; // 无响应
    }
}

// 线程函数：读取温湿度数据
void *dht11_thread(void *) {
    int ret;  
    while (running) {
    // 读取DHT11数据
    ret = read_dht11_data(&hum_int, &hum_dec, &temp_int, &temp_dec);
    if (ret == 0) {
       // 输出结果
        //printf("温度：%d.%d℃  |  湿度：%d.%d%%RH\n", temp_int, temp_dec, hum_int, hum_dec);
        send_tah_data(DATA_TYPE_TEMP,temp_int,temp_dec);
        usleep(80000);  // 暂停50毫秒
        send_tah_data(DATA_TYPE_HUM,hum_int,hum_dec);
    } else {
        printf("读取失败，错误码：%d\n", ret);
    }
    // 等待10秒再进行下一次读取（DHT11建议采样周期不小于1秒）
    delay(10000);
}
return NULL;
}
void send_tah_data(unsigned char type,int tah_int, int tah_frac) {
    int tah_parts[2]; 
    tah_parts[0] = htonl(tah_int);   // 整数部分转网络字节序
    tah_parts[1] = htonl(tah_frac);  // 小数部分转网络字节序
    // 3. 发送数据包
   tcp_send_packet(data_socket,type, sizeof(tah_parts), tah_parts);
}