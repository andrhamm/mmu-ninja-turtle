// Ninja Turtle Phase 5 bridge: Adafruit QT Py SAMD21 as a transparent USB CDC <-> UART passthrough at 115200 8N1,
// so vmmu (on the Mac or in the dev container via socat/TCP) can answer the printer's MMU port.
//
// Wiring to the CORE One L MMU port J1 (Prusa xBuddy Extension Rev 06 schematic; see 3dfarm Research/MMU Port Pinout CORE One L):
//   J1 pin 1 (printer TX, 3.3 V)  --1k--> QT Py A7 (RX)
//   QT Py A6 (TX) -> 74AHCT125 (VCC = QT Py 5V/USB) -> J1 pin 3 (printer RX; needs a 5 V driver)
//   74AHCT125 /OE <- QT Py A3, with a 10k pull-up to 5 V (buffer disabled until firmware enables it)
//   GND <-> J1 pins 7/9/11.   NEVER connect J1 pins 2,4 (RS485), 5 (5 V), 6 (reset), 8 (power-loss), 10/12 (24 V).
// The buffer is enabled only after J1 pin 1 has idled high for 100 ms (printer powered), so the bridge never drives an
// unpowered printer input. Native USB CDC has no FTDI-style latency timer; bytes are forwarded as soon as they arrive.

constexpr uint8_t RX_PIN = A7;
constexpr uint8_t OE_PIN = A3; // 74AHCT125 /OE, active low

static void WaitForPrinterIdleHigh() {
    pinMode(RX_PIN, INPUT_PULLDOWN); // a floating/unpowered line must not look "high"
    uint32_t highSince = 0;
    for (;;) {
        if (digitalRead(RX_PIN)) {
            if (!highSince)
                highSince = millis() | 1;
            else if (millis() - highSince >= 100)
                return;
        } else {
            highSince = 0;
        }
        digitalWrite(LED_BUILTIN, (millis() / 500) & 1); // slow blink: waiting for the printer
    }
}

void setup() {
    pinMode(OE_PIN, OUTPUT);
    digitalWrite(OE_PIN, HIGH); // keep the TX buffer off
    pinMode(LED_BUILTIN, OUTPUT);
    Serial.begin(115200); // USB CDC (baud ignored)
    WaitForPrinterIdleHigh();
    Serial1.begin(115200); // printer MMU port
    digitalWrite(OE_PIN, LOW); // printer is powered: enable TX
}

void loop() {
    static uint32_t lastActivity = 0;
    int n;
    while ((n = Serial.available()) > 0) { // host -> printer
        char buf[64];
        n = Serial.readBytes(buf, n > 64 ? 64 : n);
        Serial1.write((const uint8_t *)buf, n);
        lastActivity = millis();
    }
    while ((n = Serial1.available()) > 0) { // printer -> host
        char buf[64];
        n = Serial1.readBytes(buf, n > 64 ? 64 : n);
        Serial.write((const uint8_t *)buf, n);
        Serial.flush(); // push the USB packet now rather than waiting for it to fill
        lastActivity = millis();
    }
    digitalWrite(LED_BUILTIN, millis() - lastActivity < 50);
}
