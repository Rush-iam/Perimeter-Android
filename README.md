# Perimeter Android

![Perimeter Android](app/src/main/res/drawable-land-nodpi/launcher_background.jpg)

An unofficial fan port of the classic real-time strategy game **Perimeter** for Android.

Perimeter is a real-time strategy game featuring deformable terrain,
terraforming, morphing units, energy networks, protective shields, and surreal
worlds.

## Current status

The Android port is in active development and should be considered an early
testing release. Bugs, crashes, visual glitches, device-specific issues, and
incomplete features are possible.

Please report problems through [GitHub Issues](../../issues).

## Requirements

- Android 10 or newer (64-bit ARM)
- 6+ GB RAM recommended; larger missions may crash on 4 GB devices
- 5 GB of storage
- A copy of the Perimeter game (only the
[Legate Edition](https://store.steampowered.com/app/2530170/PERIMETER_Legate_Edition/)
has been tested)

## Installation

1. Download the latest APK from [GitHub Releases](../../releases/latest).
2. Copy the Perimeter game folder to your Android device, or only:

   ```text
   Mods/
   Resource/
   Scripts/
   Perimeter.ini
   ```

3. Install the APK on your device.
4. Launch **Perimeter**.
5. Choose the game folder containing `Perimeter.ini`.
6. Grant storage access when Android requests it.
7. Tap **Play**.

## Controls

- Tap: left click
- Two-finger tap: right click
- Two-finger drag: move camera
- Two-finger hold, then drag: rotate camera
- Pinch: zoom camera
- (in menus) Two-finger drag: mouse scroll

Mouse and keyboard are also supported.
For mouse play, uncheck **Disable edge scrolling** in **Options**.

## Graphics renderer

The app autoselects the best supported renderer. You can change it in **Options**:

- **DXVK 2.x / Vulkan 1.3** — preferred.
- **DXVK 1.x / Vulkan 1.1** — for older devices.
- **Sokol / GLES3** — legacy for devices without Vulkan 1.1+ support.

## Credits and license

The original Perimeter game was created by KD VISION. This Android port is
based on the [open-source Perimeter project](https://github.com/KD-lab-Open-Source/Perimeter).

The project source code is released under the GPLv3, except for third-party
libraries that retain their own licenses. See the [upstream license](Perimeter/LICENSE)
for details.

The original game content and assets are not included and remain the property
of their respective copyright holders.
