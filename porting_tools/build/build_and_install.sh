#!/bin/bash
set -e

PROJECT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD_DIR="/tmp/destinia-build"
SRC_DIR="/tmp/destinia-src"
VPK_NAME="destinia.vpk"

echo "================================================================"
echo "  Build automático para Destinia (PS Vita)"
echo "================================================================"

echo "[1/3] Preparando entorno de compilación (symlink en /tmp -- evita el bug"
echo "      de vita-pack-vpk con rutas que contienen espacios, ver toolchain_gotchas.md)..."
mkdir -p "$BUILD_DIR" "$SRC_DIR"

if [ -z "$VITASDK" ]; then
    if [ -d "/usr/local/vitasdk" ]; then
        export VITASDK="/usr/local/vitasdk"
    elif [ -d "$HOME/vitasdk" ]; then
        export VITASDK="$HOME/vitasdk"
    else
        echo "Error: VITASDK no definida y no se encontró en rutas por defecto."
        exit 1
    fi
    export PATH="$VITASDK/bin:$PATH"
fi

rsync -a \
    --exclude '.git' --exclude 'build' --exclude '.*' \
    --exclude 'decompiled' --exclude 'destinia_extract' \
    --exclude '*.apk' --exclude '*.zip' \
    "$PROJECT_DIR/" "$SRC_DIR/"

echo "[2/3] Ejecutando CMake y Make..."
cd "$BUILD_DIR"
read -p "¿Build de depuración (logging detallado)? [S/n] " DEBUG_OPTION
if [[ "$DEBUG_OPTION" =~ ^[nN]$ ]]; then
    BUILD_TYPE="Release"
else
    BUILD_TYPE="Debug"
fi
cmake "$SRC_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DCMAKE_POLICY_VERSION_MINIMUM=3.5
make -j$(sysctl -n hw.ncpu 2>/dev/null || echo 4)

VPK_PATH="$BUILD_DIR/$VPK_NAME"
if [ ! -f "$VPK_PATH" ]; then
    echo "Error: no se generó $VPK_PATH -- revisar VITA_VPKNAME en CMakeLists.txt."
    exit 1
fi
mkdir -p "$PROJECT_DIR/build"
cp "$VPK_PATH" "$PROJECT_DIR/build/"
echo "Build exitoso: $PROJECT_DIR/build/$VPK_NAME"

# ELF sin firmar (útil para simbolizar un .psp2dmp con vita-parse-core)
ELF_PATH="$BUILD_DIR/destinia"
if [ -f "$ELF_PATH" ]; then
    cp "$ELF_PATH" "$PROJECT_DIR/build/destinia.elf"
    echo "ELF con símbolos exportado a: $PROJECT_DIR/build/destinia.elf"
fi

echo ""
echo "[3/3] Instalación (opcional)"
VITA3K_APP="/Applications/Vita3K.app/Contents/MacOS/Vita3K"
if [ -x "$VITA3K_APP" ]; then
    read -p "¿Instalar y ejecutar en Vita3K ahora? [s/N] " INSTALL_VITA3K
    if [[ "$INSTALL_VITA3K" =~ ^[sS]$ ]]; then
        "$VITA3K_APP" -B OpenGL "$PROJECT_DIR/build/$VPK_NAME" > /dev/null 2>&1 &
        echo "Listo."
    fi
else
    echo "Vita3K no encontrado en la ruta por defecto."
fi
echo "Para instalar en un Vita real, usar manage_vita.py o 'make send'."
