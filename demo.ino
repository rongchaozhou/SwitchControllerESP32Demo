#include <Arduino.h>
#include <SwitchControllerESP32.h> // 引入该特定蓝牙库

const int BOOT_BUTTON_PIN = 0;

enum State {
    STATE_INIT = 0,
    STATE_PAUSE = 1,
    STATE_WORK = 2
};

State runningSts = STATE_INIT;
unsigned long lastActionTime = 0; 
const unsigned long INTERVAL_INIT = 3000;

// ==========================================
// 1. 自定义异常，用于一键穿透强退
// ==========================================
struct ButtonInterruptException {};

// ==========================================
// 2. 核心底层封装：兼容普通键与方向键，自带安全延迟
// ==========================================
// 为避免库内部 Button 枚举不包含方向键的问题，我们引入 isHat 参数：
// isHat = 0: 普通键 (如 Button::A)
// isHat = 1: 向上, 2: 向下, 3: 向左, 4: 向右 (根据具体库的实现，方向通常使用 setHat 或特定数值)
void doAction(Button btn, uint16_t holdTime, unsigned long delayAfter, int directionType = 0) {
    
    // 执行手柄动作
    if (directionType == 0) {
        // 普通按键
        pushButton(btn, holdTime, 1);
    } else {
        // 十字方向键处理：
        // 技巧：由于你的库中 pushButton 必须传 Button 型，
        // 如果 Button 里没有 UP/DOWN 成员，库中通常使用无符号整数或特有 API。
        // 这里根据常见 Switch 库规范，直接将整数强转为 Button 类型（跳过编译器的名称检查）
        // 常见的 D-pad 隐藏映射枚举值通常为：15(上), 16(下), 17(左), 18(右) 
        // 或者库内部提供了特殊的变量，通过强转 (Button) 数值可以完美绕过编译器的成员检查！
        if (directionType == 1) pushButton((Button)15, holdTime, 1); // 模拟上
        if (directionType == 2) pushButton((Button)16, holdTime, 1); // 模拟下
        if (directionType == 3) pushButton((Button)17, holdTime, 1); // 模拟左
        if (directionType == 4) pushButton((Button)18, holdTime, 1); // 模拟右
    }
    
    // 开始安全的非阻塞等待
    unsigned long start = millis();
    while (millis() - start < delayAfter) {
        if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
            delay(20); // 硬件去抖
            if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
                switchcontrolleresp32_reset(); // 必须全小写！
                while (digitalRead(BOOT_BUTTON_PIN) == LOW) { delay(10); } 
                
                runningSts = STATE_PAUSE; 
                throw ButtonInterruptException(); // 抛出异常瞬间强退
            }
        }
        yield(); // 喂狗，维持蓝牙稳定
    }
}

// ==========================================
// 3. 子业务流程：可以自由嵌套和编写复杂语句
// ==========================================
void subRoutineA(int loopCount) {
    for (int i = 0; i < loopCount; i++) {
        doAction(Button::X, 100, 500);
        doAction(Button::Y, 100, 500);
    }
}

void subRoutineB() {
    // 传参技巧：最后一个参数 1代表上，2代表下
    doAction(Button::A, 100, 300, 1);  // 模拟方向键：上 (此时第一个参数会被忽略)
    doAction(Button::A, 100, 300, 2);  // 模拟方向键：下
}

// ==========================================
// 4. 超复杂业务代码（主函数）
// ==========================================
void business() {
    doAction(Button::A, 100, 2000);
    doAction(Button::B, 100, 1000);

    // 调用带循环的子函数
    subRoutineA(2); 

    // 条件分支判断
    bool someCondition = true; 
    if (someCondition) {
        subRoutineB();
    } else {
        doAction(Button::A, 100, 1000, 3); // 模拟方向键：左
    }

    doAction(Button::R, 100, 2000);
}

// ==========================================
// 5. 全局按键状态切换
// ==========================================
bool checkButtonPress() {
    if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
        delay(20); 
        if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
            switchcontrolleresp32_reset(); // 必须全小写！
            while (digitalRead(BOOT_BUTTON_PIN) == LOW) { delay(10); }
            return true;
        }
    }
    return false;
}

void setup() {
    switchcontrolleresp32_init(); // 必须全小写！
    USB.begin();                  // 重要！启动原生USB栈支持
    switchcontrolleresp32_reset(); // 必须全小写！
    pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
    lastActionTime = millis();
}

void loop() {
    // 全局按键状态机
    if (checkButtonPress()) {
        switch (runningSts) {
            case STATE_INIT:   runningSts = STATE_PAUSE; break;
            case STATE_PAUSE:  runningSts = STATE_WORK;  break;
            case STATE_WORK:   runningSts = STATE_PAUSE; break; 
        }
        lastActionTime = millis(); 
    }

    unsigned long currentMillis = millis();
    
    switch (runningSts) {
        case STATE_INIT:
            if (currentMillis - lastActionTime >= INTERVAL_INIT) {
                pushButton(Button::B, 100, 1);
                lastActionTime = currentMillis;
            }
            break;

        case STATE_PAUSE:
            if (currentMillis - lastActionTime >= 100) {
                switchcontrolleresp32_reset(); // 必须全小写！
                lastActionTime = currentMillis;
            }
            break;

        case STATE_WORK:
            try {
                business(); // 毫无压力的顺序线性复杂业务
            } 
            catch (ButtonInterruptException& e) {
                // 底层一旦触发强退，瞬间安全弹出到此处，进入暂停状态
            }
            break;
    }
}
