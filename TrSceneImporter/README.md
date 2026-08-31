# TrSceneImporter

`TrSceneImporter` is a standalone, nvpro_core-independent import path for a
self-contained glTF 2.0 binary scene (`.glb`). It is intended for scenes
exported by UE5's glTF exporter without an additional JSON scene description.

The import is deliberately split into two layers:

1. `TrGlbImporter` decodes the GLB into the renderer-independent `TrScene` CPU
   representation.
2. `TrScene::BuildStaticRenderMesh` creates the flattened vertex/index data
   used by the current DX12 GBuffer preview. The structured `TrScene` remains
   available for later material, instance, DXR, and Lumen work.

## Build and convert

```powershell
cmake -S . -B build -DTR_DXC_ROOT=D:/dxc
cmake --build build --config Debug --target TrSceneImporter
build/bin/Debug/TrSceneImporter.exe D:/Scenes/Example.glb D:/Scenes/Example.trscene
```

The output path is optional; it defaults to the input name with a `.trscene`
extension. The converter immediately reads the result back and builds a preview
mesh, so a successful exit verifies both serialization directions.

## Load in the DX12 renderer

The renderer accepts either the source GLB or the converted cache:

```powershell
build/bin/Debug/TrD3D12Renderer.exe -scene D:/Scenes/Example.glb
build/bin/Debug/TrD3D12Renderer.exe -scene D:/Scenes/Example.trscene
```

Paths containing spaces may be quoted. With no `-scene` option, the existing
procedural Cornell Box remains the default.

## Imported data

- Active-scene node hierarchy, transforms, and mesh instances
- Static triangle meshes with 16-bit or 32-bit source indices
- Positions, normals, tangents, UV0/UV1, and vertex colors
- Metallic/roughness PBR factors and texture bindings
- Embedded image payloads, samplers, and texture references
- Punctual lights and cameras
- glTF right-handed to renderer left-handed conversion, including winding
- A versioned `.trscene` binary cache with bounds and corruption checks

The current DX12 preview bakes node transforms and scalar material properties
into one static render mesh. Embedded textures are retained in `TrScene`, but
GPU texture creation and sampling are intentionally not part of this first
import step. Alpha blend/mask is also rendered as opaque for now.

Draco/meshopt meshes, skins, animation, morph targets, and GPU-instancing
extensions are not expanded. Unsupported static-scene features produce warnings
where a safe fallback exists; compressed geometry is rejected because decoding
it silently would be incorrect.
