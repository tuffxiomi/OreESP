# Ore ESP

Standalone native ore highlighter for Levi Launcher / Preloader Android.

## What it does

- Highlights nearby ore blocks with bright colored outlines.
- Supports overworld ores, Nether quartz/gold, and ancient debris.
- Uses runtime signatures against `libminecraftpe.so`.
- Does not bundle or include BedrockTools source, headers, modules, or libraries.
- ARM64 Android build target.

## Default range

- Horizontal: 16 blocks
- Vertical: 8 blocks
- Maximum highlights: 384
- Refresh interval: 160 ms

The signatures and object offsets are derived from the supplied BedrockTools source and were checked against the supplied `libminecraftpe.so`.
