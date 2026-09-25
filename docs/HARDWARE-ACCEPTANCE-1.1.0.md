# Area51XR 1.1.0 hardware acceptance

Do not merge, tag, or publish from this checklist. Successful results qualify the branch for a separate `1.1.0-rc1` clean-package test.

## Before testing

- Close Area51XR and copy `%LOCALAPPDATA%\Area51XR` to a safe backup location.
- Confirm the source branch is `feat/v1.1-player-upgrades` and the working tree is clean.
- Build and run the automated gate before starting VR.
- Disconnect Windows Remote Desktop before judging motion or input latency.

## Current results

- Quest 3 with Virtual Desktop/VDXR: the latest user-reported v1.1 run played through and reticle toggling worked. Passthrough reported Unavailable; VDXR exposed neither `XR_FB_passthrough` nor `XR_HTC_passthrough`.
- Quest 3 with Virtual Desktop/SteamVR: v1.0 gameplay was hardware validated. A v1.1 retest is still pending.
- Quest 2 with Meta Horizon Link: a Reddit user reported smooth v1.0 gameplay. This is community-reported evidence for Quest 2 only; it does not validate Quest 3 Link/Air Link or v1.1 passthrough ([report](https://www.reddit.com/r/OculusQuest/comments/1w8kbln/comment/p88hn4a/)).
- Quest 3 with Meta Horizon Link/Air Link: deferred. After two official installer runs and requested restarts, the PC still had no launchable Link client or Start menu shortcut. This is a local PC software blocker, not evidence that Meta Link is broken generally.
- Native scoreboard persistence: automated regressions pass, but headset confirmation after restart and full relaunch has not been done.

## VDXR gameplay and persistence

- Launch with `-Runtime VDXR` and confirm the Fortydubz splash appears before gameplay.
- Confirm the pause menu order is Resume, Reticle, Passthrough, Restart Game, Quit Area51XR.
- Confirm both controllers can point, select every menu item, aim, fire, insert a coin, start, and reload off-screen.
- Confirm Reticle starts Off for a clean settings file, turns On immediately, follows the active hand, disappears off-screen and during the splash, and remains selected after a full relaunch.
- Record whether Passthrough reads Unavailable or Off. Unavailable is expected when VDXR does not expose `XR_FB_passthrough` or `XR_HTC_passthrough`.
- Earn or enter a recognizable native high score and initials using only VR controllers.
- Use Restart Game. Confirm the Fortydubz splash returns and the native high-score table remains.
- Quit completely, relaunch, and confirm the score, initials, and rank remain.
- Confirm the newest `%LOCALAPPDATA%\Area51XR\logs` session identifies VDXR and the detected passthrough extension or `none`.

## Meta Horizon Link passthrough (deferred)

No further Meta PC app installation attempts are part of this acceptance pass. The Quest 2 Reddit report covers v1.0 gameplay only; it does not establish Quest 3 compatibility, the active OpenXR runtime, or passthrough behavior. Revisit this path only when a launchable Link client is available on the test PC or another test PC.

## Score persistence

- Run without an `A51XR_DATA_ROOT` override so MAME writes to `%LOCALAPPDATA%\Area51XR\mame`.
- Earn or enter a recognizable native high score and initials using VR controllers.
- Restart the game and confirm the score remains; quit Area51XR fully, relaunch, and confirm the score, initials, and rank remain.

## SteamVR second

- Start SteamVR inside Virtual Desktop and wait for the headset and both controllers to show connected.
- Launch with `-Runtime SteamVR` and repeat splash, both-hand controls, pause menu, reticle, restart, quit, and score-persistence checks.
- Record whether Passthrough reads Unavailable or Off. Unavailable is a valid graceful result if SteamVR does not expose `XR_FB_passthrough` or `XR_HTC_passthrough`.
- If passthrough is available, repeat the opaque-game-quad, readable-menu, uninterrupted-audio, and session-resume checks.
- Confirm the newest log identifies SteamVR and accurately reports passthrough capability.

## Report back

For each runtime, report: launch pass or fail, menu pass or fail, both-hand controls pass or fail, reticle pass or fail, passthrough state and result, restart splash pass or fail, score persistence after restart, score persistence after full relaunch, and the newest log folder name.
