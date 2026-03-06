#!/usr/bin/env python3
import tflite_runtime.interpreter as tflite
import cv2
import numpy as np
import RPi.GPIO as GPIO
import time
import signal
import socket

# TCP服务器地址和端口（C程序运行的地址）
HOST = 'localhost'
PORT = 65432
# 图像尺寸
WIDTH = 224
HEIGHT = 224
BUFFER_SIZE = WIDTH * HEIGHT * 3  # 224x224 RGB

# 电机引脚定义（BCM模式）
ENA = 16
ENB = 12
IN1 = 19
IN2 = 13
IN3 = 26
IN4 = 20

# 全局变量
pwm_ena = None
pwm_enb = None
running = True


def init_gpio():
    """初始化GPIO和PWM"""
    global pwm_ena, pwm_enb
    GPIO.setmode(GPIO.BCM)
    for pin in [ENA, ENB, IN1, IN2, IN3, IN4]:
        GPIO.setup(pin, GPIO.OUT)
    pwm_ena = GPIO.PWM(ENA, 100)
    pwm_enb = GPIO.PWM(ENB, 100)
    pwm_ena.start(0)
    pwm_enb.start(0)


def cleanup_resources():
    """安全清理所有资源"""
    global pwm_ena, pwm_enb
    print("开始清理资源...")
    if pwm_ena is not None:
        pwm_ena.ChangeDutyCycle(0)
        pwm_ena.stop()
    if pwm_enb is not None:
        pwm_enb.ChangeDutyCycle(0)
        pwm_enb.stop()
    if GPIO.getmode() is not None:
        GPIO.output(IN1, GPIO.LOW)
        GPIO.output(IN2, GPIO.LOW)
        GPIO.output(IN3, GPIO.LOW)
        GPIO.output(IN4, GPIO.LOW)
        GPIO.cleanup()
    print("资源已释放")


def signal_handler(sig, frame):
    """信号处理函数"""
    global running
    print(f"收到信号 {sig}，准备退出...")
    running = False


def main():
    signal.signal(signal.SIGINT, signal_handler)
    init_gpio()

    # 连接C程序的TCP服务器
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        s.connect((HOST, PORT))
        print(f"已连接到C语言服务器 {HOST}:{PORT}")
    except Exception as e:
        print(f"连接服务器失败: {e}")
        cleanup_resources()
        return

    # 加载TFLite模型
    try:
        interpreter = tflite.Interpreter(model_path="line_follower.tflite")
        interpreter.allocate_tensors()
        input_details = interpreter.get_input_details()
        output_details = interpreter.get_output_details()
    except Exception as e:
        print(f"模型加载失败: {e}")
        s.close()
        cleanup_resources()
        return

    i = 0
    try:
        while running:
            try:
                # 接收图像数据
                data = b''
                while len(data) < BUFFER_SIZE:
                    packet = s.recv(BUFFER_SIZE - len(data))
                    if not packet:
                        raise ConnectionError("与C程序的连接已断开")
                    data += packet

                # 转为numpy数组
                frame = np.frombuffer(data, dtype=np.uint8).reshape((HEIGHT, WIDTH, 3))

                # 预处理图像
                input_frame = frame / 255.0
                input_frame = np.expand_dims(input_frame, axis=0).astype(np.float32)

                # 推理
                interpreter.set_tensor(input_details[0]['index'], input_frame)
                interpreter.invoke()
                output = interpreter.get_tensor(output_details[0]['index'])
                pred_class = np.argmax(output)
                confidence = np.max(output)

                # 电机控制逻辑
                if confidence > 0.7:
                    if pred_class == 0:  # 直行
                        print("准备直行，先停止2秒")
                        pwm_ena.ChangeDutyCycle(0)
                        pwm_enb.ChangeDutyCycle(0)
                        time.sleep(2)
                        print(f'直行 {i}')
                        GPIO.output(IN1, GPIO.HIGH)
                        GPIO.output(IN2, GPIO.LOW)
                        GPIO.output(IN3, GPIO.HIGH)
                        GPIO.output(IN4, GPIO.LOW)
                        pwm_ena.ChangeDutyCycle(15)
                        pwm_enb.ChangeDutyCycle(16)
                    elif pred_class == 1:  # 左转
                        print("准备左转，先停止2秒")
                        pwm_ena.ChangeDutyCycle(0)
                        pwm_enb.ChangeDutyCycle(0)
                        time.sleep(5)
                        print(f"左转 {i}")
                        GPIO.output(IN1, GPIO.HIGH)
                        GPIO.output(IN2, GPIO.LOW)
                        GPIO.output(IN3, GPIO.HIGH)
                        GPIO.output(IN4, GPIO.LOW)
                        pwm_ena.ChangeDutyCycle(9)
                        pwm_enb.ChangeDutyCycle(24)
                    elif pred_class == 2:  # 右转
                        print("准备右转，先停止2秒")
                        pwm_ena.ChangeDutyCycle(0)
                        pwm_enb.ChangeDutyCycle(0)
                        time.sleep(5)
                        print(f"右转 {i}")
                        GPIO.output(IN1, GPIO.HIGH)
                        GPIO.output(IN2, GPIO.LOW)
                        GPIO.output(IN3, GPIO.HIGH)
                        GPIO.output(IN4, GPIO.LOW)
                        pwm_ena.ChangeDutyCycle(22)
                        pwm_enb.ChangeDutyCycle(10)
                else:
                    GPIO.output(IN1, GPIO.LOW)
                    GPIO.output(IN2, GPIO.LOW)
                    GPIO.output(IN3, GPIO.LOW)
                    GPIO.output(IN4, GPIO.LOW)
                    pwm_ena.ChangeDutyCycle(0)
                    pwm_enb.ChangeDutyCycle(0)
                    print(f"电机已停止 - 不确定方向 {i}")

                i += 1
                time.sleep(0.1)

            except Exception as e:
                print(f"帧处理错误: {e}")
                break

    except Exception as e:
        print(f"主循环错误: {e}")

    finally:
        running = False
        s.close()
        cleanup_resources()


if __name__ == "__main__":
    main()