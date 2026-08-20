# OpenXR host architecture

Area51XR keeps headset-runtime code behind a small interface so MAME transport, gun mapping, frame snapshots, and most host behavior remain deterministic and testable without a headset.

The runtime loop is:

1. poll OpenXR session and right-hand aim/trigger state;
2. snapshot the newest MAME framebuffer through the versioned shared-memory bridge;
3. map the controller aim ray onto the same physical plane used to display the game;
4. publish normalized aim and trigger state back to MAME;
5. present the cached game frame as an OpenXR quad layer.

The first VR presentation intentionally remains a spatial 2D game plane. This provides a stable baseline for validating OpenXR lifecycle, controller alignment, latency, framebuffer transport, and original Area 51 input behavior before depth reconstruction is introduced.

## Runtime boundary

`XrRuntime` has both a deterministic simulated implementation and a Windows OpenXR implementation. The simulated runtime is used by the local regression suite. The Windows backend uses D3D11 and the Khronos OpenXR loader, dynamically loading OpenXR entry points so runtime-specific DLL import behavior stays isolated.

## MAME boundary

The MAME bridge protocol is versioned independently of the XR runtime. Gun and frame records use sequence guards to prevent either process from observing a partially updated state. MAME exports only its visible game rectangle rather than the Jaguar backing bitmap.

No ROM, CHD, executable, or original game asset is part of the source tree.
