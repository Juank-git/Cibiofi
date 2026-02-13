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
const char* ssid = "Loic";
const char* password = "Loic1234";

// IP estática - se calculará automáticamente basándose en la red del router
IPAddress local_IP;
IPAddress gateway;
IPAddress subnet;

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

// Canal WiFi/ESP-NOW - Detectado automáticamente del router
const int ESP_NOW_INITIAL_CHANNEL = 1;  // Canal inicial para sincronización
int ESP_NOW_CHANNEL = ESP_NOW_INITIAL_CHANNEL;  // Se actualizará con el canal del router

uint8_t aliceMAC[] = {0x0C,0x4E,0xA0,0x65,0x48,0xCC};  // MAC de la Super Mini 1 (Alice)
uint8_t bobMAC[] = {0x0C,0x4E,0xA0,0x65,0x48,0x80};     // MAC de la Super Mini 2 (Bob)

// Estructuras de comunicación ESP-NOW
struct CommandData {
  uint8_t cmd;          // Tipo de comando
  uint32_t pulseNum;    // Número de pulso actual
  uint32_t totalPulses; // Total de pulsos a transmitir
} __attribute__((packed));

// Nueva estructura para comandos de movimiento manual
struct ManualMoveCommand {
  uint8_t cmd;          // CMD_MOVE_MANUAL
  float targetAngle;    // Ángulo objetivo
  uint32_t reserved;    // Reservado para mantener tamaño
} __attribute__((packed));

struct ResponseData {
  uint8_t status;       // Estado: READY, ERROR, HOME_COMPLETE
  uint32_t pulseNum;    // Número de pulso
  int base;             // Base usada (Alice: 0-1, Bob: 0-1)
  int bit;              // Bit enviado (solo Alice: 0-1)
  float angle;          // Ángulo alcanzado
} __attribute__((packed));

// Comandos
#define CMD_SET_CHANNEL 0x00   // Configurar canal WiFi (debe ser el primero)
#define CMD_PING 0x01          // Comando de ping para verificar conexión
#define CMD_HOME 0x02
#define CMD_PREPARE_PULSE 0x03
#define CMD_ABORT 0x04
#define CMD_START_PROTOCOL 0x05
#define CMD_MOVE_MANUAL 0x06   // Comando para movimiento manual

// Estados (DEBEN coincidir con Alice/Bob)
#define STATUS_PONG 0              // Respuesta al ping
#define STATUS_HOME_COMPLETE 1
#define STATUS_READY 2
#define STATUS_ERROR 3

// Flags de estado de los motores
bool aliceReady = false;
bool bobReady = false;
bool aliceHomed = false;
bool bobHomed = false;

// Flags de conexión (para LEDs)
bool aliceConnected = false;
bool bobConnected = false;

// Flags de sincronización de canal (para Fase 2)
bool aliceChannelConfigured = false;
bool bobChannelConfigured = false;

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
#define RESET_PIN 5
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
esp_err_t sendCommandToAlice(uint8_t cmd, uint32_t pulseNum = 0);
esp_err_t sendCommandToBob(uint8_t cmd, uint32_t pulseNum = 0);
void prepareNextPulse();

// Helper para servir archivos SPIFFS de forma optimizada
void serveFile(const char* path, const char* contentType, bool enableCache = true) {
  File file = SPIFFS.open(path, "r");
  if (!file) {
    server.send(404, "text/plain", "Archivo no encontrado");
    return;
  }
  
  // Obtener tamaño del archivo
  size_t fileSize = file.size();
  
  // Configurar cabeceras ANTES de enviar
  if (enableCache) {
    server.sendHeader("Cache-Control", "max-age=86400");
  }
  server.sendHeader("Connection", "close");
  
  // Usar setContentLength para garantizar tamaño correcto
  server.setContentLength(fileSize);
  server.send(200, contentType, "");
  
  // Enviar archivo en bloques de 1KB
  uint8_t buffer[1024];
  while (file.available()) {
    size_t bytesRead = file.read(buffer, sizeof(buffer));
    server.client().write(buffer, bytesRead);
    yield(); // Permitir que el watchdog se ejecute
  }
  
  file.close();
}

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
    delay(300);
    digitalWrite(LED_ALICE_PIN, LOW);
    delay(200);
    digitalWrite(LED_BOB_PIN, HIGH);
    delay(300);
    digitalWrite(LED_BOB_PIN, LOW);
    delay(200);
  }
  Serial.println("TEST LEDs completado");
  
  // ==============================================
  // Configuración Wi-Fi con IP automática terminada en .100
  // ==============================================
  Serial.println("Conectando a Wi-Fi para detectar red...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  // Conectar primero con DHCP para obtener configuración de red
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nError: No se pudo conectar al WiFi");
    return;
  }
  
  Serial.println("\nConectado temporalmente con DHCP");
  
  // Obtener configuración de red del router
  gateway = WiFi.gatewayIP();
  subnet = WiFi.subnetMask();
  
  // Construir IP local con los primeros 3 octetos del gateway y .100 al final
  local_IP = IPAddress(gateway[0], gateway[1], gateway[2], 100);
  
  Serial.printf("Red detectada: %s\n", gateway.toString().c_str());
  Serial.printf("Configurando IP estática: %s\n", local_IP.toString().c_str());
  
  // Desconectar y reconectar con IP estática
  WiFi.disconnect();
  delay(500);
  
  WiFi.mode(WIFI_AP_STA);  // Modo híbrido para ESP-NOW
  WiFi.config(local_IP, gateway, subnet);
  WiFi.begin(ssid, password);
  
  attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nError: No se pudo reconectar con IP estática");
    return;
  }
  
  Serial.println("\nWi-Fi conectado con IP estática.");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  
  // Obtener y mostrar información del canal WiFi
  uint8_t wifiChannel;
  wifi_second_chan_t secondChannel;
  esp_wifi_get_channel(&wifiChannel, &secondChannel);
  Serial.printf("Canal WiFi del router detectado: %d\n", wifiChannel);
  Serial.printf("Canal secundario: %d\n", secondChannel);
  
  // Actualizar ESP_NOW_CHANNEL con el canal del router
  ESP_NOW_CHANNEL = wifiChannel;
  Serial.printf("ESP-NOW usará canal: %d\n", ESP_NOW_CHANNEL);
  
  // IMPORTANTE: Desconectar WiFi temporalmente para sincronización con Alice/Bob
  Serial.println("\n⚠ Desconectando WiFi temporalmente para sincronizar dispositivos...");
  WiFi.disconnect();
  delay(500);

  // Configuración de archivos SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("Error montando SPIFFS");
    return;
  }

  // ==============================================
  // Configuración del servidor web y web socket
  // ==============================================
  // Usar función helper optimizada para servir archivos
  server.on("/", HTTP_GET, []() {
    serveFile("/index.html", "text/html", false); // No cache para HTML
  });
  
  server.on("/styles.css", HTTP_GET, []() {
    serveFile("/styles.css", "text/css");
  });

  server.on("/script.js", HTTP_GET, []() {
    serveFile("/script.js", "application/javascript");
  });

  server.on("/scriptPost.js", HTTP_GET, []() {
    serveFile("/scriptPost.js", "application/javascript");
  });

  server.on("/scriptCascade.js", HTTP_GET, []() {
    serveFile("/scriptCascade.js", "application/javascript");
  });

  server.on("/favicon.ico", HTTP_GET, []() {
    serveFile("/favicon.ico", "image/x-icon");
  });
  
  // Configuración del servidor para mejorar estabilidad
  server.enableCORS(true);  // Habilitar CORS si es necesario
  
  server.begin();
  Serial.println("Servidor HTTP iniciado");
  
  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);
  Serial.println("WebSocket iniciado en puerto 81");

  // ==============================================
  // Configurar ESP-NOW para Alice y Bob
  // ==============================================
  Serial.println("\n=== Configurando ESP-NOW ===");
  
  esp_wifi_set_ps(WIFI_PS_NONE);
  
  // FASE 1: Cambiar a canal 1 para sincronizar con Alice/Bob
  Serial.printf("Fase 1: Cambiando a canal %d (canal de sincronización)\n", ESP_NOW_INITIAL_CHANNEL);
  esp_wifi_set_channel(ESP_NOW_INITIAL_CHANNEL, WIFI_SECOND_CHAN_NONE);
  
  if (esp_now_init() != ESP_OK) {
    Serial.println("[ERR] ESP-NOW init");
    return;
  }
  
  // Registrar callbacks
  esp_now_register_send_cb(onESPNowSend);
  esp_now_register_recv_cb(onESPNowReceive);
  
  // Agregar peers en canal 1
  esp_now_peer_info_t peerAlice = {};
  memcpy(peerAlice.peer_addr, aliceMAC, 6);
  peerAlice.channel = ESP_NOW_INITIAL_CHANNEL;
  peerAlice.encrypt = false;
  
  esp_now_peer_info_t peerBob = {};
  memcpy(peerBob.peer_addr, bobMAC, 6);
  peerBob.channel = ESP_NOW_INITIAL_CHANNEL;
  peerBob.encrypt = false;
  
  if (esp_now_add_peer(&peerAlice) != ESP_OK || esp_now_add_peer(&peerBob) != ESP_OK) {
    Serial.println("[ERR] Agregar peers");
    return;
  }
  
  Serial.println("[OK] ESP-NOW iniciado en canal 1");
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
  Serial.println();
  
  // FASE 2: Enviar configuración de canal a Alice y Bob (con confirmación)
  Serial.printf("\nFase 2: Enviando configuración de canal %d a Alice y Bob...\n", ESP_NOW_CHANNEL);
  delay(1500);  // Esperar que Alice/Bob estén listos
  
  // Reset de flags de confirmación
  aliceChannelConfigured = false;
  bobChannelConfigured = false;
  
  // Enviar con reintentos inteligentes (máximo 4 intentos)
  int aliceAttempts = 0, bobAttempts = 0;
  for(int i = 0; i < 4 && (!aliceChannelConfigured || !bobChannelConfigured); i++) {
    // Enviar solo a quien aún no ha confirmado
    if(!aliceChannelConfigured) {
      if(sendCommandToAlice(CMD_SET_CHANNEL, (uint32_t)ESP_NOW_CHANNEL) == ESP_OK) {
        aliceAttempts++;
        Serial.printf("  → Alice intento %d\n", i+1);
      }
    }
    
    if(!bobChannelConfigured) {
      if(sendCommandToBob(CMD_SET_CHANNEL, (uint32_t)ESP_NOW_CHANNEL) == ESP_OK) {
        bobAttempts++;
        Serial.printf("  → Bob intento %d\n", i+1);
      }
    }
    
    delay(300);  // Esperar respuesta
  }
  
  Serial.printf("\nResultado: Alice=%s (%d envíos), Bob=%s (%d envíos)\n", 
                aliceChannelConfigured ? "✓" : "✗", aliceAttempts,
                bobChannelConfigured ? "✓" : "✗", bobAttempts);
  
  if(!aliceChannelConfigured || !bobChannelConfigured) {
    Serial.println("⚠ Advertencia: Algunos dispositivos no confirmaron. Continuando...");
  }
  
  delay(1000);  // Tiempo adicional para cambio de canal
  
  // FASE 3: Cambiar Central al canal del router
  Serial.printf("\nFase 3: Cambiando Central al canal %d del router\n", ESP_NOW_CHANNEL);
  
  // Eliminar peers actuales
  esp_now_del_peer(aliceMAC);
  esp_now_del_peer(bobMAC);
  
  // Cambiar al canal del router
  esp_wifi_set_channel(ESP_NOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  
  // Volver a agregar peers en el nuevo canal
  peerAlice.channel = ESP_NOW_CHANNEL;
  peerBob.channel = ESP_NOW_CHANNEL;
  
  if (esp_now_add_peer(&peerAlice) != ESP_OK || esp_now_add_peer(&peerBob) != ESP_OK) {
    Serial.println("[ERR] Agregar peers en canal definitivo");
    return;
  }
  
  Serial.printf("[OK] ESP-NOW configurado en canal %d\n", ESP_NOW_CHANNEL);
  
  // FASE 4: Reconectar WiFi
  Serial.println("\nFase 4: Reconectando WiFi...");
  WiFi.begin(ssid, password);
  int reconnectAttempts = 0;
  while (WiFi.status() != WL_CONNECTED && reconnectAttempts < 20) {
    delay(500);
    Serial.print(".");
    reconnectAttempts++;
  }
  
  if(WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✓ WiFi reconectado");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n✗ Error reconectando WiFi");
  }
  
  // Verificar tamaños de estructuras
  Serial.printf("\n[Central] sizeof(CommandData): %d bytes\n", sizeof(CommandData));
  Serial.printf("[Central] sizeof(ResponseData): %d bytes\n", sizeof(ResponseData));
  Serial.printf("[Central] sizeof(ManualMoveCommand): %d bytes\n\n", sizeof(ManualMoveCommand));
  
  
  // Enviar ping inicial a Alice y Bob
  Serial.println("\n=== Detectando dispositivos ===");
  delay(1000);  // Esperar configuración de Alice/Bob
  
  // Enviar PING con más reintentos y mayor espaciado
  for(int i = 0; i < 5 && !aliceConnected; i++) {
    Serial.printf("Ping Alice intento %d/5...\n", i+1);
    sendCommandToAlice(CMD_PING, 0);
    delay(500);
  }
  
  for(int i = 0; i < 5 && !bobConnected; i++) {
    Serial.printf("Ping Bob intento %d/5...\n", i+1);
    sendCommandToBob(CMD_PING, 0);
    delay(500);
  }
  
  // Esperar respuestas con timeout más largo
  Serial.println("Esperando respuestas...");
  unsigned long pingStart = millis();
  while(millis() - pingStart < 5000 && (!aliceConnected || !bobConnected)) {
    delay(10);
  }
  
  Serial.println("\n=== Estado ===");
  Serial.printf("Alice: %s\n", aliceConnected ? "✓" : "✗");
  Serial.printf("Bob:   %s\n", bobConnected ? "✓" : "✗");
  Serial.println("==============\n");
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
    Serial.printf("\n[PROTOCOLO] Finalizado en pulso %d de %d\n", currentPulseNum, totalPulses);
    Serial.println("[FPGA] TX_ENDED_ID recibido - Protocolo completado correctamente");
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
  // Optimizado: Callback vacío para máxima velocidad (errores visibles en timeout)
  // Si se necesita debug, descomentar línea siguiente:
  // if (status != ESP_NOW_SEND_SUCCESS) Serial.printf("[TX ERR] %d\n", status);
}

void onESPNowReceive(const uint8_t *mac_addr, const uint8_t *data, int len) {
  // Optimizado: Eliminado Serial.printf para reducir latencia en callback crítico
  
  if(len != sizeof(ResponseData)) {
    return;  // Error silencioso - callback debe ser rápido
  }
  
  ResponseData response;
  memcpy(&response, data, sizeof(response));
  
  // Determinar origen comparando MAC
  bool isAlice = (memcmp(mac_addr, aliceMAC, 6) == 0);
  bool isBob = (memcmp(mac_addr, bobMAC, 6) == 0);
  
  // Actualizar LEDs de conexión (solo primera vez)
  if(isAlice && !aliceConnected) {
    aliceConnected = true;
    digitalWrite(LED_ALICE_PIN, HIGH);
    Serial.println("[✓] Alice conectada");
  } else if(isBob && !bobConnected) {
    bobConnected = true;
    digitalWrite(LED_BOB_PIN, HIGH);
    Serial.println("[✓] Bob conectado");
  }
  
  // Procesar respuesta según estado
  switch(response.status) {
    case STATUS_PONG:
      // Verificar si es confirmación de cambio de canal (pulseNum contiene el canal)
      if(response.pulseNum == ESP_NOW_CHANNEL) {
        if(isAlice && !aliceChannelConfigured) {
          aliceChannelConfigured = true;
          Serial.println("[✓] Alice confirmó canal " + String(ESP_NOW_CHANNEL));
        } else if(isBob && !bobChannelConfigured) {
          bobChannelConfigured = true;
          Serial.println("[✓] Bob confirmó canal " + String(ESP_NOW_CHANNEL));
        }
      }
      break;
      
    case STATUS_HOME_COMPLETE:
      if(isAlice) {
        aliceHomed = true;
        Serial.println("[Alice] HOME OK");
        Serial.printf("[DEBUG] aliceHomed=%d, bobHomed=%d\n", aliceHomed, bobHomed);
      } else {
        bobHomed = true;
        Serial.println("[Bob] HOME OK");
        Serial.printf("[DEBUG] aliceHomed=%d, bobHomed=%d\n", aliceHomed, bobHomed);
      }
      break;
      
    case STATUS_READY:
      if(isAlice) {
        aliceReady = true;
        baseAlice = response.base;
        bitAlice = response.bit;
        angleAlice = response.angle;
        // OPTIMIZADO: Solo loguear si el protocolo no está activo
        if (!start_protocol) {
          Serial.printf("[Alice] READY #%d B:%d b:%d A:%.1f\n", 
                        response.pulseNum, baseAlice, bitAlice, angleAlice);
        }
      } else {
        bobReady = true;
        baseBob = response.base;
        angleBob = response.angle;
        // OPTIMIZADO: Solo loguear si el protocolo no está activo
        if (!start_protocol) {
          Serial.printf("[Bob] READY #%d B:%d A:%.1f\n", 
                        response.pulseNum, baseBob, angleBob);
        }
      }
      break;
      
    case STATUS_ERROR:
      Serial.printf("[ERROR] %s - Pulso %d\n", isAlice ? "Alice" : "Bob", response.pulseNum);
      break;
  }
}

esp_err_t sendCommandToAlice(uint8_t cmd, uint32_t pulseNum) {
  CommandData command = {cmd, pulseNum, totalPulses};
  esp_err_t result = esp_now_send(aliceMAC, (uint8_t*)&command, sizeof(command));
  return result;
}

esp_err_t sendCommandToBob(uint8_t cmd, uint32_t pulseNum) {
  CommandData command = {cmd, pulseNum, totalPulses};
  esp_err_t result = esp_now_send(bobMAC, (uint8_t*)&command, sizeof(command));
  return result;
}

// Funciones para movimiento manual
void sendManualMoveToAlice(float angle) {
  ManualMoveCommand command = {CMD_MOVE_MANUAL, angle, 0};
  esp_err_t result = esp_now_send(aliceMAC, (uint8_t*)&command, sizeof(command));
  
  if(result == ESP_OK) {
    Serial.printf("[Manual] Alice -> %.2f°\n", angle);
  } else {
    Serial.printf("[Alice Manual TX ERR] %d\n", result);
  }
}

void sendManualMoveToBob(float angle) {
  ManualMoveCommand command = {CMD_MOVE_MANUAL, angle, 0};
  esp_err_t result = esp_now_send(bobMAC, (uint8_t*)&command, sizeof(command));
  
  if(result == ESP_OK) {
    Serial.printf("[Manual] Bob -> %.2f°\n", angle);
  } else {
    Serial.printf("[Bob Manual TX ERR] %d\n", result);
  }
}

void prepareNextPulse() {
  aliceReady = false;
  bobReady = false;
  sendCommandToAlice(CMD_PREPARE_PULSE, currentPulseNum);
  sendCommandToBob(CMD_PREPARE_PULSE, currentPulseNum);
  yield();  // OPTIMIZADO: Permitir procesamiento inmediato de respuestas ESP-NOW
}

// ==============================================
// Funciones obsoletas (ELIMINADAS - usar prepareNextPulse())
// ==============================================
// prepareAlice() y prepareBob() fueron reemplazadas por prepareNextPulse()
// que es más eficiente al resetear flags y añadir yield() automáticamente

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
      // Guardar el número total de pulsos configurados
      totalPulses = num_pulsos;
      
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

      // Enviar comando de homing a ambos motores (reinicia flags internamente)
      sendHomingCommand();
      
      // Esperar a que ambos completen el homing (flags set by onESPNowReceive)
      Serial.println("Esperando homing de motores...");
      Serial.printf("[DEBUG ANTES] aliceHomed=%d, bobHomed=%d\n", aliceHomed, bobHomed);
      
      unsigned long homingTimeout = millis();
      while ((!aliceHomed || !bobHomed) && millis() - homingTimeout < 30000) {
        // Procesar eventos del servidor y websocket para que los callbacks funcionen
        server.handleClient();
        webSocket.loop();
        yield();  // Permitir que se ejecuten los callbacks ESP-NOW
        delay(10);  // Pequeña pausa para no saturar el CPU
        
        // Debug cada segundo
        static unsigned long lastDebug = 0;
        if (millis() - lastDebug > 1000) {
          Serial.printf("[DEBUG WAIT] aliceHomed=%d, bobHomed=%d (%.1fs)\n", 
                        aliceHomed, bobHomed, (millis() - homingTimeout) / 1000.0);
          lastDebug = millis();
        }
      }
      
      if (aliceHomed && bobHomed) {
        Serial.println("Homing completado en ambos motores");
        
        // Apagar LEDs al iniciar protocolo (indicadores de conexión ya no necesarios)
        digitalWrite(LED_ALICE_PIN, LOW);
        digitalWrite(LED_BOB_PIN, LOW);
        Serial.println("[LEDs OFF] Protocolo iniciado - Indicadores de conexión apagados");
        
        currentPulseNum = 0;
        prepareNextPulse();  // OPTIMIZADO: Reemplaza prepareAlice()+prepareBob()
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

    // Parsear el mensaje JSON
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, message);

    if (!error) {
        // Verificar si es comando de movimiento manual
        if (doc.containsKey("type")) {
            String type = doc["type"].as<String>();
            
            if (type == "MOVE_ALICE") {
                float angle = doc["angle"];
                sendManualMoveToAlice(angle);
                webSocket.sendTXT(num, "Moviendo Alice a " + String(angle) + "°");
                return;
            }
            else if (type == "MOVE_BOB") {
                float angle = doc["angle"];
                sendManualMoveToBob(angle);
                webSocket.sendTXT(num, "Moviendo Bob a " + String(angle) + "°");
                return;
            }
        }
        
        // Si no es comando manual, es configuración del protocolo
        uint32_t num_pulsos = doc["num_pulsos"];
        uint32_t duracion_us = doc["duracion_us"];

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
    if (!start_protocol) return;
    
    Serial.println("\n[ABORT] Deteniendo protocolo...");
    resetCounters();
    generateResetPulse();
    
    sendCommandToAlice(CMD_ABORT, 0);
    sendCommandToBob(CMD_ABORT, 0);
    
    start_protocol = false;
    aliceReady = false;
    bobReady = false;
    aliceHomed = false;
    bobHomed = false;
    aliceConnected = false;
    bobConnected = false;
    aliceChannelConfigured = false;
    bobChannelConfigured = false;
    
    digitalWrite(LED_ALICE_PIN, LOW);
    digitalWrite(LED_BOB_PIN, LOW);
    
    while (UARTFPGA.available() > 0) {
        UARTFPGA.read();
    }
    Serial.println("[OK] Protocolo abortado\n");
}

void sendHomingCommand() {
    Serial.println("[HOMING] Iniciando...");
    aliceHomed = false;
    bobHomed = false;
    sendCommandToAlice(CMD_HOME, 0);
    sendCommandToBob(CMD_HOME, 0);
}

void waitForMotorsReady() {
    unsigned long timeout = millis();
    // OPTIMIZADO: Timeout reducido de 10s a 3s (los motores deberían responder en <1s)
    while ((!aliceReady || !bobReady) && millis() - timeout < 3000) {
        // OPTIMIZADO: Solo yield() sin delay - reduce latencia de ~3ms a ~0.01ms/ciclo
        server.handleClient();  // Mantener WebSocket activo
        webSocket.loop();
        yield();  // Permitir callbacks ESP-NOW
    }
    
    if (aliceReady && bobReady) {
        // Motores listos (silencioso para no saturar serial)
    } else {
        Serial.printf("\n[ERROR CRÍTICO] Timeout esperando motores en pulso %d\n", currentPulseNum);
        if (!aliceReady) Serial.println("  Alice no respondió");
        if (!bobReady) Serial.println("  Bob no respondió");
        Serial.println("[ABORT] Deteniendo protocolo por timeout\n");
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
