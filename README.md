# Area51XR

Area51XR is an experimental VR compatibility layer for the 1995 *Area 51* arcade game. The goal is to preserve the original game logic and light-gun behavior while adapting presentation and input for modern OpenXR headsets.

The project is in early development. The current code establishes the build, test, and diagnostic path needed before emulator and OpenXR integration begins.

## Build

Windows development uses CMake and Visual Studio Build Tools 2022 or Visual Studio 2022.

```powershell
./scripts/build.ps1
./scripts/test.ps1
```

The diagnostic host can also exercise projection code without a headset:

```powershell
./scripts/launch.ps1 --simulate-gun 0.5 0.5
```

## Project rules

- No ROMs, CHDs, game executables, or copyrighted game assets are distributed here.
- Game-specific files stay on the user's machine.
- Open-source dependencies and borrowed code must retain their original licenses and attribution.
