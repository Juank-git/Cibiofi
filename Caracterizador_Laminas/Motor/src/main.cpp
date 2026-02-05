#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <SPI.h>
#include <TMC2130Stepper.h>
#include <AccelStepper.h>
#include <math.h>

// ======================
// CONFIGURACIÓN - MOTOR CARACTERIZADOR
// ======================

// --- PINOUT ---
#define SPI_MOSI   6
#define SPI_MISO   5
#define SPI_SCLK   4
#define SPI_CS     7

#define ENABLE_PIN  3
#define DIR_PIN     2
#define STEP_PIN    1

#define HALL_SENSOR_PIN 10  // Pin del sensor Hall para homing

#define SM_RESOLUTION 200
#define GEAR_RATIO 3.0

// Motor parameters
int   stepperCurrent = 500;   // mA
int   stepperSpeed   = 6000;  // steps/s (mismo que Central original)
int   stepperAcc     = 50000; // steps/s^2 (mismo que Central original)
int   microsteps     = 4;

// WiFi TX power
int   wifiTxPower = 34;  // ESP32-C3 Super Mini funciona mejor con potencia moderada

// Canal WiFi/ESP-NOW (debe coincidir con el Central)
const int ESP_NOW_CHANNEL = 11;

// MAC del ESP32 Central (se aprenderá automáticamente)
uint8_t centralMAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
bool centralRegistered = false;

// Driver + stepper
TMC2130Stepper driver = TMC2130Stepper(SPI_CS, SPI_MOSI, SPI_MISO, SPI_SCLK);
AccelStepper stepper = AccelStepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);

// ==============================================
// ESTRUCTURAS ESP-NOW
// ==============================================
enum Command {
  CMD_PING = 0,
  CMD_HOME = 1,
  CMD_MOVE_TO_ANGLE = 2,  // Nuevo: mover a ángulo específico
  CMD_ABORT = 3
};

struct CommandData {
  uint8_t cmd;           // Comando
  float targetAngle;     // Ángulo objetivo (para CMD_MOVE_TO_ANGLE)
  uint32_t reserved;     // Reservado
} __attribute__((packed));

enum Status {
  STATUS_PONG = 0,
  STATUS_HOME_COMPLETE = 1,
  STATUS_READY = 2,       // Motor listo en posición
  STATUS_ERROR = 3
};

struct ResponseData {
  uint8_t status;
  float currentAngle;    // Ángulo actual del motor
  uint32_t reserved;
} __attribute__((packed));

// ==============================================
// VARIABLES
// ==============================================
volatile bool hallTriggered = false;
bool isHomed = false;
float currentTargetAngle = 0.0;

// ==============================================
// FUNCIONES
// ==============================================

void IRAM_ATTR hallISR() {
    hallTriggered = true;
}

long angleToSteps(float angle) {
  return -(long)round((angle / 360.0) * (SM_RESOLUTION * microsteps * GEAR_RATIO));
}

float getCurrentAngle() {
    return -(stepper.currentPosition() * 360.0) / (SM_RESOLUTION * microsteps * GEAR_RATIO);
}

void performHoming() {
    Serial.println("[Motor] Iniciando homing...");
    
    pinMode(HALL_SENSOR_PIN, INPUT);
    int initialState = digitalRead(HALL_SENSOR_PIN);
    
    hallTriggered = false;
    attachInterrupt(digitalPinToInterrupt(HALL_SENSOR_PIN), hallISR, FALLING);
    
    stepper.setMaxSpeed(5500);
    stepper.setAcceleration(50000);
    
    long stepsFor360 = (long)round((SM_RESOLUTION * microsteps * GEAR_RATIO));
    stepper.moveTo(stepper.currentPosition() + stepsFor360 * 3);
    
    // Si el sensor ya está activado, alejarse primero
    if (initialState == LOW) {
        Serial.println("[Motor] Sensor ya activado, alejándose...");
        detachInterrupt(digitalPinToInterrupt(HALL_SENSOR_PIN));
        
        int steps = 0;
        while (digitalRead(HALL_SENSOR_PIN) == LOW && steps < 500) {
            stepper.moveTo(stepper.currentPosition() + 1);
            while (stepper.distanceToGo() != 0) {
                stepper.run();
            }
            steps++;
        }
        
        hallTriggered = false;
        attachInterrupt(digitalPinToInterrupt(HALL_SENSOR_PIN), hallISR, FALLING);
        stepper.moveTo(stepper.currentPosition() + stepsFor360 * 3);
    }
    
    // Búsqueda rápida del sensor
    while (!hallTriggered) {
        stepper.run();
    }
    
    long positionAtTrigger = stepper.currentPosition();
    detachInterrupt(digitalPinToInterrupt(HALL_SENSOR_PIN));
    
    // Mover 345° adicionales
    long stepsFor345 = (long)round((345.0 / 360.0) * (SM_RESOLUTION * microsteps * GEAR_RATIO));
    stepper.moveTo(positionAtTrigger + stepsFor345);
    while (stepper.distanceToGo() != 0) {
        stepper.run();
        yield();
    }
    
    // Aproximación fina
    stepper.setMaxSpeed(4000);
    while (digitalRead(HALL_SENSOR_PIN) == HIGH) {
        stepper.moveTo(stepper.currentPosition() + 1);
        while (stepper.distanceToGo() != 0) {
            stepper.run();
        }
        yield();
    }
    
    // Establecer posición 0
    stepper.setCurrentPosition(0);
    
    // Restaurar velocidad normal
    stepper.setMaxSpeed(stepperSpeed);
    stepper.setAcceleration(stepperAcc);
    
    isHomed = true;
    Serial.println("[Motor] Homing completado - Posición 0°");
    
    // Notificar al Central
    if (centralRegistered) {
        ResponseData response = {STATUS_HOME_COMPLETE, 0.0, 0};
        esp_now_send(centralMAC, (uint8_t*)&response, sizeof(response));
    }
}

void moveToAngle(float targetAngle) {
    if (!isHomed) {
        Serial.println("[Motor] ERROR: Not homed");
        if (centralRegistered) {
            ResponseData response = {STATUS_ERROR, getCurrentAngle(), 0};
            esp_now_send(centralMAC, (uint8_t*)&response, sizeof(response));
        }
        return;
    }
    
    Serial.printf("[Motor] Moviendo a %.2f°\n", targetAngle);
    
    long steps = angleToSteps(targetAngle);
    stepper.moveTo(steps);
    
    while (stepper.distanceToGo() != 0) {
        stepper.run();
        yield();
    }
    
    float finalAngle = getCurrentAngle();
    Serial.printf("[Motor] Posición alcanzada: %.2f°\n", finalAngle);
    
    // Pequeña pausa para estabilización
    delay(100);
    
    // Notificar que está listo
    if (centralRegistered) {
        ResponseData response = {STATUS_READY, finalAngle, 0};
        esp_now_send(centralMAC, (uint8_t*)&response, sizeof(response));
    }
}

void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
    if (len != sizeof(CommandData)) return;
    
    CommandData cmd;
    memcpy(&cmd, incomingData, sizeof(cmd));
    
    // Registro automático del Central
    if (!centralRegistered) {
        memcpy(centralMAC, mac, 6);
        
        if (!esp_now_is_peer_exist(centralMAC)) {
            esp_now_peer_info_t peerInfo = {};
            memcpy(peerInfo.peer_addr, centralMAC, 6);
            peerInfo.channel = ESP_NOW_CHANNEL;
            peerInfo.encrypt = false;
            
            if (esp_now_add_peer(&peerInfo) == ESP_OK) {
                centralRegistered = true;
                Serial.print("[Motor] ✓ Central registrado: ");
                for (int i = 0; i < 6; i++) {
                    Serial.printf("%02X", centralMAC[i]);
                    if (i < 5) Serial.print(":");
                }
                Serial.println();
            }
        } else {
            centralRegistered = true;
        }
    }
    
    // Procesar comandos
    switch (cmd.cmd) {
        case CMD_PING: {
            ResponseData response = {STATUS_PONG, getCurrentAngle(), 0};
            esp_now_send(centralMAC, (uint8_t*)&response, sizeof(response));
            break;
        }
        
        case CMD_HOME:
            Serial.println("[Motor] HOME");
            performHoming();
            break;
            
        case CMD_MOVE_TO_ANGLE:
            Serial.printf("[Motor] MOVE_TO %.2f°\n", cmd.targetAngle);
            currentTargetAngle = cmd.targetAngle;
            moveToAngle(cmd.targetAngle);
            break;
            
        case CMD_ABORT:
            Serial.println("[Motor] ABORT");
            stepper.stop();
            break;
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== MOTOR CARACTERIZADOR - ESP-NOW ===");
    
    // Configuración WiFi/ESP-NOW
    WiFi.mode(WIFI_STA);
    esp_wifi_set_ps(WIFI_PS_NONE);
    esp_wifi_set_max_tx_power(wifiTxPower);
    esp_wifi_set_channel(ESP_NOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
    
    Serial.print("[Motor] MAC: ");
    Serial.println(WiFi.macAddress());
    Serial.printf("[Motor] Canal: %d\n", ESP_NOW_CHANNEL);
    
    // Inicializar ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("[Motor] ERROR: ESP-NOW init");
        return;
    }
    
    if (esp_now_register_recv_cb(OnDataRecv) != ESP_OK) {
        Serial.println("[Motor] ERROR: Callback RX");
        return;
    }
    
    Serial.println("[Motor] ESP-NOW OK - Esperando Central");
    Serial.printf("[Motor] sizeof(CommandData): %d bytes\n", sizeof(CommandData));
    Serial.printf("[Motor] sizeof(ResponseData): %d bytes\n", sizeof(ResponseData));
    
    // Configurar pines y SPI
    pinMode(SPI_CS, OUTPUT);
    digitalWrite(SPI_CS, HIGH);
    pinMode(ENABLE_PIN, OUTPUT);
    pinMode(DIR_PIN, OUTPUT);
    pinMode(STEP_PIN, OUTPUT);
    digitalWrite(ENABLE_PIN, HIGH);
    
    SPI.begin(SPI_SCLK, SPI_MISO, SPI_MOSI, SPI_CS);
    delay(50);
    
    // Inicializar TMC2130
    driver.begin();
    delay(50);
    digitalWrite(ENABLE_PIN, LOW);
    
    if (driver.test_connection()) {
        driver.rms_current(stepperCurrent);
        driver.stealthChop(0);
        driver.pwm_autoscale(true);
        driver.microsteps(microsteps);
        Serial.println("[Motor] TMC2130 OK");
    } else {
        Serial.println("[Motor] ERROR: TMC2130");
    }
    
    stepper.setEnablePin(ENABLE_PIN);
    stepper.setMaxSpeed(stepperSpeed);
    stepper.setAcceleration(stepperAcc);
    stepper.setPinsInverted(false, true, false);
    stepper.enableOutputs();
    
    Serial.println("[Motor] READY\n");
}

void loop() {
    stepper.run();
    yield();  // Optimizado: yield() en lugar de delay()
}
