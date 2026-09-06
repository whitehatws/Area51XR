# Area51XR 1.1.0 release notes

Status: hardware acceptance candidate. Do not merge, tag, or publish until VDXR and SteamVR testing passes.

## Added

- Native local Area 51 high scores, initials, military ranks, and cabinet data now persist through pause, restart, full exit, relaunch, and newly extracted player versions.
- MAME NVRAM, writable CHD differences, configuration, and input data are stored under `%LOCALAPPDATA%\Area51XR\mame`.
- Existing v1.0 install-local player data is backed up and migrated once when found.
- A gameplay reticle can be toggled from the pause menu. It defaults to Off, follows the active controller's actual game-screen intersection, and never changes MAME aim coordinates.
- OpenXR passthrough can be toggled from the pause menu when the active runtime exposes and successfully initializes `XR_FB_passthrough` or `XR_HTC_passthrough`. It defaults to Off and displays Unavailable everywhere else.
- Content-signature media discovery recognizes the supported Area 51 ROM components and CHD even when source filenames differ. Original files are never renamed or modified.
- Player preferences use a versioned `settings-v1.ini` file under `%LOCALAPPDATA%\Area51XR` and safely return to defaults if the file is missing, incompatible, or corrupt.

## Pause menu

1. Resume
2. Reticle: Off or On
3. Passthrough: Off, On, or Unavailable
4. Restart Game
5. Quit Area51XR

The menu pointer remains available regardless of the gameplay-reticle preference. Either controller can operate every menu item.

## Passthrough limits

Passthrough support is selected by OpenXR extension capability, not by a headset-name check. This candidate implements the `XR_FB_passthrough` and `XR_HTC_passthrough` composition-layer paths and keeps the game quad fully opaque above the surrounding passthrough layer. Runtime support, permission behavior, and image quality still require hardware confirmation.

VDXR and SteamVR remain validated for v1.0 gameplay. Their v1.1 passthrough behavior is not yet hardware validated. Meta Quest Link remains unverified because the Meta PC application failed before Area51XR could launch during v1.0 testing.

## Local data and backup

- Scores and MAME cabinet data: `%LOCALAPPDATA%\Area51XR\mame`
- Preferences: `%LOCALAPPDATA%\Area51XR\settings-v1.ini`
- Session logs: `%LOCALAPPDATA%\Area51XR\logs`
- Automatic migration backups: `%LOCALAPPDATA%\Area51XR\Backups`

To back up local scores, close Area51XR and copy `%LOCALAPPDATA%\Area51XR\mame`. To reset scores, close Area51XR, back up that folder if desired, then remove only `%LOCALAPPDATA%\Area51XR\mame\nvram` and `%LOCALAPPDATA%\Area51XR\mame\diff`. MAME will create clean cabinet data on the next launch.

## Media boundary

Area51XR still contains no Area 51 ROMs, CHDs, artwork, audio, video, or other original game assets. Players must provide compatible media they are legally entitled to use. Discovery fails closed when a component is missing, invalid, or represented by more than one candidate.
