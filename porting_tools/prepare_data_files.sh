#!/bin/bash
# ==============================================================================
# Script para empaquetar y desplegar los archivos de datos de Destinia
# hacia ux0:data/destinia/ (PS Vita fisica).
# ==============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

SOURCE_APK_DIR="$PROJECT_ROOT/destinia_extract"
OUTPUT_DATA_DIR="$PROJECT_ROOT/ux0_data/destinia"

echo "================================================================"
echo "  Preparando Archivos de Datos para Destinia (PS Vita)"
echo "================================================================"

mkdir -p "$OUTPUT_DATA_DIR/assets"
mkdir -p "$OUTPUT_DATA_DIR/sound"
mkdir -p "$OUTPUT_DATA_DIR/saves"
mkdir -p "$OUTPUT_DATA_DIR/logs"

# 1. Copiar librería nativa libdestinia_jni.so
if [ -f "$SOURCE_APK_DIR/lib/armeabi/libdestinia_jni.so" ]; then
    echo "[1/3] Copiando libdestinia_jni.so..."
    cp "$SOURCE_APK_DIR/lib/armeabi/libdestinia_jni.so" "$OUTPUT_DATA_DIR/libdestinia_jni.so"
else
    echo "[-] Error: no se encontró libdestinia_jni.so en $SOURCE_APK_DIR/lib/armeabi/"
    exit 1
fi

# 2. Copiar assets del juego (formato propio del motor: .ddt/.tdt/.wmb/.agd)
if [ -d "$SOURCE_APK_DIR/assets" ]; then
    echo "[2/3] Copiando assets..."
    cp -R "$SOURCE_APK_DIR/assets/"* "$OUTPUT_DATA_DIR/assets/"
else
    echo "[-] Error: no se encontró la carpeta assets/"
    exit 1
fi

# 3. Copiar y organizar sonidos (convertir MP3 BGM a OGG Vorbis para Tremor y copiar OGG SFX)
if [ -d "$SOURCE_APK_DIR/res/raw" ]; then
    echo "[3/3] Procesando y copiando efectos de sonido y BGM a sound/..."
    for f in "$SOURCE_APK_DIR/res/raw/"*.ogg; do
        [ -f "$f" ] && cp "$f" "$OUTPUT_DATA_DIR/sound/"
    done

    for f in "$SOURCE_APK_DIR/res/raw/"*.mp3; do
        if [ -f "$f" ]; then
            base=$(basename "$f" .mp3)
            if which ffmpeg >/dev/null 2>&1; then
                echo "  Convertiendo $base.mp3 a $base.ogg..."
                ffmpeg -y -i "$f" -c:a libvorbis -q:a 4 "$OUTPUT_DATA_DIR/sound/$base.ogg" >/dev/null 2>&1 || cp "$f" "$OUTPUT_DATA_DIR/sound/"
            else
                cp "$f" "$OUTPUT_DATA_DIR/sound/"
            fi
        fi
    done
else
    echo "[-] Error: no se encontró res/raw/"
    exit 1
fi

# Limpieza de archivos ocultos de macOS
find "$OUTPUT_DATA_DIR" -name "._*" -delete 2>/dev/null || true
find "$OUTPUT_DATA_DIR" -name ".DS_Store" -delete 2>/dev/null || true

echo ""
echo "[+] Datos preparados exitosamente en: $OUTPUT_DATA_DIR"
du -sh "$OUTPUT_DATA_DIR"

echo ""
echo "Listo. Para transferir a una PS Vita física vía FTP/USB:"
echo "  python3 porting_tools/manage_vita.py"
