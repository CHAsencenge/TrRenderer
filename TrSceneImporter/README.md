# TrSceneImporter

`TrSceneImporter` is a standalone, nvpro_core-independent import path for a
self-contained glTF 2.0 binary scene (`.glb`). It is intended for scenes
exported by UE5's glTF exporter without an additional JSON scene description.

The import is deliberately split into two layers:

1. `TrGlbImporter` decodes the GLB into the renderer-independent `TrScene` CPU
   representation.
2. `TrRuntimeScene` uploads every referenced source mesh once and keeps active
   nodes as instances. Mesh/primitive/instance/material IDs have stable source
   contracts; mesh and primitive local AABBs, instance world AABBs, the full
   hierarchy, and current/previous world transforms are retained. Node
   transforms are not baked into duplicated vertices.

## Build and convert

```powershell
cmake -S . -B build -DTR_DXC_ROOT=D:/dxc
cmake --build build --config Debug --target TrSceneImporter
build/bin/Debug/TrSceneImporter.exe D:/Scenes/Example.glb D:/Scenes/Example.trscene
```

The output path is optional; it defaults to the input name with a `.trscene`
extension. The converter immediately reads the result back, validates the active
hierarchy, and calculates world bounds, so a successful exit verifies both
serialization directions without flattening the scene.

## Load in the DX12 renderer

The renderer accepts either the source GLB or the converted cache:

```powershell
build/bin/Debug/TrD3D12Renderer.exe -scene D:/Scenes/Example.glb
build/bin/Debug/TrD3D12Renderer.exe -scene D:/Scenes/Example.trscene
```

Paths containing spaces may be quoted. With no `-scene` option, a procedural
runtime-scene validation case is used: a six-primitive room mesh plus shared
sphere/cube meshes instanced under nested, animated parent nodes.

## Imported data

- Active-scene node hierarchy, transforms, and mesh instances
- Static triangle meshes with 16-bit or 32-bit source indices
- Positions, normals, tangents, UV0/UV1/UV2, and vertex colors
- Metallic/roughness PBR factors and texture bindings
- Embedded image payloads, samplers, and texture references
- Punctual lights and cameras
- glTF right-handed to renderer left-handed conversion, including winding
- A versioned `.trscene` binary cache with bounds and corruption checks

The DX12 renderer preserves mesh/primitive/instance relationships, uploads
embedded WIC-decodable textures, and samples base-color, metallic-roughness,
normal, occlusion, and emissive maps. Alpha mask participates in the prepass;
alpha blend is rendered by the forward transparent pass.

Draco/meshopt meshes, skins, animation, morph targets, and GPU-instancing
extensions are not expanded. Unsupported static-scene features produce warnings
where a safe fallback exists; compressed geometry is rejected because decoding
it silently would be incorrect.
