# Move Anything CLAP Host Module

A Move Anything module that hosts arbitrary CLAP plugins in-process, usable as a standalone sound generator or as a component in Signal Chain.

## Features

- Load any CLAP plugin from `/data/UserData/move-anything/modules/clap/plugins/`
- Browse and select plugins via the QuickJS UI
- Control plugin parameters via encoder banks
- Usable as sound generator in Signal Chain patches
- CLAP audio FX plugins can be used in the chain's audio FX slot

## Important: Plugin Compatibility

**The Move has specific requirements for CLAP plugins:**

- ARM64 Linux (aarch64) binaries only
- No GUI dependencies (X11, Cairo, OpenGL, GTK, etc.)
- glibc 2.35 or older (Move's system library version)
- GLIBCXX 3.4.29 or older

Most pre-built CLAP plugins are x86_64 and include GUI code, so you'll typically need to **build from source** with headless options.

### When to Use CLAP vs Native Ports

| Use CLAP Host | Consider Native Port |
|---------------|---------------------|
| Plugin builds easily headless | Complex dependency chains |
| Minimal dependencies | JUCE-based with glibc issues |
| Effects and simple synths | Major synths you'll use often |
| Quick experimentation | Need deep Move UI integration |

For complex synths like Surge XT or Vital, the effort to resolve glibc/libstdc++ compatibility issues is often comparable to porting the DSP core natively.

## Installation

### From Release

```bash
./scripts/install.sh
```

### From Source

```bash
./scripts/build.sh
./scripts/install.sh
```

## Usage

1. Copy `.clap` plugin files to `/data/UserData/move-anything/modules/clap/plugins/` on the Move
2. If plugins need shared libraries, copy `.so` files to the same directory
3. Select the CLAP module from the host menu
4. Use the UI to browse and load plugins
5. Adjust parameters with the encoders

## Building Plugins

See [BUILDING.md](BUILDING.md) for detailed build instructions for specific plugin frameworks (SA_Toolkit, LSP Plugins, clap-plugins, etc.).

## License

MIT License - see LICENSE file
