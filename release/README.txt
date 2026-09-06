AREA51XR
========

Area51XR is a Windows PCVR compatibility layer for the 1995 Area 51 arcade game. It preserves the original game logic and light-gun behavior while presenting the game in an OpenXR headset and mapping either tracked controller to the arcade gun.

QUICK START
-----------

1. Extract the entire Area51XR ZIP to a normal writable folder.
2. Connect your VR headset to the PC before launching Area51XR.
3. Put your legally obtained Area 51 game media in the included media\area51 folder, or use an existing compatible media folder.
4. Double-click:

   Play Area51XR.cmd

The launcher verifies the game media before starting. If valid media filenames differ, Area51XR can recognize the supported files by exact content signatures and stage canonical copies without renaming or modifying the originals.

VR CONNECTION OPTIONS
---------------------

Area51XR uses OpenXR and does not require Virtual Desktop.

Hardware-validated Quest 3 paths:

- Virtual Desktop / VDXR
- Virtual Desktop with the SteamVR runtime

Additional launcher paths that remain unvalidated:

- Meta Horizon Link / Air Link
- Steam Link + SteamVR

The launcher uses the currently active OpenXR runtime first in Auto mode, then checks installed compatible runtimes. The headset connection should already be active before Area51XR is launched.

Advanced runtime selection:

  powershell -ExecutionPolicy Bypass -File .\Start-Area51XR.ps1 -Runtime MetaLink
  powershell -ExecutionPolicy Bypass -File .\Start-Area51XR.ps1 -Runtime SteamVR
  powershell -ExecutionPolicy Bypass -File .\Start-Area51XR.ps1 -Runtime VDXR

CONTROLS
--------

Left or right aim          Light gun aim
Either trigger             Fire / select menu item
Aim outside screen + fire  Reload
Y or B                     Insert Coin
X or A                     Start / Continue
Either thumbstick click    Open / close pause menu
Pause menu                  Resume / toggles / Restart Game / Quit Area51XR

PAUSE MENU
----------

Resume
Reticle: Off / On (defaults to Off)
Passthrough: Off / On / Unavailable (defaults to Off)
Restart Game
Quit Area51XR

The pause-menu pointer is always available. The optional gameplay reticle appears only while aiming at the game screen. Passthrough requires support from the active OpenXR runtime.

PERSISTENT LOCAL DATA
---------------------

The original Area 51 scoreboard, initials, military ranks, and cabinet data are preserved under:

  %LOCALAPPDATA%\Area51XR\mame

Player preferences are stored in:

  %LOCALAPPDATA%\Area51XR\settings-v1.ini

Session logs are stored in:

  %LOCALAPPDATA%\Area51XR\logs

Restart Game preserves the native high-score table. Area51XR backs up and migrates v1.0 install-local MAME data when it is found. To back up scores, close Area51XR and copy the mame folder above. To reset scores, close Area51XR and remove only its mame\nvram and mame\diff folders after making any desired backup.

Keyboard fallback:

5                          Insert Coin
1                          Player 1 Start

GAME MEDIA
----------

No original game files are distributed with Area51XR.

Default loose-file layout:

media\
  area51\
    2-c_area_51_hh.hh
    2-c_area_51_hl.hl
    2-c_area_51_lh.lh
    2-c_area_51_ll.ll
    jagwave.rom
    area51.chd

You may instead point the launcher at an existing compatible media directory:

  powershell -ExecutionPolicy Bypass -File .\Start-Area51XR.ps1 -RomPath "D:\path\to\media"

TROUBLESHOOTING
---------------

If the headset does not connect:

- Confirm the headset is already connected through Meta Horizon Link, Steam Link/SteamVR, or Virtual Desktop.
- Confirm that connection method exposes a working OpenXR runtime on the PC.
- Try the matching -Runtime option shown above.
- Review the newest folder under %LOCALAPPDATA%\Area51XR\logs for runtime and emulator diagnostics.

If the game media fails verification, confirm that your Area 51 ROM set and CHD match the emulator version bundled with this release.

Enemy firearms in the original Area 51 game may appear visually without a distinct firing sound. Area51XR does not alter or replace the original game audio mix.

LEGAL
-----

Area51XR does not contain or distribute Area 51 ROMs, CHDs, or other original game assets. Users must provide game media they are legally entitled to use.

The bundled arcade emulator component is a modified open-source component derived from the MAME project and is distributed under its applicable GPL terms. Corresponding source for the exact modified emulator build is distributed as a separate source archive with the Area51XR release. See emulator\SOURCE.txt and emulator\docs\legal for details.

MAME is a registered trademark of Gregory Ember. Area51XR is an independent project and is not endorsed by or affiliated with MAMEdev, Atari Games, Mesa Logic, Time Warner Interactive, Meta, Valve, or Virtual Desktop.
