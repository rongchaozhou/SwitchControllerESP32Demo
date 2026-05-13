#include <Arduino.h>
#include <SwitchControllerESP32.h> // 引入该特定蓝牙库

void setup() {
    // 1. 初始化手柄底层蓝牙配置
    switchcontrolleresp32_init(); 
    
    // 2. 启动原生 USB 栈支持（该库要求必须调用它来初始化描述符）
    USB.begin(); 
    
    // 3. 复位手柄状态为全未按下
    switchcontrolleresp32_reset(); 
}

void loop() {
    // 该库的专属快捷函数：参数1: 按键名称, 参数2: 按下持续毫秒数, 参数3: 循环次数
    // 作用：模拟按下 B 键 100 毫秒，然后自动释放。
    pushButton(Button::B, 100, 1); 

    // // 每隔 3 秒执行一次上面的按下动作
    delay(3000); 
}
