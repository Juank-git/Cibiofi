# Motor Caracterizador - Interfaz Web

Control de motor paso a paso mediante interfaz web servida desde ESP32-C3.

## 🔧 Características

- **Control vía WebSocket**: Comunicación en tiempo real entre navegador y ESP32
- **Interfaz web minimalista**: Diseño técnico tipo laboratorio
- **Homing automático**: Calibración con sensor Hall
- **Precisión controlada**: Validación de ángulos según micropasos configurados
- **Punto de acceso WiFi**: El ESP32 crea su propia red WiFi (no requiere router)
- **Sin procesamiento pesado en ESP32**: Toda la validación y lógica UI en JavaScript

## 📋 Especificaciones Técnicas

- **Microcontrolador**: ESP32-C3 Super Mini
- **Driver**: TMC2130 (SPI)
- **Motor**: Stepper con reducción 3:1
- **Resolución**: 200 pasos/rev × 4 micropasos × 3 reducción = **0.15°/paso**
- **Sensor**: Hall effect para homing
- **Comunicación**: WebSocket sobre WiFi AP

## 📁 Estructura del Proyecto

```
Motor_ESP32-C3/
├── src/
│   └── main.cpp              # Código principal del ESP32
├── data/                     # Archivos web (suben a LittleFS)
│   ├── index.html            # Interfaz de usuario
│   ├── script.js             # Lógica WebSocket y validaciones
│   ├── style.css             # Estilos minimalistas
│   ├── favicon.svg           # Icono del sitio
│   └── README_FAVICON.md     # Instrucciones para favicon
├── platformio.ini            # Configuración del proyecto
└── README.md                 # Este archivo
```

## 🚀 Instalación

### 1. Requisitos Previos

- [PlatformIO](https://platformio.org/) (extensión de VS Code)
- Cable USB-C para el ESP32-C3

### 2. Compilar y Subir el Firmware

```bash
# En VS Code con PlatformIO
# Presiona Ctrl+Shift+P y selecciona:
PlatformIO: Upload
```

O desde terminal:
```bash
pio run --target upload
```

### 3. Subir Archivos al Sistema de Archivos (LittleFS)

**IMPORTANTE**: Los archivos HTML, CSS, JS deben subirse al ESP32.

```bash
# En VS Code con PlatformIO
# Presiona Ctrl+Shift+P y selecciona:
PlatformIO: Upload Filesystem Image
```

O desde terminal:
```bash
pio run --target uploadfs
```

## 📡 Conexión y Uso

### 1. Conectar a la Red WiFi

Después de flashear el ESP32:

1. El ESP32 creará una red WiFi llamada: **`Motor-Caracterizador`**
2. Contraseña: **`12345678`**
3. Desde tu PC/tablet/móvil, conéctate a esa red WiFi

### 2. Abrir la Interfaz Web

1. En el navegador, visita: **`http://192.168.4.1`**
2. Verás la interfaz de control del motor
3. Espera a que el WebSocket se conecte (indicador verde)

### 3. Operación

#### Primer Uso - Homing:
1. Presiona el botón **HOMING** para calibrar el motor
2. El motor buscará el sensor Hall y establecerá posición 0°
3. Espera a que termine (verás "Homing: SÍ")

#### Mover a un Ángulo:
1. Ingresa el ángulo deseado (0-360°)
2. Presiona **MOVER** o Enter
3. El sistema validará que sea múltiplo de 0.15° (resolución mínima)
4. Si no es exacto, ofrecerá redondear automáticamente

#### Ángulos Predefinidos:
- Usa los botones 0°, 45°, 90°, 135°, 180°, 225°, 270°, 315°
- Se mueven automáticamente al hacer clic

#### Botón STOP:
- Detiene el motor inmediatamente en caso de emergencia

## ⚙️ Configuración Avanzada

### Cambiar Credenciales WiFi

Edita en `main.cpp`:
```cpp
const char* ssid = "Motor-Caracterizador";
const char* password = "12345678";  // Mínimo 8 caracteres
```

### Ajustar Parámetros del Motor

En `main.cpp`:
```cpp
int stepperCurrent = 500;   // mA - Corriente RMS
int stepperSpeed = 6000;    // steps/s - Velocidad máxima
int stepperAcc = 50000;     // steps/s² - Aceleración
int microsteps = 4;         // Micropasos (1, 2, 4, 8, 16, 32, 64, 128, 256)
```

### Cambiar Ángulo Mínimo

Modifica cualquiera de estos valores:
```cpp
#define SM_RESOLUTION 200   // Pasos por revolución del motor
#define GEAR_RATIO 3.0      // Relación de reducción
int microsteps = 4;         // Micropasos del driver
```

**Ángulo mínimo = 360° / (SM_RESOLUTION × microsteps × GEAR_RATIO)**

## 🔌 Conexiones Hardware

### TMC2130 (SPI)
- **MOSI** → GPIO 6
- **MISO** → GPIO 5
- **SCLK** → GPIO 4
- **CS** → GPIO 7

### Motor
- **STEP** → GPIO 1
- **DIR** → GPIO 2
- **ENABLE** → GPIO 3

### Sensor Hall
- **Hall Sensor** → GPIO 10 (con pull-up)

## 🐛 Solución de Problemas

### No aparece la red WiFi
- Verifica que el firmware se haya subido correctamente
- Revisa el monitor serial (115200 baud)
- Reinicia el ESP32 (botón RESET)

### No se carga la página web
- Asegúrate de haber subido los archivos con "Upload Filesystem Image"
- Verifica en el monitor serial que LittleFS se montó correctamente
- Intenta borrar flash: `pio run --target erase`

### WebSocket no conecta
- Refresca la página (F5)
- Verifica que estés en la red WiFi correcta
- Revisa la consola del navegador (F12) para errores

### El motor no se mueve
1. Verifica que el TMC2130 esté correctamente conectado
2. Asegúrate de haber hecho homing primero
3. Revisa el monitor serial para mensajes de error
4. Verifica las conexiones STEP, DIR, ENABLE

### Error de validación de ángulo
- El ángulo debe ser múltiplo de 0.15° (con configuración por defecto)
- Usa los botones predefinidos o acepta el redondeo automático

## 📊 API WebSocket

### Mensajes Cliente → ESP32

```javascript
// Obtener estado
{ cmd: "getStatus" }

// Iniciar homing
{ cmd: "home" }

// Mover a ángulo
{ cmd: "move", angle: 45.5 }

// Detener
{ cmd: "stop" }
```

### Mensajes ESP32 → Cliente

```javascript
{
  status: "idle",      // "idle", "moving", "homing", "error"
  homed: true,         // boolean
  angle: 45.30,        // float - ángulo actual
  targetAngle: 45.30,  // float - ángulo objetivo
  moving: false,       // boolean
  minAngle: 0.15       // float - resolución angular
}
```

## 📝 Notas

- El sistema envía estado cada 1 segundo mientras hay clientes conectados
- Todas las validaciones de ángulo se hacen en JavaScript (client-side)
- El ESP32 solo ejecuta comandos válidos
- La reconexión WebSocket es automática

## 🔄 Versiones

### v1.0 (Actual)
- Control por interfaz web con WebSocket
- Homing automático con sensor Hall
- Validación de ángulos en tiempo real
- Interfaz minimalista técnica
- Soporte para ángulos predefinidos

## 📄 Licencia

Proyecto de código abierto para uso educativo y científico.

---

**Desarrollado para sistema de caracterización de láminas ópticas**
