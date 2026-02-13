#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <TMC2130Stepper.h>
#include <AccelStepper.h>
#include <math.h>

// ======================
// CONFIGURACIÓN - MOTOR CARACTERIZADOR WEB
// ======================

// --- CREDENCIALES WIFI (MODO STA) ---
const char* ssid = "Loic";           // Nombre de tu red WiFi
const char* password = "Loic1234";   // Contraseña de tu red WiFi

// Potencia TX WiFi (valores: 8-84, donde 8=2dBm, 52=13dBm, 84=21dBm, unidad=0.25dBm)
int wifiTxPower = 34;  // 34 = 8.5dBm ≈ 8dBm (ESP32-C3 usa escala 0-84)

// IP estática - se calculará automáticamente basándose en la red del router
IPAddress local_IP;
IPAddress gateway;
IPAddress subnet;

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
int   stepperAcc     = 18000; // steps/s^2 (reducido para estabilidad)
int   microsteps     = 4;

// Driver + stepper
TMC2130Stepper driver = TMC2130Stepper(SPI_CS, SPI_MOSI, SPI_MISO, SPI_SCLK);
AccelStepper stepper = AccelStepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);

// Servidor Web HTTP
AsyncWebServer server(80);

// ==============================================
// VARIABLES DE ESTADO
// ==============================================
volatile bool hallTriggered = false;
bool isHomed = false;
float currentTargetAngle = 0.0;
enum MotorState { IDLE, HOMING, MOVING, ERROR_STATE };
MotorState motorState = IDLE;

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
    motorState = HOMING;
    
    pinMode(HALL_SENSOR_PIN, INPUT);
    int initialState = digitalRead(HALL_SENSOR_PIN);
    
    hallTriggered = false;
    attachInterrupt(digitalPinToInterrupt(HALL_SENSOR_PIN), hallISR, FALLING);
    
    stepper.setMaxSpeed(4500);     // Reducido de 5500 para evitar trabado
    stepper.setAcceleration(18000); // Reducido de 50000 para aceleración ajustada
    
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
    motorState = IDLE;
    Serial.println("[Motor] Homing completado - Posición 0°");
}

void moveToAngle(float targetAngle) {
    if (!isHomed) {
        Serial.println("[Motor] ERROR: Not homed");
        motorState = ERROR_STATE;
        return;
    }
    
    Serial.printf("[Motor] Moviendo a %.2f°\n", targetAngle);
    motorState = MOVING;
    
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
    
    motorState = IDLE;
}

// ==============================================
// FUNCIONES HTTP API
// ==============================================

String getStatusJSON() {
    StaticJsonDocument<256> doc;
    
    const char* stateStr = "idle";
    if (motorState == HOMING) stateStr = "homing";
    else if (motorState == MOVING) stateStr = "moving";
    else if (motorState == ERROR_STATE) stateStr = "error";
    
    doc["status"] = stateStr;
    doc["homed"] = isHomed;
    doc["angle"] = getCurrentAngle();
    doc["targetAngle"] = currentTargetAngle;
    doc["moving"] = (stepper.distanceToGo() != 0);
    
    // Calcular ángulo mínimo
    float minAngle = 360.0 / (SM_RESOLUTION * microsteps * GEAR_RATIO);
    doc["minAngle"] = minAngle;
    
    String output;
    serializeJson(doc, output);
    return output;
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== MOTOR CARACTERIZADOR - WEB INTERFACE ===");
    
    // ========== LITTLEFS ==========
    if (!LittleFS.begin(true)) {
        Serial.println("[LittleFS] ERROR: Mount failed");
        return;
    }
    Serial.println("[LittleFS] Montado OK");
    
    // ========== WIFI STA con IP estática .100 ==========
    Serial.println("[WiFi] Conectando a red WiFi...");
    WiFi.mode(WIFI_STA);
    WiFi.setTxPower((wifi_power_t)wifiTxPower);  // Configurar potencia TX (8dBm)
    WiFi.begin(ssid, password);
    
    // Conectar primero con DHCP para obtener configuración de red
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\n[WiFi] ERROR: No se pudo conectar");
        return;
    }
    
    Serial.println("\n[WiFi] Conectado temporalmente con DHCP");
    
    // Obtener configuración de red del router
    gateway = WiFi.gatewayIP();
    subnet = WiFi.subnetMask();
    
    // Construir IP local con los primeros 3 octetos del gateway y .100 al final
    local_IP = IPAddress(gateway[0], gateway[1], gateway[2], 100);
    
    Serial.printf("[WiFi] Red detectada: %s\n", gateway.toString().c_str());
    Serial.printf("[WiFi] Configurando IP estática: %s\n", local_IP.toString().c_str());
    
    // Desconectar y reconectar con IP estática
    WiFi.disconnect();
    delay(500);
    
    WiFi.mode(WIFI_STA);
    WiFi.config(local_IP, gateway, subnet);
    WiFi.begin(ssid, password);
    
    attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\n[WiFi] ERROR: No se pudo reconectar con IP estática");
        return;
    }
    
    IPAddress IP = WiFi.localIP();
    Serial.println("\n[WiFi] Conectado con IP estática");
    Serial.printf("[WiFi] Red: %s\n", ssid);
    Serial.printf("[WiFi] IP: %s\n", IP.toString().c_str());
    Serial.printf("[WiFi] Potencia TX: %d (≈8dBm)\n", wifiTxPower);
    Serial.printf("[WiFi] Gateway: %s\n", gateway.toString().c_str());
    
    // ========== API REST ENDPOINTS ==========
    
    // GET /api/status - Obtener estado del motor
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request){
        String json = getStatusJSON();
        request->send(200, "application/json", json);
    });
    
    // POST /api/home - Iniciar homing
    server.on("/api/home", HTTP_POST, [](AsyncWebServerRequest *request){
        Serial.println("[API] Comando: HOME");
        performHoming();
        request->send(200, "application/json", "{\"success\":true}");
    });
    
    // POST /api/move - Mover a ángulo específico
    server.on("/api/move", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
            StaticJsonDocument<128> doc;
            DeserializationError error = deserializeJson(doc, (const char*)data);
            
            if (error) {
                request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                return;
            }
            
            float angle = doc["angle"];
            Serial.printf("[API] Comando: MOVE to %.2f°\n", angle);
            currentTargetAngle = angle;
            moveToAngle(angle);
            request->send(200, "application/json", "{\"success\":true}");
    });
    
    // POST /api/stop - Detener motor
    server.on("/api/stop", HTTP_POST, [](AsyncWebServerRequest *request){
        Serial.println("[API] Comando: STOP");
        stepper.stop();
        motorState = IDLE;
        request->send(200, "application/json", "{\"success\":true}");
    });
    
    // ========== SERVIDOR WEB ==========
    // Página principal
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/index.html", "text/html");
    });
    
    // Archivos estáticos
    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/style.css", "text/css");
    });
    
    server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/script.js", "application/javascript");
    });
    
    server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
        // Intentar servir .ico, si no existe usar .svg
        if (LittleFS.exists("/favicon.ico")) {
            request->send(LittleFS, "/favicon.ico", "image/x-icon");
        } else {
            request->send(LittleFS, "/favicon.svg", "image/svg+xml");
        }
    });
    
    server.on("/favicon.svg", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/favicon.svg", "image/svg+xml");
    });
    
    // 404 handler
    server.onNotFound([](AsyncWebServerRequest *request){
        request->send(404, "text/plain", "Not Found");
    });
    
    server.begin();
    Serial.printf("[Server] Servidor web iniciado\n");
    Serial.printf("[Server] Acceder desde: http://%s\n", IP.toString().c_str());
    
    // ========== SPI Y MOTOR ==========
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
        driver.stealthChop(0);        // SpreadCycle para mejor torque en alta velocidad
        driver.pwm_autoscale(true);   // Ajuste automático de PWM
        driver.microsteps(microsteps);
        
        // Configuración avanzada para reducir trabado en movimientos rápidos
        driver.toff(4);               // Off time (duración apagado chopper) - balance velocidad/estabilidad
        driver.blank_time(24);        // Tiempo de blanking (reduce ruido)
        driver.hysteresis_start(3);   // Histéresis inicio
        driver.hysteresis_end(1);     // Histéresis fin
        driver.interpolate(true);     // Interpolación a 256 microsteps (suaviza movimiento)
        
        Serial.println("[Motor] TMC2130 OK (SpreadCycle + Interpolación)");
    } else {
        Serial.println("[Motor] ERROR: TMC2130 no responde");
    }
    
    stepper.setEnablePin(ENABLE_PIN);
    stepper.setMaxSpeed(stepperSpeed);
    stepper.setAcceleration(stepperAcc);
    stepper.setPinsInverted(false, true, false);
    stepper.enableOutputs();
    
    float minAngle = 360.0 / (SM_RESOLUTION * microsteps * GEAR_RATIO);
    Serial.printf("[Motor] Ángulo mínimo: %.2f°\n", minAngle);
    Serial.println("[Motor] READY\n");
}

void loop() {
    // OPTIMIZADO: Movimiento no bloqueante solo si hay distancia por recorrer
    if (stepper.distanceToGo() != 0) {
        stepper.run();
    }
    yield();
}
