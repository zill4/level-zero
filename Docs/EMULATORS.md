# Headset-free testing (no PICO headset available)

Checked 2026-10-17 on zill4 (UE 5.8.3, Android SDK emulator 37.1.11).

## Summary
| Target | Can it run Level Zero (UE 5.8 OpenXR)? | Evidence |
|---|---|---|
| **PICO Emulator** (PICO OS 6.0 / Android 16, x86_64 image, `emulator-5554`) | **Very likely yes, not yet confirmed for Level Zero.** It ships a real OpenXR runtime (`com.pico.xr.openxr_runtime`, `libopenxr_forwardloader.so`), Vulkan 1.3, and an ARM translation layer (`libhoudini`, abilist `arm64-v8a,x86_64`). Oomi's **arm64** UE 5.8 OpenXR APK (`io.crispcode.oomi.pico`) is running on it right now through houdini. | `dumpsys package io.crispcode.oomi.pico` → `primaryCpuAbi=arm64-v8a`; its process maps `libhoudini.so`; screen recording shows it rendering. PICO's emulator doc lists OpenXR rendering features (multiview, MSAA, composition layers) for the Unity OpenXR mode; it does not document Unreal. |
| Level Zero **x86_64** APK on the PICO emulator | Installs (`primaryCpuAbi=x86_64`). **Launch not tested yet**: PICO OS refused to start it because another app was in Full Space ("current full space don't allow start fullscreen"): Oomi's window-mode agent was mid-test on the shared emulator, so I didn't stop it. | logcat 13:38 CT |
| **Android XR Emulator** (Android Studio, `android-34/google-xr` + `android-xr-preview-playstore` x86_64; AVDs `Oomi_XR_Headset*`) | **No.** Google: "The emulator is designed for developing Android apps in Android Studio. It's not currently supported for OpenXR or Unity applications of any kind." Oomi's own XR notes reached the same result. | developer.android.com/develop/xr/jetpack-xr-sdk/studio-tools |

## What this means for "Android XR first, then PICO"
- There is **no emulator path for an Android XR OpenXR APK**. Android XR checks without a Galaxy XR headset are limited to:
  1. building the APK (arm64, OpenXR 1.1 manifest feature, SDK 34+) and statically verifying it, and
  2. running the game on a **desktop OpenXR runtime** (e.g. Meta XR Simulator, which Oomi already pins) via VR Preview to check the OpenXR session, input and comfort, plus UE's Android mobile-renderer preview for the rendering path.
- The **PICO emulator is the only place an Android OpenXR APK actually executes** on this PC. So in practice the loop is: desktop OpenXR preview → PICO emulator → (later) real headsets.
- An Android XR (Galaxy XR) build needs Google's Android XR OpenXR plugin/extensions and Min/Target SDK 34. It's a separate store variant from PICO (different plugin set). Not added yet; it's a small follow-up to `Tools/package_android.ps1`.

## Build variants (Tools/package_android.ps1)
| Variant | ABI | XR plugins | Use |
|---|---|---|---|
| `-Variant Store` | arm64-v8a | OpenXR + **PICO OpenXR 1.6.1** | Store builds / real PICO headsets. Also the best bet for the PICO emulator (runs via houdini, like Oomi's) |
| `-Variant Emulator` | x86_64 | stock OpenXR only (PICO's plugin ships an **arm64-only** loader, `libopenxr_loader_pico.so`) | Native-speed emulator testing; uses the emulator's forward loader |

The emulator variant temporarily edits `LevelZero.uproject` (PICOOpenXR off) and `Config/DefaultEngine.ini` (x86_64 on, arm64 off) and restores both afterwards: UBT ignores `-ini:` arch overrides from UAT. If a build is killed mid-way, run `git checkout -- LevelZero.uproject Config/DefaultEngine.ini`.

## To finish the emulator check (needs the emulator free)
```powershell
$adb="$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe"
& $adb -s emulator-5554 install -r -g "D:\Unreal Projects\LevelZero\Packaged\Emulator\Android_ASTC\LevelZero-x64.apk"
# only when nothing else is using Full Space on the emulator:
& $adb -s emulator-5554 shell monkey -p com.crispcode.levelzero -c android.intent.category.LAUNCHER 1
& $adb -s emulator-5554 shell screenrecord --time-limit 40 /sdcard/lz.mp4
& $adb -s emulator-5554 logcat -d | Select-String "LevelZeroMaze|OpenXR|FATAL"
```
Then repeat with the Store (arm64) APK.

## Step 2 check (2026-10-17 14:32 CT): skipped, emulator busy
`emulator-5554` was relaunched by the Oomi side at 14:08 CT. `topResumedActivity` = `io.crispcode.oomi.pico/com.epicgames.unreal.GameActivity` (Oomi in the foreground; the PICO scenario provider is also resumed). Per the rule "only if no other app is in Full Space; never force-stop Oomi", the arm64 Store APK check was **not** run. Run the commands above when `dumpsys activity activities | grep topResumedActivity` shows only the PICO launcher.

## Step 3a check (2026-10-17 21:52 CT): skipped, emulator busy
`emulator-5554`: `topResumedActivity` = `io.crispcode.oomi.pico/com.epicgames.unreal.GameActivity` (Oomi in the foreground; the PICO scenario provider is also resumed). Per the rule, the arm64 Store APK (Tripo monster + props build) was **not** installed and Oomi was not stopped. Run the commands above when only the PICO launcher is resumed.
