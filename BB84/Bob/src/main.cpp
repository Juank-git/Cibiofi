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
int   stepperSpeed   = 4000;  // steps/s
int   stepperAcc     = 25000; // steps/s^2
int   microsteps     = 4;

// WiFi TX power (valores válidos: 8-84, donde 8=2dBm, 84=21dBm, unidad=0.25dBm)
// Valores comunes: 52(13dBm), 78(19.5dBm), 84(21dBm - máxima potencia)
int   wifiTxPower = 34;  // ESP32-C3 Super Mini funciona mejor con potencia moderada

// canal WiFi/ESP-NOW (debe ser igual en Central, Alice y Bob)
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
} __attribute__((packed));

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
} __attribute__((packed));

// ==============================================
// (centralMAC y centralRegistered declarados arriba con las constantes)
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
    Serial.printf("[Bob] Posición actual antes de homing: %ld pasos\n", stepper.currentPosition());
    
    // Configurar el pin del sensor Hall con pull-up (ya que es de colector abierto)
    pinMode(HALL_SENSOR_PIN, INPUT);
    
    // Leer estado inicial del sensor
    int initialState = digitalRead(HALL_SENSOR_PIN);
    Serial.printf("[Bob] Estado inicial sensor Hall (pin %d): %d\n", HALL_SENSOR_PIN, initialState);
    
    // Reiniciar flag de interrupción
    hallTriggered = false;
    
    // Habilitar interrupción en el pin del sensor: se dispara en flanco descendente (cuando el sensor pasa a LOW)
    attachInterrupt(digitalPinToInterrupt(HALL_SENSOR_PIN), hallISR, FALLING);
    Serial.println("[Bob] Interrupción configurada en modo FALLING");
    
    // Configurar alta aceleración y velocidad para el giro rápido
    stepper.setMaxSpeed(5500);
    stepper.setAcceleration(50000);
    
    // Calcular el número de pasos correspondientes a 360°
    long stepsFor360 = (long)round((SM_RESOLUTION * microsteps * GEAR_RATIO));
    Serial.printf("[Bob] Pasos por vuelta: %ld (iniciando búsqueda en 3 vueltas)\n", stepsFor360);
    
    // Iniciar movimiento continuo: dar vuelta completa hasta detectar el imán positivo
    stepper.moveTo(stepper.currentPosition() + stepsFor360 * 3);
    
    // Si el sensor ya está activado (LOW), primero alejarse hasta que esté desactivado (HIGH)
    if (initialState == LOW) {
        Serial.println("[Bob] Sensor ya activado, alejándose primero...");
        detachInterrupt(digitalPinToInterrupt(HALL_SENSOR_PIN));
        
        // Avanzar hasta que el sensor se desactive
        int steps = 0;
        while (digitalRead(HALL_SENSOR_PIN) == LOW && steps < 500) {
            stepper.moveTo(stepper.currentPosition() + 1);
            while (stepper.distanceToGo() != 0) {
                stepper.run();
            }
            steps++;
        }
        
        Serial.printf("[Bob] Sensor desactivado después de %d pasos\n", steps);
        
        // Reiniciar flag y reconectar interrupción
        hallTriggered = false;
        attachInterrupt(digitalPinToInterrupt(HALL_SENSOR_PIN), hallISR, FALLING);
        
        // Reconfigurar movimiento para búsqueda completa
        stepper.moveTo(stepper.currentPosition() + stepsFor360 * 3);
    }
    
    // Giro rápido hasta detectar el imán positivo mediante la interrupción
    Serial.println("[Bob] Iniciando búsqueda del sensor Hall...");
    
    while (!hallTriggered) {
        stepper.run();
    }
    
    // Registrar la posición en que se activó el sensor
    long positionAtTrigger = stepper.currentPosition();
    Serial.printf("[Bob] ✓ Sensor Hall detectado en posición: %ld\n", positionAtTrigger);
    
    // Deshabilitar la interrupción para evitar activaciones adicionales
    detachInterrupt(digitalPinToInterrupt(HALL_SENSOR_PIN));
    
    // Calcular el número de pasos correspondientes a 345° (suponiendo SM_RESOLUTION * microsteps pasos por revolución)
    long stepsFor345 = (long)round((345.0 / 360.0) * (SM_RESOLUTION * microsteps * GEAR_RATIO));
    
    // Mover 345° adicionales desde el punto de detección
    stepper.moveTo(positionAtTrigger + stepsFor345);
    while (stepper.distanceToGo() != 0) {
        stepper.run();
        yield();
    }
    
    Serial.println("[Bob] Aproximación fina: avanzando paso a paso...");
    
    // Configurar parámetros para movimiento muy lento y preciso
    stepper.setMaxSpeed(4000);
    
    // Avanzar paso a paso encuestando hasta que se active el sensor (cuando el imán positivo esté en posición)
    while (digitalRead(HALL_SENSOR_PIN) == HIGH) {
        stepper.moveTo(stepper.currentPosition() + 1);  // Mover 1 paso
        while (stepper.distanceToGo() != 0) {
            stepper.run();
        }
        yield();
    }
    
    // Homing completado: establecer la posición actual (en pasos) como 0
    stepper.setCurrentPosition(0);
    
    // Restaurar velocidad normal
    stepper.setMaxSpeed(stepperSpeed);
    stepper.setAcceleration(stepperAcc);
    
    isHomed = true;
    Serial.println("[Bob] Homing completado - Posición 0 establecida");
    
    // Notificar al ESP32 central vía ESP-NOW
    if (centralRegistered) {
        ResponseData response = {STATUS_HOME_COMPLETE, 0, 0, 0, 0.0};
        esp_now_send(centralMAC, (uint8_t*)&response, sizeof(response));
        Serial.println("[Bob] HOME_COMPLETE");
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
        Serial.println("[Bob] ERROR: Not homed");
        if (centralRegistered) {
            ResponseData response = {STATUS_ERROR, pulseNum, 0, 0, 0.0};
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
        ResponseData response = {STATUS_READY, pulseNum, baseBob, 0, currentTargetAngle};
        esp_now_send(centralMAC, (uint8_t*)&response, sizeof(response));
    }
}

// Callback ESP-NOW para comandos desde el Central
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
    if (len != sizeof(CommandData)) return;
    
    CommandData cmd;
    memcpy(&cmd, incomingData, sizeof(cmd));
    
    // Registro automático del Central (primera vez)
    if (!centralRegistered) {
        // Copiar MAC del remitente
        memcpy(centralMAC, mac, 6);
        
        // Verificar si ya está registrado como peer
        if (!esp_now_is_peer_exist(centralMAC)) {
            // Agregar Central como peer
            esp_now_peer_info_t peerInfo = {};
            memcpy(peerInfo.peer_addr, centralMAC, 6);
            peerInfo.channel = ESP_NOW_CHANNEL;
            peerInfo.encrypt = false;
            
            if (esp_now_add_peer(&peerInfo) == ESP_OK) {
                centralRegistered = true;
                Serial.print("[Bob] ✓ Central registrado: ");
                for (int i = 0; i < 6; i++) {
                    Serial.printf("%02X", centralMAC[i]);
                    if (i < 5) Serial.print(":");
                }
                Serial.println();
            } else {
                Serial.println("[Bob] ERROR: No se pudo registrar Central como peer");
            }
        } else {
            centralRegistered = true;
        }
    }
    
    // Procesamiento rápido de comandos
    switch (cmd.cmd) {
        case CMD_PING: {
            ResponseData response = {STATUS_PONG, 0, 0, 0, 0.0};
            esp_now_send(centralMAC, (uint8_t*)&response, sizeof(response));
            break;
        }
        
        case CMD_HOME:
            Serial.println("[Bob] HOME");
            performHoming();
            break;
            
        case CMD_PREPARE_PULSE:
            Serial.printf("[Bob] PREPARE #%d\n", cmd.pulseNum);
            currentPulseNum = cmd.pulseNum;
            prepareForNextPulse(cmd.pulseNum);
            break;
            
        case CMD_ABORT:
            Serial.println("[Bob] ABORT");
            stepper.stop();
            break;
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== BOB - ESP-NOW Motor ===");
    
    // Configuración WiFi/ESP-NOW optimizada
    WiFi.mode(WIFI_STA);
    esp_wifi_set_ps(WIFI_PS_NONE);
    esp_wifi_set_max_tx_power(wifiTxPower);
    esp_wifi_set_channel(ESP_NOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
    
    Serial.print("[Bob] MAC: ");
    Serial.println(WiFi.macAddress());
    Serial.printf("[Bob] Canal: %d\n", ESP_NOW_CHANNEL);
    
    // Inicializar ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("[Bob] ERROR: ESP-NOW init");
        return;
    }
    
    // Registrar callback de recepción
    if (esp_now_register_recv_cb(OnDataRecv) != ESP_OK) {
        Serial.println("[Bob] ERROR: Callback RX");
        return;
    }
    
    Serial.println("[Bob] ESP-NOW OK - Esperando conexión del Central");
    
    // Verificar tamaños de estructuras
    Serial.printf("[Bob] sizeof(CommandData): %d bytes\n", sizeof(CommandData));
    Serial.printf("[Bob] sizeof(ResponseData): %d bytes\n", sizeof(ResponseData));
    
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
        Serial.println("[Bob] TMC2130 OK");
    } else {
        Serial.println("[Bob] ERROR: TMC2130");
    }
    
    stepper.setEnablePin(ENABLE_PIN);
    stepper.setMaxSpeed(stepperSpeed);
    stepper.setAcceleration(stepperAcc);
    stepper.setPinsInverted(true, true, true);
    stepper.enableOutputs();
    
    Serial.println("[Bob] READY\n");
}

void loop() {
    // Movimiento no bloqueante
    stepper.run();
    yield();  // Optimizado: yield() en lugar de delay(1) para mejor respuesta
}
