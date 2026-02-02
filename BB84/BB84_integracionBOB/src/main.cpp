#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <esp_now.h>
#include <esp_wifi.h>

// ==============================================
// Configuración de RED
// ==============================================


/*
const char* ssid = "Loic";
const char* password = "Loic1234";

// IP estática
IPAddress local_IP(192, 168, 137, 100);
IPAddress gateway(192, 168, 137, 1);
IPAddress subnet(255, 255, 255, 0);
*/

const char* ssid = "Anjoca_2.4G";
const char* password = "6025598137";

// IP estática
IPAddress local_IP(192, 168, 1, 100);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

// ==============================================
// LEDs de Conexión
// ==============================================
#define LED_ALICE_PIN 23  // LED Rojo - Indica conexión con Alice (intercambiado)
#define LED_BOB_PIN 22    // LED Azul - Indica conexión con Bob (intercambiado)

// ==============================================
// ESP-NOW - Comunicación con Alice y Bob
// ==============================================
uint8_t aliceMAC[] = {0x0C,0x4E,0xA0,0x64,0xC0,0xB8};  // MAC de la Super Mini 1 (Alice)
uint8_t bobMAC[] = {0x0C,0x4E,0xA0,0x65,0x48,0x3C};     // MAC de la Super Mini 2 (Bob)

// Estructuras de comunicación ESP-NOW
struct CommandData {
  uint8_t cmd;          // Tipo de comando
  uint32_t pulseNum;    // Número de pulso actual
  uint32_t totalPulses; // Total de pulsos a transmitir
};

struct ResponseData {
  uint8_t status;       // Estado: READY, ERROR, HOME_COMPLETE
  uint32_t pulseNum;    // Número de pulso
  int base;             // Base usada (Alice: 0-1, Bob: 0-1)
  int bit;              // Bit enviado (solo Alice: 0-1)
  float angle;          // Ángulo alcanzado
};

// Comandos
#define CMD_PING 0x00          // Comando de ping para verificar conexión
#define CMD_HOME 0x01
#define CMD_PREPARE_PULSE 0x02
#define CMD_ABORT 0x03
#define CMD_START_PROTOCOL 0x04

// Estados
#define STATUS_PONG 0x00              // Respuesta al ping
#define STATUS_HOME_COMPLETE 0x10
#define STATUS_READY 0x20
#define STATUS_ERROR 0x30

// Flags de estado de los motores
bool aliceReady = false;
bool bobReady = false;
bool aliceHomed = false;
bool bobHomed = false;

// Flags de conexión (para LEDs)
bool aliceConnected = false;
bool bobConnected = false;

// Datos recibidos de Alice y Bob
int baseAlice = 0;
int bitAlice = 0;
float angleAlice = 0.0;
int baseBob = 0;
float angleBob = 0.0;

// Contador de pulsos
uint32_t currentPulseNum = 0;
uint32_t totalPulses = 0;

// ==============================================
// Comunicación ESP32 <-> FPGA
// ==============================================
#define START_BYTE 0xAA
#define EMPTY_ID 0xFE  // ID que indica que las memorias FIFO están vacías
#define TX_ENDED_ID 0xFD  // ID que indica que se terminó el protocolo
#define FIFO_0_ID 0xF0
#define FIFO_1_ID 0xF1

HardwareSerial UARTFPGA(2);
#define RX_PIN 16
#define TX_PIN 17
#define RESET_PIN 25
#define NEXT_PULSE_PIN 4

// Señal de control de FPGA
bool start_protocol = false;

// Flags para control de mensajes de la FPGA
bool empty_id_received = false;
bool tx_ended_received = false;

// Contadores de detecciones
uint32_t detector0_count = 0;
uint32_t detector1_count = 0;

// ==============================================
// Declaraciones de Funciones
// ==============================================
void enviarConfiguracion(uint32_t num_pulsos, uint32_t duracion_us);
void sendDataToWeb();
void generateResetPulse();
void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);
void handleWebSocketMessage(uint8_t num, uint8_t* payload, size_t length);
void resetCounters();
void abortarProtocolo();
void checkUARTFPGAMessages();
void generateNextPulseReady();
void sendHomingCommand();
void waitForMotorsReady();
void onESPNowSend(const uint8_t *mac_addr, esp_now_send_status_t status);
void onESPNowReceive(const uint8_t *mac_addr, const uint8_t *data, int len);
void sendCommandToAlice(uint8_t cmd, uint32_t pulseNum = 0);
void sendCommandToBob(uint8_t cmd, uint32_t pulseNum = 0);
void prepareNextPulse();

void setup() {
  // Inicialización de comunicaciones
  Serial.begin(115200);
  Serial.println("Iniciando configuración...");
  UARTFPGA.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
  
  // Configuración del pin de reset FPGA
  pinMode(RESET_PIN, OUTPUT);
  digitalWrite(RESET_PIN, HIGH);

  // Configuración del pin de pulso de siguiente qubit
  pinMode(NEXT_PULSE_PIN, OUTPUT);
  digitalWrite(NEXT_PULSE_PIN, HIGH);  // Nivel inicial alto
  
  // Configuración de LEDs de conexión
  pinMode(LED_ALICE_PIN, OUTPUT);
  pinMode(LED_BOB_PIN, OUTPUT);
  digitalWrite(LED_ALICE_PIN, LOW);  // Apagado inicialmente
  digitalWrite(LED_BOB_PIN, LOW);    // Apagado inicialmente
  Serial.printf("LEDs de conexión configurados:\n");
  Serial.printf("  - Alice: Pin %d (Rojo)\n", LED_ALICE_PIN);
  Serial.printf("  - Bob:   Pin %d (Azul)\n", LED_BOB_PIN);
  
  // TEST: Parpadear LEDs para verificar hardware
  Serial.println("TEST LEDs: Parpadeando...");
  for(int i = 0; i < 3; i++) {
    digitalWrite(LED_ALICE_PIN, HIGH);
    digitalWrite(LED_BOB_PIN, HIGH);
    delay(200);
    digitalWrite(LED_ALICE_PIN, LOW);
    digitalWrite(LED_BOB_PIN, LOW);
    delay(200);
  }
  Serial.println("TEST LEDs completado");
  
  // ==============================================
  // Configuración Wi-Fi
  // ==============================================
  WiFi.config(local_IP, gateway, subnet);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Conectando a Wi-Fi...");
  }
  Serial.println("Wi-Fi conectado.");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // Configuración de archivos SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("Error montando SPIFFS");
    return;
  }

  // ==============================================
  // Configuración del servidor web y web socket
  // ==============================================
  server.on("/", HTTP_GET, []() {
    File file = SPIFFS.open("/index.html", "r");
    if (!file) {
      server.send(500, "text/plain", "Error al abrir index.html");
      return;
    }
    String html = file.readString();
    server.send(200, "text/html", html);
    file.close();
  });
  
  server.on("/styles.css", HTTP_GET, []() {
    File file = SPIFFS.open("/styles.css", "r");
    if (!file) {
      server.send(500, "text/plain", "Error al abrir styles.css");
      return;
    }
    String css = file.readString();
    server.send(200, "text/css", css);
    file.close();
  });

  server.on("/script.js", HTTP_GET, []() {
    File file = SPIFFS.open("/script.js", "r");
    if (!file) {
      server.send(500, "text/plain", "Error al abrir script.js");
      return;
    }
    String css = file.readString();
    server.send(200, "application/javascript", css);
    file.close();
  });

  server.on("/scriptPost.js", HTTP_GET, []() {
    File file = SPIFFS.open("/scriptPost.js", "r");
    if (!file) {
      server.send(500, "application/javascript", "Error al abrir scriptPost.js");
      return;
    }
    String css = file.readString();
    server.send(200, "text/css", css);
    file.close();
  });

  server.on("/scriptCascade.js", HTTP_GET, []() {
    File file = SPIFFS.open("/scriptCascade.js", "r");
    if (!file) {
      server.send(500, "text/plain", "Error al abrir scriptCascade.js");
      return;
    }
    String css = file.readString();
    server.send(200, "application/javascript", css);
    file.close();
  });

  // Servir favicon (opcional)
  server.on("/favicon.ico", HTTP_GET, []() {
    File file = SPIFFS.open("/favicon.ico", "r");
    if (!file) {
      server.send(404, "text/plain", "favicon no encontrado");
      return;
    }
    server.streamFile(file, "image/x-icon");
    file.close();
  });
  
  server.begin();
  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);

  // ==============================================
  // Configurar ESP-NOW para Alice y Bob
  // ==============================================
  Serial.println("Configurando ESP-NOW...");
  
  // Modo híbrido: WiFi AP/STA + ESP-NOW
  WiFi.mode(WIFI_AP_STA);
  esp_wifi_set_ps(WIFI_PS_NONE);
  
  // Fijar canal 13 para ESP-NOW
  int channel = 13;
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  
  if (esp_now_init() != ESP_OK) {
    Serial.println("[ERR] ESP-NOW init falló");
    return;
  }
  Serial.println("[OK] ESP-NOW inicializado");
  
  // Registrar callbacks
  esp_now_register_send_cb(onESPNowSend);
  esp_now_register_recv_cb(onESPNowReceive);
  
  // Agregar peer Alice
  esp_now_peer_info_t peerAlice{};
  memcpy(peerAlice.peer_addr, aliceMAC, 6);
  peerAlice.channel = channel;
  peerAlice.encrypt = false;
  
  if (esp_now_add_peer(&peerAlice) != ESP_OK) {
    Serial.println("[ERR] No se pudo agregar peer Alice");
    return;
  }
  Serial.println("[OK] Peer Alice agregado");
  
  // Agregar peer Bob
  esp_now_peer_info_t peerBob{};
  memcpy(peerBob.peer_addr, bobMAC, 6);
  peerBob.channel = channel;
  peerBob.encrypt = false;
  
  if (esp_now_add_peer(&peerBob) != ESP_OK) {
    Serial.println("[ERR] No se pudo agregar peer Bob");
    return;
  }
  Serial.println("[OK] Peer Bob agregado");
  
  // Mostrar MACs esperadas
  Serial.println("\n=== MACs Configuradas ===");
  Serial.print("Alice: ");
  for(int i = 0; i < 6; i++) {
    Serial.printf("%02X", aliceMAC[i]);
    if(i < 5) Serial.print(":");
  }
  Serial.println();
  Serial.print("Bob:   ");
  for(int i = 0; i < 6; i++) {
    Serial.printf("%02X", bobMAC[i]);
    if(i < 5) Serial.print(":");
  }
  Serial.println("\n========================\n");
  
  Serial.println("Configuración inicial completa.");
  
  // Enviar ping inicial a Alice y Bob para detectar conexión
  Serial.println("\n=== Enviando PING inicial ===");
  delay(2000);  // Esperar a que Alice/Bob terminen su setup
  
  Serial.println("Enviando PING a Alice...");
  sendCommandToAlice(CMD_PING, 0);
  
  Serial.println("Enviando PING a Bob...");
  sendCommandToBob(CMD_PING, 0);
  
  Serial.println("Esperando respuestas (2 segundos)...");
  unsigned long pingStart = millis();
  while(millis() - pingStart < 2000) {
    delay(50);  // Dar tiempo a que lleguen las respuestas
  }
  
  Serial.println("\n=== Estado de conexión ===");
  Serial.printf("Alice: %s\n", aliceConnected ? "✓ CONECTADA" : "✗ NO DETECTADA");
  Serial.printf("Bob:   %s\n", bobConnected ? "✓ CONECTADO" : "✗ NO DETECTADO");
  Serial.println("========================\n");
}


void loop() {
  server.handleClient();
  webSocket.loop();

  if (!start_protocol) return; // Esperar a que se inicie el protocolo
  
  // Verificar mensajes de la FPGA
  checkUARTFPGAMessages();

  // Verificar si se recibió el mensaje de FIFO vacía
  if (empty_id_received) {
    sendDataToWeb();
    currentPulseNum++;
    prepareNextPulse();  // Enviar comandos a Alice y Bob via ESP-NOW
    waitForMotorsReady();  // Esperar a que ambos motores estén listos
    generateNextPulseReady();
    empty_id_received = false;
  }

  // Verificar si se recibió el mensaje de finalización de transmisión
  if (tx_ended_received) {
    sendDataToWeb();
    abortarProtocolo();
    tx_ended_received = false;
    start_protocol = false;
  }
}

// ==============================================
// Funciones ESP-NOW
// ==============================================

void onESPNowSend(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Callback de envío (opcional para depuración)
}

void onESPNowReceive(const uint8_t *mac_addr, const uint8_t *data, int len) {
  // Debug: mostrar MAC recibida
  Serial.print("[ESP-NOW RX] MAC recibida: ");
  for(int i = 0; i < 6; i++) {
    Serial.printf("%02X", mac_addr[i]);
    if(i < 5) Serial.print(":");
  }
  Serial.printf(" | Tamaño: %d bytes\n", len);
  
  if(len != sizeof(ResponseData)) {
    Serial.printf("[WARN] Tamaño incorrecto. Esperado: %d, Recibido: %d\n", sizeof(ResponseData), len);
    return;
  }
  
  ResponseData response;
  memcpy(&response, data, sizeof(response));
  
  // Determinar si es de Alice o Bob comparando MAC
  bool isAlice = (memcmp(mac_addr, aliceMAC, 6) == 0);
  bool isBob = (memcmp(mac_addr, bobMAC, 6) == 0);
  const char* source = isAlice ? "Alice" : (isBob ? "Bob" : "DESCONOCIDO");
  
  Serial.printf("[DEBUG] Comparación MAC: isAlice=%d, isBob=%d\n", isAlice, isBob);
  
  // Actualizar flags de conexión y encender LEDs
  if(isAlice && !aliceConnected) {
    aliceConnected = true;
    digitalWrite(LED_ALICE_PIN, HIGH);
    Serial.println("✓✓✓ [LED ROJO ON] Alice conectada ✓✓✓");
    Serial.printf("Pin %d = HIGH\n", LED_ALICE_PIN);
  } else if(isBob && !bobConnected) {
    bobConnected = true;
    digitalWrite(LED_BOB_PIN, HIGH);
    Serial.println("✓✓✓ [LED AZUL ON] Bob conectado ✓✓✓");
    Serial.printf("Pin %d = HIGH\n", LED_BOB_PIN);
  }
  
  Serial.printf("[ESP-NOW] Respuesta de %s: status=%d, pulso=%d\n", 
                source, response.status, response.pulseNum);
  
  switch(response.status) {
    case STATUS_PONG:
      // Respuesta al ping - no hacer nada más que confirmar conexión
      Serial.printf("[%s] PONG recibido - Conexión verificada\n", source);
      break;
    case STATUS_HOME_COMPLETE:
      if(isAlice) {
        aliceHomed = true;
        Serial.println("[Alice] Homing completado");
      } else {
        bobHomed = true;
        Serial.println("[Bob] Homing completado");
      }
      break;
      
    case STATUS_READY:
      if(isAlice) {
        aliceReady = true;
        baseAlice = response.base;
        bitAlice = response.bit;
        angleAlice = response.angle;
        Serial.printf("[Alice] READY - Base:%d Bit:%d Ángulo:%.2f\n", 
                      baseAlice, bitAlice, angleAlice);
      } else {
        bobReady = true;
        baseBob = response.base;
        angleBob = response.angle;
        Serial.printf("[Bob] READY - Base:%d Ángulo:%.2f\n", 
                      baseBob, angleBob);
      }
      break;
      
    case STATUS_ERROR:
      Serial.printf("[ERROR] %s reportó un error en pulso %d\n", source, response.pulseNum);
      break;
  }
}

void sendCommandToAlice(uint8_t cmd, uint32_t pulseNum) {
  CommandData command;
  command.cmd = cmd;
  command.pulseNum = pulseNum;
  command.totalPulses = totalPulses;
  
  esp_err_t result = esp_now_send(aliceMAC, (uint8_t*)&command, sizeof(command));
  
  if(result == ESP_OK) {
    Serial.printf("[→ Alice] Comando %d enviado (pulso %d)\n", cmd, pulseNum);
  } else {
    Serial.printf("[ERR] Error enviando a Alice: %d\n", result);
  }
}

void sendCommandToBob(uint8_t cmd, uint32_t pulseNum) {
  CommandData command;
  command.cmd = cmd;
  command.pulseNum = pulseNum;
  command.totalPulses = totalPulses;
  
  esp_err_t result = esp_now_send(bobMAC, (uint8_t*)&command, sizeof(command));
  
  if(result == ESP_OK) {
    Serial.printf("[→ Bob] Comando %d enviado (pulso %d)\n", cmd, pulseNum);
  } else {
    Serial.printf("[ERR] Error enviando a Bob: %d\n", result);
  }
}

void prepareNextPulse() {
  aliceReady = false;
  bobReady = false;
  sendCommandToAlice(CMD_PREPARE_PULSE, currentPulseNum);
  sendCommandToBob(CMD_PREPARE_PULSE, currentPulseNum);
}

// ==============================================
// Funciones obsoletas (mantener compatibilidad)
// ==============================================

void prepareAlice() {
  // Obsoleta: Se usa prepareNextPulse() que llama a sendCommandToAlice()
  sendCommandToAlice(CMD_PREPARE_PULSE, currentPulseNum);
}

void prepareBob() {
  // Obsoleta: Se usa prepareNextPulse() que llama a sendCommandToBob()
  sendCommandToBob(CMD_PREPARE_PULSE, currentPulseNum);
}

void checkUARTFPGAMessages() {
  static uint32_t lastPrintTime = 0;
  static uint32_t messageCount = 0;

  while (UARTFPGA.available() > 0) {
    uint8_t incomingByte = UARTFPGA.read();
    messageCount++;

    switch (incomingByte) {
      case FIFO_0_ID:
        detector0_count++;
        break;

      case FIFO_1_ID:
        detector1_count++;
        break;

      case EMPTY_ID:
        empty_id_received = true;
        Serial.println("[FPGA] EMPTY_ID received - FIFOs empty");
        break;

      case TX_ENDED_ID:
        tx_ended_received = true;
        Serial.println("[FPGA] TX_ENDED_ID received - Protocol finished");
        while (UARTFPGA.available() > 0) {
          uint8_t remainingByte = UARTFPGA.read();
        }
        break;
    }
  }

  // Print status summary every second (avoid flooding serial)
  if (messageCount > 0 && millis() - lastPrintTime > 1000) {
    Serial.printf("[FPGA] Communication status: D0=%d, D1=%d, Messages=%d\n", 
                  detector0_count, detector1_count, messageCount);
    messageCount = 0;
    lastPrintTime = millis();
  }
}

void resetCounters() {
    detector0_count = 0;
    detector1_count = 0;
}

void sendDataToWeb() {
    // Calcular bit recibido basado en los conteos de detectores
    int bitRecibido;
    if (detector0_count > detector1_count) {
        bitRecibido = 0;
    } else if (detector0_count < detector1_count) {
        bitRecibido = 1;
    } else {
        bitRecibido = random() % 2; // Empate en conteos
    }

    StaticJsonDocument<200> jsonDoc;
    JsonObject conteos = jsonDoc.createNestedObject("conteos");
    conteos["detector0"] = detector0_count;
    conteos["detector1"] = detector1_count;
    jsonDoc["baseAlice"] = baseAlice;
    jsonDoc["bitEnviado"] = bitAlice;
    jsonDoc["baseBob"] = baseBob;
    jsonDoc["bitRecibido"] = bitRecibido;

    String jsonString;
    serializeJson(jsonDoc, jsonString);
    webSocket.broadcastTXT(jsonString);

    resetCounters();
    Serial.println("Conteos enviados y contadores reiniciados.");
}

void enviarConfiguracion(uint32_t num_pulsos, uint32_t duracion_us) {
  uint32_t dead_time_us = 0x000FFF;  // Dead time por defecto
  if (!start_protocol) {
      Serial.println("\n=== Iniciando configuración a FPGA ===");
      Serial.printf("Número de pulsos: %u\n", num_pulsos);
      Serial.printf("Duración (us): %u\n", duracion_us);
      Serial.printf("Dead time (us): %u\n", dead_time_us);
      
      // Enviar START_BYTE
      UARTFPGA.write(START_BYTE);

      // Enviar num_pulsos (3 bytes)
      UARTFPGA.write((num_pulsos >> 16) & 0xFF);
      UARTFPGA.write((num_pulsos >> 8) & 0xFF);
      UARTFPGA.write(num_pulsos & 0xFF);

      // Enviar duracion_us (3 bytes)
      UARTFPGA.write((duracion_us >> 16) & 0xFF);
      UARTFPGA.write((duracion_us >> 8) & 0xFF);
      UARTFPGA.write(duracion_us & 0xFF);

      // Enviar dead_time_us (3 bytes)
      UARTFPGA.write((dead_time_us >> 16) & 0xFF);
      UARTFPGA.write((dead_time_us >> 8) & 0xFF);
      UARTFPGA.write(dead_time_us & 0xFF);

      Serial.println("=== Configuración enviada completamente ===\n");

      // Enviar comando de homing a ambos motores
      sendHomingCommand();
      
      // Esperar a que ambos completen el homing (flags set by onESPNowReceive)
      Serial.println("Esperando homing de motores...");
      unsigned long homingTimeout = millis();
      while ((!aliceHomed || !bobHomed) && millis() - homingTimeout < 30000) {
        delay(100);  // ESP-NOW callbacks handle responses asynchronously
      }
      
      if (aliceHomed && bobHomed) {
        Serial.println("Homing completado en ambos motores");
        
        // Apagar LEDs al iniciar protocolo (indicadores de conexión ya no necesarios)
        digitalWrite(LED_ALICE_PIN, LOW);
        digitalWrite(LED_BOB_PIN, LOW);
        Serial.println("[LEDs OFF] Protocolo iniciado - Indicadores de conexión apagados");
        
        currentPulseNum = 0;
        prepareAlice();
        prepareBob();
        waitForMotorsReady();
        resetCounters();
        generateNextPulseReady();
        start_protocol = true;
      } else {
        Serial.println("ERROR: Timeout en homing de motores");
        if (!aliceHomed) Serial.println("  - Alice no completó homing");
        if (!bobHomed) Serial.println("  - Bob no completó homing");
      }
      
  } else {
      Serial.println("Protocolo ya iniciado. Bloqueando reenvío de configuración.");
  }
}

void handleWebSocketMessage(uint8_t num, uint8_t* payload, size_t length) {
    String message = String((char*)payload).substring(0, length);
    Serial.println("Mensaje recibido: " + message);

    // Comandos de control de motores ahora se reenvían a los Super Minis
    if (message == "HOMING_ALL") {
        sendHomingCommand();
        webSocket.sendTXT(num, "Comando de homing enviado a Alice y Bob");
        return;
    }
    
    if (message == "HOMING1") {
        sendCommandToAlice(CMD_HOME, 0);
        webSocket.sendTXT(num, "Comando de homing enviado a Alice");
        return;
    }
    
    if (message == "HOMING2") {
        sendCommandToBob(CMD_HOME, 0);
        webSocket.sendTXT(num, "Comando de homing enviado a Bob");
        return;
    }

    // Movimiento manual de motores - no soportado con ESP-NOW (CommandData no incluye ángulo)
    // Las estructuras ESP-NOW solo soportan comandos predefinidos
    if (message.startsWith("MOVE1:") || message.startsWith("MOVE2:")) {
        webSocket.sendTXT(num, "Movimiento manual no disponible en modo ESP-NOW");
        return;
    }

    // Verificar si es un comando de abortar
    if (message == "abort") {
        abortarProtocolo();
        StaticJsonDocument<200> response;
        response["status"] = "ok";
        response["message"] = "Protocolo abortado.";
        String jsonResponse;
        serializeJson(response, jsonResponse);
        webSocket.sendTXT(num, jsonResponse);
        return;
    }

    // Parsear el mensaje JSON para la configuración del protocolo
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, message);

    if (!error) {
        uint32_t num_pulsos = doc["num_pulsos"];
        uint32_t duracion_us = doc["duracion_us"];
        // Eliminada la línea que extraía dead_time_us

        if (num_pulsos <= 16777215 && duracion_us <= 16777215) {
            enviarConfiguracion(num_pulsos, duracion_us);
            StaticJsonDocument<200> response;
            response["status"] = "ok";
            response["message"] = "Configuración enviada correctamente.";

            String jsonResponse;
            serializeJson(response, jsonResponse);
            webSocket.sendTXT(num, jsonResponse);

        } else {
            webSocket.sendTXT(num, "Error: Valores fuera de rango.");
        }
    } else {
        webSocket.sendTXT(num, "Error: Formato JSON inválido.");
    }
}

void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_TEXT:
            handleWebSocketMessage(num, payload, length);
            break;
        default:
            break;
    }
}

// Función para generar pulso de reset
void generateResetPulse() {
    digitalWrite(RESET_PIN, LOW);  // Activar reset
    delay(5);                    // Mantener reset por 5ms
    digitalWrite(RESET_PIN, HIGH); // Desactivar reset
    Serial.println("Pulso de reset enviado a la FPGA");
}

// Función que envía señal a la FPGA para generar el siguiente pulso
void generateNextPulseReady() {
  digitalWrite(NEXT_PULSE_PIN, LOW); 
  delay(5);                    
  digitalWrite(NEXT_PULSE_PIN, HIGH); 
  Serial.println("Next pulse ready enviado a la FPGA");
}

void abortarProtocolo() {
    if (start_protocol) {
        Serial.println("\n=== Abortando protocolo ===");
        resetCounters();
        generateResetPulse();
        
        // Enviar comando de abortar a los Super Minis via ESP-NOW
        sendCommandToAlice(CMD_ABORT, 0);
        sendCommandToBob(CMD_ABORT, 0);
        
        start_protocol = false;
        aliceReady = false;
        bobReady = false;
        aliceHomed = false;
        bobHomed = false;
        
        // Apagar LEDs al abortar protocolo
        aliceConnected = false;
        bobConnected = false;
        digitalWrite(LED_ALICE_PIN, LOW);
        digitalWrite(LED_BOB_PIN, LOW);
        Serial.println("[LEDs OFF] Conexiones reiniciadas");
        
        while (UARTFPGA.available() > 0) {
            UARTFPGA.read();
        }
        delay(1);
        Serial.println("=== Protocolo abortado ===\n");
    }
}

void sendHomingCommand() {
    Serial.println("Enviando comando HOME a Alice y Bob...");
    aliceHomed = false;
    bobHomed = false;
    
    sendCommandToAlice(CMD_HOME, 0);
    sendCommandToBob(CMD_HOME, 0);
}

void waitForMotorsReady() {
    Serial.println("Esperando a que ambos motores estén listos...");
    unsigned long timeout = millis();
    while ((!aliceReady || !bobReady) && millis() - timeout < 10000) {
        delay(10);  // ESP-NOW maneja mensajes automáticamente en background
    }
    
    if (aliceReady && bobReady) {
        Serial.println("✓ Ambos motores listos");
    } else {
        Serial.println("⚠ WARNING: Timeout esperando motores");
        if (!aliceReady) Serial.println("  - Alice no está lista");
        if (!bobReady) Serial.println("  - Bob no está listo");
    }
}

// Manejador de eventos WebSocket para Alice
// ==============================================
// FUNCIONES WEBSOCKET OBSOLETAS (ESP-NOW las reemplaza)
// ==============================================
// Estas funciones se mantienen solo para compatibilidad de compilación
// pero ya no se usan con ESP-NOW

void onAliceEvent(WStype_t type, uint8_t* payload, size_t length) {
  // Obsoleta - ESP-NOW maneja la comunicación
}

void onBobEvent(WStype_t type, uint8_t* payload, size_t length) {
  // Obsoleta - ESP-NOW maneja la comunicación
}

void handleAliceMessage(uint8_t* payload, size_t length) {
  // Obsoleta - onESPNowReceive() maneja los mensajes de Alice
}

void handleBobMessage(uint8_t* payload, size_t length) {
  // Obsoleta - onESPNowReceive() maneja los mensajes de Bob
}
