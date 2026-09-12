#if 0
#include <Arduino.h>
#include <M5Unified.h>

void setup() {
    auto config = M5.config();
    M5.begin(config);
    Serial.begin(115200);

    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(20, 40);
    M5.Display.println("Hello, World!");
    Serial.println("Hello, World!");
}

void loop() {
    M5.update();
    delay(10);
}
#endif