#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <SPI.h>
#include <TMC2130Stepper.h>
#include <AccelStepper.h>
#include <math.h>

// ======================
// CONFIGURACIÓN - ALICE
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

// MAC del ESP32 Central (obtener con WiFi.macAddress())
uint8_t centralMAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};  // Se actualizará en setup si es necesario

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
  int bit;               // Bit aleatorio (0 o 1)
  float angle;           // Ángulo de rotación
} __attribute__((packed));

// Flag de registro del Central
bool centralRegistered = false;

// ==============================================
// Variables Protocolo BB84 - ALICE
// ==============================================
// Matriz de ángulos de polarización para Alice
// [Base][Bit] = Ángulo
float angulosRotacionAlice[2][2] = {
  {47.7, 2.7},   // Base 0 (+): [Horizontal (bit 0), Vertical (bit 1)]
  {25.2, 70.2}   // Base 1 (x): [Diagonal (bit 0), Antidiagonal (bit 1)]
};

int baseAlice = 0;
int bitAlice = 0;
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
    Serial.println("[Alice] Iniciando homing...");
    Serial.printf("[Alice] Posición actual antes de homing: %ld pasos\n", stepper.currentPosition());
    
    // Configurar el pin del sensor Hall con pull-up (ya que es de colector abierto)
    pinMode(HALL_SENSOR_PIN, INPUT);
    
    // Leer estado inicial del sensor
    int initialState = digitalRead(HALL_SENSOR_PIN);
    Serial.printf("[Alice] Estado inicial sensor Hall (pin %d): %d\n", HALL_SENSOR_PIN, initialState);
    
    // Reiniciar flag de interrupción
    hallTriggered = false;
    
    // Habilitar interrupción en el pin del sensor: se dispara en flanco descendente (cuando el sensor pasa a LOW)
    attachInterrupt(digitalPinToInterrupt(HALL_SENSOR_PIN), hallISR, FALLING);
    Serial.println("[Alice] Interrupción configurada en modo FALLING");
    
    // Configurar alta aceleración y velocidad para el giro rápido
    stepper.setMaxSpeed(5500);
    stepper.setAcceleration(50000);
    
    // Calcular el número de pasos correspondientes a 360°
    long stepsFor360 = (long)round((SM_RESOLUTION * microsteps * GEAR_RATIO));
    Serial.printf("[Alice] Pasos por vuelta: %ld (iniciando búsqueda en 3 vueltas)\n", stepsFor360);
    
    // Iniciar movimiento continuo: dar vuelta completa hasta detectar el imán positivo
    stepper.moveTo(stepper.currentPosition() + stepsFor360 * 3);
    
    // Si el sensor ya está activado (LOW), primero alejarse hasta que esté desactivado (HIGH)
    if (initialState == LOW) {
        Serial.println("[Alice] Sensor ya activado, alejándose primero...");
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
        
        Serial.printf("[Alice] Sensor desactivado después de %d pasos\n", steps);
        
        // Reiniciar flag y reconectar interrupción
        hallTriggered = false;
        attachInterrupt(digitalPinToInterrupt(HALL_SENSOR_PIN), hallISR, FALLING);
        
        // Reconfigurar movimiento para búsqueda completa
        stepper.moveTo(stepper.currentPosition() + stepsFor360 * 3);
    }
    
    // Giro rápido hasta detectar el imán positivo mediante la interrupción
    Serial.println("[Alice] Iniciando búsqueda del sensor Hall...");
    
    while (!hallTriggered) {
        stepper.run();
    }
    
    // Registrar la posición en que se activó el sensor
    long positionAtTrigger = stepper.currentPosition();
    Serial.printf("[Alice] ✓ Sensor Hall detectado en posición: %ld\n", positionAtTrigger);
    
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
    
    Serial.println("[Alice] Aproximación fina: avanzando paso a paso...");
    
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
    Serial.println("[Alice] Homing completado - Posición 0 establecida");
    
    // Notificar al ESP32 central vía ESP-NOW
    if (centralRegistered) {
        ResponseData response = {STATUS_HOME_COMPLETE, 0, 0, 0, 0.0};
        esp_now_send(centralMAC, (uint8_t*)&response, sizeof(response));
        Serial.println("[Alice] HOME_COMPLETE");
    }
}

// Mover a ángulo específico
void moveToAngle(float targetAngle) {
    Serial.printf("[Alice] Moviendo a %.2f grados\n", targetAngle);
    
    long steps = angleToSteps(targetAngle);
    stepper.moveTo(steps);
    
    while (stepper.distanceToGo() != 0) {
        stepper.run();
        yield();
    }
    
    float currentAngle = getCurrentAngle();
    Serial.printf("[Alice] Movimiento completado - Posición: %.2f grados\n", currentAngle);
}

// Preparar para el siguiente pulso (selección aleatoria de base y bit)
void prepareForNextPulse(uint32_t pulseNum) {
    if (!isHomed) {
        Serial.println("[Alice] ERROR: Not homed");
        if (centralRegistered) {
            ResponseData response = {STATUS_ERROR, pulseNum, 0, 0, 0.0};
            esp_now_send(centralMAC, (uint8_t*)&response, sizeof(response));
        }
        return;
    }
    
    // Generar base y bit aleatorios usando esp_random()
    baseAlice = esp_random() % 2;
    bitAlice = esp_random() % 2;
    
    // Calcular ángulo objetivo
    currentTargetAngle = angulosRotacionAlice[baseAlice][bitAlice];
    
    Serial.printf("[Alice] Pulso %d - Base:%d Bit:%d Ángulo:%.2f\n", 
                  pulseNum, baseAlice, bitAlice, currentTargetAngle);
    
    // Mover al ángulo
    moveToAngle(currentTargetAngle);
    
    // Notificar que está listo vía ESP-NOW
    if (centralRegistered) {
        ResponseData response = {STATUS_READY, pulseNum, baseAlice, bitAlice, currentTargetAngle};
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
                Serial.print("[Alice] ✓ Central registrado: ");
                for (int i = 0; i < 6; i++) {
                    Serial.printf("%02X", centralMAC[i]);
                    if (i < 5) Serial.print(":");
                }
                Serial.println();
            } else {
                Serial.println("[Alice] ERROR: No se pudo registrar Central como peer");
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
            Serial.println("[Alice] HOME");
            performHoming();
            break;
            
        case CMD_PREPARE_PULSE:
            Serial.printf("[Alice] PREPARE #%d\n", cmd.pulseNum);
            currentPulseNum = cmd.pulseNum;
            prepareForNextPulse(cmd.pulseNum);
            break;
            
        case CMD_ABORT:
            Serial.println("[Alice] ABORT");
            stepper.stop();
            break;
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== ALICE - ESP-NOW Motor ===");
    
    // Configuración WiFi/ESP-NOW optimizada
    WiFi.mode(WIFI_STA);
    esp_wifi_set_ps(WIFI_PS_NONE);
    esp_wifi_set_max_tx_power(wifiTxPower);
    esp_wifi_set_channel(ESP_NOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
    
    Serial.print("[Alice] MAC: ");
    Serial.println(WiFi.macAddress());
    Serial.printf("[Alice] Canal: %d\n", ESP_NOW_CHANNEL);
    
    // Inicializar ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("[Alice] ERROR: ESP-NOW init");
        return;
    }
    
    // Registrar callback de recepción
    if (esp_now_register_recv_cb(OnDataRecv) != ESP_OK) {
        Serial.println("[Alice] ERROR: Callback RX");
        return;
    }
    
    // Pre-registrar Central como peer (permite recibir mensajes unicast)
    // NOTA: La MAC del Central se aprenderá dinámicamente al recibir el primer mensaje
    // pero agregamos un peer broadcast para aceptar de cualquier origen inicialmente
    Serial.println("[Alice] ESP-NOW OK - Esperando conexión del Central");
    
    // Verificar tamaños de estructuras
    Serial.printf("[Alice] sizeof(CommandData): %d bytes\n", sizeof(CommandData));
    Serial.printf("[Alice] sizeof(ResponseData): %d bytes\n", sizeof(ResponseData));
    
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
        Serial.println("[Alice] TMC2130 OK");
    } else {
        Serial.println("[Alice] ERROR: TMC2130");
    }
    
    stepper.setEnablePin(ENABLE_PIN);
    stepper.setMaxSpeed(stepperSpeed);
    stepper.setAcceleration(stepperAcc);
    stepper.setPinsInverted(true, true, true);
    stepper.enableOutputs();
    
    Serial.println("[Alice] READY\n");
}

void loop() {
    // Movimiento no bloqueante
    stepper.run();
    yield();  // Optimizado: yield() en lugar de delay(1) para mejor respuesta
}
