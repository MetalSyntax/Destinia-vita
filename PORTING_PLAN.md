# Plan de Port — DESTINIA (PS Vita)

> Documento de Arquitectura, Análisis Estático Verificado de `libdestinia_jni.so`, `decompiled/apk_jadx/`, Mapeo de Símbolos JNI y Hoja de Ruta para el port de **DESTINIA** (Android) a **PS Vita**.

---

## 0. Contexto y Estrategia General

* **Juego:** DESTINIA (데스티니아)
* **Desarrollador / Publisher:** Gamevil / Morisoft
* **Paquete Java:** `game.destiniaeng`
* **Activity Principal:** `game.destiniaeng.Destinia`
* **Librerías Nativas:**
  * `libdestinia_jni.so` (284 KB, arquitectura ARM 32-bit `armeabi`) — Motor principal 2D C/WIPI y software rasterizer
* **TITLEID asignado:** `DESTINIA1`
* **Ruta de Datos en PS Vita:** `ux0:data/destinia/`
* **Ruta de la Librería:** `ux0:data/destinia/libdestinia_jni.so`
* **Ruta de Assets:** `ux0:data/destinia/assets/`
* **Ruta de Sonido:** `ux0:data/destinia/sound/`
* **Ruta de Guardado:** `ux0:data/destinia/saves/`

### Arquitectura del Motor (Gamevil / WIPI C-Engine)
1. **Motor 2D en C:** DESTINIA fue desarrollado sobre el motor clásico C de Gamevil/Morisoft para plataformas móviles coreanas (WIPI), posteriormente encapsulado en Android mediante JNI.
2. **Buffer Gráfico Software:** El motor dibuja por software en un buffer lineal de **400 x 240 en formato RGB565** (192,000 bytes).
3. **Ciclo de Vida JNI:**
   * `Java_game_destiniaeng_GameThread_initGame`: Inicializa el motor pasando el identificador de dispositivo, modelo, resolución (400x240) e idioma (`isKorean`).
   * `Java_game_destiniaeng_GameThread_jniRun`: Ejecuta un tick del motor, procesa la lógica del juego, escribe el frame completo de 400x240 RGB565 en el buffer y retorna el tiempo de espera en milisegundos (`sleepTime`).
   * `Java_game_destiniaeng_GameThread_jniTouch`: Envía eventos de entrada (coordenadas x, y dentro de 400x240 y acción: 0=Down, 1=Up, 2=Move, 3=Menu).
   * `Java_game_destiniaeng_GameThread_destroyGame`: Libera recursos al salir.
4. **Carga de Assets y Guardado:**
   * Los assets binarios (`.wmb`, `.agd`, `.wpn`, `.tdt`, `.sdt`, `.gdt`) se cargan mediante callbacks JNI `getAssetResSize` y `getAssetRes`.
   * Los archivos de guardado se gestionan directamente en C con llamadas libc (`fopen`, `remove`, etc.) bajo la ruta `/data/data/game.destiniaeng/files/`, la cual es interceptada y redirigida a `ux0:data/destinia/saves/`.

---

## 1. Detección de Arquitectura y Gráficos

* **Arquitectura Binaria:** ARMv5TE / ARMv6 (`armeabi`), código ARM/Thumb. Totalmente compatible de forma nativa con el CPU Cortex-A9 de la PS Vita en modo `softfp`.
* **Gráficos:** El motor nativo genera el framebuffer 400x240 RGB565 por software. Se carga como textura dinámica OpenGL ES vía `vitaGL` (`glTexImage2D` / `glTexSubImage2D`) y se dibuja como un quad escalado a 960x544 (con aspect ratio correcto o pantalla completa).
* **Resolución PS Vita:** 960 x 544 (con escalado lineal o punto a punto 2.266x).

---

## 2. Catálogo de Símbolos JNI

### A. Funciones Nativas Exportadas (`libdestinia_jni.so`)

| Símbolo Exportado | Firma JNI / Descripción |
|---|---|
| `Java_game_destiniaeng_GameThread_initGame` | `([BI[BIIIB)V` — Inicializa el motor (UniqueID, PhoneModel, Width, Height, IsKorean) |
| `Java_game_destiniaeng_GameThread_jniRun` | `([BI)I` — Renderiza 1 frame al buffer RGB565 (192,000 bytes) y devuelve `sleepTime` en ms |
| `Java_game_destiniaeng_GameThread_jniTouch` | `(III)V` — Envía evento táctil (x: 0-400, y: 0-240, acción: 0=Down, 1=Up, 2=Move, 3=Menu) |
| `Java_game_destiniaeng_GameThread_destroyGame` | `()V` — Limpia recursos nativos y finaliza el motor |
| `Java_game_destiniaeng_GameThread_bannerShow` | `()I` — Consulta estado de banners publicitarios (devuelve 0) |
| `Java_game_destiniaeng_GameThread_setEarnedCoin` | `(I)V` — Otorga monedas/recompensas |
| `Java_game_destiniaeng_GameThread_setGamePlaying` | `(B)V` — Actualiza estado de juego activo |
| `Java_game_destiniaeng_GameThread_setUniqueID` | `([BI)V` — Envía Unique ID al motor |
| `Java_game_destiniaeng_GameThread_procCommMsg` | `(II)I` — Procesa mensajes de comunicación de red |
| `Java_game_destiniaeng_GameThread_procCommRead` | `([B)I` — Procesa buffer de lectura de red |
| `Java_game_destiniaeng_CommManager_commGetConType` | `()I` — Consulta tipo de conexión de red (0 = desconectado) |
| `Java_game_destiniaeng_CommManager_commCheck` | `([B)I` — Verificación de conectividad |

### B. Métodos Java Invocados por el Motor (`jNativeGameCls`)

| Método Java | Firma JNI | Implementación en FalsoJNI / Port |
|---|---|---|
| `playSound` | `(I)V` | Reproducción de SFX (`soundIdx < 100`) o BGM (`soundIdx >= 100`) vía mezclador nativo |
| `setSoundVolume` | `(III)V` | Control de volumen de BGM y SFX (tipo, volumen actual, volumen máximo) |
| `playVib` | `(I)V` | Vibración de la consola (`sceCtrlSetActuator` o stub) |
| `getAssetResSize` | `([B)I` | Consulta tamaño en bytes de un archivo en `ux0:data/destinia/assets/<nombre>` |
| `getAssetRes` | `([B)[B` | Lee y retorna array de bytes del asset desde `ux0:data/destinia/assets/<nombre>` |
| `gotoURL` | `([B)V` | Apertura de URLs externas (stub) |
| `logEvent` | `([B)V` | Registro de eventos / telemetría Flurry (stub / log) |

---

## 3. Guía de Decompilación Reproducible

```bash
# 1. Decompilación Java con jadx
jadx -d "decompiled/apk_jadx" "destinia-1-0-6-en-kr-android.apk"

# 2. Decompilar .so con Ghidra y Angr (Docker devrvk/so-decompiler)
docker run --rm --platform linux/amd64 -v "/Volumes/Seagate/PSVITA Develop:/app" devrvk/so-decompiler \
  decompile "/app/Destinia-vita/destinia_extract/lib/armeabi/libdestinia_jni.so" \
  "/app/Destinia-vita/decompiled/libdestinia_jni_armeabi/ghidra"
```

Re-ejecutable en cualquier momento con `porting_tools/build/decompile_all.sh`.

---

## 4. Fases del Port (Checklist)

- [x] **Fase 0: Preparación del Entorno**
  - [x] Repositorio configurado, `.gitignore` anti-DMCA.
  - [x] `porting_tools/` configurado (`manage_vita.py`, `build_and_install.sh`, `prepare_data_files.sh`).
  - [x] Submódulo / librerías `falso_jni`, `so_util`, `libc_bridge`, `fios`, `kubridge`, `sha1`.
  - [x] Decompilación completa: Java (`decompiled/apk_jadx/`) y Ghidra C (`decompiled/libdestinia_jni_armeabi/ghidra/`).

- [x] **Fase 1: Configuración del Loader y Mapeo de Símbolos**
  - [x] Adaptar `source/dynlib.c` con la tabla completa de imports requeridos por `libdestinia_jni.so`.
  - [x] Redirección de I/O en `source/reimpl/io.c` (`/data/data/game.destiniaeng/files/` -> `ux0:data/destinia/saves/`).
  - [x] Implementar carga segura de `libdestinia_jni.so` en `source/main.c`.
  - [x] Compilación exitosa del ejecutable `build/destinia.elf` y empaquetado inicial `build/destinia.vpk`.

- [x] **Fase 2: Implementación de la Tabla JNI (FalsoJNI)**
  - [x] Mapear `jNativeGameCls` y registrar los métodos `getAssetResSize`, `getAssetRes`, `playSound`, `setSoundVolume`, `playVib`, `gotoURL`, `logEvent`.
  - [x] Implementar `getAssetResSize` y `getAssetRes` leyendo directamente desde `ux0:data/destinia/assets/` con retornos de `jbyteArray` en Dalvik/FalsoJNI.
  - [x] Llamar a `Java_game_destiniaeng_GameThread_initGame` con parámetros calibrados (UniqueID, PhoneModel, 400, 240, `isKorean = 0`).

- [x] **Fase 3: Pipeline de Renderizado Gráfico (vitaGL)**
  - [x] Configuración de ventana y contexto `vitaGL` a 960x544.
  - [x] Creación de textura GL `GL_RGB` / `GL_UNSIGNED_SHORT_5_6_5` de 400x240.
  - [x] Bucle principal con `jniRun` alimentando el buffer de 192,000 bytes.
  - [x] Actualización de textura con `glTexSubImage2D` y dibujo de quad texturizado con aspect ratio 5:3 centrado en pantalla.
  - [x] Sincronización y limitador de framerate respetando `sleepTime`.

- [x] **Fase 4: Input Táctil y Botones Físicos**
  - [x] Soporte para pantalla táctil frontal mapeada proporcionalmente a 400x240.
  - [x] Mapeo de botones físicos de la PS Vita a acciones del juego:
    - D-Pad / Stick Analógico Izquierdo -> Pad direccional virtual.
    - Botón Cruz (X) / Círculo (O) -> Botón de ataque / Confirmar.
    - Botón Cuadrado (□) / Triángulo (Δ) / R1 -> Botones de habilidad / Items rápidos.
    - Botón L1 -> Slot rápido de poción.
    - Start -> Menú principal / `jniTouch(0, 0, 3)`.
    - Select -> Estado / Mapa.

- [x] **Fase 5: Sistema de Audio**
  - [x] Mezclador de audio nativo para PS Vita (`SceAudioOut` con hilo dedicado).
  - [x] Reproductor de BGM en loop con `Tremor` (`libvorbisidec`) para `bg100.ogg` .. `bg107.ogg`.
  - [x] 4 canales de efectos de sonido SFX simultáneos (`eff00.ogg` .. `eff25.ogg`, `eff50.ogg`).
  - [x] Enlace con `playSound(soundIdx)` y `setSoundVolume(soundType, cur, max)`.

- [x] **Fase 6: LiveArea, Empaquetado VPK y Preparación de Datos**
  - [x] Assets LiveArea generados y adaptados a 8 bits indexados (`icon0.png`, `bg0.png`, `pic0.png`, `startup.png`, `template.xml`).
  - [x] `porting_tools/prepare_data_files.sh` con conversión de audio y empaquetado de datos a `ux0_data/destinia/`.
  - [x] Build automatizado completo (`build/destinia.vpk`, `build/destinia.elf`, `build/destinia_enhanced.vpk`).

- [x] **Fase 7: Mejoras Gráficas por Hardware y Triage de Estabilidad**
  - [x] **Resolución de Crash #1 (Data Abort en `jniRun` por `realloc` de FalsoJNI):**
    - Se refactorizó `javaDynArrays` a `JavaDynArray**` en `FalsoJNI_ImplBridge.c`. Cada array dinámico se reserva individualmente en el heap, evitando que un redimensionamiento de la tabla invalide punteros cacheados (`j_buffer`).
    - Se implementó reciclaje automático de buffers en `Java_getAssetRes` para prevenir fugas de memoria.
  - [x] **Resolución de Crash #2 (Data Abort en `serialize_shader` / `unserialize_shader` de vitaGL):**
    - Se migró el pipeline de escalado a un motor fijo acelerado por GPU en `graphics_enhancer.c`, eliminando la inestabilidad de shaders dinámicos en caliente.
    - Se añadió purga automática de binarios `.gxp` corruptos de `ux0:data/shader_cache/DESTINIA1/` en `init.c`.
  - [x] **Filtros Gráficos Seleccionables en Tiempo Real (`L1 + R1 + △`):**
    - Píxel-Art Nítido 2x (`GL_NEAREST`).
    - Suavizado Bilineal 2x (`GL_LINEAR`).
    - Vibrancia OLED / LCD Nítido (Realce de brillo y saturación +15%).
    - Vibrancia OLED / LCD Suave.
  - [x] **Relaciones de Aspecto Dinámicas (`L1 + R1 + ▢`):**
    - 5:3 Fit (906 $\times$ 544) ocupando toda la altura sin deformación.
    - Integer 2x (800 $\times$ 480) centrado píxel-perfecto.
    - Pantalla Completa 16:9 (960 $\times$ 544).
  - [x] Persistencia automática de configuración en `ux0:data/destinia/graphics_config.ini`.

---

## 5. Historial de Correcciones y Triage Técnico

| ID | Síntoma / Error | Causa Raíz | Solución Aplicada |
|---|---|---|---|
| **#01** | `Data Abort (0x30004)` en `jniRun` durante el combate | `jda_extend()` reasignaba `javaDynArrays` con `realloc`, invalidando el puntero `j_buffer` (192 KB). `GetByteArrayElements` devolvía `NULL` y el motor escribía píxeles en dirección 0. | `javaDynArrays` convertido a tabla de punteros estables `JavaDynArray**`. Cada array conserva su dirección fija en memoria. |
| **#02** | `Data Abort` en `serialize_shader` (`0xe0003066`) | vitaGL intentaba guardar caché GXP en `ux0:data/shader_cache/DESTINIA1/` pero la carpeta no existía. `sceIoOpen` devolvía `ENOENT` y fallaba al escribir. | Creación automática de directorios de caché y transición a pipeline acelerado por hardware sin depender del traductor dinámico GLSL. |
| **#03** | `Data Abort` en `unserialize_shader` (`0xe000307a`) | vitaGL intentaba leer un `.gxp` corrupto de 0 bytes dejado por el cuelgue anterior. | Rutina de limpieza en `init.c` que elimina archivos `.gxp` corruptos al arrancar. |

