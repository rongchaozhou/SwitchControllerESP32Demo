#include <Arduino.h>
#include <SwitchControllerESP32.h> // 引入该特定蓝牙库

// 定义 BOOT 键对应的引脚，ESP32-S3 固定的 BOOT 键通常为 GPIO 0
const int BOOT_BUTTON_PIN = 0; 

// 运行控制状态量：true 代表继续按 A，false 代表停止
int runningSts = 0; //0初始状态,此时会不停按B; 1暂停状态, 此时什么都不做; 2业务状态,此时执行业务代码

void setup() {
    // 1. 初始化手柄底层蓝牙配置
    switchcontrolleresp32_init();

    // 2. 启动原生 USB 栈支持（该库要求必须调用它来初始化描述符）
    USB.begin();

    // 3. 复位手柄状态为全未按下
    switchcontrolleresp32_reset();

    // 4. 初始化 BOOT 键引脚为上拉输入模式（BOOT 键未按下时为高电平，按下时为低电平）
    pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
}

void loop() {
    // 0初始状态,此时会不停按B
    if (runningSts == 0) {
        // 检查在等待的 3 秒内，BOOT 键是否被按下
        // 为了防止 delay 堵塞导致无法读取按键，我们将 3000ms 拆分成小段进行实时检测
        for (int i = 0; i < 30; i++) {
            // 如果检测到 BOOT 键被按下（低电平）
            if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
                runningSts = 1; // 改变标志位，结束初始状态
                switchcontrolleresp32_reset(); // 松开所有按键，防止误操作卡死
                while (digitalRead(BOOT_BUTTON_PIN) == LOW) { delay(10); } //等待用户松开 BOOT 键
                break; // 跳出检测循环
            }
            delay(100); // 每 100ms 探测一次按键状态
        }

        // 如果在上述 3 秒等待中没有触发停止，则发送一次 B 键动作
        if (runningSts == 0) {
            // 作用：模拟按下 B 键 100 毫秒，然后自动释放
            pushButton(Button::B, 100, 1);
        }
    } else if (runningSts == 1) { //暂停状态 检测是否按下BOOT 进入业务状态
        for (int i = 0; i < 30; i++) {
            if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
                runningSts = 2; // 改变标志位，进入业务状态
                switchcontrolleresp32_reset(); // 松开所有按键，防止误操作卡死
                while (digitalRead(BOOT_BUTTON_PIN) == LOW) { delay(10); } //等待用户松开 BOOT 键
                break; // 跳出检测循环
            }
            delay(100); // 每 100ms 探测一次按键状态
        }
        if (runningSts == 1) {
            switchcontrolleresp32_reset(); 
            delay(100); 
        }
    } else {
        //业务代码: 此时只是每3秒发送一次A
        for (int i = 0; i < 30; i++) {
            if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
                runningSts = 1; // 改变标志位，进入暂停状态
                switchcontrolleresp32_reset(); // 松开所有按键，防止误操作卡死
                while (digitalRead(BOOT_BUTTON_PIN) == LOW) { delay(10); } //等待用户松开 BOOT 键
                break; // 跳出检测循环
            }
            delay(100); // 每 100ms 探测一次按键状态
        }
        if (runningSts == 2) {
            pushButton(Button::A, 100, 1);
        }
    }
}
