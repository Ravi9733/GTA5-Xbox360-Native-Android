# GTA V Xbox 360 → Android native integration plan

This repository is an Android host/smoke-test, not a playable port. The goal is a native ARM64 Android application that runs the Xbox 360 GTA V static-recompilation project directly, without an emulator.

## Upstream compatibility finding

The current upstream [SanRecomp](https://github.com/OZORDI/SanRecomp) is a desktop-oriented static recompilation project. Its top-level CMake requires `VCPKG_ROOT` and builds the desktop app, tools, and third-party tree. Its own status lists rendering/Xenos shader conversion, RAGE structures, audio, save handling, and GTA-specific fixes as unfinished. Therefore, simply copying its executable into this Android project or wrapping it with JNI cannot produce gameplay.

Keep the upstream code and this Android host separate until a minimal Android target can compile. Do not commit proprietary game dumps or generated private game data.

## Required implementation stages

### A. Establish an Android build boundary
- Keep the existing Gradle/NDK application as the Android entry point.
- Add a dedicated Android CMake target rather than changing desktop defaults.
- Inventory upstream dependencies and mark each as: Android-compatible, needs an Android backend, or blocks the port.
- Build a small ARM64 shared library with the Android NDK in CI before importing the full game runtime.

**Acceptance:** clean GitHub Actions build for `arm64-v8a`, with no desktop-only headers/libraries accidentally linked.

### B. Real game-file access
- Keep using Android's Storage Access Framework so the user selects their own extracted game folder.
- Replace filename-only checks with a documented validation stage for required files and readable content.
- Provide a reusable seekable native file API (`NativeGameFile`) over Android document URIs: open, length, absolute-offset reads, and close. Do not assume a SAF URI is a normal POSIX path.
- Route the XEX/RPF header validation through this native API so CI-built app code exercises the JNI bridge during a user-selected folder scan.
- Next: adapt this bridge to SanRecomp's actual filesystem interface once its expected interface and runtime target are imported.
- Keep persistent URI permission and show clear errors for missing/unreadable files.

**Acceptance:** validation reads actual file bytes through the selected tree and reports missing files without claiming the game has been loaded.

### C. Recompilation/runtime integration
- Identify the exact upstream entry point, generated-code registration, import tables, memory assumptions, and initialization sequence.
- Port platform services behind narrow interfaces: file access, threading/synchronization, clock/timing, logging, input, audio, and save data.
- Preserve upstream attribution and license obligations; never check in the user's game files.

**Acceptance:** a minimal upstream runtime target links for Android ARM64, with missing platform services explicitly stubbed or implemented—not silently bypassed.

### D. Graphics and audio
- The existing Vulkan code only clears and presents the Android surface. It is not a GTA renderer.
- Determine the upstream graphics API contract before selecting a renderer path. Xenos shader conversion and GTA/RAGE rendering must be implemented or integrated; a swapchain alone is insufficient.
- Port audio dependencies to Android and validate timing/latency with the game loop.

**Acceptance:** runtime-generated frame output and audio are demonstrated from the game, not a clear-color test screen.

### E. Input, lifecycle, and saves
- Map touch controls and Android game controllers to the GTA control model.
- Suspend/resume the game loop safely with Activity lifecycle changes and recreate graphics resources after surface loss.
- Keep save data in user-accessible app-specific storage with clear import/export behavior.

**Acceptance:** can enter gameplay, control the character/vehicle, pause/resume, and retain a test save across relaunches.

### F. Device validation
- Use a legally acquired Xbox 360 game dump supplied locally by the user.
- Test on physical ARM64 Android hardware; record startup logs, crashes, frame pacing, audio behavior, and known limitations.
- Do not label a build “playable” until actual in-game control and rendering have been verified.

## Current verified baseline

- Android Activity/JNI host and ARM64 NDK build.
- Vulkan Android surface, swapchain, clear/present loop, and lifecycle recreation.
- SAF folder selection and filename scanning only.
- GitHub Actions debug APK build succeeds.

## Current blockers

- No Android-specific SanRecomp target is integrated.
- SAF random-access reads are now exercised by the Android folder-validation path through a reusable JNI wrapper, but are not yet connected to SanRecomp's upstream filesystem/runtime interface.
- GTA V rendering, audio, control mapping, save handling, and game-specific initialization are not integrated.
- No real gameplay has been verified on Android.


## Generated game-code discovery

The user's available inputs are `default.xex` plus Xbox 360 GOD/game-data files; the presence of generated C++ output is not yet known. These files are not interchangeable: `default.xex` is input to the recompilation pipeline, while generated C++ sources and their registry/build metadata are separate outputs.

The Android project must not fabricate a placeholder game module or claim successful runtime initialization when those outputs are absent. The next concrete integration gate is to inspect the local SanRecomp/XenonRecomp preparation output for generated `.cpp/.h` sources, a function registry/import metadata, and the exact build manifest. Only then can an Android-target CMake target link real generated code and begin satisfying the runtime ABI. The current native SAF bridge is a prerequisite for file reads, not a game-code loader.
