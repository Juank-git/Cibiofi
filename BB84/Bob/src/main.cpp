#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <SPI.h>
#include <TMC2130Stepper.h>
#include <AccelStepper.h>
#include <math.h>

// ======================
// CONFIGURACIÓN - BOB
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
int   stepperSpeed   = 5000;  // steps/s
int   stepperAcc     = 40000; // steps/s^2
int   microsteps     = 4;

// Driver + stepper
TMC2130Stepper driver = TMC2130Stepper(SPI_CS, SPI_MOSI, SPI_MISO, SPI_SCLK);
AccelStepper stepper = AccelStepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);

// ==============================================
// ESTRUCTURAS ESP-NOW
// ==============================================
// Comandos desde el ESP32 Central
enum Command {
  CMD_PING = 0,          // Ping para verificar conexión
  CMD_HOME = 1,
  CMD_PREPARE_PULSE = 2,
  CMD_ABORT = 3
};

struct CommandData {
  uint8_t cmd;           // Comando (HOME, PREPARE_PULSE, ABORT)
  uint32_t pulseNum;     // Número de pulso
  uint32_t totalPulses;  // Total de pulsos (opcional)
};

// Respuestas hacia el ESP32 Central
enum Status {
  STATUS_PONG = 0,            // Respuesta al ping
  STATUS_HOME_COMPLETE = 1,
  STATUS_READY = 2,
  STATUS_ERROR = 3
};

struct ResponseData {
  uint8_t status;        // HOME_COMPLETE, READY, ERROR
  uint32_t pulseNum;     // Número de pulso
  int base;              // Base aleatoria (0 o 1)
  int bit;               // Bit aleatorio (siempre 0 para Bob)
  float angle;           // Ángulo de rotación
};

// MAC del ESP32 Central (se aprende automáticamente)
uint8_t centralMAC[6];
bool centralRegistered = false;

// ==============================================
// Variables Protocolo BB84 - BOB
// ==============================================
// Ángulos de medición para Bob
// [Base] = Ángulo
float angulosRotacionBob[2] = {
  13.95,  // Base 0 (+): Rectilinea
  36.45   // Base 1 (x): Diagonal
};

int baseBob = 0;
float currentTargetAngle = 0.0;
uint32_t currentPulseNum = 0;

// Flag de homing
volatile bool hallTriggered = false;
bool isHomed = false;

// ==============================================
// FUNCIONES
// ==============================================

// ISR para sensor Hall
void IRAM_ATTR hallISR() {
    hallTriggered = true;
}

// Convertir ángulo a pasos
long angleToSteps(float angle) {
  return (long)round((angle / 360.0) * (SM_RESOLUTION * microsteps * GEAR_RATIO));
}

// Obtener ángulo actual
float getCurrentAngle() {
    return stepper.currentPosition() * 360.0 / (SM_RESOLUTION * microsteps * GEAR_RATIO);
}

// Rutina de homing
void performHoming() {
    Serial.println("[Bob] Iniciando homing...");
    
    // Configurar el pin del sensor Hall con pull-up
    pinMode(HALL_SENSOR_PIN, INPUT);
    
    // Verificar estado inicial del sensor
    int initialState = digitalRead(HALL_SENSOR_PIN);
    Serial.printf("[Bob] Estado inicial sensor Hall: %d\n", initialState);
    
    // Reiniciar flag
    hallTriggered = false;
    
    // Habilitar interrupción en flanco descendente
    attachInterrupt(digitalPinToInterrupt(HALL_SENSOR_PIN), hallISR, FALLING);
    
    // Fase 1: Búsqueda rápida
    stepper.setMaxSpeed(5500);
    stepper.setAcceleration(50000);
    
    long stepsFor360 = (long)round((SM_RESOLUTION * microsteps * GEAR_RATIO));
    stepper.moveTo(stepper.currentPosition() + stepsFor360 * 3);  // 3 vueltas máximo
    
    // Timeout de 15 segundos para búsqueda
    unsigned long searchStart = millis();
    while (!hallTriggered && (millis() - searchStart < 15000)) {
        stepper.run();
        yield();
    }
    
    if (!hallTriggered) {
        detachInterrupt(digitalPinToInterrupt(HALL_SENSOR_PIN));
        Serial.println("[Bob] ERROR: Timeout en búsqueda del sensor Hall");
        stepper.stop();
        
        // Notificar error al Central
        if (centralRegistered) {
            ResponseData response;
            response.status = STATUS_ERROR;
            response.pulseNum = 0;
            response.base = 0;
            response.bit = 0;
            response.angle = 0.0;
            esp_now_send(centralMAC, (uint8_t*)&response, sizeof(response));
        }
        return;
    }
    
    long positionAtTrigger = stepper.currentPosition();
    detachInterrupt(digitalPinToInterrupt(HALL_SENSOR_PIN));
    
    // Fase 2: Aproximación (345°)
    long stepsFor345 = (long)round((345.0 / 360.0) * (SM_RESOLUTION * microsteps * GEAR_RATIO));
    stepper.moveTo(positionAtTrigger + stepsFor345);
    while (stepper.distanceToGo() != 0) {
        stepper.run();
        yield();
    }
    
    // Fase 3: Ajuste fino
    Serial.println("[Bob] Aproximación fina...");
    stepper.setMaxSpeed(4000);
    
    // Timeout de 5 segundos para ajuste fino
    unsigned long fineAdjustStart = millis();
    int stepCount = 0;
    while (digitalRead(HALL_SENSOR_PIN) == HIGH && (millis() - fineAdjustStart < 5000)) {
        stepper.moveTo(stepper.currentPosition() + 1);
        while (stepper.distanceToGo() != 0) {
            stepper.run();
            yield();
        }
        stepCount++;
        yield();
        
        // Verificar que no estemos dando demasiadas vueltas
        if (stepCount > stepsFor360) {
            Serial.println("[Bob] ERROR: Ajuste fino excedió 1 vuelta completa");
            break;
        }
    }
    
    if (digitalRead(HALL_SENSOR_PIN) == HIGH) {
        Serial.println("[Bob] WARNING: Ajuste fino no encontró posición exacta");
    }
    
    // Establecer posición 0
    stepper.setCurrentPosition(0);
    
    // Restaurar velocidad normal
    stepper.setMaxSpeed(stepperSpeed);
    stepper.setAcceleration(stepperAcc);
    
    isHomed = true;
    Serial.println("[Bob] Homing completado - Posición 0 establecida");
    
    // Notificar al ESP32 central vía ESP-NOW
    if (centralRegistered) {
        ResponseData response;
        response.status = STATUS_HOME_COMPLETE;
        response.pulseNum = 0;
        response.base = 0;
        response.bit = 0;  // Bob no usa bit
        response.angle = 0.0;
        
        esp_err_t result = esp_now_send(centralMAC, (uint8_t*)&response, sizeof(response));
        if (result == ESP_OK) {
            Serial.println("[Bob] HOME_COMPLETE enviado al Central");
        } else {
            Serial.printf("[Bob] Error enviando HOME_COMPLETE: %d\n", result);
        }
    }
}

// Mover a ángulo específico
void moveToAngle(float targetAngle) {
    Serial.printf("[Bob] Moviendo a %.2f grados\n", targetAngle);
    
    long steps = angleToSteps(targetAngle);
    stepper.moveTo(steps);
    
    while (stepper.distanceToGo() != 0) {
        stepper.run();
        yield();
    }
    
    float currentAngle = getCurrentAngle();
    Serial.printf("[Bob] Movimiento completado - Posición: %.2f grados\n", currentAngle);
}

// Preparar para el siguiente pulso (selección aleatoria de base)
void prepareForNextPulse(uint32_t pulseNum) {
    if (!isHomed) {
        Serial.println("[Bob] ERROR: No se ha realizado homing");
        
        if (centralRegistered) {
            ResponseData response;
            response.status = STATUS_ERROR;
            response.pulseNum = pulseNum;
            response.base = 0;
            response.bit = 0;
            response.angle = 0.0;
            
            esp_now_send(centralMAC, (uint8_t*)&response, sizeof(response));
        }
        return;
    }
    
    // Generar base aleatoria usando esp_random()
    baseBob = esp_random() % 2;
    
    // Calcular ángulo objetivo
    currentTargetAngle = angulosRotacionBob[baseBob];
    
    Serial.printf("[Bob] Pulso %d - Base:%d Ángulo:%.2f\n", 
                  pulseNum, baseBob, currentTargetAngle);
    
    // Mover al ángulo
    moveToAngle(currentTargetAngle);
    
    // Notificar que está listo vía ESP-NOW
    if (centralRegistered) {
        ResponseData response;
        response.status = STATUS_READY;
        response.pulseNum = pulseNum;
        response.base = baseBob;
        response.bit = 0;  // Bob no usa bit
        response.angle = currentTargetAngle;
        
        esp_err_t result = esp_now_send(centralMAC, (uint8_t*)&response, sizeof(response));
        if (result == ESP_OK) {
            Serial.printf("[Bob] READY enviado - Pulso:%d Base:%d\n", pulseNum, baseBob);
        } else {
            Serial.printf("[Bob] Error enviando READY: %d\n", result);
        }
    }
}

// Callback ESP-NOW para comandos desde el Central
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
    if (len == sizeof(CommandData)) {
        CommandData cmd;
        memcpy(&cmd, incomingData, sizeof(cmd));
        
        // Guardar MAC del Central (primera vez)
        if (!centralRegistered) {
            memcpy(centralMAC, mac, 6);
            centralRegistered = true;
            Serial.print("[Bob] MAC Central registrada: ");
            for (int i = 0; i < 6; i++) {
                Serial.printf("%02X", centralMAC[i]);
                if (i < 5) Serial.print(":");
            }
            Serial.println();
            
            // Agregar Central como peer
            esp_now_peer_info_t peerInfo;
            memset(&peerInfo, 0, sizeof(peerInfo));
            memcpy(peerInfo.peer_addr, centralMAC, 6);
            peerInfo.channel = 13;
            peerInfo.encrypt = false;
            esp_now_add_peer(&peerInfo);
        }
        
        // Procesar comando
        switch (cmd.cmd) {
            case CMD_PING:
                Serial.println("[Bob] PING recibido - Respondiendo PONG");
                {
                    ResponseData response;
                    response.status = STATUS_PONG;
                    response.pulseNum = 0;
                    response.base = 0;
                    response.bit = 0;
                    response.angle = 0.0;
                    
                    esp_err_t result = esp_now_send(centralMAC, (uint8_t*)&response, sizeof(response));
                    if (result == ESP_OK) {
                        Serial.println("[Bob] PONG enviado exitosamente");
                    } else {
                        Serial.printf("[Bob] ERROR enviando PONG: %d\n", result);
                    }
                }
                break;
            
            case CMD_HOME:
                Serial.println("[Bob] Comando HOME recibido");
                performHoming();
                break;
                
            case CMD_PREPARE_PULSE:
                Serial.printf("[Bob] Comando PREPARE_PULSE recibido - Pulso:%d\n", cmd.pulseNum);
                currentPulseNum = cmd.pulseNum;
                prepareForNextPulse(cmd.pulseNum);
                break;
                
            case CMD_ABORT:
                Serial.println("[Bob] Comando ABORT recibido");
                stepper.stop();
                break;
                
            default:
                Serial.printf("[Bob] Comando desconocido: %d\n", cmd.cmd);
                break;
        }
    } else {
        Serial.printf("[Bob] Mensaje recibido con tamaño incorrecto: %d bytes\n", len);
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n======================");
    Serial.println("BOB - ESP-NOW Motor Control");
    Serial.println("======================");
    
    // Configurar WiFi en modo estación (sin conectar a AP)
    WiFi.mode(WIFI_STA);
    delay(100);
    Serial.println("[Bob] Modo STA activado");
    
    // Desactivar power save y maximizar potencia TX
    esp_wifi_set_ps(WIFI_PS_NONE);
    esp_wifi_set_max_tx_power(78); // 19.5 dBm
    Serial.println("[Bob] RF optimizado");
    
    // Mostrar MAC
    Serial.print("[Bob] MAC: ");
    Serial.println(WiFi.macAddress());
    
    // Fijar canal 13
    esp_wifi_set_channel(13, WIFI_SECOND_CHAN_NONE);
    Serial.println("[Bob] Canal fijado en 13");
    
    // Inicializar ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("[Bob] ERROR: esp_now_init() falló");
        return;
    }
    Serial.println("[Bob] ESP-NOW inicializado");
    
    // Registrar callback de recepción
    if (esp_now_register_recv_cb(OnDataRecv) != ESP_OK) {
        Serial.println("[Bob] ERROR: No se pudo registrar callback RX");
        return;
    }
    Serial.println("[Bob] Callback RX registrado");
    
    // Configurar pines y SPI
    pinMode(SPI_CS, OUTPUT);
    digitalWrite(SPI_CS, HIGH);
    
    pinMode(ENABLE_PIN, OUTPUT);
    pinMode(DIR_PIN, OUTPUT);
    pinMode(STEP_PIN, OUTPUT);
    digitalWrite(ENABLE_PIN, HIGH);
    
    // Inicializar SPI
    SPI.begin(SPI_SCLK, SPI_MISO, SPI_MOSI, SPI_CS);
    
    delay(100);
    Serial.println("[Bob] Iniciando driver TMC2130...");
    driver.begin();
    delay(100);
    digitalWrite(ENABLE_PIN, LOW);
    
    if (driver.test_connection()) {
        Serial.println("[Bob] Driver TMC2130 detectado");
        driver.rms_current(stepperCurrent);
        driver.stealthChop(0);
        driver.pwm_autoscale(true);
        driver.microsteps(microsteps);
    } else {
        Serial.println("[Bob] ERROR: No se detectó driver");
    }
    
    stepper.setEnablePin(ENABLE_PIN);
    stepper.setMaxSpeed(stepperSpeed);
    stepper.setAcceleration(stepperAcc);
    stepper.setPinsInverted(true, true, true);
    stepper.enableOutputs();
    
    Serial.println("[Bob] Configuración completa - Esperando comandos del ESP32 Central vía ESP-NOW\n");
}

void loop() {
    // Movimiento no bloqueante
    stepper.run();
    delay(1);
}
