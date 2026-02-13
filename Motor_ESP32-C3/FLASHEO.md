# Guía de Flasheo - Motor Caracterizador

## 📌 Checklist Rápido

- [ ] PlatformIO instalado en VS Code
- [ ] ESP32-C3 conectado por USB
- [ ] Puerto COM identificado
- [ ] Compilación sin errores
- [ ] Firmware subido
- [ ] **IMPORTANTE**: Sistema de archivos subido (HTML/CSS/JS)
- [ ] Red WiFi visible
- [ ] Interfaz web accesible

## 🔧 Pasos Detallados

### 1. Instalar PlatformIO

Si aún no tienes PlatformIO:

1. Abre VS Code
2. Ve a Extensions (Ctrl+Shift+X)
3. Busca "PlatformIO IDE"
4. Instala la extensión oficial
5. Reinicia VS Code

### 2. Abrir el Proyecto

1. En VS Code: File → Open Folder
2. Selecciona la carpeta `Motor_ESP32-C3`
3. PlatformIO detectará automáticamente el proyecto

### 3. Verificar Puerto COM

#### En Windows (PowerShell):
```powershell
# Listar puertos COM
Get-PnpDevice -Class Ports

# O en cmd
mode
```

#### Actualizar platformio.ini:
```ini
upload_port = COM11      # Cambia por tu puerto
monitor_port = COM11     # Mismo puerto
```

### 4. Compilar el Código

**Opción A - VS Code**:
- Presiona `Ctrl+Shift+P`
- Escribe: `PlatformIO: Build`
- O haz clic en el ícono ✓ (checkmark) en la barra inferior

**Opción B - Terminal**:
```bash
pio run
```

Espera a que termine. Debes ver:
```
SUCCESS
```

### 5. Subir el Firmware

**Opción A - VS Code**:
- Presiona `Ctrl+Shift+P`
- Escribe: `PlatformIO: Upload`
- O haz clic en el ícono → (flecha derecha) en la barra inferior

**Opción B - Terminal**:
```bash
pio run --target upload
```

**Si hay error de conexión**:
1. Desconecta el ESP32
2. Mantén presionado el botón BOOT
3. Conecta el USB
4. Suelta BOOT
5. Vuelve a intentar upload

### 6. ⚠️ PASO CRÍTICO: Subir Sistema de Archivos

**ESTE PASO ES OBLIGATORIO** para que funcione la interfaz web.

**Opción A - VS Code**:
- Presiona `Ctrl+Shift+P`
- Escribe: `PlatformIO: Upload Filesystem Image`
- Espera a que termine (puede tomar 30-60 segundos)

**Opción B - Terminal**:
```bash
pio run --target uploadfs
```

**Verifica que aparezca**:
```
Building FS image...
Uploading...
Success
```

### 7. Verificar en Monitor Serial

1. Abre el monitor serial:
   - Presiona `Ctrl+Shift+P`
   - Escribe: `PlatformIO: Monitor`
   - O haz clic en el ícono 🔌 en la barra inferior

2. Deberías ver:
```
=== MOTOR CARACTERIZADOR - WEB INTERFACE ===
[LittleFS] Montado OK
[WiFi] AP iniciado: Motor-Caracterizador
[WiFi] IP: 192.168.4.1
[WiFi] Contraseña: 12345678
[Server] Servidor web iniciado en http://192.168.4.1
[Motor] TMC2130 OK
[Motor] Ángulo mínimo: 0.15°
[Motor] READY
```

### 8. Conectarse

1. **En tu PC/móvil**:
   - Abre configuración WiFi
   - Busca red: `Motor-Caracterizador`
   - Contraseña: `12345678`
   - Conecta

2. **Abrir navegador**:
   - URL: `http://192.168.4.1`
   - Debe cargar la interfaz web

3. **Verificar WebSocket**:
   - Debe aparecer indicador verde
   - "WebSocket: Conectado"

## 🛠️ Comandos Útiles

### Limpiar y Recompilar
```bash
pio run --target clean
pio run
```

### Borrar Flash Completamente
```bash
pio run --target erase
```
Después deberás subir firmware y filesystem nuevamente.

### Ver Información del Dispositivo
```bash
pio device list
```

### Monitor Serial con Filtro
```bash
pio device monitor --filter direct
```

## ❌ Errores Comunes

### Error: "Could not open port"
- **Causa**: Otro programa usa el puerto (monitor serial abierto)
- **Solución**: Cierra todos los monitores seriales y vuelve a intentar

### Error: "Connecting..."
- **Causa**: ESP32 no está en modo bootloader
- **Solución**: Mantén BOOT presionado mientras conectas USB

### Error: "Flash size mismatch"
- **Causa**: Configuración incorrecta del board
- **Solución**: Verifica que `platformio.ini` tenga: `board = esp32-c3-devkitm-1`

### Error: Página 404 Not Found
- **Causa**: No se subieron los archivos del filesystem
- **Solución**: Ejecuta `pio run --target uploadfs`

### Error: TMC2130 no responde
- **Causa**: Conexiones SPI incorrectas o driver sin alimentación
- **Solución**: 
  1. Verifica conexiones MOSI, MISO, SCLK, CS
  2. Verifica alimentación del driver (VM)
  3. Revisa que CS esté en GPIO 7

### Error: WebSocket no conecta
- **Causa**: Firmware antiguo o archivos JS desactualizados
- **Solución**: Sube nuevamente firmware Y filesystem

## 📊 Troubleshooting Avanzado

### Verificar Archivos en LittleFS

Agrega temporalmente este código en `setup()`:

```cpp
File root = LittleFS.open("/");
File file = root.openNextFile();
while (file) {
    Serial.print("FILE: ");
    Serial.println(file.name());
    file = root.openNextFile();
}
```

Deberías ver:
```
FILE: /index.html
FILE: /style.css
FILE: /script.js
FILE: /favicon.svg
```

### Probar Conexión sin WiFi

Modifica temporalmente para usar WiFi de tu casa:

```cpp
WiFi.mode(WIFI_STA);
WiFi.begin("TuWiFi", "TuPassword");
```

Luego busca la IP en el monitor serial.

### Capturar Logs del WebSocket

Abre consola del navegador (F12) y ejecuta:

```javascript
// Ver mensajes WebSocket
ws.addEventListener('message', (e) => {
    console.log('WS ◄', e.data);
});
```

## ✅ Lista de Verificación Post-Flasheo

1. [x] Monitor serial muestra "READY"
2. [x] Red WiFi visible
3. [x] Página web carga correctamente
4. [x] WebSocket conecta (indicador verde)
5. [x] Todos los archivos CSS/JS cargan (F12 → Network)
6. [x] Botón HOME responde
7. [x] Input de ángulo acepta valores
8. [x] Estado se actualiza cada segundo

## 🔄 Actualización de Código

Para actualizar solo el código (sin filesystem):

```bash
pio run --target upload
```

Para actualizar solo archivos web:

```bash
pio run --target uploadfs
```

Para actualizar ambos:

```bash
pio run --target upload && pio run --target uploadfs
```

## 📞 Soporte

Si después de seguir estos pasos no funciona:

1. Ejecuta `pio run --target erase`
2. Sube firmware: `pio run --target upload`
3. Sube filesystem: `pio run --target uploadfs`
4. Verifica todos los mensajes en monitor serial
5. Captura pantalla del monitor serial y consola del navegador (F12)

---

**Última actualización**: Motor Caracterizador v1.0
