# Area51XR

Area51XR is a Windows PCVR compatibility layer for the 1995 *Area 51* arcade game. It preserves the original game logic and light-gun behavior while presenting the game through OpenXR and mapping either tracked VR controller to the Player 1 gun.

## Current playable milestone

The current pre-release build has been hardware-tested on Meta Quest 3 with Virtual Desktop / VDXR and with the SteamVR runtime over Virtual Desktop. It supports the full Player 1 game loop:

- original Area 51 gameplay running in the patched arcade emulator component
- OpenXR VR screen presentation
- identical left- and right-controller light-gun support
- either trigger to fire
- authentic off-screen reload by aiming outside the game screen and firing
- Quest Y or B to insert Coin 1
- Quest X or A to Start / Continue
- either thumbstick click opens the in-headset Resume / Restart / Quit menu
- custom “FORTYDUBZ PRESENTS” startup screen
- keyboard fallback: `5` for Coin 1 and `1` for Player 1 Start
- MAME low-latency mode and fresher-frame sampling for reduced input/display latency
- persistent play launcher and diagnostics

The v1.0 launch target is the stable flat-screen VR/light-gun experience above. Experimental depth reconstruction and spatial mesh work remains outside the v1.0 launch gate.

## OpenXR runtime strategy

Area51XR uses process-scoped OpenXR runtime selection and supports:

- the currently active Windows OpenXR runtime
- Meta Horizon Link / Air Link
- SteamVR
- Virtual Desktop / VDXR

Hardware-validated Quest 3 paths are Virtual Desktop / VDXR and the SteamVR runtime over Virtual Desktop. Meta Horizon Link could not be tested because the Meta PC software failed before Area51XR launched. Steam Link transport remains unvalidated.

## Development play command

Windows development currently uses MSYS2 UCRT64, CMake/Ninja, a targeted patched emulator build, the Khronos OpenXR loader, and optional ONNX Runtime tooling for non-launch reconstruction experiments.

```powershell
powershell -ExecutionPolicy Bypass -File scripts\play-area51-vr.ps1 `
    -RomPath "D:\MAME\roms"
```

Optional runtime override:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\play-area51-vr.ps1 `
    -RomPath "D:\MAME\roms" `
    -Runtime MetaLink
```

Valid runtime overrides are `Auto`, `Active`, `VDXR`, `MetaLink`, and `SteamVR`.

## Public release packaging

The repository contains a standalone release launcher under `release/` and a packaging pipeline:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\package-release.ps1 `
    -Version "1.0.0-rc1"
```

The packaging pipeline produces:

- `Area51XR-<version>-win64.zip`
- `Area51XR-<version>-mame-source.zip`

The player package is designed to run without a source checkout, CMake, or MSYS2 installed on the player's machine. The source package provides the full corresponding source for the modified GPL emulator component.

The packaging pipeline has a hard guard that refuses to create the player ZIP if it finds the Area 51 CHD, ROM archive, or known loose ROM filenames in the release stage.

## Game media

No ROMs, CHDs, original game executables, or copyrighted Area 51 game assets are distributed by this project.

Players must provide game media they are legally entitled to use. The public launcher verifies the media with the bundled emulator component before starting VR.

## Project rules

- Never distribute Area 51 ROMs, CHDs, or original copyrighted game assets.
- Keep the public Area51XR product identity separate from MAME branding and trademarks.
- Preserve licenses and attribution for all open-source dependencies and modified components.
- Do not claim experimental depth/mesh reconstruction is part of v1.0 until it is actually implemented and hardware-validated.
