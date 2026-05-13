#include <Arduino.h>
#include <SwitchControllerESP32.h> // 引入该特定蓝牙库
#include <Adafruit_NeoPixel.h>      // 新增：引入灯光控制库

const int BOOT_BUTTON_PIN = 0;

// --- 新增：板载 RGB 灯配置 ---
#define LED_PIN        48  // 大多数 ESP32-S3 的板载灯固定在 GPIO 48
#define NUM_PIXELS      1  // 板子上只有 1 颗灯
Adafruit_NeoPixel pixel(NUM_PIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

enum State {
    STATE_INIT = 0,
    STATE_PAUSE = 1,
    STATE_WORK = 2
};

State runningSts = STATE_INIT;
unsigned long lastActionTime = 0; 
const unsigned long INTERVAL_INIT = 3000;

// ==========================================
// 新增：根据当前状态自动刷新 LED 颜色与亮度
// ==========================================
void updateLed() {
    pixel.setBrightness(1); // 强制全局最暗亮度，护眼省电
    
    switch (runningSts) {
        case STATE_INIT:
            pixel.setPixelColor(0, pixel.Color(0, 0, 255));   // 初始状态：亮蓝灯
            break;
        case STATE_PAUSE:
            pixel.setPixelColor(0, pixel.Color(255, 0, 0));   // 暂停状态：亮红灯
            break;
        case STATE_WORK:
            pixel.setPixelColor(0, pixel.Color(255, 255, 0)); // 业务状态：亮黄灯 (红255+绿255=黄)
            break;
    }
    pixel.show(); // 刷新硬件灯珠
}

// ==========================================
// 1. 自定义异常，用于一键穿透强退
// ==========================================
struct ButtonInterruptException {};

// ==========================================
// 2. 核心底层封装：兼容普通键与方向键，自带安全延迟
// ==========================================
void doAction(Button btn, uint16_t holdTime, unsigned long delayAfter, int directionType = 0) {
    
    // 执行手柄动作
    if (directionType == 0) {
        pushButton(btn, holdTime, 1);
    } else {
        if (directionType == 1) pushButton((Button)15, holdTime, 1); 
        if (directionType == 2) pushButton((Button)16, holdTime, 1); 
        if (directionType == 3) pushButton((Button)17, holdTime, 1); 
        if (directionType == 4) pushButton((Button)18, holdTime, 1); 
    }
    
    // 开始安全的非阻塞等待
    unsigned long start = millis();
    while (millis() - start < delayAfter) {
        if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
            delay(20); // 硬件去抖
            if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
                switchcontrolleresp32_reset(); 
                while (digitalRead(BOOT_BUTTON_PIN) == LOW) { delay(10); } 
                
                runningSts = STATE_PAUSE; 
                updateLed(); // 核心：业务中途被强退时，灯光立刻从黄色切为红色！
                throw ButtonInterruptException(); // 抛出异常瞬间强退
            }
        }
        yield(); 
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
    doAction(Button::A, 100, 300, 1);  
    doAction(Button::A, 100, 300, 2);  
}

// ==========================================
// 4. 超复杂业务代码（主函数）
// ==========================================
void business() {
    doAction(Button::A, 100, 2000);
    doAction(Button::B, 100, 1000);

    subRoutineA(2); 

    bool someCondition = true; 
    if (someCondition) {
        subRoutineB();
    } else {
        doAction(Button::A, 100, 1000, 3); 
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
            switchcontrolleresp32_reset(); 
            while (digitalRead(BOOT_BUTTON_PIN) == LOW) { delay(10); }
            return true;
        }
    }
    return false;
}

void setup() {
    // 初始化灯珠
    pixel.begin();
    updateLed(); // 启动时立刻根据 STATE_INIT 点亮蓝灯

    switchcontrolleresp32_init(); 
    USB.begin();                  
    switchcontrolleresp32_reset(); 
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
        updateLed(); // 核心：用户手动按键切状态时，灯光立刻同步刷新！
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
                switchcontrolleresp32_reset(); 
                lastActionTime = currentMillis;
            }
            break;

        case STATE_WORK:
            try {
                business(); 
            } 
            catch (ButtonInterruptException& e) {
                // 异常被拦截后，因为在 doAction 里已经更新了 updateLed()，这里什么都不用管
            }
            break;
    }
}
