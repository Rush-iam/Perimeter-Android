# Perimeter for Android

![Perimeter Android](app/src/main/res/drawable-land-nodpi/launcher_background.jpg)

An unofficial fan port of the classic real-time strategy game **Perimeter** for Android.

This project brings Perimeter to modern Android devices. The APK contains the
Android port and engine, but does not include the original game's copyrighted
data files. A legally obtained copy of the game is required to play.

## About Perimeter

Perimeter is a real-time strategy game featuring deformable terrain,
terraforming, morphing units, energy networks, protective shields, and surreal
worlds.

## Current status

The Android port is in active development and should be considered an early
testing release. Bugs, crashes, visual glitches, device-specific issues, and
incomplete features are possible.

Current builds target 64-bit ARM Android devices (`arm64-v8a`) running Android
10 or newer.

## Download

Download the latest user-facing APK from the project's
[GitHub Releases](../../releases/latest) page.

## Requirements

- Android 10 or newer
- A 64-bit ARM Android device (`arm64-v8a`)
- A legally obtained copy of the original Perimeter game files
- Approximately 4.5 GB of storage for the game content, plus space for the APK

## Installation

The APK does not include the original Perimeter game content. You must obtain
the game from a legal source, such as the [Perimeter: Legate Edition Steam
release](https://store.steampowered.com/app/2530170/PERIMETER_Legate_Edition/).

1. Download the latest APK from [GitHub Releases](../../releases/latest).
2. Copy the entire game folder to your Android device, or copy only:

   ```text
   Perimeter.ini
   Mods/
   Resource/
   Scripts/
   ```

3. Install the APK on your Android device.
4. Launch **Perimeter**.
5. Choose the game folder containing `Perimeter.ini`.
6. Grant storage access when Android requests it.
7. Tap **Play**.

## Controls

The game runs in landscape orientation:

- Single-finger touch: left mouse button
- Two-finger tap: right mouse button
- Two-finger drag: move the camera
- Pinch: zoom in or out

## Graphics renderer

The app automatically selects the best supported renderer. You can change it
under **Options**:

- **DXVK 2.x / Vulkan 1.3** — preferred.
- **DXVK 1.x / Vulkan 1.1** — for older devices.
- **Sokol / GLES3** — fallback for devices without suitable Vulkan support.

## Reporting problems

Please report problems through [GitHub Issues](../../issues). Include:

- Device model
- Android version
- Selected renderer
- What happened and how to reproduce it
- Screenshots or video, if available

## Credits and license

The original Perimeter game was created by KD VISION. This Android port is
based on the [open-source Perimeter project](https://github.com/KD-lab-Open-Source/Perimeter).

The project source code is released under the GPLv3, except for third-party
libraries that retain their own licenses. See the [upstream license](Perimeter/LICENSE)
for details.

The original game content and assets are not included and remain the property
of their respective copyright holders.
