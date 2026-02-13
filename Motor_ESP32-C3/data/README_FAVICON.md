# Favicon

El archivo `favicon.svg` se puede usar directamente en navegadores modernos.

## Opción 1: Usar SVG (Recomendado)
El archivo `favicon.svg` ya está incluido y funcionará en Chrome, Firefox, Edge, Safari modernos.

## Opción 2: Convertir a ICO
Si necesitas un archivo `.ico` para compatibilidad con navegadores antiguos:

1. Usar herramienta online: https://convertio.co/es/svg-ico/
2. Subir `favicon.svg`
3. Descargar como `favicon.ico`
4. Reemplazar en la carpeta `data/`

## Opción 3: Generar con ImageMagick
Si tienes ImageMagick instalado:
```bash
convert favicon.svg -resize 32x32 favicon.ico
```

El código del ESP32 está configurado para servir tanto `.ico` como `.svg`.
