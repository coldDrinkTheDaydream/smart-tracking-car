import tflite_runtime.interpreter as tflite
import cv2
import numpy as np
import RPi.GPIO as GPIO
import time
import struct
import os
import sys
import socket
import signal # 用于捕获终止信号
client_socket = None

# 从命令行参数获取已打开的socket文件描述符
def get_socket_from_fd():
    if len(sys.argv) < 2:
        print("请提供socket文件描述符作为参数")
        return None
        
    try:
        fd = int(sys.argv[1])
        # 验证文件描述符有效性（避免无效FD）
        os.fstat(fd)
        # 从文件描述符创建socket对象
        global client_socket
        client_socket = socket.fromfd(fd, socket.AF_INET, socket.SOCK_STREAM)
        print(f"成功获取socket（FD：{fd}）")
        return client_socket
    except ValueError:
        print("错误：socket文件描述符不是整数")
        return None
    except OSError as e:
        print(f"错误：socket文件描述符无效（{e}）")
        return None
    except Exception as e:
        print(f"错误：创建socket失败（{e}）")
        return None

def signal_handler(signum, frame):
    """信号捕获函数：收到SIGTERM时优雅退出"""
    print(f"\n收到终止信号（SIGTERM），正在清理资源...")
    # 新增：退出虚拟环境
    try:
        # 使用bash执行deactivate命令
        subprocess.run(["bash", "-c", "deactivate"], check=True)
        print("虚拟环境已退出")
    except subprocess.CalledProcessError as e:
        print(f"退出虚拟环境失败：{e}")
    # 1. 关闭socket（避免连接残留）
    if client_socket:
        client_socket.close()
        print("socket已关闭")
    # 2. 其他清理逻辑（如保存数据、关闭文件等）
    # ...（根据你的业务需求添加）
    print("资源清理完成，脚本退出")
    sys.exit(0) # 正常退出


# 电机控制引脚定义（BCM模式）
ENA = 16  # 左电机使能 (PWM)
ENB = 12  # 右电机使能 (PWM)
IN1 = 19  # 左电机正转
IN2 = 13  # 左电机反转
IN3 = 26  # 右电机正转
IN4 = 20  # 右电机反转

# 初始化GPIO和PWM变量
pwm_ena = None
pwm_enb = None

def init_gpio():
    """初始化GPIO和PWM"""
    global pwm_ena, pwm_enb
    GPIO.setmode(GPIO.BCM)
    for pin in [ENA, ENB, IN1, IN2, IN3, IN4]:
        GPIO.setup(pin, GPIO.OUT)
    
    # 设置PWM，频率为100Hz
    pwm_ena = GPIO.PWM(ENA, 100)
    pwm_enb = GPIO.PWM(ENB, 100)
    
    # 初始化为0，电机不转
    pwm_ena.start(0)
    pwm_enb.start(0)

def cleanup_gpio():
    """安全清理GPIO资源"""
    global pwm_ena, pwm_enb
    
    # 先停止电机
    if pwm_ena is not None:
        pwm_ena.ChangeDutyCycle(0)
        pwm_ena.stop()
        pwm_ena = None
        
    if pwm_enb is not None:
        pwm_enb.ChangeDutyCycle(0)
        pwm_enb.stop()
        pwm_enb = None
    
    # 清理GPIO
    GPIO.output(IN1, GPIO.LOW)
    GPIO.output(IN2, GPIO.LOW)
    GPIO.output(IN3, GPIO.LOW)
    GPIO.output(IN4, GPIO.LOW)
    
    GPIO.cleanup()

def run_auto_mode(client_socket):
    # 初始化GPIO
    init_gpio()
    
    # 加载TFLite模型
    try:
        interpreter = tflite.Interpreter(model_path="line_follower.tflite")
        interpreter.allocate_tensors()
        input_details = interpreter.get_input_details()
        output_details = interpreter.get_output_details()
    except Exception as e:
        print(f"模型加载失败: {e}")
        cleanup_gpio()
        return
    
    
    # 初始化摄像头
    cap = cv2.VideoCapture(0)
    if not cap:
        cleanup_gpio()
        return
    
    try:
        while True:
            ret, frame = cap.read()
            if not ret:
                print("无法获取图像帧，重试...")
                # 尝试重新打开摄像头
                cap.release()
                cap = init_camera()
                if not cap or not cap.isOpened():
                    print("重新打开摄像头失败，程序将退出")
                    break
                time.sleep(0.5)
                continue
            
            # 编码图像为JPEG并发送
            _, buffer = cv2.imencode('.jpg', frame)
            data = buffer.tobytes()
            
            # 发送数据长度和图像数据
            size = len(data)
            data_type = 1
            try:
                buffer = struct.pack("!B", data_type) + struct.pack("!I", size) + data
                client_socket.sendall(buffer)
            except Exception as e:
                print(f"发送数据出错: {e}")
                break
            
            # 预处理图像
            input_frame = cv2.resize(frame, (224, 224))
            input_frame = input_frame / 255.0
            input_frame = np.expand_dims(input_frame, axis=0).astype(np.float32)
            
            # 推理
            interpreter.set_tensor(input_details[0]['index'], input_frame)
            interpreter.invoke()
            output = interpreter.get_tensor(output_details[0]['index'])
            pred_class = np.argmax(output)
            confidence = np.max(output)
            
            # 控制逻辑
            if confidence > 0.7:
                if pred_class == 0:  # 直行
                    print("直行")
                    GPIO.output(IN1, GPIO.HIGH)
                    GPIO.output(IN2, GPIO.LOW)
                    GPIO.output(IN3, GPIO.HIGH)
                    GPIO.output(IN4, GPIO.LOW)
                    pwm_ena.ChangeDutyCycle(15)
                    pwm_enb.ChangeDutyCycle(16)
                elif pred_class == 1:  # 左转
                    print("左转")
                    GPIO.output(IN1, GPIO.HIGH)
                    GPIO.output(IN2, GPIO.LOW)
                    GPIO.output(IN3, GPIO.HIGH)
                    GPIO.output(IN4, GPIO.LOW)
                    pwm_ena.ChangeDutyCycle(5)
                    pwm_enb.ChangeDutyCycle(17)
                elif pred_class == 2:  # 右转
                    print("右转")
                    GPIO.output(IN1, GPIO.HIGH)
                    GPIO.output(IN2, GPIO.LOW)
                    GPIO.output(IN3, GPIO.HIGH)
                    GPIO.output(IN4, GPIO.LOW)
                    pwm_ena.ChangeDutyCycle(22)
                    pwm_enb.ChangeDutyCycle(10)
            else:
                # 停止电机
                GPIO.output(IN1, GPIO.LOW)
                GPIO.output(IN2, GPIO.LOW)
                GPIO.output(IN3, GPIO.LOW)
                GPIO.output(IN4, GPIO.LOW)
                pwm_ena.ChangeDutyCycle(0)
                pwm_enb.ChangeDutyCycle(0)
                print("电机已停止 - 不确定方向")
            
            if cv2.waitKey(1) == ord('q'):
                break
                
            time.sleep(0.1)
            
    except KeyboardInterrupt:
        print("程序被用户中断")
    except Exception as e:
        print(f"发生错误: {e}")
    finally:
        # 确保所有资源都被正确释放
        if cap:
            cap.release()
        cv2.destroyAllWindows()
        cleanup_gpio()
        print("资源已释放")

if __name__ == "__main__":
    # 1. 注册信号捕获：监听SIGTERM（C端kill命令发送的信号）
    signal.signal(signal.SIGTERM, signal_handler)
     # 2. 获取socket对象（与上位机通信）
    if not get_socket_from_fd():
        sys.exit(1) # 无法获取socket，直接退出
    run_auto_mode(client_socket)
        
    