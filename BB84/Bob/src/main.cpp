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
int   stepperCurrent = 600;   // mA (incrementado para mejor torque)
int   stepperSpeed   = 4000;  // steps/s
int   stepperAcc     = 18000; // steps/s^2 (reducido de 25000 para estabilidad)
int   microsteps     = 4;

// WiFi TX power (valores válidos: 8-84, donde 8=2dBm, 84=21dBm, unidad=0.25dBm)
// Valores comunes: 52(13dBm), 78(19.5dBm), 84(21dBm - máxima potencia)
int   wifiTxPower = 34;  // ESP32-C3 Super Mini funciona mejor con potencia moderada

// Canal WiFi/ESP-NOW - se sincronizará automáticamente con el Central
const int ESP_NOW_INITIAL_CHANNEL = 1;  // Canal inicial para recibir configuración
int ESP_NOW_CHANNEL = ESP_NOW_INITIAL_CHANNEL;  // Canal actual (se actualizará automáticamente)
bool channelConfigured = false;  // Flag de sincronización de canal

// MAC del ESP32 Central (se aprenderá automáticamente)
uint8_t centralMAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
bool centralRegistered = false;

// Driver + stepper
TMC2130Stepper driver = TMC2130Stepper(SPI_CS, SPI_MOSI, SPI_MISO, SPI_SCLK);
AccelStepper stepper = AccelStepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);

// ==============================================
// Sistema de Cola de Comandos (evita bloqueo en callback)
// ==============================================
struct PendingCommand {
  uint8_t cmd;
  uint32_t pulseNum;
  bool pending;
};

PendingCommand pendingCmd = {0, 0, false};
volatile bool abortRequested = false;

// Flag de optimización: desactivar logging durante protocolo activo
bool protocolActive = false;

// ==============================================
// ESTRUCTURAS ESP-NOW
// ==============================================
// Comandos desde el ESP32 Central
enum Command {
  CMD_SET_CHANNEL = 0,   // Configurar canal WiFi (debe ser el primero)
  CMD_PING = 1,          // Ping para verificar conexión
  CMD_HOME = 2,
  CMD_PREPARE_PULSE = 3,
  CMD_ABORT = 4
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
    
    // Reset abort flag
    abortRequested = false;
    
    // Configurar el pin del sensor Hall
    pinMode(HALL_SENSOR_PIN, INPUT);
    int initialState = digitalRead(HALL_SENSOR_PIN);
    
    // Reiniciar flag de interrupción
    hallTriggered = false;
    
    // Habilitar interrupción en el pin del sensor
    attachInterrupt(digitalPinToInterrupt(HALL_SENSOR_PIN), hallISR, FALLING);
    
    // Configurar aceleración y velocidad para el giro rápido (reducidas para estabilidad)
    stepper.setMaxSpeed(4500);     // Reducido de 5500 para evitar trabado
    stepper.setAcceleration(18000); // Reducido de 50000 para aceleración ajustada
    
    // Calcular el número de pasos correspondientes a 360°
    long stepsFor360 = (long)round((SM_RESOLUTION * microsteps * GEAR_RATIO));
    
    // Iniciar movimiento continuo: dar vuelta completa hasta detectar el imán positivo
    stepper.moveTo(stepper.currentPosition() + stepsFor360 * 3);
    
    // Si el sensor ya está activado (LOW), primero alejarse hasta que esté desactivado (HIGH)
    if (initialState == LOW) {
        Serial.println("[Bob] Sensor ya activado, alejándose...");
        detachInterrupt(digitalPinToInterrupt(HALL_SENSOR_PIN));
        
        // Avanzar hasta que el sensor se desactive (OPTIMIZADO: con yield())
        int steps = 0;
        while (digitalRead(HALL_SENSOR_PIN) == LOW && steps < 500) {
            stepper.moveTo(stepper.currentPosition() + 1);
            while (stepper.distanceToGo() != 0) {
                stepper.run();
                yield();  // CRÍTICO: Permitir ESP-NOW durante alejamiento
            }
            steps++;
            yield();
        }
        
        // Reiniciar flag y reconectar interrupción
        hallTriggered = false;
        attachInterrupt(digitalPinToInterrupt(HALL_SENSOR_PIN), hallISR, FALLING);
        
        // Reconfigurar movimiento para búsqueda completa
        stepper.moveTo(stepper.currentPosition() + stepsFor360 * 3);
    }
    
    // Giro rápido hasta detectar el imán positivo mediante la interrupción
    while (!hallTriggered && !abortRequested) {
        stepper.run();
        yield();  // Permitir callbacks ESP-NOW
    }
    
    if (abortRequested) {
        Serial.println("[Bob] Homing abortado");
        detachInterrupt(digitalPinToInterrupt(HALL_SENSOR_PIN));
        stepper.stop();
        return;
    }
    
    // Deshabilitar la interrupción y registrar posición
    detachInterrupt(digitalPinToInterrupt(HALL_SENSOR_PIN));
    long positionAtTrigger = stepper.currentPosition();
    Serial.printf("[Bob] ✓ Sensor detectado: %ld\n", positionAtTrigger);
    
    // Calcular el número de pasos correspondientes a 345° (suponiendo SM_RESOLUTION * microsteps pasos por revolución)
    long stepsFor345 = (long)round((345.0 / 360.0) * (SM_RESOLUTION * microsteps * GEAR_RATIO));
    
    // Mover 345° adicionales desde el punto de detección
    stepper.moveTo(positionAtTrigger + stepsFor345);
    while (stepper.distanceToGo() != 0 && !abortRequested) {
        stepper.run();
        yield();
    }
    
    if (abortRequested) {
        Serial.println("[Bob] Homing abortado en fase 345°");
        stepper.stop();
        return;
    }
    
    // Aproximación fina optimizada: avanzar en bloques pequeños para reducir overhead
    stepper.setMaxSpeed(3000);  // Velocidad moderada para precisión sin ser excesivamente lento
    
    // Avanzar en bloques de 3 pasos (más eficiente que 1 paso) hasta activar sensor
    while (digitalRead(HALL_SENSOR_PIN) == HIGH && !abortRequested) {
        stepper.moveTo(stepper.currentPosition() + 3);  // Mover 3 pasos por iteración
        while (stepper.distanceToGo() != 0 && !abortRequested) {
            stepper.run();
            yield();
        }
        yield();
    }
    
    // Ajuste fino final: retroceder 2 pasos para centrar mejor
    if (!abortRequested) {
        stepper.moveTo(stepper.currentPosition() - 2);
        while (stepper.distanceToGo() != 0 && !abortRequested) {
            stepper.run();
            yield();
        }
    }
    
    if (abortRequested) {
        Serial.println("[Bob] Homing abortado en fase fina");
        stepper.stop();
        return;
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
    if (!protocolActive) {
        Serial.printf("[Bob] Moviendo a %.2f grados\n", targetAngle);
    }
    
    abortRequested = false;
    long steps = angleToSteps(targetAngle);
    stepper.moveTo(steps);
    
    while (stepper.distanceToGo() != 0 && !abortRequested) {
        stepper.run();
        yield();
    }
    
    if (abortRequested) {
        if (!protocolActive) {
            Serial.println("[Bob] Movimiento abortado");
        }
        stepper.stop();
        return;
    }
    
    if (!protocolActive) {
        float currentAngle = getCurrentAngle();
        Serial.printf("[Bob] Movimiento completado - Posición: %.2f grados\n", currentAngle);
    }
}

// Preparar para el siguiente pulso (selección aleatoria de base)
void prepareForNextPulse(uint32_t pulseNum) {
    if (!isHomed) {
        if (!protocolActive) {
            Serial.println("[Bob] ERROR: Not homed");
        }
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
    
    // OPTIMIZADO: Solo loguear si no está en protocolo activo
    if (!protocolActive) {
        Serial.printf("[Bob] Pulso %d - Base:%d Ángulo:%.2f\n", 
                      pulseNum, baseBob, currentTargetAngle);
    }
    
    // Mover al ángulo
    moveToAngle(currentTargetAngle);
    
    // Notificar que está listo vía ESP-NOW (INMEDIATAMENTE)
    if (centralRegistered) {
        ResponseData response = {STATUS_READY, pulseNum, baseBob, 0, currentTargetAngle};
        esp_now_send(centralMAC, (uint8_t*)&response, sizeof(response));
    }
}

// Callback ESP-NOW para comandos desde el Central
// CRÍTICO: Este callback debe ser NO BLOQUEANTE
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
    if (len != sizeof(CommandData)) return;
    
    CommandData cmd;
    memcpy(&cmd, incomingData, sizeof(cmd));
    
    // [PRIORIDAD CRÍTICA] Registrar Central PRIMERO (antes de procesar cualquier comando)
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
    
    // [PRIORIDAD ALTA] Responder a PING inmediatamente
    if (cmd.cmd == CMD_PING) {
        Serial.println("[Bob] • PING recibido del Central, respondiendo PONG...");
        ResponseData response = {STATUS_PONG, 0, 0, 0, 0.0};
        esp_err_t result = esp_now_send(centralMAC, (uint8_t*)&response, sizeof(response));
        if (result != ESP_OK) {
            Serial.printf("[Bob] ✗ Error enviando PONG: %d\n", result);
        }
        return;  // Salir inmediatamente
    }
    
    // [PRIORIDAD ALTA] Configuración de canal (debe procesarse SIEMPRE como PING)
    if (cmd.cmd == CMD_SET_CHANNEL) {
        int newChannel = cmd.pulseNum;  // El canal viene en pulseNum
        
        // Si ya está configurado, solo reenviar confirmación (idempotente)
        if (channelConfigured && ESP_NOW_CHANNEL == newChannel) {
            // Ya configurado - solo confirmar nuevamente
            ResponseData response = {STATUS_PONG, (uint32_t)newChannel, 0, 0, 0.0};
            esp_now_send(centralMAC, (uint8_t*)&response, sizeof(response));
            return;
        }
        
        // Primera vez: configurar canal
        if (!channelConfigured) {
            Serial.printf("[Bob] • Configurando canal: %d\n", newChannel);
            
            // CRÍTICO: Si el Central ya está registrado, actualizar su canal ANTES de cambiar
            if (centralRegistered && esp_now_is_peer_exist(centralMAC)) {
                esp_now_del_peer(centralMAC);
            }
            
            // Cambiar canal WiFi
            esp_wifi_set_channel(newChannel, WIFI_SECOND_CHAN_NONE);
            ESP_NOW_CHANNEL = newChannel;
            
            // Re-registrar el Central con el nuevo canal
            if (centralRegistered) {
                esp_now_peer_info_t peerInfo = {};
                memcpy(peerInfo.peer_addr, centralMAC, 6);
                peerInfo.channel = newChannel;
                peerInfo.encrypt = false;
                
                if (esp_now_add_peer(&peerInfo) == ESP_OK) {
                    Serial.printf("[Bob] ✓ Central re-registrado en canal %d\n", newChannel);
                } else {
                    Serial.println("[Bob] ✗ ERROR: No se pudo re-registrar Central");
                }
            }
            
            channelConfigured = true;
            Serial.printf("[Bob] ✓ Canal sincronizado: %d (confirmando...)\n", ESP_NOW_CHANNEL);
        }
        
        // Enviar confirmación al Central
        ResponseData response = {STATUS_PONG, (uint32_t)newChannel, 0, 0, 0.0};
        esp_now_send(centralMAC, (uint8_t*)&response, sizeof(response));
        return;
    }
    
    // Comandos no críticos: agregar a cola (NO ejecutar aquí para evitar bloqueo)
    switch (cmd.cmd) {
        case CMD_HOME:
            Serial.println("[Bob] • Comando HOME recibido, encolando...");
            pendingCmd.cmd = cmd.cmd;
            pendingCmd.pulseNum = cmd.pulseNum;
            pendingCmd.pending = true;
            break;
            
        case CMD_PREPARE_PULSE:
            if (!protocolActive) {
                Serial.printf("[Bob] • Comando PREPARE_PULSE #%d recibido\n", cmd.pulseNum);
            }
            pendingCmd.cmd = cmd.cmd;
            pendingCmd.pulseNum = cmd.pulseNum;
            pendingCmd.pending = true;
            protocolActive = true;  // Activar modo rápido cuando empieza el protocolo
            break;
            
        case CMD_ABORT:
            Serial.println("[Bob] • Comando ABORT recibido, deteniendo motor...");
            pendingCmd.cmd = cmd.cmd;
            pendingCmd.pulseNum = cmd.pulseNum;
            pendingCmd.pending = true;
            abortRequested = true;
            protocolActive = false;  // Desactivar modo rápido
            stepper.stop();
            break;
            
        default:
            if (!protocolActive) {
                Serial.printf("[Bob] ⚠ Comando desconocido: 0x%02X\n", cmd.cmd);
            }
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
    
    // Iniciar en canal predeterminado (será actualizado por Central)
    esp_wifi_set_channel(ESP_NOW_INITIAL_CHANNEL, WIFI_SECOND_CHAN_NONE);
    
    Serial.print("[Bob] MAC: ");
    Serial.println(WiFi.macAddress());
    Serial.printf("[Bob] Canal inicial: %d (esperando configuración del Central)\n", ESP_NOW_INITIAL_CHANNEL);
    
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
    
    // Configurar pines y SPI
    pinMode(SPI_CS, OUTPUT);
    digitalWrite(SPI_CS, HIGH);
    pinMode(ENABLE_PIN, OUTPUT);
    pinMode(DIR_PIN, OUTPUT);
    pinMode(STEP_PIN, OUTPUT);
    digitalWrite(ENABLE_PIN, HIGH);
    
    SPI.begin(SPI_SCLK, SPI_MISO, SPI_MOSI, SPI_CS);
    delay(30);  // Reducido de 50ms
    
    // Inicializar TMC2130
    driver.begin();
    delay(30);  // Reducido de 50ms
    digitalWrite(ENABLE_PIN, LOW);
    
    if (driver.test_connection()) {
        driver.rms_current(stepperCurrent);
        driver.stealthChop(0);        // SpreadCycle para mejor torque en alta velocidad
        driver.pwm_autoscale(true);   // Ajuste automático de PWM
        driver.microsteps(microsteps);
        
        // Configuración avanzada para reducir trabado en movimientos rápidos
        driver.toff(4);               // Off time (duración apagado chopper) - balance velocidad/estabilidad
        driver.blank_time(24);        // Tiempo de blanking (reduce ruido)
        driver.hysteresis_start(3);   // Histéresis inicio
        driver.hysteresis_end(1);     // Histéresis fin
        driver.interpolate(true);     // Interpolación a 256 microsteps (suaviza movimiento)
        
        Serial.println("[Bob] TMC2130 OK (SpreadCycle + Interpolación)");
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
    // Procesar comandos pendientes de la cola
    if (pendingCmd.pending) {
        pendingCmd.pending = false;  // Marcar como procesándose
        
        switch (pendingCmd.cmd) {
            case CMD_HOME:
                Serial.println("[Bob] Ejecutando HOME");
                performHoming();
                break;
                
            case CMD_PREPARE_PULSE:
                // OPTIMIZADO: Sin logging durante protocolo para máxima velocidad
                if (!protocolActive) {
                    Serial.printf("[Bob] Ejecutando PREPARE #%d\n", pendingCmd.pulseNum);
                }
                currentPulseNum = pendingCmd.pulseNum;
                prepareForNextPulse(pendingCmd.pulseNum);
                break;
                
            case CMD_ABORT:
                Serial.println("[Bob] Ejecutando ABORT");
                abortRequested = true;
                stepper.stop();
                break;
        }
    }
    
    // OPTIMIZADO: Movimiento no bloqueante solo si hay distancia por recorrer
    if (stepper.distanceToGo() != 0) {
        stepper.run();
    }
    yield();  // Permitir callbacks ESP-NOW
}
