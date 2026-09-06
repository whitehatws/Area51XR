# Area51XR 1.1.0 hardware acceptance

Do not merge, tag, or publish from this checklist. Successful results qualify the branch for a separate `1.1.0-rc1` clean-package test.

## Before testing

- Close Area51XR and copy `%LOCALAPPDATA%\Area51XR` to a safe backup location.
- Confirm the source branch is `feat/v1.1-player-upgrades` and the working tree is clean.
- Build and run the automated gate before starting VR.
- Disconnect Windows Remote Desktop before judging motion or input latency.

## VDXR first

- Launch with `-Runtime VDXR` and confirm the Fortydubz splash appears before gameplay.
- Confirm the pause menu order is Resume, Reticle, Passthrough, Restart Game, Quit Area51XR.
- Confirm both controllers can point, select every menu item, aim, fire, insert a coin, start, and reload off-screen.
- Confirm Reticle starts Off for a clean settings file, turns On immediately, follows the active hand, disappears off-screen and during the splash, and remains selected after a full relaunch.
- Record whether Passthrough reads Unavailable or Off. If Off, turn it On and confirm the room appears only around the fully opaque game quad while the menu remains readable and usable.
- While passthrough is active, Resume and confirm video, audio, credits, aiming, and calibration continue without a reset.
- Earn or enter a recognizable native high score and initials using only VR controllers.
- Use Restart Game. Confirm the Fortydubz splash returns and the native high-score table remains.
- Quit completely, relaunch, and confirm the score, initials, and rank remain.
- Confirm the newest `%LOCALAPPDATA%\Area51XR\logs` session identifies VDXR and the detected passthrough extension or `none`.

## SteamVR second

- Start SteamVR inside Virtual Desktop and wait for the headset and both controllers to show connected.
- Launch with `-Runtime SteamVR` and repeat splash, both-hand controls, pause menu, reticle, restart, quit, and score-persistence checks.
- Record whether Passthrough reads Unavailable or Off. Unavailable is a valid graceful result if SteamVR does not expose `XR_FB_passthrough` or `XR_HTC_passthrough`.
- If passthrough is available, repeat the opaque-game-quad, readable-menu, uninterrupted-audio, and session-resume checks.
- Confirm the newest log identifies SteamVR and accurately reports passthrough capability.

## Report back

For each runtime, report: launch pass or fail, menu pass or fail, both-hand controls pass or fail, reticle pass or fail, passthrough state and result, restart splash pass or fail, score persistence after restart, score persistence after full relaunch, and the newest log folder name.
