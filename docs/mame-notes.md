# MAME integration notes

Area 51 runs on Atari CoJag hardware in MAME. The current driver exposes two useful integration points for Area51XR:

- `jaguar_state::cojag_gun_input_r` is the centralized light-gun read path. It obtains crosshair coordinates, packs Y in the upper 16 bits, and returns X in the lower word with the 9-bit value XORed by `0x1ff`.
- `jaguar_state::screen_update` copies the completed Jaguar framebuffer to MAME's output bitmap, making it the least invasive place to observe finished frames.

The first bridge should leave game logic and hit detection inside MAME. Area51XR supplies controller-derived aim coordinates and consumes completed video frames plus timing metadata.

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
