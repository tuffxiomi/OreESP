# Ore ESP implementation notes

The implementation is standalone. It does not include BedrockTools source, headers, modules, assets, or a BedrockTools `.so`.

## Runtime signatures copied from the supplied source

The mod resolves only these signatures at runtime against `libminecraftpe.so`:

- `RenderLevel`
- `TessellatorBegin`
- `TessellatorColor`
- `TessellatorVertex`
- `MeshHelpersRenderMeshImmediately2` with `MeshHelpersRenderMeshImmediately` fallback
- `BlockSourceGetBlock`
- `ClientInstanceUpdate`
- `ClientInstanceGetLocalPlayer`

The patterns are embedded in `src/signatures.cpp` and use the same wildcard byte syntax as the supplied project.

## Offsets used

World/client:

- `Actor::mDimension = 448 (0x1C0)`
- `Actor::mStateVectorComponent = 0x208`
- `Dimension::mBlockSource = 208 (0xD0)`
- `Block::mBlockType = 0x68`
- `BlockType::mNameInfo = 0x88`
- `NameInfo::mFullName = 0x40`
- `HashedString::mString = 0x8`

Rendering:

- `LevelRenderer::mLevelRendererPlayer = 0x420`
- `LevelRendererPlayer::mCamPos = 0x61C`
- `LevelRendererPlayer::mSelectionOverlayMaterial = 0x1030`
- `ScreenContext::mTessellator = 0xB8`
- `ScreenContext::mColorHolder = 0x30`

These are taken from the supplied BedrockTools layout and are not implemented as a BedrockTools dependency.

## Supplied-binary verification

A byte-pattern scan of the supplied `libminecraftpe.so` found one match for each required pattern used by the renderer/block path, including:

- RenderLevel: `0xAE0CAA8`
- TessellatorBegin: `0xA3FA640`
- TessellatorColor: `0xA3FABF4`
- TessellatorVertex: `0xA3FAE90`
- MeshHelpersRenderMeshImmediately2: `0xADBAEC4`
- BlockSourceGetBlock: `0xF2541EC`
- ClientInstanceUpdate: `0x943ECF4`
- ClientInstanceGetLocalPlayer: `0x9443404`

The offsets above are file offsets from the supplied ELF image and are only used here as a verification that the patterns are unique; the mod itself uses resolved runtime addresses.
