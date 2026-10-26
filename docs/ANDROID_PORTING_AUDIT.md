# Android ARM64 porting audit

**Current state:** the repository has an Android/JNI foundation with a Vulkan swapchain and clear/present loop, plus a persistent Android folder picker that checks for game-file names. It is not a playable game, and no GTA V executable or game assets are included.

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
2. **Platform/window/input:** an Android activity/window/surface path, Vulkan surface lifecycle handling, pause/resume, and persistent read-only Storage Access Framework folder selection now exist. Touch/controller input and connecting the selected document tree to the game file APIs remain unfinished.
3. **Rendering:** Android Vulkan swapchain, command buffers, render pass, frame synchronization, clear/present, and out-of-date swapchain recreation are implemented as a smoke-test renderer. The GTA V renderer, Xenos shader conversion, shader caches, and actual game graphics are not integrated.
4. **Audio:** replace or configure platform-specific audio dependencies for Android.
5. **Filesystem:** a persistent folder picker scans for `default.xex` and `xbox360.rpf` names. It does not yet parse, mount, or feed those files to SanRecomp; complete game filesystem integration remains unfinished.
6. **Game-specific correctness:** verify the generated PPC code, imports, memory layout, threading, GPU synchronization, save data and game patches on ARM64.
7. **Testing:** build and launch on an actual Android device with user-supplied game files. A successful debug APK is only a foundation test, not proof the game runs.

## Work completed in this repository

- Android Gradle app targeting `arm64-v8a`.
- JNI/C++ shared-library target.
- Vulkan instance creation and Android `Surface` creation.
- Vulkan physical-device discovery, graphics + presentation queue selection, logical-device creation, and surface format/present-mode capability queries.
- CI workflow successfully built and uploaded a debug APK artifact for the Vulkan surface probe.
- CI setup fix: the Android setup action was trying to install the removed SDK package named `tools`; the workflow now requests `platform-tools` only before installing explicit SDK/NDK packages.

## Next order of work

1. Keep the ARM64 foundation build green in CI.
2. Add a Vulkan swapchain, frame synchronization, and a minimal clear/present loop on the Android surface.
3. Create a separate Android-native platform target and resolve NDK dependency builds without breaking desktop upstream.
4. Add Android lifecycle, touch/controller input, audio, and game-file access adapters.
5. Integrate upstream recompilation code in a license-compliant way, preserving attribution and isolating Android patches.
6. Test against a legally owned game dump; only then describe any game functionality as working.

No emulator is part of this plan. No APK should be advertised as a playable GTA V port until that has actually been demonstrated.
