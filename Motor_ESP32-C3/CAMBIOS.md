# Resumen de Cambios - Motor Caracterizador Web

## 📋 Cambios Implementados

### ✅ 1. Eliminado ESP-NOW
- ❌ Removidas todas las includes de `esp_now.h`, `esp_wifi.h`
- ❌ Eliminadas estructuras `CommandData` y `ResponseData`
- ❌ Eliminado callback `OnDataRecv()`
- ❌ Removido registro de peers y canal WiFi
- ❌ Eliminadas variables `centralMAC`, `centralRegistered`

### ✅ 2. Agregado Servidor Web + WebSocket
- ✔️ Añadidas librerías: `ESPAsyncWebServer`, `AsyncTCP`, `LittleFS`, `ArduinoJson`
- ✔️ Servidor HTTP en puerto 80
- ✔️ WebSocket en `/ws` para comunicación bidireccional
- ✔️ Sistema de archivos LittleFS para servir HTML/CSS/JS
- ✔️ WiFi configurado como Access Point (192.168.4.1)

### ✅ 3. Nueva Arquitectura de Comunicación

#### Antes (ESP-NOW):
```
ESP32 Central ←[ESP-NOW]→ ESP32 Motor
```

#### Ahora (WebSocket):
```
Navegador ←[WebSocket/WiFi]→ ESP32-C3 Motor
```

### ✅ 4. API WebSocket

#### Comandos Cliente → ESP32:
```json
{ "cmd": "home" }
{ "cmd": "move", "angle": 45.5 }
{ "cmd": "stop" }
{ "cmd": "getStatus" }
```

#### Respuestas ESP32 → Cliente:
```json
{
  "status": "idle|moving|homing|error",
  "homed": true|false,
  "angle": 45.30,
  "targetAngle": 45.30,
  "moving": true|false,
  "minAngle": 0.15
}
```

### ✅ 5. Funcionalidad del Motor

**CONSERVADA** toda la lógica del motor:
- ✔️ `performHoming()` - Calibración con sensor Hall
- ✔️ `moveToAngle()` - Movimiento preciso
- ✔️ `angleToSteps()` - Conversión ángulo a pasos
- ✔️ `getCurrentAngle()` - Obtener posición actual
- ✔️ Manejo de TMC2130 por SPI
- ✔️ Control con AccelStepper

**MODIFICADO**:
- Estado del motor ahora usa enum `MotorState`
- Broadcasting automático de estado cada 1 segundo
- Actualización de estado en tiempo real vía WebSocket

### ✅ 6. Interfaz Web

#### [index.html](data/index.html)
- Panel de estado del sistema (conexión, motor, ángulo)
- Input para ángulo objetivo
- Botones: HOMING, MOVER, STOP
- 8 botones de ángulos predefinidos (0°, 45°, 90°, 135°, 180°, 225°, 270°, 315°)
- Display de resolución angular (ángulo mínimo)
- Indicador de conexión WebSocket

#### [script.js](data/script.js)
- Conexión WebSocket automática con reconexión
- **Validación client-side**:
  - Rango 0-360°
  - Múltiplo del ángulo mínimo (0.15°)
  - Oferta de redondeo automático
- Actualización UI en tiempo real
- Manejo de eventos de botones
- Gestión de estado del motor
- Logging en consola del navegador

#### [style.css](data/style.css)
- Diseño minimalista técnico (estilo laboratorio)
- Esquema de colores oscuros
- Fuente monoespaciada (Courier New)
- Indicadores visuales de estado
- Animaciones sutiles (pulse, blink)
- Responsive design
- Alto contraste para legibilidad

### ✅ 7. Sistema de Archivos (LittleFS)

Estructura en el ESP32:
```
/index.html       → Página principal
/style.css        → Estilos
/script.js        → Lógica cliente
/favicon.svg      → Icono del sitio
```

**OJO**: Estos archivos deben subirse con `pio run --target uploadfs`

### ✅ 8. Configuración WiFi

```cpp
SSID: "Motor-Caracterizador"
Password: "12345678"
IP: 192.168.4.1
Modo: Access Point (no requiere router)
```

### ✅ 9. Parámetros del Motor

**SIN CAMBIOS**:
```cpp
SM_RESOLUTION = 200 steps/rev
GEAR_RATIO = 3.0
microsteps = 4
stepperCurrent = 500 mA
stepperSpeed = 6000 steps/s
stepperAcc = 50000 steps/s²
```

**Ángulo mínimo resultante**: 360° / (200 × 4 × 3) = **0.15°**

## 📊 Comparación de Carga de Procesamiento

### Antes (ESP-NOW):
```
ESP32 Central:
- Servidor web HTML/CSS/JS
- Lógica de aplicación
- Comunicación ESP-NOW
- Procesamiento de datos

ESP32 Motor:
- Recepción ESP-NOW
- Control del motor
- Envío de estados
```

### Ahora (WebSocket):
```
Navegador (Cliente):
- Renderizado de UI
- Validación de ángulos ✨
- Cálculos de redondeo ✨
- Gestión de estado UI ✨
- Polling automático ✨

ESP32 Motor:
- Servidor web básico
- WebSocket (bajo overhead)
- Control del motor
- Broadcast de estado cada 1s
```

**Resultado**: El ESP32 tiene **mucho menos carga** ya que solo maneja:
1. Servir archivos estáticos (una vez)
2. Mantener conexión WebSocket
3. Controlar el motor

Todo el procesamiento complejo (validaciones, UI, lógica) está en JavaScript del navegador.

## 🔄 Flujo de Operación

1. **Inicio**:
   - ESP32 crea red WiFi "Motor-Caracterizador"
   - Usuario se conecta desde PC/móvil
   
2. **Conexión**:
   - Navegador carga `http://192.168.4.1`
   - Archivos HTML/CSS/JS se descargan una vez
   - WebSocket establece conexión bidireccional
   
3. **Operación**:
   - JavaScript pide estado cada 1 segundo
   - Usuario ingresa ángulo → validación en JS
   - Si es válido → envía comando por WebSocket
   - ESP32 ejecuta movimiento
   - ESP32 broadcast estado actualizado
   - JavaScript actualiza UI

4. **Ventajas**:
   - ✔️ Sin delays en ESP32 (no espera HTTP requests)
   - ✔️ Comunicación instantánea (WebSocket)
   - ✔️ UI siempre actualizada (broadcast automático)
   - ✔️ Validación sin cargar ESP32 (client-side)

## 📁 Archivos Modificados

- ✏️ [platformio.ini](platformio.ini) - Librerías actualizadas
- ✏️ [src/main.cpp](src/main.cpp) - Código completamente refactorizado
- ✏️ [data/index.html](data/index.html) - Interfaz web nueva
- ✏️ [data/script.js](data/script.js) - Lógica JavaScript nueva
- ✏️ [data/style.css](data/style.css) - Estilos nuevos
- ➕ [data/favicon.svg](data/favicon.svg) - Icono del sitio
- ➕ [README.md](README.md) - Documentación completa
- ➕ [FLASHEO.md](FLASHEO.md) - Guía de instalación
- ➕ [data/README_FAVICON.md](data/README_FAVICON.md) - Info del favicon

## ⚠️ IMPORTANTE: Proceso de Flasheo

### Orden obligatorio:

1. **Compilar**: `pio run`
2. **Subir firmware**: `pio run --target upload`
3. **⚠️ CRÍTICO**: `pio run --target uploadfs` (sube HTML/CSS/JS)
4. Reiniciar ESP32
5. Conectar a WiFi "Motor-Caracterizador"
6. Abrir `http://192.168.4.1`

**Si olvidas el paso 3**, verás 404 al abrir la página web.

## 🎯 Objetivos Cumplidos

- ✅ Control por página web (sin ESP32 Central adicional)
- ✅ Máximo procesamiento en cliente (JavaScript)
- ✅ Mínimo procesamiento en ESP32 (solo motor)
- ✅ Movimiento preciso considerando micropasos
- ✅ Homing con sensor Hall funcional
- ✅ Interfaz minimalista técnica
- ✅ WebSocket para comunicación eficiente
- ✅ Validación de ángulos en tiempo real
- ✅ Auto-reconexión
- ✅ Documentación completa

## 🚀 Próximos Pasos

1. Leer [README.md](README.md) para entender el sistema
2. Seguir [FLASHEO.md](FLASHEO.md) para instalar
3. Probar la interfaz web
4. Ajustar parámetros del motor si es necesario
5. ¡Disfrutar del control preciso!

---

**¿Necesitas ayuda?** Revisa la sección de troubleshooting en FLASHEO.md
