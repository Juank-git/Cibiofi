# Bob - Receptor de Fotones (Protocolo BB84)

## Descripción

Bob es el módulo receptor del sistema BB84. Selecciona bases de medición aleatoriamente y controla el motor paso a paso que orienta los analizadores para medir los fotones recibidos. Bob no genera bits, solo mide la polarización de los fotones transmitidos por Alice.

## Hardware

- **Microcontrolador:** ESP32-C3 Super Mini (esp32-c3-devkitm-1)
- **Driver motor:** TMC2130 (comunicación SPI)
- **Motor:** Paso a paso con relación de engranajes 3:1
- **Sensor:** Hall para homing (pin 10)
- **Pinout:**
  - SPI: MOSI=6, MISO=5, SCLK=4, CS=7
  - Motor: STEP=1, DIR=2, ENABLE=3
  - Sensor Hall: Pin 10

## Ángulos de Medición BB84

Bob utiliza dos bases de medición:

| Base | Ángulo | Analizador |
|------|--------|------------|
| 0 (+) | 13.95° | Rectilíneo |
| 1 (×) | 36.45° | Diagonal |

**Nota:** Bob selecciona la base aleatoriamente, pero no genera bits. Los bits se determinan por los detectores FPGA (detector 0 o detector 1).

## Configuración y Carga

### Preparación del Hardware

**⚠️ IMPORTANTE - Evitar Cortocircuito:**

Antes de cargar código mediante USB, se debe **desconectar el ESP32-C3 de la PCB**. Ver [procedimiento seguro](../../MAC/README.md#cómo-cargar-el-código-a-una-placa) en el proyecto MAC.

### Cargar el Código

**⚠️ Conectar solo un microcontrolador a la vez** para evitar interferencias en los puertos de comunicación.

Navegar a la carpeta del proyecto Bob:

```powershell
cd C:\ruta\al\repositorio\Cibiofi\BB84\BB84\Bob
```

Ejecutar el comando de carga:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -t upload -t monitor -e esp32-c3-devkitm-1
```

## Funcionamiento

### Secuencia de Inicio

1. **Inicialización ESP-NOW**: Arranca en canal 1
2. **Espera sincronización**: Recibe configuración de canal del Central
3. **Cambia de canal**: Se sincroniza con el canal del router WiFi
4. **Espera comandos**: Aguarda instrucciones del Central

### Proceso de Homing

Cuando recibe `CMD_HOME` del Central:

1. Detecta sensor Hall mediante interrupción
2. Busca posición de referencia (imán)
3. Realiza aproximación fina paso a paso
4. Establece posición 0°
5. Notifica `STATUS_HOME_COMPLETE` al Central

### Medición de Pulsos

Para cada pulso (comando `CMD_PREPARE_PULSE`):

1. Genera **base aleatoria** (0 o 1)
2. Calcula **ángulo** según tabla de medición
3. Mueve motor al ángulo calculado
4. Espera a que FPGA detecte el fotón
5. Notifica `STATUS_READY` al Central con base y ángulo

**Diferencia con Alice:** Bob no genera ni transmite bits. El bit medido se determina por cuál detector (0 o 1) de la FPGA se activa.

## Protocolo de Comunicación

### Comandos Recibidos del Central

| Comando | Acción |
|---------|--------|
| `CMD_SET_CHANNEL` | Configura canal WiFi (automático) |
| `CMD_PING` | Responde con `STATUS_PONG` |
| `CMD_HOME` | Ejecuta rutina de homing |
| `CMD_PREPARE_PULSE` | Prepara siguiente medición |
| `CMD_ABORT` | Detiene motor |

### Mensajes Enviados al Central

Envía estructura `ResponseData` con:
- **status**: Estado actual
- **pulseNum**: Número de pulso
- **base**: Base de medición seleccionada (0 o 1)
- **bit**: Siempre 0 (no aplica para Bob)
- **angle**: Ángulo alcanzado

## Configuración del Motor

Parámetros configurables en [src/main.cpp](src/main.cpp):

```cpp
int stepperCurrent = 500;   // Corriente del motor (mA)
int stepperSpeed   = 4000;  // Velocidad (pasos/s)
int stepperAcc     = 25000; // Aceleración (pasos/s²)
int microsteps     = 4;     // Micropasos del driver
```

## Interpretación de Resultados

Bob NO determina directamente el bit recibido. La medición se interpreta de la siguiente forma:

1. **Bob selecciona base**: Orienta analizador según base aleatoria
2. **Fotón atraviesa analizador**: Dependiendo de su polarización
3. **FPGA detecta**: Cual detector (0 o 1) captura el fotón
4. **Central registra**: Base de Bob + Detector activado = Bit medido

**Coincidencia de bases:**
- Si base de Alice == base de Bob → El bit se mide correctamente
- Si base de Alice ≠ base de Bob → El bit medido es aleatorio (se descarta)

## Solución de Problemas

### No se conecta con Central

1. Verificar que la dirección MAC de Bob esté correcta en [Central](../Central/README.md#2-configurar-direcciones-mac)
2. Asegurar que Bob se encienda ANTES que Central
3. Revisar mensajes de sincronización en el monitor serial

### Homing no funciona

1. Verificar conexión del sensor Hall (pin 10)
2. Comprobar polaridad del imán
3. Revisar que el sensor esté a distancia correcta del imán
4. Ver mensajes de debug durante homing

### Motor no se mueve

1. Verificar alimentación de la PCB (5V)
2. Comprobar conexiones SPI al TMC2130
3. Revisar mensaje "TMC2130 OK" en el monitor serial
4. Verificar que el pin ENABLE esté correctamente conectado

### Primera carga del código

Si es la primera vez que se programa:
1. Mantener presionado el botón **BOOT**
2. Conectar el USB mientras está presionado
3. Soltar BOOT después de 2 segundos
4. Ejecutar comando de carga

## Diferencias con Alice

| Aspecto | Alice | Bob |
|---------|-------|-----|
| **Rol** | Emisor | Receptor |
| **Bases** | 2 (con 2 polarizaciones cada una) | 2 (una orientación por base) |
| **Genera bits** | Sí (aleatoriamente) | No |
| **Determina bit** | Al transmitir | FPGA al detectar |
| **Ángulos** | 4 ángulos diferentes | 2 ángulos diferentes |

## Especificaciones Técnicas

- **Resolución motor:** 200 pasos/revolución
- **Relación engranajes:** 3:1
- **Micropasos:** 4 (configurable)
- **Precisión angular:** ~0.15° por microstep
- **Generación aleatoria:** Hardware RNG (`esp_random()`) solo para base
- **Comunicación:** ESP-NOW (baja latencia)
- **Sincronización:** Automática con Central

## Documentación Relacionada

- [README Principal BB84](../README.md) - Visión general del sistema
- [Central](../Central/README.md) - Configuración del coordinador
- [Alice](../Alice/README.md) - Configuración del emisor
- [MAC](../../MAC/README.md) - Obtener dirección MAC y procedimiento de carga seguro
