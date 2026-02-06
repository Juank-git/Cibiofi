# Protocolo BB84 - Criptografía Cuántica

## Descripción

Este proyecto implementa el protocolo **BB84**, un sistema de distribución de claves cuánticas que permite a dos partes (Alice y Bob) establecer una clave criptográfica compartida con seguridad garantizada por las leyes de la física cuántica.

## ¿Qué es BB84?

BB84 es el primer protocolo de criptografía cuántica, propuesto por Charles Bennett y Gilles Brassard en 1984. Permite detectar cualquier intento de espionaje en la transmisión de información, ya que cualquier medición de un estado cuántico lo perturba de manera detectable.

## Componentes del Sistema

El sistema está compuesto por tres módulos independientes:

### 📡 **Central** (ESP32 DevKit)
- Coordina la comunicación entre Alice y Bob mediante ESP-NOW
- Proporciona interfaz web para configuración y monitoreo
- Se conecta a FPGA para control de transmisión de fotones
- Recopila y presenta resultados del protocolo

### 📤 **Alice** (ESP32-C3 Super Mini)
- Genera bits aleatorios y selecciona bases de polarización
- Controla motor paso a paso para orientar polarizadores
- Transmite fotones polarizados según protocolo BB84
- Se sincroniza automáticamente con el Central

### 📥 **Bob** (ESP32-C3 Super Mini)
- Selecciona bases de medición aleatoriamente
- Controla motor paso a paso para orientar analizadores
- Mide fotones recibidos
- Se sincroniza automáticamente con el Central

## Funcionamiento General

1. **Inicialización**: Los tres dispositivos se sincronizan automáticamente en el mismo canal WiFi
2. **Calibración**: Alice y Bob realizan homing de sus motores para posiciones de referencia
3. **Transmisión**: Alice genera y transmite fotones con polarización aleatoria
4. **Medición**: Bob mide los fotones con bases aleatorias
5. **Comparación**: El Central recopila bases y bits para análisis post-protocolo
6. **Resultados**: La interfaz web muestra estadísticas del intercambio cuántico

## Características Principales

- ✅ Sincronización automática de canal WiFi entre dispositivos
- ✅ Comunicación de baja latencia mediante ESP-NOW
- ✅ Control de precisión de motores paso a paso con drivers TMC2130
- ✅ Interfaz web intuitiva para configuración y monitoreo
- ✅ Generación de números aleatorios por hardware
- ✅ Sistema de homing automático con sensores Hall
- ✅ Detección de fotones mediante FPGA

## Requisitos de Hardware

- **1x ESP32 DevKit** (Central)
- **2x ESP32-C3 Super Mini** (Alice y Bob)
- **2x Motor paso a paso NEMA con encoder magnético**
- **2x Driver TMC2130**
- **2x Sensor Hall** (para homing)
- **1x FPGA** (control de emisión y detección de fotones)
- **Componentes ópticos** (polarizadores, beam splitters, detectores)

## Requisitos de Software

- PlatformIO IDE (extensión de VS Code)
- Git (para clonar el repositorio)

## Estructura del Proyecto

```
BB84/
├── README.md                 # Este archivo
├── Central/                  # Coordinador principal (ESP32 Dev)
│   ├── src/main.cpp
│   ├── data/                 # Archivos web (HTML, CSS, JS)
│   └── platformio.ini
├── Alice/                    # Emisor de fotones (ESP32-C3)
│   ├── src/main.cpp
│   └── platformio.ini
└── Bob/                      # Receptor de fotones (ESP32-C3)
    ├── src/main.cpp
    └── platformio.ini
```

## Inicio Rápido

### 1. Clonar el Repositorio

Ver [instrucciones de clonación](../README.md#clonar-el-repositorio-en-tu-equipo-local) en el README principal.

### 2. Obtener Direcciones MAC

Seguir la guía del proyecto [MAC](../MAC/README.md) para obtener las direcciones MAC de Alice y Bob.

### 3. Configurar Direcciones MAC en Central

Actualizar las direcciones MAC en [Central/src/main.cpp](Central/src/main.cpp):

```cpp
uint8_t aliceMAC[] = {0x0C,0x4E,0xA0,0x65,0x48,0xCC};  // Actualizar con MAC de Alice
uint8_t bobMAC[] = {0x0C,0x4E,0xA0,0x65,0x48,0x3C};     // Actualizar con MAC de Bob
```

### 4. Cargar Código a cada Dispositivo

Consultar los archivos README individuales de cada módulo para instrucciones detalladas de carga.

## Documentación Adicional

- [Central](Central/README.md) - Configuración y uso del coordinador
- [Alice](Alice/README.md) - Configuración del emisor
- [Bob](Bob/README.md) - Configuración del receptor
- [MAC](../MAC/README.md) - Herramienta para obtener direcciones MAC

## Notas Importantes

- ⚠️ Los tres dispositivos deben estar encendidos antes de iniciar el protocolo
- ⚠️ La sincronización de canal WiFi es automática; no requiere configuración manual
- ⚠️ El Central debe conectarse primero al router WiFi antes de sincronizar con Alice y Bob
- ⚠️ Realizar homing antes de cada sesión de transmisión para garantizar precisión

## Licencia

Este proyecto es parte del trabajo de investigación del CIBioFi y está disponible para fines educativos y de investigación.
