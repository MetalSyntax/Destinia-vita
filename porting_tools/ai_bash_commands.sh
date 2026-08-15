#!/bin/bash
# ==============================================================================
# Script de inspección y desensamblado (Comandos Bash utilizados por IA / Debug)
#
# Plantilla genérica para Destinia -- a diferencia de otros ports, ACÁ NO hay
# direcciones/offsets hardcodeados todavía: los de Immortal Dusk (de donde se
# adaptó este script) son específicos de SU binario compilado y no significan
# nada en libdestinia_jni.so. Encontrar los propios con Ghidra
# (decompiled/libdestinia_jni_armeabi/ghidra/) o `nm`/`objdump -T` antes de
# armar disasm_thumb() con offsets reales, siguiendo so-crash-triage.
# ==============================================================================

set -e

# Configuración de variables con valores por defecto
VITASDK_PATH="${VITASDK:-/Users/metalsyntax/vitasdk}"
export PATH="$VITASDK_PATH/bin:$PATH"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

SO_PATH="${1:-$PROJECT_ROOT/destinia_extract/lib/armeabi/libdestinia_jni.so}"
if [ ! -f "$SO_PATH" ]; then
    SO_PATH=$(find "$PROJECT_ROOT" -name "libdestinia_jni.so" ! -name "._*" | head -n 1)
fi
PATCH_C_PATH="$PROJECT_ROOT/source/patch.c"
DYNLIB_C_PATH="$PROJECT_ROOT/source/dynlib.c"
MAIN_C_PATH="$PROJECT_ROOT/source/main.c"

echo "=============================================================================="
echo "          EJECUTANDO COMANDOS BASH DE ANÁLISIS Y DESENSAMBLADO                "
echo "=============================================================================="
echo " SO Target: $SO_PATH"
echo " VitaSDK:   $VITASDK_PATH"
echo "=============================================================================="

if [ ! -f "$SO_PATH" ]; then
    echo "[-] Error: No se encontró el archivo .so en: $SO_PATH"
    exit 1
fi

disasm_thumb() {
    local start=$1
    local stop=$2
    local label=$3
    echo ""
    echo "[*] Disassembly ($label): $start -> $stop"
    arm-vita-eabi-objdump -d -M force-thumb --start-address="$start" --stop-address="$stop" "$SO_PATH"
}

# 1. Exports JNI (convención Java_*) -- punto de partida real para este .so.
echo ""
echo "[*] Exports JNI (Java_*) en $SO_PATH:"
arm-vita-eabi-readelf -W --dyn-syms "$SO_PATH" | grep "Java_" || true

# 2. Imports sin resolver (lo que el .so espera que el loader le provea).
echo ""
echo "[*] Imports (UND) en $SO_PATH:"
arm-vita-eabi-readelf -W --dyn-syms "$SO_PATH" | grep "UND" || true

# 3. Símbolos habituales en motores de esta época (audio/imagen) -- ajustar
#    la búsqueda según lo que aparezca en el pseudo-C de Ghidra.
for pattern in "Decode" "png_" "ov_" "mp3" "Sound" "Audio"; do
    echo ""
    echo "[*] Símbolos que matchean '$pattern':"
    arm-vita-eabi-readelf -W --dyn-syms "$SO_PATH" | grep "$pattern" || true
done

# 4. Ejemplo de uso de disasm_thumb() una vez que se tengan offsets reales
#    (de Ghidra o de un .psp2dmp con so-crash-triage):
#   disasm_thumb "0x000XXXXX" "0x000YYYYY" "Descripción de la función"

# 5. Verificación de parches e imports en el código fuente del loader
if [ -f "$PATCH_C_PATH" ]; then
    echo ""
    echo "[*] Ocurrencias de 'png_create_struct_2' en patch.c:"
    grep "png_create_struct_2" -B 2 -A 2 "$PATCH_C_PATH" || true
fi

if [ -f "$DYNLIB_C_PATH" ]; then
    echo ""
    echo "[*] Ocurrencias de 'malloc' en dynlib.c:"
    grep -i "malloc" -B 2 -A 2 "$DYNLIB_C_PATH" || true
fi

if [ -f "$MAIN_C_PATH" ]; then
    echo ""
    echo "[*] Configuración de sceLibcHeapSize en main.c:"
    grep "sceLibcHeapSize" "$MAIN_C_PATH" || true
fi

echo ""
echo "[+] Todos los comandos de inspección ejecutados exitosamente."
