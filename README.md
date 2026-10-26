# GTA V Xbox 360 — Native Android Port (ARM64)

This repository is the Android-native development workspace for investigating a port of the Xbox 360 release of Grand Theft Auto V.

## Status

**Early foundation only — not a playable GTA V port.** The app currently contains an Android ARM64/JNI foundation, a Vulkan swapchain with a clear/present frame loop, Android surface lifecycle handling, and a persistent Storage Access Framework folder picker for checking a user-owned game dump. It does not execute the GTA V game engine, and it does not include an APK release advertised as a playable game.

This is **not an emulator**. The purpose is to develop an Android-native platform target for static-recompiled game code.

## Upstream research

Upstream project: https://github.com/OZORDI/SanRecomp

SanRecomp's README identifies it as an early Xbox 360 GTA V static-recompilation project. Its generated PowerPC C++ code and GTA5 configuration are a potential starting point, but its checked-in application build is desktop-oriented and is not a drop-in Android target.

See [the Android porting audit](docs/ANDROID_PORTING_AUDIT.md) for concrete blockers and the planned order of work.

## Current repository contents

- Android Gradle app targeting `arm64-v8a`
- Android NDK C++ JNI shared library
- Vulkan loader, Android surface, swapchain, command buffers, render pass, frame synchronization and clear/present loop
- Surface resize/out-of-date recovery and Activity pause/resume handling
- Persistent Android folder picker and read-only scan for `default.xex` / `xbox360.rpf` names
- GitHub Actions workflow that builds a debug APK and uploads it as an artifact
- Porting audit for Android platform, renderer, audio, storage and game-code integration
- No proprietary game assets or executable files

## Build

Open the repository in Android Studio with Android SDK and NDK installed, or use the GitHub Actions workflow. The workflow builds `app/build/outputs/apk/debug/app-debug.apk` and uploads it as an artifact if successful.

[View build status and artifacts](https://github.com/Ravi9733/GTA5-Xbox360-Native-Android/actions)

## Next engineering gates

1. Keep the ARM64 native foundation building in CI.
2. Turn the Vulkan clear/present path into a renderer that can draw the game's actual graphics.
3. Build Android-compatible third-party dependencies and isolate desktop-only dependencies from the Android target.
4. Integrate upstream recompilation code in a license-compliant way, preserving attribution and keeping Android patches separate.
5. Add Android game-file access for a legally owned dump and verify required files without uploading them to this repository.
6. Connect the Android folder access layer to the game's file system and test with legally owned game files on a physical Android device.

A successful starter APK or Vulkan probe is **not** proof that GTA V runs. The game engine, renderer, audio, controls, file handling and game-specific fixes still need to be integrated and tested.

## Game files and copyright

No Xbox 360 game files are included. Do not commit `default.xex`, `xbox360.rpf`, extracted game archives, or other copyrighted game data to this public repository. Use only a game dump you are legally entitled to use.
