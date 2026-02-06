# Lector de Dirección MAC - ESP32-C3

## Descripción

Este proyecto permite obtener la **dirección MAC** de microcontroladores ESP32-C3 Super Mini. La dirección MAC es un identificador único de hardware que permite diferenciar cada placa de manera individual.

## ¿Para qué sirve?

La dirección MAC es esencial para:

- **Identificar placas individualmente** en proyectos con múltiples ESP32-C3
- **Configurar redes Wi-Fi** con control de acceso basado en MAC
- **Documentar hardware** manteniendo un registro de las direcciones MAC de cada placa
- **Depuración** al trabajar con varios dispositivos simultáneamente

En el contexto del protocolo BB84, cada placa (Alice y Bob) puede ser identificada mediante su dirección MAC única.

## Requisitos

- **Hardware:** ESP32-C3 Super Mini (esp32-c3-devkitm-1)
- **Software:** 
  - Extensión de PlatformIO instalada en VS Code
  - Cable USB tipo C para conectar la placa

## Cómo Cargar el Código a una Placa

**⚠️ IMPORTANTE - Evitar Cortocircuito:**

Antes de cargar código mediante USB, se debe **desconectar el ESP32-C3 de la PCB**. La PCB proporciona alimentación de 5V al pin VIN del microcontrolador. Si se conecta el cable USB mientras el ESP32-C3 está en la PCB, habrá dos fuentes de alimentación simultáneas (USB y PCB), lo que puede causar:

- ⚡ Cortocircuito entre las fuentes de alimentación
- 💥 Daño permanente al ESP32-C3 o a la PCB
- 🔥 Sobrecalentamiento de componentes

**Procedimiento seguro para programar:**
1. Desconectar la alimentación externa de la PCB
2. Remover cuidadosamente el ESP32-C3 de la PCB
3. **Insertar el ESP32-C3 en una protoboard** para evitar que los pines hagan contacto entre sí o con superficies conductoras
4. Conectar el ESP32-C3 a la computadora mediante cable USB
5. Cargar el código (seguir pasos a continuación)
6. Desconectar el cable USB
7. Reinsertar el ESP32-C3 en la PCB

### Paso 1: Conectar la Placa

1. Se conecta el ESP32-C3 Super Mini a la computadora mediante un cable USB tipo C
2. Se espera a que Windows reconozca el dispositivo (puede tardar unos segundos)

**⚠️ Importante - Primera vez o si no se detecta:**

Si es la primera vez que se usa el dispositivo o si surgen problemas para cargar el código, puede ser necesario entrar manualmente en modo de programación:

1. **Presionar** el botón **BOOT**
2. **Mientras está presionado**, conectar el cable USB al computador
3. **Soltar** el botón BOOT después de 2 segundos
4. La placa ahora debería estar en modo programación y lista para recibir código

<img src="images/boot_esp32-c3.jpg" alt="Botón BOOT" width="500">

En descargas posteriores, este procedimiento generalmente no es necesario, ya que el ESP32-C3 entra automáticamente en modo programación.

### Paso 2: Navegar a la Carpeta del Proyecto

Abrir una terminal (PowerShell) y navegar a la carpeta del proyecto MAC:

```powershell
cd C:\ruta\al\repositorio\Cibiofi\BB84\MAC
```

### Paso 3: Cargar y Monitorear

Ejecutar el siguiente comando para compilar, subir el código y abrir el monitor serial:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -t upload -t monitor -e esp32-c3-devkitm-1
```

Este comando realizará automáticamente:
- ✅ Compilación del código
- ✅ Carga del programa a la placa
- ✅ Apertura del monitor serial para ver el resultado

### Paso 4: Leer la Dirección MAC

Una vez cargado el código, el monitor serial mostrará algo similar a:

```
=== DIRECCIÓN MAC ===
String: 0C:4E:A0:65:48:XX
Array:  {0x0C, 0x4E, 0xA0, 0x65, 0x48, 0xXX}
```

**Importante:** Se debe anotar esta dirección MAC y etiquetar físicamente la placa para futuras referencias.

## Repetir para Múltiples Placas

Si se necesita obtener la dirección MAC de varias placas:

1. Se desconecta la placa actual
2. Se conecta la siguiente placa
3. Se ejecuta nuevamente el comando del Paso 3
4. Se anota la nueva dirección MAC

Se repite este proceso para cada placa que se necesite identificar.

## Solución de Problemas

### La placa no se detecta

- Verificar que el cable USB sea de datos (no solo de carga)
- Intentar con otro puerto USB
- En Windows, verificar en el Administrador de Dispositivos que aparezca el puerto COM

### Error de permisos

Si aparece un error de puerto en uso, se debe cerrar cualquier otro monitor serial abierto.

### El monitor no muestra nada

- Presionar el botón RESET en la placa ESP32-C3
- Verificar que `monitor_speed = 115200` esté configurado en `platformio.ini`

## Configuración Técnica

El proyecto está configurado con los siguientes parámetros en `platformio.ini`:

- **Plataforma:** ESP32 (Espressif32)
- **Board:** esp32-c3-devkitm-1
- **Framework:** Arduino
- **Monitor Speed:** 115200 baud
- **Upload Speed:** 460800 baud
- **USB CDC:** Habilitado para comunicación serial por USB
