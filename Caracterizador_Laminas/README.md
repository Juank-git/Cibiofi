# Caracterizador de Láminas de Media Onda

Sistema automatizado para caracterizar láminas de media onda mediante barridos angulares de potencia óptica. Permite determinar los ángulos de polarización óptimos necesarios para experimentos de criptografía cuántica (protocolo BB84).

## ¿Cómo Funciona?

El sistema rota automáticamente una lámina de media onda mientras mide la potencia óptica transmitida en cada posición angular. Esto permite identificar:

- **Ángulo de máxima transmisión**: Donde pasa más luz
- **Ángulo de mínima transmisión**: Donde se bloquea la luz (extinción)

### Arquitectura del Sistema

```
[Interfaz Web] ←──→ [ESP32 Central] ←──→ [ESP32-C3 Motor] ──→ [Motor Stepper]
                           │
                           ↓
                    [Python Script]
                           │
                           ↓
                    [Medidor PM100D]
```

**Componentes:**
- **ESP32 Central**: Coordina el experimento y gestiona la interfaz web
- **ESP32-C3 Motor**: Controla el motor paso a paso con el driver TMC2130
- **Python Script**: Comunica el medidor de potencia Thorlabs PM100D con el ESP32
- **Interfaz Web**: Permite configurar y visualizar el barrido en tiempo real

## Carga de Microcontroladores

### Requisitos Previos

1. Instalar [PlatformIO](https://platformio.org/install) (extensión de VS Code recomendada)
2. Instalar Python y dependencias:
   ```bash
   pip install pyvisa pyvisa-py pyserial
   ```

### Scripts de Python

El proyecto incluye dos scripts para gestionar el medidor de potencia Thorlabs PM100D:

#### `verificar.py` - Identificar Dispositivos VISA
**Propósito:** Lista todos los dispositivos VISA (USB/GPIB) conectados al sistema.

**Uso:**
```bash
python Central/scripts/verificar.py
```

**Salida esperada:**
```
Recursos encontrados: ('USB0::4883::32888::P0021181::0::INSTR',)
```

El identificador mostrado se usa en `driver_medidor.py` para conectar con el medidor específico.

#### `driver_medidor.py` - Interfaz ESP32-Medidor
**Propósito:** Actúa como puente entre el ESP32 Central y el medidor PM100D, leyendo potencia óptica bajo demanda.

**Configuración requerida:**
- Línea 5: Puerto COM del ESP32
- Línea 11: Identificador del medidor (obtenido con `verificar.py`)

**Debe ejecutarse antes de iniciar barridos** para que el ESP32 pueda solicitar mediciones.

### 1. Cargar Motor (ESP32-C3 Super Mini)

**⚠️ IMPORTANTE:** Desconectar el ESP32-C3 de la PCB antes de programar. Ver [procedimiento seguro](../../MAC/README.md#cómo-cargar-el-código-a-una-placa).

Navegar a la carpeta del Motor:
```powershell
cd C:\ruta\al\repositorio\Cibiofi\BB84\Caracterizador_Laminas\Motor
```

Cargar el código:
```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -t upload -t monitor -e esp32-c3-devkitm-1
```

**Obtener la dirección MAC del motor:**
1. El monitor serial se abrirá automáticamente después de la carga
2. Copiar la MAC mostrada (formato: `0C:4E:A0:XX:XX:XX`)
3. Guardar para el siguiente paso
4. Presionar `Ctrl+C` para cerrar el monitor

### 2. Cargar Central (ESP32)

**a) Actualizar MAC del motor en el código:**

Editar `Central/src/main.cpp` línea 24:
```cpp
uint8_t motorMAC[] = {0x0C, 0x4E, 0xA0, 0x64, 0xC0, 0xB8};  // ⚠️ Cambiar por la MAC obtenida
```

**b) Identificar y configurar el medidor de potencia:**

Primero, identificar el medidor conectado:
```bash
cd Central/scripts
python verificar.py
```

Este script mostrará los dispositivos VISA conectados. Buscar la línea similar a:
```
Recursos encontrados: ('USB0::4883::32888::P0021181::0::INSTR',)
```

Copiar el identificador completo (ej: `USB0::4883::32888::P0021181::0::INSTR`)

**c) Configurar puerto COM y medidor en el script Python:**

Editar `Central/scripts/driver_medidor.py`:
- **Línea 5**: Cambiar puerto COM
  ```python
  ESP32_SERIAL_PORT = "COM3"  # Cambiar al puerto correcto del ESP32
  ```
- **Línea 11**: Cambiar identificador del medidor
  ```python
  instr = rm.open_resource("USB0::4883::32888::P0021181::0::INSTR")  # ⚠️ Poner el identificador obtenido con verificar.py
  ```

**d) Cargar firmware y archivos web:**

Navegar a la carpeta del Central:
```powershell
cd C:\ruta\al\repositorio\Cibiofi\BB84\Caracterizador_Laminas\Central
```

Cargar el código del ESP32:
```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -t upload -t monitor -e esp32dev
```

Una vez verificado, cerrar el monitor (`Ctrl+C`) y cargar la interfaz web:
```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -t uploadfs -e esp32dev
```

### 3. Verificación

**Verificar conexión del motor:**
1. Encender el motor (alimentar PCB con 12-24V)
2. Reconectar el ESP32-C3 a la PCB (después de programarlo)
3. Abrir monitor serial del Central (si no está abierto):
   ```powershell
   & "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" device monitor -e esp32dev
   ```
4. El monitor debe mostrar: `Motor: ✓`

Si muestra `Motor: ✗`:
- Verificar que la MAC esté correcta en `Central/src/main.cpp`
- Asegurar que el router WiFi esté en canal 11 (o cambiar `ESP_NOW_CHANNEL` en ambos códigos)

## Uso Rápido

### Primera Configuración (solo una vez)

Si es la primera vez usando el medidor de potencia:
1. Conectar el medidor PM100D por USB
2. Ejecutar: `python Central/scripts/verificar.py`
3. Copiar el identificador mostrado
4. Actualizar `Central/scripts/driver_medidor.py` línea 11 con el identificador

### Operación Normal

1. **Encender sistema:**
   - Alimentar motor (PCB con 12-24V)
   - Conectar ESP32 Central por USB
   - Conectar medidor PM100D por USB
   - Ejecutar script Python: `python Central/scripts/driver_medidor.py`

2. **Acceder a interfaz web:**
   - Abrir navegador: `http://192.168.137.200`

3. **Calibrar (Homing):**
   - Clic en botón "Homing" (esperar un par de segundos)

4. **Configurar barrido:**
   - Ángulo máximo: `180°`
   - Paso angular: `1°`
   - Muestras por punto: `1`
   - Ejecuciones: `1`

5. **Iniciar caracterización:**
   - Clic en "Iniciar"
   - Los datos se grafican en tiempo real
   - Al finalizar, clic en "Guardar Series"

## Notas Importantes

- ⚠️ El script Python debe ejecutarse **antes** de iniciar el barrido
- 🔄 Ejecutar "Homing" después de encender el sistema o cambiar montaje mecánico
- 📁 Los datos se guardan en formato CSV dentro del ESP32 (descargar desde la web)

## Solución Rápida de Problemas

| Problema | Solución |
|----------|----------|
| Motor no conecta | Verificar MAC en `Central/src/main.cpp` y canal WiFi |
| Python sin datos | Verificar puerto COM en `driver_medidor.py` línea 5 |
| Python no detecta medidor | Ejecutar `verificar.py` y actualizar identificador en `driver_medidor.py` línea 11 |
| Error "No resources found" | Verificar que el medidor PM100D esté conectado por USB y encendido |
| Homing falla | Verificar conexión sensor Hall (pin 10) y posición del imán |
| Barrido sin mediciones | Asegurar que `driver_medidor.py` esté ejecutándose antes de iniciar barrido |

---

**Ver también**: [Manual de Montaje PCB](../README.md) para información del hardware
