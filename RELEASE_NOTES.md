# DESTINIA (PS Vita / PSTV) — Release v1.0.0

Native ARM loader and port of Gamevil's classic Action-RPG **DESTINIA** (데스티니아) for PlayStation Vita and PlayStation TV.

---

## 🌟 Features

- **🎮 Full Physical Controls:** Seamless control mapping for D-Pad, Left Analog Stick, Face Buttons, Triggers, and Menu navigation.
- **🎨 Hardware Visual Enhancements & Filters:**
  - **Sharp Pixel-Art 2x (Default):** Crisp, pixel-perfect rendering without distortion or shimmering.
  - **Smooth Bilinear 2x:** Smooth sub-pixel blending for players who prefer filtered visuals.
  - **Vibrant OLED / LCD Mode:** Brightness, contrast, and saturation boost (+15%) tailored for both PS Vita 1000 (OLED) and 2000 (LCD) screens.
  - **Aspect Ratio Selector:** Native 5:3 Fit (`906x544`), Integer 2x Center (`800x480`), and Fullscreen 16:9 (`960x544`).
  - **On-the-Fly Hotkeys:** Cycle filters (`L1 + R1 + △`) and aspect ratios (`L1 + R1 + ▢`) during gameplay with automatic config saving.
- **🔊 High-Fidelity Audio:** Multi-channel Vorbis (`Tremor`) audio engine supporting looping background music (`BGM`) and simultaneous sound effects (`SFX`).
- **⚡ Rock-Solid 60 FPS Native Execution:** Direct execution on ARM Cortex-A9 CPU with GPU hardware-accelerated rasterization (via `vitaGL`).
- **🛡️ Anti-DMCA Compliant:** Clean data extraction scripts to extract assets from your legally acquired Android APK.

---

## 🎮 Controls

| Action | PS Vita Input |
|---|---|
| **Movement / Direction** | **D-Pad** or **Left Analog Stick** |
| **Attack / Confirm** | **Cross (✕)** or **Circle (○)** |
| **Skill Slot 1** | **Square (□)** |
| **Skill Slot 2** | **Triangle (△)** |
| **Skill Slot 3** | **R1 Trigger** |
| **Quick Potion / Slot 1** | **L1 Trigger** |
| **Main Menu / Back** | **Start** |
| **Status / Map** | **Select** |
| **Cycle Graphics Filter** | **L1 + R1 + Triangle (△)** |
| **Cycle Aspect Ratio** | **L1 + R1 + Square (□)** |
| **Touch Controls** | **Front Touch Screen** |

---

## 📥 Installation Instructions

### Prerequisites
1. PlayStation Vita with Henkaku / Enso CFW.
2. [**kubridge.skprx**](https://github.com/TheOfficialFloW/FdFmx/releases) (v0.2 or newer) installed in `ur0:tai/config.txt`.
3. [**libshacccg.suprx**](https://github.com/SKGleba/ShaRKBR33D) extracted.

### Setup Steps
1. Install **`destinia_enhanced.vpk`** (or `destinia.vpk`) using **VitaShell**.
2. Obtain the Android APK file for **DESTINIA v1.0.6** (Package: `game.destiniaeng`).
3. Extract the assets using the included PC script:
   ```bash
   ./porting_tools/prepare_data_files.sh /path/to/destinia-1-0-6.apk
   ```
   Or manually place the game files into:
   - `ux0:data/destinia/libdestinia_jni.so`
   - `ux0:data/destinia/assets/`
   - `ux0:data/destinia/sound/`
4. Launch **DESTINIA** from your LiveArea!

---

## 🛠️ Technical Changelog

- **FalsoJNI Heap Architecture Refactoring:** Fixed pointer invalidation on internal dynamic array reallocations (`JavaDynArray**`), preventing Data Abort crashes during asset loading and combat events.
- **Memory Optimization:** Added automatic buffer recycling for resource loaders (`Java_getAssetRes`), eliminating memory leaks during map transitions.
- **Crash-Proof Hardware Pipeline:** Switched from unstable runtime shader translation to a dedicated hardware-accelerated rendering pipeline in `vitaGL`.
- **Automatic Cache Maintenance:** Added early purging for corrupted `.gxp` cache files.

---

## 👏 Credits & Acknowledgements

- **Gamevil / Morisoft** for creating DESTINIA.
- **Rinnegatamante** for [vitaGL](https://github.com/Rinnegatamante/vitaGL).
- **TheFloW** for [kubridge](https://github.com/TheOfficialFloW/FdFmx) and soloader foundation.
- **Volodymyr Atamanenko** and **Andy Nguyen** for [FalsoJNI](https://github.com/v-atamanenko/falsojni).
