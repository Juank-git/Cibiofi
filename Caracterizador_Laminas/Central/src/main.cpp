#include <Arduino.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include <WebSocketsServer.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <esp_now.h>
#include <esp_wifi.h>

// ==============================================
// Configuración de RED
// ==============================================
const char* ssid_sta = "Loic";
const char* password_sta = "loic1234";

// IP estática
IPAddress local_IP(192, 168, 137, 200);
IPAddress gateway(192, 168, 137, 1);
IPAddress subnet(255, 255, 255, 0);

WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

// ==============================================
// ESP-NOW - Comunicación con Motor
// ==============================================
const int ESP_NOW_CHANNEL = 11;

// IMPORTANTE: Cambiar por la MAC del ESP32-C3 Motor
// Obtener con WiFi.macAddress() del Motor
uint8_t motorMAC[] = {0x0C, 0x4E, 0xA0, 0x64, 0xC0, 0xB8};  // ⚠️ ACTUALIZAR

// Estructuras ESP-NOW (deben coincidir con Motor)
enum Command {
  CMD_PING = 0,
  CMD_HOME = 1,
  CMD_MOVE_TO_ANGLE = 2,
  CMD_ABORT = 3
};

struct CommandData {
  uint8_t cmd;
  float targetAngle;
  uint32_t reserved;
} __attribute__((packed));

enum Status {
  STATUS_PONG = 0,
  STATUS_HOME_COMPLETE = 1,
  STATUS_READY = 2,
  STATUS_ERROR = 3
};

struct ResponseData {
  uint8_t status;
  float currentAngle;
  uint32_t reserved;
} __attribute__((packed));

// Flags de estado
bool motorConnected = false;
bool motorHomed = false;
bool motorReady = false;
float motorCurrentAngle = 0.0;

// ==============================================
// Comunicación Serial con Python
// ==============================================
#define PYTHON_SERIAL Serial  // Puerto USB

// ==============================================
// Variables de Caracterización
// ==============================================
float angleMax = 0.0;
float angleStep = 0.0;
int numSamples = 0;
int numExecutions = 1;
int currentExecution = 0;

bool processStarted = false;
bool isPaused = false;
float currentAngle = 0.0;

// ==============================================
// Funciones ESP-NOW
// ==============================================

void onESPNowSend(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Optimizado: callback vacío para máxima velocidad
}

void onESPNowReceive(const uint8_t *mac_addr, const uint8_t *data, int len) {
  if(len != sizeof(ResponseData)) return;
  
  ResponseData response;
  memcpy(&response, data, sizeof(response));
  
  bool isMotor = (memcmp(mac_addr, motorMAC, 6) == 0);
  
  if(isMotor && !motorConnected) {
    motorConnected = true;
    Serial.println("[Central] ✓ Motor conectado");
  }
  
  switch(response.status) {
    case STATUS_PONG:
      break;
      
    case STATUS_HOME_COMPLETE:
      motorHomed = true;
      Serial.println("[Central] Motor homing completado");
      webSocket.broadcastTXT("STATUS:HOMING_COMPLETE");
      break;
      
    case STATUS_READY:
      motorReady = true;
      motorCurrentAngle = response.currentAngle;
      Serial.printf("[Central] Motor listo en %.2f°\n", motorCurrentAngle);
      break;
      
    case STATUS_ERROR:
      Serial.println("[Central] ERROR en motor");
      webSocket.broadcastTXT("STATUS:ERROR:Motor error");
      break;
  }
}

void sendCommandToMotor(uint8_t cmd, float angle = 0.0) {
  CommandData command = {cmd, angle, 0};
  esp_err_t result = esp_now_send(motorMAC, (uint8_t*)&command, sizeof(command));
  
  if(result != ESP_OK) {
    Serial.printf("[Central] ERROR enviando comando: %d\n", result);
  }
}

void waitForMotorReady() {
  unsigned long timeout = millis();
  while (!motorReady && millis() - timeout < 10000) {
    yield();  // Optimizado: sin delay bloqueante
  }
  
  if (!motorReady) {
    Serial.println("[Central] TIMEOUT esperando motor");
    webSocket.broadcastTXT("STATUS:ERROR:Motor timeout");
  }
}

// ==============================================
// Funciones de Medición (Python Serial)
// ==============================================

float getMeasurementFromPython(int numSamples) {
  // Solicitar medición a Python
  PYTHON_SERIAL.println("GET_POWER:" + String(numSamples));
  PYTHON_SERIAL.flush();
  
  float totalPower = 0.0;
  int receivedSamples = 0;
  
  // Limpiar buffer
  while(PYTHON_SERIAL.available()) { 
    PYTHON_SERIAL.read(); 
  }
  
  // Recibir muestras de Python
  unsigned long timeout = millis();
  while (receivedSamples < numSamples && millis() - timeout < 10000) {
    if (PYTHON_SERIAL.available() > 0) {
      String data = PYTHON_SERIAL.readStringUntil('\n');
      data.trim();
      
      if (data.length() > 0) {
        float powerMicroW = data.toFloat();
        totalPower += powerMicroW;
        receivedSamples++;
        timeout = millis();  // Reset timeout en cada muestra
      }
    }
    yield();
  }
  
  if (receivedSamples < numSamples) {
    Serial.printf("[Central] ADVERTENCIA: Solo %d/%d muestras recibidas\n", 
                  receivedSamples, numSamples);
  }
  
  return (receivedSamples > 0) ? (totalPower / receivedSamples) : 0.0;
}

// ==============================================
// WebSocket Handlers
// ==============================================

void handleWebSocketMessage(uint8_t num, uint8_t* payload, size_t length) {
    String message = String((char*)payload).substring(0, length);

    if (message.startsWith("SAVE_SERIES:")) {
        DynamicJsonDocument doc(8192);
        deserializeJson(doc, message.substring(12));
        
        String dirPath = doc["dirPath"].as<String>();
        JsonArray series = doc["series"];
        
        if (!SPIFFS.mkdir(dirPath)) {
            Serial.println("Error creando directorio: " + dirPath);
            return;
        }

        DynamicJsonDocument filesInfo(1024);
        JsonArray filesArray = filesInfo.to<JsonArray>();

        for(JsonVariant v : series) {
            String fileName = dirPath + "/" + v["name"].as<String>();
            File file = SPIFFS.open(fileName, "w");
            
            if(!file) {
                Serial.println("Error creando archivo: " + fileName);
                continue;
            }

            file.print(v["content"].as<String>());
            file.close();

            JsonObject fileInfo = filesArray.createNestedObject();
            fileInfo["name"] = v["name"].as<String>();
            fileInfo["path"] = fileName;
        }

        String response;
        serializeJson(filesArray, response);
        webSocket.sendTXT(num, "SAVE_COMPLETE:" + response);
    }
    else if (message.startsWith("CONFIG:")) {
        int firstComma = message.indexOf(',');
        int secondComma = message.indexOf(',', firstComma + 1);
        int thirdComma = message.indexOf(',', secondComma + 1);

        angleMax = message.substring(7, firstComma).toFloat();
        angleStep = message.substring(firstComma + 1, secondComma).toFloat();
        numSamples = message.substring(secondComma + 1, thirdComma).toInt();
        numExecutions = message.substring(thirdComma + 1).toInt();
        currentExecution = 0;
        currentAngle = 0.0;

        Serial.println("Configuración recibida:");
        Serial.printf("  Ángulo máximo: %.2f°\n", angleMax);
        Serial.printf("  Paso angular: %.2f°\n", angleStep);
        Serial.printf("  Muestras por punto: %d\n", numSamples);
        Serial.printf("  Ejecuciones: %d\n", numExecutions);

        // Verificar que el motor esté listo
        if (!motorConnected) {
            webSocket.sendTXT(num, "STATUS:ERROR:Motor no conectado");
            return;
        }
        
        if (!motorHomed) {
            webSocket.sendTXT(num, "STATUS:ERROR:Motor sin homing");
            return;
        }

        processStarted = true;
        isPaused = false;
        webSocket.sendTXT(num, "STATUS:START");
    }
    else if (message == "PAUSE") {
        isPaused = true;
        webSocket.sendTXT(num, "STATUS:PAUSED");
        Serial.println("Proceso pausado");
    }
    else if (message == "RESUME") {
        isPaused = false;
        webSocket.sendTXT(num, "STATUS:RESUMED");
        Serial.println("Proceso reanudado");
    }
    else if (message == "RESET") {
        processStarted = false;
        isPaused = false;
        
        Serial.println("Ejecutando reset y homing...");
        motorHomed = false;
        sendCommandToMotor(CMD_HOME);
        
        webSocket.sendTXT(num, "STATUS:RESETTING");
    }
    else if (message == "HOMING") {
        processStarted = false;
        isPaused = false;
        
        Serial.println("Homing solicitado desde web...");
        motorHomed = false;
        sendCommandToMotor(CMD_HOME);
        
        webSocket.sendTXT(num, "STATUS:HOMING");
    }
}

void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    if (type == WStype_TEXT) {
        handleWebSocketMessage(num, payload, length);
    }
}

// ==============================================
// Setup y Loop
// ==============================================

void setup() {
    PYTHON_SERIAL.begin(115200);
    delay(500);
    Serial.println("\n=== CENTRAL - CARACTERIZADOR LÁMINAS ===");

    // Montar SPIFFS
    if (!SPIFFS.begin(true)) {
        Serial.println("Error montando SPIFFS");
        return;
    }

    // Configurar WiFi
    WiFi.mode(WIFI_AP_STA);
    WiFi.config(local_IP, gateway, subnet);
    WiFi.begin(ssid_sta, password_sta);
    
    Serial.print("Conectando a WiFi...");
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
        delay(500);
        Serial.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n✓ WiFi conectado");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\n✗ Error conectando WiFi");
    }

    // Configurar ESP-NOW
    Serial.println("\n=== Configurando ESP-NOW ===");
    esp_wifi_set_ps(WIFI_PS_NONE);
    
    uint8_t wifiChannel;
    wifi_second_chan_t secondChannel;
    esp_wifi_get_channel(&wifiChannel, &secondChannel);
    Serial.printf("Canal WiFi: %d\n", wifiChannel);
    
    if (wifiChannel != ESP_NOW_CHANNEL) {
        Serial.printf("⚠️ ADVERTENCIA: Router en canal %d, ESP-NOW en canal %d\n", 
                      wifiChannel, ESP_NOW_CHANNEL);
    }
    
    if (esp_now_init() != ESP_OK) {
        Serial.println("ERROR: ESP-NOW init");
        return;
    }
    
    esp_now_register_send_cb(onESPNowSend);
    esp_now_register_recv_cb(onESPNowReceive);
    
    // Agregar Motor como peer
    esp_now_peer_info_t peerMotor = {};
    memcpy(peerMotor.peer_addr, motorMAC, 6);
    peerMotor.channel = ESP_NOW_CHANNEL;
    peerMotor.encrypt = false;
    
    if (esp_now_add_peer(&peerMotor) != ESP_OK) {
        Serial.println("ERROR: Agregar peer Motor");
        return;
    }
    
    Serial.println("✓ ESP-NOW configurado");
    Serial.print("Motor MAC: ");
    for(int i = 0; i < 6; i++) {
        Serial.printf("%02X", motorMAC[i]);
        if(i < 5) Serial.print(":");
    }
    Serial.println();
    
    Serial.printf("sizeof(CommandData): %d bytes\n", sizeof(CommandData));
    Serial.printf("sizeof(ResponseData): %d bytes\n", sizeof(ResponseData));

    // Configurar servidor web
    server.on("/", HTTP_GET, []() {
        File file = SPIFFS.open("/index.html", "r");
        if (!file) {
            server.send(500, "text/plain", "Error cargando página");
            return;
        }
        server.streamFile(file, "text/html");
        file.close();
    });

    server.on("/favicon.ico", HTTP_GET, []() {
        File file = SPIFFS.open("/favicon.ico", "r");
        if (!file) {
            server.send(404, "text/plain", "Favicon no encontrado");
            return;
        }
        server.streamFile(file, "image/x-icon");
        file.close();
    });

    server.on("/download", HTTP_GET, []() {
        String path = server.arg("path");
        if(SPIFFS.exists(path)) {
            File file = SPIFFS.open(path, "r");
            server.streamFile(file, "text/plain");
            file.close();
        } else {
            server.send(404, "text/plain", "File not found");
        }
    });

    server.begin();
    Serial.println("Servidor HTTP iniciado");

    // Configurar WebSocket
    webSocket.begin();
    webSocket.onEvent(onWebSocketEvent);
    Serial.println("WebSocket iniciado\n");

    // Ping al motor
    Serial.println("=== Detectando Motor ===");
    delay(1000);
    
    for(int i = 0; i < 5 && !motorConnected; i++) {
        Serial.printf("Ping motor %d/5...\n", i+1);
        sendCommandToMotor(CMD_PING);
        delay(500);
    }
    
    Serial.printf("Motor: %s\n", motorConnected ? "✓" : "✗");
    Serial.println("=======================\n");
    
    Serial.println("Sistema listo");
}

void loop() {
    server.handleClient();
    webSocket.loop();

    if (processStarted && !isPaused) {
        // Calcular siguiente ángulo
        float nextAngle = currentAngle + angleStep;
        bool isLastStep = (abs(angleMax - nextAngle) < 0.01 || nextAngle > angleMax);
        
        if (isLastStep) {
            nextAngle = angleMax;  // Asegurar que llegue exactamente al ángulo máximo
        }

        Serial.printf("\n[Barrido] Moviendo a %.2f°\n", nextAngle);
        
        // Enviar comando de movimiento al motor
        motorReady = false;
        sendCommandToMotor(CMD_MOVE_TO_ANGLE, nextAngle);
        
        // Esperar a que el motor esté listo
        waitForMotorReady();
        
        if (!motorReady) {
            processStarted = false;
            return;
        }

        // Tomar medición via Python
        Serial.printf("[Barrido] Tomando %d mediciones...\n", numSamples);
        float averagePower = getMeasurementFromPython(numSamples);
        
        Serial.printf("[Barrido] Potencia promedio: %.6f µW\n", averagePower);
        
        // Enviar datos a web
        webSocket.broadcastTXT("DATA:" + String(motorCurrentAngle) + "," + String(averagePower));
        
        currentAngle = nextAngle;

        // Verificar si completamos el barrido
        if (isLastStep) {
            currentExecution++;
            Serial.printf("✓ Ejecución %d/%d completada\n", currentExecution, numExecutions);
            
            if (currentExecution < numExecutions) {
                webSocket.broadcastTXT("STATUS:NEW_RUN");
                Serial.println("Iniciando nueva ejecución...");
                
                // Verificar si angleMax es múltiplo de 360°
                bool isFullRotation = (abs(fmod(angleMax, 360.0)) < 0.01);
                
                if (!isFullRotation) {
                    // Volver a 0° si no es rotación completa
                    motorReady = false;
                    sendCommandToMotor(CMD_MOVE_TO_ANGLE, 0.0);
                    waitForMotorReady();
                }
                
                currentAngle = 0.0;
                processStarted = true;
            } else {
                // Todas las ejecuciones completadas
                bool isFullRotation = (abs(fmod(angleMax, 360.0)) < 0.01);
                if (!isFullRotation) {
                    motorReady = false;
                    sendCommandToMotor(CMD_MOVE_TO_ANGLE, 0.0);
                    waitForMotorReady();
                }
                
                processStarted = false;
                webSocket.broadcastTXT("STATUS:COMPLETE");
                Serial.println("✅ Caracterización completada\n");
            }
        }
    }
    
    delay(10);  // Pequeña pausa para no saturar el loop
}
