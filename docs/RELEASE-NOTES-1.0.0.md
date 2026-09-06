# Area51XR 1.0.0

**FORTYDUBZ PRESENTS**

Area51XR brings the original 1995 *Area 51* arcade light-gun experience to Windows PCVR through OpenXR. It preserves the original game logic and arcade behavior while adding tracked VR aiming, mirrored controller support, a headset-native pause menu, and a standalone player launcher.

## Highlights

- Original Area 51 arcade gameplay presented on a VR screen
- Either controller can aim and fire
- Automatic active-hand switching
- Authentic aim-off-screen reload
- Y or B inserts a coin
- X or A starts or continues
- Either thumbstick click opens the pause menu
- Resume, Restart Game, and Quit Area51XR controls
- Fortydubz startup splash, including after restart
- Low-latency framebuffer and controller bridge
- Automatic OpenXR runtime discovery
- Standalone Windows player with local diagnostics
- No developer tools required on the player's PC

## Requirements

- Windows 10 or Windows 11, 64-bit
- An OpenXR-compatible PCVR headset connection
- A VR-capable Windows PC
- Compatible Area 51 game media that the player is legally entitled to use

Game media is not included.

## Tested hardware

- Meta Quest 3 with Virtual Desktop / VDXR
- Meta Quest 3 with Virtual Desktop / SteamVR

Meta Horizon Link / Air Link and Steam Link launcher paths are implemented but were not hardware validated for this release.

## Quick start

1. Download and extract `Area51XR-1.0.0-win64.zip`.
2. Place compatible media under `media\area51\`.
3. Connect the headset to the PC.
4. Double-click `Play Area51XR.cmd`.
5. Put on the headset and play.

The first clean launch may print a message about setting default EEPROM values. This is normal initialization and is not an error.

## Controls

| Action | Left controller | Right controller |
|---|---|---|
| Aim | Controller aim | Controller aim |
| Fire | Trigger | Trigger |
| Reload | Aim outside screen and fire | Aim outside screen and fire |
| Insert coin | Y | B |
| Start or continue | X | A |
| Pause menu | Thumbstick click | Thumbstick click |
| Menu selection | Point and pull trigger | Point and pull trigger |

Keyboard fallback: `5` inserts a coin and `1` starts Player 1.

## Downloads

Both release archives belong together:

- `Area51XR-1.0.0-win64.zip`: standalone Windows player
- `Area51XR-1.0.0-mame-source.zip`: complete corresponding source for the modified emulator component

## Performance note

Windows Remote Desktop can compete with PCVR streaming for GPU encoding and introduce visible latency. Disconnect RDP without signing out before evaluating or playing in VR.

## Legal

Area51XR contains no Area 51 ROMs, CHDs, original executables, artwork, audio, video, or other original game assets. Users must supply media they are legally entitled to use.

The bundled modified emulator component is derived from MAME and distributed under its applicable GPL terms. Matching corresponding source is supplied with this release.

Area51XR is an independent project and is not endorsed by or affiliated with MAMEdev, Atari Games, Mesa Logic, Time Warner Interactive, Meta, Valve, Virtual Desktop, Fandom, or the original game's rights holders.
