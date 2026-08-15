#!/bin/bash
# Script para decompilar APK y archivos .so
# Creado automáticamente como una skill de IA.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASE_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Buscar APK automáticamente
APK_FILE=$(find "$BASE_DIR" -maxdepth 1 -name "*.apk" ! -name "._*" | head -n 1)
if [ -z "$APK_FILE" ]; then
    APK_FILE="${BASE_DIR}/destinia-1-0-6-en-kr-android.apk"
fi

# Buscar directorio de librerías .so
if [ -d "${BASE_DIR}/destinia_extract/lib" ]; then
    LIB_DIR="${BASE_DIR}/destinia_extract/lib"
elif [ -d "${BASE_DIR}/lib" ]; then
    LIB_DIR="${BASE_DIR}/lib"
else
    LIB_DIR="${BASE_DIR}"
fi

DECOMPILED_DIR="${BASE_DIR}/decompiled"
APK_OUT_DIR="${DECOMPILED_DIR}/apk_jadx"

mkdir -p "$APK_OUT_DIR"

echo "Iniciando decompilación con JADX para: $(basename "$APK_FILE")..."
if command -v jadx >/dev/null 2>&1; then
  jadx -d "$APK_OUT_DIR" "$APK_FILE"
else
  rel_apk="${APK_FILE#$BASE_DIR/}"
  docker run --rm -v "${BASE_DIR}:/app" ubuntu:latest bash -c "
    cd /tmp
    apt-get update && apt-get install -y wget unzip default-jre
    wget -qO- https://github.com/skylot/jadx/releases/download/v1.4.7/jadx-1.4.7.zip > jadx.zip
    unzip -q jadx.zip -d jadx
    ./jadx/bin/jadx -d /app/decompiled/apk_jadx \"/app/$rel_apk\"
  "
fi
echo "JADX Finalizado. Resultados en ${APK_OUT_DIR}"

echo "Iniciando decompilación de .so con Ghidra y Angr (so-decompiler)..."
if [ ! -d "$LIB_DIR" ]; then
    echo "Directorio de librerías no encontrado en: $LIB_DIR"
else
    # so_out incluye el ABI (basename del directorio padre, ej. armeabi-v7a) porque
    # libnativeinterface.so existe con el mismo nombre en armeabi Y armeabi-v7a — sin el ABI
    # en el nombre de carpeta, la segunda decompilación pisaría los resultados de la primera.
    find "$LIB_DIR" -type f -name "*.so" | while read -r so_file; do
        so_name=$(basename "$so_file")
        abi=$(basename "$(dirname "$so_file")")
        so_out="${DECOMPILED_DIR}/${so_name%.so}_${abi}/ghidra"
        mkdir -p "$so_out"

        # Calcular la ruta relativa para el contenedor
        rel_so_path="${so_file#$BASE_DIR/}"
        rel_so_out="${so_out#$BASE_DIR/}"

        echo "Decompilando $so_name ($abi)..."
        docker run --rm --platform linux/amd64 -v "${BASE_DIR}:/app" devrvk/so-decompiler decompile "/app/$rel_so_path" "/app/$rel_so_out"
    done
fi

echo "Decompilación de archivos .so finalizada. Resultados en ${DECOMPILED_DIR}/<lib>_<abi>/ghidra/"
