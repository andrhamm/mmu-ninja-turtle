// Ninja Turtle Phase 5 bridge: Adafruit QT Py SAMD21 as a transparent USB CDC <-> UART passthrough at 115200 8N1,
// so vmmu (on the Mac or in the dev container via socat/TCP) can answer the printer's MMU port.
// Wiring (3.3 V logic): QT Py TX (A6) -> printer MMU RX, QT Py RX (A7) <- printer MMU TX, GND <-> GND.
// Native USB CDC has no FTDI-style latency timer; bytes are forwarded as soon as they arrive.
// UNVERIFIED: the printer MMU port's logic level/pinout -- confirm before connecting (see docs/afc-pro-port.md, recon note).

void setup() {
    Serial.begin(115200); // USB CDC (baud ignored)
    Serial1.begin(115200); // printer MMU port
    pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
    static uint32_t lastActivity = 0;
    int n;
    while ((n = Serial.available()) > 0) { // host -> printer
        uint8_t buf[64];
        n = Serial.readBytes(buf, n > 64 ? 64 : n);
        Serial1.write(buf, n);
        lastActivity = millis();
    }
    while ((n = Serial1.available()) > 0) { // printer -> host
        uint8_t buf[64];
        n = Serial1.readBytes(buf, n > 64 ? 64 : n);
        Serial.write(buf, n);
        Serial.flush(); // push the USB packet now rather than waiting for it to fill
        lastActivity = millis();
    }
    digitalWrite(LED_BUILTIN, millis() - lastActivity < 50);
}
