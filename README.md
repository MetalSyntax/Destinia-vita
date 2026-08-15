# DESTINIA — PS Vita Port

<p align="center">
  <img src="extras/livearea/pic0.png" width="700" alt="DESTINIA PS Vita Banner" />
</p>

<p align="center">
  <b>Port nativo del clásico RPG de acción DESTINIA (데스티니아) de Gamevil / Morisoft para PlayStation Vita y PlayStation TV.</b>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Platform-PS%20Vita%20%7C%20PS%20TV-003791.svg?style=flat-square&logo=playstation" alt="Platform PS Vita" />
  <img src="https://img.shields.io/badge/Title%20ID-DESTINIA1-ff69b4.svg?style=flat-square" alt="Title ID DESTINIA1" />
  <img src="https://img.shields.io/badge/Engine-Gamevil%20WIPI%20C--Engine-brightgreen.svg?style=flat-square" alt="Engine" />
  <img src="https://img.shields.io/badge/Renderer-vitaGL%20%28RGB565%20Quad%29-orange.svg?style=flat-square" alt="Renderer" />
</p>

---

## 📖 Descripción

**DESTINIA** es un aclamado action-RPG en 2D lanzado originalmente por Gamevil y Morisoft para dispositivos móviles. Este port ejecuta de forma nativa la librería compilada en C (`libdestinia_jni.so`) de la versión para Android en el procesador ARM Cortex-A9 de la PlayStation Vita a través de un cargador dinámico (*soloader*) y la emulación de entorno Android (*FalsoJNI*).

### ✨ Características del Port

- **Ejecución Nativa en CPU ARMv7**: Rendimiento suave a 60 FPS sin capas de emulación pesadas.
- **Pipeline Gráfico con vitaGL**: Renderizado del framebuffer nativo de 400x240 RGB565 mediante textura acelerada por hardware escalada a 960x544 preservando la relación de aspecto 5:3.
- **Controles Físicos Completos + Touch**: Mapeo integral de botones físicos de la consola (D-Pad, Stick Analógico, Botones de Acción, Gatillos L/R, Start, Select) y soporte total de la pantalla táctil frontal.
- **Motor de Audio Dedicado**: Mezclador multicanal nativo con hilo dedicado (`SceAudioOut`), decodificación OGG Vorbis (`libvorbisidec` / Tremor) para música de fondo (BGM) y efectos de sonido (SFX).
- **Gestión de Guardado en ux0**: Redirección transparente de llamadas I/O a `ux0:data/destinia/saves/`.
- **LiveArea Completo**: Iconos y pantallas de arranque adaptados en formato PNG 8-bits indexado (256 colores) bajo Title ID `DESTINIA1`.

---

## 🎮 Controles

| Botón PS Vita | Acción en Destinia |
| :--- | :--- |
| **D-Pad / Stick Analógico Izquierdo** | Movimiento del personaje / Navegación en menús |
| **Cruz ($\times$) / Círculo ($\bigcirc$)** | Ataque principal / Interactuar / Confirmar |
| **Cuadrado ($\square$)** | Habilidad activa 1 |
| **Triángulo ($\triangle$)** | Habilidad activa 2 |
| **Gatillo R1** | Habilidad activa 3 |
| **Gatillo L1** | Ranura de objeto rápido / Poción |
| **START** | Menú principal del juego |
| **SELECT** | Minimapa / Mapa de la zona |
| **Pantalla Táctil Frontal** | Control táctil completo directo (menús, combate, inventario) |

---

## 📋 Requisitos Previos

Para ejecutar el juego en tu PS Vita o PS TV necesitarás:

1. Una consola PS Vita / PS TV con Custom Firmware (**HENkaku** o **Enso**) en firmware 3.60 o 3.65+.
2. [**kubridge.skprx**](https://github.com/TheOfficialFloW/kubridge/releases) (v0.8.1 o superior) instalado en `ur0:tai/config.txt`.
3. [**libshacccg.suprx**](https://samilops2.gitbook.io/vita-troubleshooting-guide/shader-compiler/extract-libshacccg.suprx) instalado en `ur0:data/` (se puede extraer con la aplicación *ShaRKBR3ED*).
4. Archivo APK legítimo de **DESTINIA v1.0.6** (versión Android EN/KR, paquete `game.destiniaeng`).

---

## 📦 Instrucciones de Instalación

### Método Automático (Recomendado)

1. Instala el archivo `destinia.vpk` en tu consola usando **VitaShell**.
2. En tu PC, coloca el archivo APK (`destinia-1-0-6-en-kr-android.apk`) en la raíz del proyecto o extráelo en `destinia_extract/`.
3. Ejecuta el script de preparación:
   ```bash
   ./porting_tools/prepare_data_files.sh
   ```
4. Transfiere la carpeta generada `ux0_data/destinia/` hacia `ux0:data/destinia/` en tu consola PS Vita mediante FTP o cable USB con VitaShell (o usando `python3 porting_tools/manage_vita.py`).

### Estructura Final de Archivos en `ux0:data/destinia/`

```text
ux0:data/destinia/
├── libdestinia_jni.so      <- Librería nativa extraída de lib/armeabi/
├── assets/                 <- Archivos de datos del juego (.wmb, .agd, .wpn, .tdt, etc.)
├── sound/                  <- Música y efectos de sonido en formato OGG
├── saves/                  <- Partidas guardadas (creado automáticamente)
└── logs/                   <- Registros de depuración
```

---

## 🛠️ Compilación desde el Código Fuente

El proyecto utiliza el toolchain oficial **VitaSDK** (modo `softfp`) y CMake.

### Prerrequisitos de Compilación

- **VitaSDK** configurado en tu entorno (`$VITASDK` en el `PATH`).
- Librerías VitaSDK: `vitaGL`, `vitashark`, `mathneon`, `vorbisidec`, `pthread`.
- CMake (>= 3.14) y Make / Ninja.

### Pasos de Compilación

```bash
# 1. Crear y entrar en el directorio de compilación
mkdir -p build && cd build

# 2. Configurar el proyecto con el toolchain de PS Vita
cmake ..

# 3. Compilar el ejecutable ELF y empaquetar el VPK
make -j$(nproc)
```

Esto generará `build/destinia.vpk` listo para transferir e instalar en la consola.

---

## 🏗️ Estructura del Proyecto

- `source/`: Código fuente del cargador nativo en C/C++ (ciclo de vida, renderizado GL, I/O, audio, input, bindings JNI).
- `lib/`: Librerías auxiliares (`falso_jni`, `so_util`, `libc_bridge`, `fios`, `sha1`, `kubridge`).
- `extras/`: Recursos del LiveArea (`icon0.png`, `bg0.png`, `pic0.png`, `startup.png`, `template.xml`), `cpuinfo`, `meminfo`.
- `porting_tools/`: Herramientas de automatización, preparación de assets (`prepare_data_files.sh`) y scripts de despliegue (`manage_vita.py`).

---

## ⚖️ Aviso Legal (Disclaimer)

Este repositorio contiene **únicamente** el código fuente del cargador libre de código propietario (SoLoader) y herramientas de adaptación. **No contiene assets con derechos de autor, datos del juego, música ni binarios propietarios de DESTINIA.** Para jugar es indispensable poseer una copia legítima del juego original para Android.

---

## 👥 Créditos y Agradecimientos

- **Gamevil** & **Morisoft**: Desarrolladores originales de DESTINIA.
- **TheFloW**: Por `so_util`, `kubridge` y las técnicas fundamentales de carga dinámica de ejecutables Android en PS Vita.
- **Rinnegatamante**: Por `vitaGL` y soporte continuo a la escena de ports de PS Vita.
- **v-atamanenko**: Por `FalsoJNI` y la plantilla base `soloader-boilerplate`.
- **Comunidad Vita**: A todos los desarrolladores y entusiastas del homebrew de PS Vita.

