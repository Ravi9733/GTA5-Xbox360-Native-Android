# GTA V Xbox 360 — Native Android Port (ARM64)

This repository is the Android-native development workspace for investigating a port of the Xbox 360 release of Grand Theft Auto V.

## Status

**Early foundation only — this is not a playable GTA V port and does not contain an APK release yet.**

The first commit establishes an Android ARM64 app + JNI/C++ build path. It is not an emulator, and it does not run the game yet. The starter screen and native library are not the game engine.

## Upstream research

The public SanRecomp project is an early static-recompilation project. Its public build files are desktop-oriented (CMake/vcpkg and platform-specific dependencies); Android/ARM64 is not currently a supported target in the checked-in build configuration. Porting it requires Android-specific work in the platform layer, renderer, audio, input, storage, and build/dependency setup. Upstream documentation should be rechecked before syncing code because README/build-guide descriptions may differ.

- Upstream: https://github.com/OZORDI/SanRecomp

## What is in this repository

- Android Gradle app targeting arm64-v8a
- Android NDK C++ shared-library target (JNI)
- GitHub Actions workflow that attempts to build a debug APK
- No proprietary game assets or executable files

## Next engineering gates

1. Make the native Android starter build reproducibly in CI.
2. Audit upstream source, license, actual GTA V code/data requirements, and dependencies.
3. Design and implement Android platform backends rather than adding an emulator.
4. Integrate compatible recompilation output only after the Android build path and required APIs are understood.
5. Test with game files the user legally owns. A successful starter APK is **not** proof that GTA V runs.

## Build locally

Open this repository in Android Studio with Android SDK and NDK installed, then build the app debug variant. CI is configured to run assembleDebug and upload the APK artifact if the build succeeds.

**Important:** No Xbox 360 game files are included. Never commit default.xex, xbox360.rpf, or other copyrighted game data to this public repository.