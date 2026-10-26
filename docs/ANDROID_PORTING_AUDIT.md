# Android ARM64 porting audit

**Current state:** the repository builds a small Android/JNI foundation only. It is not a playable game, and no GTA V executable or game assets are included.

## Upstream examined

Upstream project: https://github.com/OZORDI/SanRecomp

The upstream README identifies this as a static recompilation project for the Xbox 360 release of GTA V and says it is early development. Its source contains generated PowerPC C++ files and a GTA5 configuration, but the checked-in application build is desktop-oriented.

The upstream top-level CMake configuration:
- requires `VCPKG_ROOT`;
- builds a desktop application and third-party dependency tree;
- uses platform-specific graphics and desktop UI dependencies;
- on Linux explicitly requires X11, GLib and GIO;
- links SDL, SDL_mixer, CURL, FFmpeg and other third-party libraries;
- has shader generation paths that are not yet suitable as a drop-in Android Vulkan pipeline.

The upstream build guide asks for the user's own legally acquired game files, including `default.xex` and `xbox360.rpf`, in `SanRecompLib/private/`. These must never be committed to this public repository.

## Android blockers — not solved by compiling a wrapper

1. **Build system:** replace desktop-only CMake assumptions and cross-compile compatible third-party dependencies with Android NDK/arm64-v8a.
2. **Platform/window/input:** add an Android activity/window/surface path, touch and controller input, lifecycle handling, pause/resume, and storage permissions.
3. **Rendering:** bring up a Vulkan Android surface and adapt the renderer to Android; ensure shader tooling and shader caches can target the device.
4. **Audio:** replace or configure platform-specific audio dependencies for Android.
5. **Filesystem:** implement access to legally owned game files using Android's storage model, without assuming desktop paths.
6. **Game-specific correctness:** verify the generated PPC code, imports, memory layout, threading, GPU synchronization, save data and game patches on ARM64.
7. **Testing:** build and launch on an actual Android device with user-supplied game files. A successful debug APK is only a foundation test, not proof the game runs.

## Work completed in this repository

- Android Gradle app targeting `arm64-v8a`.
- JNI/C++ shared-library target.
- CI workflow for a debug APK.
- CI setup fix: the Android setup action was trying to install the removed SDK package named `tools`; the workflow now requests `platform-tools` only before installing explicit SDK/NDK packages.

## Next order of work

1. Get the foundation CI build green.
2. Create a separate Android-native platform target and resolve NDK dependency builds without breaking desktop upstream.
3. Add Android Vulkan surface/lifecycle and input/audio adapters.
4. Integrate upstream source in a license-compliant way, preserving upstream attribution and isolating Android patches.
5. Test against a legally owned game dump; only then describe any game functionality as working.

No emulator is part of this plan. No APK should be advertised as a playable GTA V port until that has actually been demonstrated.
