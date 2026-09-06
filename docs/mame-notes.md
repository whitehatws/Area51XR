# MAME integration notes

Validated upstream revision: `06516a9dc07f68e21092340f04e1a6f6d6a58260`.

Area 51 runs on Atari CoJag hardware in MAME. The current driver exposes two useful integration points for Area51XR:

- `jaguar_state::cojag_gun_input_r` is the centralized light-gun read path. It obtains crosshair coordinates, packs Y in the upper 16 bits, and returns X in the lower word with the 9-bit value XORed by `0x1ff`.
- `jaguar_state::screen_update` copies the completed Jaguar framebuffer to MAME's output bitmap, making it the least invasive place to observe finished frames.

The first bridge should leave game logic and hit detection inside MAME. Area51XR supplies controller-derived aim coordinates and consumes completed video frames plus timing metadata.

## Native cabinet persistence

The pinned Area51XR MAME revision defines Area 51 as the `cojagr3k` machine. Its driver maps an 8 KB shared `nvram` region at `0x18000000-0x18001fff` and attaches MAME's NVRAM device with an all-ones first-run default. The game itself owns the contents, including its native high-score table, initials, military ranks, and cabinet settings. Area51XR does not parse or replace that data.

The driver also mounts the `area51` image as an IDE hard disk. The source CHD remains read-only. If the emulated cabinet writes to the disk, MAME places changes in its separate difference directory.

Player launches explicitly route the following MAME directories under `%LOCALAPPDATA%\Area51XR\mame`:

- `nvram` for the native cabinet EEPROM/NVRAM state
- `diff` for writable CHD differences
- `cfg` for emulator configuration
- `input` for recorded input configuration

Restart Game calls `machine.schedule_hard_reset()` and never deletes these directories. The v1.1 launcher also performs a one-time, backed-up migration of data found in v1.0's install-local MAME directories.

## Initial bridge contract

The XR process and any MAME-side adapter should communicate through versioned plain-old-data messages. The protocol must not contain ROM data or game assets.

Input packet fields:

- protocol version
- monotonically increasing sequence number
- normalized aim X/Y
- trigger state
- reload/start/menu flags as they are added

Frame packet fields:

- protocol version
- monotonically increasing frame number
- width/height/pixel format
- presentation timestamp
- payload size or shared-buffer identifier

The transport is deliberately unspecified for the first implementation. Tests should validate packing, ranges, and compatibility before selecting shared memory or local IPC.
