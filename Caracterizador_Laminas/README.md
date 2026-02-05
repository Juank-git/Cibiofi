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

### 1. Cargar Motor (ESP32-C3 Super Mini)

```bash
cd Motor
pio lib install
pio run --target upload
```

**Obtener la dirección MAC del motor:**
1. Abrir monitor serial: `pio device monitor`
2. Copiar la MAC mostrada (formato: `0C:4E:A0:XX:XX:XX`)
3. Guardar para el siguiente paso

### 2. Cargar Central (ESP32)

**a) Actualizar MAC del motor en el código:**

Editar `Central/src/main.cpp` línea 24:
```cpp
uint8_t motorMAC[] = {0x0C, 0x4E, 0xA0, 0x64, 0xC0, 0xB8};  // ⚠️ Cambiar por la MAC obtenida
```

**b) Configurar puerto COM en el script Python:**

Editar `Central/scripts/driver_medidor.py` línea 5:
```python
ESP32_SERIAL_PORT = "COM3"  # Cambiar al puerto correcto
```

**c) Cargar firmware y archivos web:**
```bash
cd Central
pio lib install
pio run --target upload      # Subir código
pio run --target uploadfs    # Subir interfaz web (SPIFFS)
```

### 3. Verificación

**Verificar conexión del motor:**
1. Encender el motor (alimentar PCB con 12-24V)
2. Monitor serial del Central debe mostrar: `Motor: ✓`

Si muestra `Motor: ✗`:
- Verificar que la MAC esté correcta en `Central/src/main.cpp`
- Asegurar que el router WiFi esté en canal 11 (o cambiar `ESP_NOW_CHANNEL` en ambos códigos)

## Uso Rápido

1. **Encender sistema:**
   - Alimentar motor (PCB con 12-24V)
   - Conectar ESP32 Central por USB
   - Ejecutar script Python: `python Central/scripts/driver_medidor.py`

2. **Acceder a interfaz web:**
   - Abrir navegador: `http://192.168.137.200`

3. **Calibrar (Homing):**
   - Clic en botón "Homing" (esperar ~30 segundos)

4. **Configurar barrido:**
   - Ángulo máximo: `180°`
   - Paso angular: `1°`
   - Muestras por punto: `500`
   - Ejecuciones: `1`

5. **Iniciar caracterización:**
   - Clic en "Iniciar"
   - Los datos se grafican en tiempo real
   - Al finalizar, clic en "Guardar Series"

## Notas Importantes

- ⚠️ El script Python debe ejecutarse **antes** de iniciar el barrido
- 📊 Un barrido completo de 0° a 180° toma aproximadamente **15-20 minutos**
- 🔄 Ejecutar "Homing" después de encender el sistema o cambiar montaje mecánico
- 📁 Los datos se guardan en formato CSV dentro del ESP32 (descargar desde la web)

## Solución Rápida de Problemas

| Problema | Solución |
|----------|----------|
| Motor no conecta | Verificar MAC en `Central/src/main.cpp` y canal WiFi (11) |
| Python sin datos | Verificar puerto COM en `driver_medidor.py` |
| Homing falla | Verificar conexión sensor Hall (pin 10) y posición del imán |

---

**Ver también**: [Manual de Montaje PCB](../README.md) para información del hardware
