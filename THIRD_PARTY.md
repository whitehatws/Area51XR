# Third-party components

Area51XR does not distribute original *Area 51* game data.

The development/bootstrap tooling can obtain these external components separately:

- **MAME** — emulator source and its own third-party dependencies; see the MAME project for applicable licenses.
- **Khronos OpenXR SDK / loader** — Apache License 2.0.
- **Microsoft ONNX Runtime** — MIT License. The Windows bootstrap pins `Microsoft.ML.OnnxRuntime` 1.28.0.
- **Depth Anything V2 Small (ViT-S)** — Apache License 2.0. Area51XR intentionally targets the Small model; larger Depth Anything V2 variants use different/non-commercial licensing and are not part of the bootstrap.

Downloaded runtimes, models, emulator source, game files, captures, generated meshes, reconstruction quality reports, sequence manifests, sequence diagnostics, and bridge-captured replay artifacts are kept outside source control.
