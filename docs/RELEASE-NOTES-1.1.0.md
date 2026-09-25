# Area51XR 1.1.0 release notes

Status: hardware acceptance candidate. The latest user-reported Quest 3/VDXR run completed gameplay and reticle-toggle checks. Manual scoreboard persistence and a v1.1 SteamVR retest remain open. Meta Link passthrough is deferred because the Link desktop client did not appear on the current PC after two official install attempts and restarts. Do not merge, tag, or publish while the remaining checks are open.

The first VDXR hardware passes exposed a compositor failure when presentation changed from the 640x360 startup splash to the 320x240 live game frame. The candidate now uses one stable 320x240 presentation surface for the splash, live game, pause menu, and restart splash. It also retains any resolution-specific OpenXR swapchain until session shutdown. In the latest user-reported v1.1 VDXR run, gameplay and reticle toggling worked; passthrough remained unavailable because the runtime did not expose a supported extension.

## Added

- Native local Area 51 high scores, initials, military ranks, and cabinet data now persist through pause, restart, full exit, relaunch, and newly extracted player versions.
- MAME NVRAM, writable CHD differences, configuration, and input data are stored under `%LOCALAPPDATA%\Area51XR\mame`.
- Existing v1.0 install-local player data is backed up and migrated once when found.
- A gameplay reticle can be toggled from the pause menu. It defaults to Off, follows the active controller's actual game-screen intersection, and never changes MAME aim coordinates.
- OpenXR passthrough can be toggled from the pause menu when the active runtime exposes and successfully initializes `XR_FB_passthrough` or `XR_HTC_passthrough`. It defaults to Off and displays Unavailable everywhere else.
- Meta Quest passthrough can be tested through Meta Horizon Link when Developer Runtime Features and Passthrough over Meta Quest Link are enabled and the headset is connected through Link.
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

VDXR and SteamVR are validated for v1.0 gameplay on Quest 3. A Reddit user also reported smooth v1.0 gameplay on Quest 2 through Meta Horizon Link ([report](https://www.reddit.com/r/OculusQuest/comments/1w8kbln/comment/p88hn4a/)); the comment does not identify an OpenXR runtime and does not validate Quest 3 Link, v1.1, or passthrough. In the latest user-reported v1.1 Quest 3/VDXR run, the runtime exposed neither supported passthrough extension, so the pause menu correctly reported Unavailable. Meta Link passthrough remains unverified and is deferred because the desktop client did not become launchable on the current PC after two official install/restart attempts.

## Local data and backup

- Scores and MAME cabinet data: `%LOCALAPPDATA%\Area51XR\mame`
- Preferences: `%LOCALAPPDATA%\Area51XR\settings-v1.ini`
- Session logs: `%LOCALAPPDATA%\Area51XR\logs`
- Automatic migration backups: `%LOCALAPPDATA%\Area51XR\Backups`

To back up local scores, close Area51XR and copy `%LOCALAPPDATA%\Area51XR\mame`. To reset scores, close Area51XR, back up that folder if desired, then remove only `%LOCALAPPDATA%\Area51XR\mame\nvram` and `%LOCALAPPDATA%\Area51XR\mame\diff`. MAME will create clean cabinet data on the next launch.

## Media boundary

Area51XR still contains no Area 51 ROMs, CHDs, artwork, audio, video, or other original game assets. Players must provide compatible media they are legally entitled to use. Discovery fails closed when a component is missing, invalid, or represented by more than one candidate.
