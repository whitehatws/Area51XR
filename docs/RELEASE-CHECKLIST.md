# Area51XR v1.0 Release Gate

This checklist defines the launch gate for the first public Windows PCVR release. Experimental depth reconstruction and spatial mesh presentation are explicitly outside this gate.

## Gameplay

- [x] Area 51 boots with verified user-supplied media.
- [x] Game framebuffer is presented through OpenXR.
- [x] Either Quest 3 controller can aim and track the Player 1 gun.
- [x] Either trigger fires.
- [x] Aiming outside the game screen and firing reloads.
- [x] Quest Y or B inserts Coin 1.
- [x] Quest X or A starts and continues Player 1.
- [x] Continue-after-death flow works without keyboard input.
- [x] Either thumbstick click opens the in-headset Resume / Restart / Quit menu.
- [x] Restart returns through the Fortydubz startup splash.
- [x] Keyboard `5` and `1` remain fallback Coin/Start inputs.
- [x] Low-latency play mode is enabled.
- [x] Gameplay remains stable for sustained play on the known-good VDXR path.

## Audio

- [x] General game audio reaches the headset on the known-good VDXR path.
- [ ] Final sanity run confirms Player 1 gun, voices, music/ambience, explosions, UI/credit sounds, and other major effects are audible.
- [x] Do not treat the apparent absence of distinct enemy firearm reports as an Area51XR audio failure unless a stock/reference run proves those sounds should exist.

## OpenXR / PCVR transport

- [x] Virtual Desktop / VDXR hardware path validated on Quest 3.
- [x] Launcher no longer requires VDXR and supports process-scoped runtime selection.
- [ ] Meta Horizon Link / Air Link free path validated on Quest 3.
- [x] SteamVR runtime validated on Quest 3 over Virtual Desktop.
- [ ] Steam Link transport validated on Quest 3.
- [ ] Auto runtime selection validated with at least one free runtime active.

Launch requirement: at least one free Quest 3 PCVR connection path must pass before v1.0 is published. Virtual Desktop is the currently validated Quest 3 transport. Steam Link or Meta Horizon Link must still pass before claiming a validated free transport.

## Player package

- [x] Standalone public launcher exists under `release/`.
- [x] Double-click `.cmd` entrypoint exists.
- [x] Player package does not require a Git checkout, CMake, Ninja, or MSYS2 on the player's machine.
- [x] Launcher verifies game media before starting VR.
- [x] First-run media folder/documentation exists.
- [x] Runtime diagnostics and play logs are written locally.
- [x] Release packaging script inspects/stages native DLL dependencies.
- [x] Release packaging script generates SHA-256 file manifest.
- [ ] Generated RC player ZIP launches successfully from a fresh extracted folder.
- [ ] Generated RC player ZIP is tested with media outside the development repository.

## Copyright / media boundary

- [x] No Area 51 ROMs or CHDs are tracked for release.
- [x] Packaging script rejects `area51.zip`, `area51.chd`, and known loose Area 51 ROM filenames.
- [x] Release documentation tells users to provide media they are legally entitled to use.
- [ ] Final ZIP contents manually inspected before upload.

## Modified emulator licensing

- [x] MAME-derived integration header has an explicit GPL-compatible license header.
- [x] Player package includes MAME `COPYING` and `docs/legal` files.
- [x] Player package contains a source notice with the exact upstream revision.
- [x] Packaging script creates a full corresponding-source ZIP from the exact upstream revision with Area51XR modifications overlaid.
- [x] Third-party notices identify the OpenXR loader and toolchain runtime libraries.
- [ ] Player ZIP and corresponding-source ZIP are uploaded together for the public release.

## Product/version cleanup

- [ ] Bump Area51XR user-visible version to `1.0.0` after RC validation.
- [ ] Update README runtime validation status after free-path testing.
- [ ] Create/freeze `release/v1.0` branch from the validated commit.
- [ ] Merge the validated release line to `main` with clean history.
- [ ] Create `v1.0.0` tag from the exact release commit.

## GitHub Release

- [ ] Build final `Area51XR-1.0.0-win64.zip`.
- [ ] Build final `Area51XR-1.0.0-mame-source.zip`.
- [ ] Verify SHA-256 hashes after ZIP creation.
- [ ] Draft GitHub Release notes with controls, requirements, free runtime options, legal media requirement, and known limitations.
- [ ] Attach both ZIPs.
- [ ] Publish only after all launch-required items above pass.
