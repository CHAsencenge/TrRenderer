# Shader organization

Shader code is split by ownership rather than by shader stage.

- `Common/ABI`: GPU-visible structs and enum/flag values only. These files do
  not declare resources or implement algorithms. Their layouts mirror
  `Source/Renderer/TrRenderConstants.h`.
- `Common/Geometry`: reusable vertex-input and geometry transforms.
- `Common/Material`: material flags, coverage sampling, full surface sampling,
  and normal reconstruction.
- `Common/Lighting`: light evaluation, diffuse lighting, and spherical
  harmonics.
- `Common/Utility`: depth, projection, and fullscreen-triangle helpers.
- `Raster`, `Compute`, and `Lumen`: pass entry points. Each pass owns its
  resource declarations, register bindings, and pass-specific `b2` constants.

The stable constant-register convention is:

- `b0`: scene
- `b1`: view
- `b2`: pass
- `b3`: primitive
- `b4`: material
- `b5`: draw/root constants

Shared algorithms should be side-effect free and receive data or resource
objects through parameters. Avoid introducing global bindings in `Common`.

Depth/coverage passes should call `TrSampleMaterialCoverage` and sample only
the base-color alpha when required. Surface passes should reuse that result and
call `TrSampleMaterialSurface`, preventing a second base-color sample.
