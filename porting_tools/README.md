# Herramientas de Porting Android → PS Vita (Destinia)

Herramientas adaptadas para el proyecto so-loader (VitaSDK + FalsoJNI) de **Destinia** en PS Vita.

## Build y despliegue

- **`build/build_and_install.sh`** — build completo vía staging en `/tmp` (workaround para rutas con espacios) y generación de VPK y ELF con CMake/Make directo.
- **`manage_vita.py`** — herramienta interactiva de despliegue y debug por FTP/WiFi (VitaShell):
  - Subida de `.vpk` o únicamente `eboot.bin` a `ux0:app/DESTINIA1/`.
  - Descarga del `.psp2dmp` (core dump) y logs más recientes de `ux0:data/destinia/logs/`.
  - Análisis automático de crash dumps con `parse_dump.py`.
  - Sincronización y verificación de assets (`ux0:data/destinia/`).
- **`prepare_data_files.sh`** — empaqueta y prepara todos los archivos de datos para la consola PS Vita física.
- **`misc/get_dump.sh`** — descarga el `.psp2dmp` más reciente de la consola por FTP. Uso: `./get_dump.sh <IP-de-la-vita>`.

## Análisis y Debugging de Crashes

- **`parse_dump.py`** — analizador avanzado de volcados de memoria `.psp2dmp` integrando `vita-parse-core`, símbolos ELF y símbolos exportados del `.so` (Ghidra/objdump).
- **`ai_bash_commands.sh`** — plantilla de comandos bash para desensamblado con `arm-vita-eabi-objdump`/`readelf` y búsqueda de símbolos en `libdestinia_jni.so`. A diferencia de otros ports, todavía no tiene offsets hardcodeados -- hay que encontrarlos con Ghidra o un `.psp2dmp` real (ver skill `so-crash-triage`) antes de usar `disasm_thumb()`.

## Decompilación

- **`build/decompile_all.sh`** — decompila el APK con JADX (`classes.dex` → Java) y `libdestinia_jni.so` (ABI `armeabi`) → pseudo-C usando `devrvk/so-decompiler` (Ghidra & Angr).

## Utilidades varias

- **`build/clean_macos.sh`** — elimina archivos residuales `._*` de macOS.
- **`misc/translate_docs.py`** — traducción de documentación Markdown por lotes.
