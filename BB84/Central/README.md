# Central - Coordinador del Protocolo BB84

## Descripción

El módulo Central es el coordinador principal del sistema BB84. Sincroniza la comunicación entre Alice y Bob mediante ESP-NOW, proporciona una interfaz web para configuración y monitoreo, y gestiona la interacción con la FPGA para el control de transmisión y detección de fotones.

## Hardware

- **Microcontrolador:** ESP32 DevKit (esp32dev)
- **Conexión FPGA:** UART (pines 16 RX, 17 TX)
- **LEDs de estado:** 
  - Pin 23: LED Alice (rojo)
  - Pin 22: LED Bob (azul)
- **Pines de control FPGA:**
  - Pin 5: Reset
  - Pin 4: Next Pulse

## Configuración Inicial

### 1. Configurar Red WiFi

Editar credenciales WiFi en [src/main.cpp](src/main.cpp):

```cpp
const char* ssid = "Loic";           // Nombre de la red WiFi
const char* password = "Loic1234";    // Contraseña de la red
```

### 2. Configurar Direcciones MAC

Obtener las direcciones MAC de Alice y Bob usando el [proyecto MAC](../../MAC/README.md), y actualizarlas en [src/main.cpp](src/main.cpp):

```cpp
uint8_t aliceMAC[] = {0x0C,0x4E,0xA0,0x65,0x48,0xCC};  // MAC de Alice
uint8_t bobMAC[] = {0x0C,0x4E,0xA0,0x65,0x48,0x3C};     // MAC de Bob
```

### 3. Cargar el Código

Navegar a la carpeta del proyecto Central:

```powershell
cd C:\ruta\al\repositorio\Cibiofi\BB84\BB84\Central
```

Ejecutar el comando de carga:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -t upload -t monitor -e esp32dev
```

## Funcionamiento

### Secuencia de Inicio

1. **Conexión WiFi**: Se conecta al router y detecta automáticamente el canal WiFi
2. **Sincronización ESP-NOW**: 
   - Se desconecta temporalmente del WiFi
   - Cambia a canal 1 para sincronizar con Alice y Bob
   - Envía configuración de canal a Alice y Bob
   - Cambia al canal del router
   - Reconecta al WiFi
3. **Verificación de dispositivos**: Envía ping a Alice y Bob
4. **Servidor Web**: Inicia en `http://192.168.137.100`
5. **WebSocket**: Escucha en puerto 81 para control en tiempo real

### Interfaz Web

Acceder desde un navegador en la misma red:

```
http://192.168.137.100
```

Funciones disponibles:
- **Homing**: Calibrar posiciones de Alice y Bob
- **Configurar protocolo**: Número de pulsos y duración
- **Iniciar transmisión**: Ejecutar protocolo BB84
- **Monitoreo en vivo**: Ver resultados en tiempo real
- **Abortar**: Detener protocolo en ejecución

## Protocolo de Comunicación

### Comandos ESP-NOW enviados a Alice/Bob

| Comando | Valor | Descripción |
|---------|-------|-------------|
| `CMD_SET_CHANNEL` | 0x00 | Configurar canal WiFi |
| `CMD_PING` | 0x01 | Verificar conexión |
| `CMD_HOME` | 0x02 | Iniciar homing |
| `CMD_PREPARE_PULSE` | 0x03 | Preparar siguiente pulso |
| `CMD_ABORT` | 0x04 | Abortar operación |

### Respuestas recibidas de Alice/Bob

| Estado | Valor | Descripción |
|--------|-------|-------------|
| `STATUS_PONG` | 0 | Respuesta a ping |
| `STATUS_HOME_COMPLETE` | 1 | Homing completado |
| `STATUS_READY` | 2 | Listo para transmitir |
| `STATUS_ERROR` | 3 | Error detectado |

## Comunicación con FPGA

### Protocolo UART (115200 baud)

**Mensajes enviados al Central:**
- `0xF0`: Detector 0 activado
- `0xF1`: Detector 1 activado
- `0xFE`: FIFO vacía (lista para siguiente pulso)
- `0xFD`: Transmisión completada

**Comandos enviados a FPGA:**
```
[START_BYTE][N_pulsos_H][N_pulsos_L][Duración_H][Duración_L][Dead_time_H][Dead_time_L]
```

## Solución de Problemas

### Alice o Bob no se conectan

1. Verificar que las direcciones MAC sean correctas
2. Asegurar que Alice y Bob estén encendidos
3. Revisar que los tres dispositivos tengan alimentación estable
4. Verificar LEDs de estado (Alice: pin 23, Bob: pin 22)

### No se puede acceder a la interfaz web

1. Verificar que el computador esté en la misma red WiFi
2. Probar con IP directa: `http://192.168.137.100`
3. Revisar la configuración de IP estática en el código

### Errores de sincronización de canal

1. Asegurar que Alice y Bob se enciendan ANTES que Central
2. Verificar mensajes de sincronización en el monitor serial
3. Reiniciar los tres dispositivos si es necesario

## Configuración Técnica

- **Plataforma:** ESP32 (Espressif32)
- **Board:** esp32dev
- **Framework:** Arduino
- **Comunicación:**
  - WiFi: 2.4 GHz
  - ESP-NOW: Comunicación peer-to-peer
  - UART FPGA: 115200 baud

## Documentación Relacionada

- [README Principal BB84](../README.md) - Visión general del sistema
- [Alice](../Alice/README.md) - Configuración del emisor
- [Bob](../Bob/README.md) - Configuración del receptor
- [MAC](../../MAC/README.md) - Obtener direcciones MAC
