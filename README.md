# Move Everything CLAP Host Module

> **⚠️ Proof of Concept**
>
> This module is experimental. There are currently no known released third-party CLAP plugins that can be loaded without modification. Most plugins require rebuilding from source with headless options and ARM64 cross-compilation. See "Plugin Compatibility" below for details.

A Move Everything module that hosts arbitrary CLAP plugins in-process, usable as a standalone sound generator or as a component in Signal Chain.

## Prerequisites

- [Move Everything](https://github.com/charlesvestal/move-everything) installed on your Ableton Move
- SSH access enabled: http://move.local/development/ssh
- ARM64 Linux CLAP plugins (see "Plugin Compatibility" below)

## Features

- Load any CLAP plugin from `/data/UserData/move-anything/modules/sound_generators/clap/plugins/`
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

1. Copy `.clap` plugin files to `/data/UserData/move-anything/modules/sound_generators/clap/plugins/` on the Move
2. If plugins need shared libraries, copy `.so` files to the same directory
3. Select the CLAP module from the host menu
4. Use the UI to browse and load plugins
5. Adjust parameters with the encoders

## Building Plugins

See [BUILDING.md](BUILDING.md) for detailed build instructions for specific plugin frameworks (SA_Toolkit, LSP Plugins, clap-plugins, etc.).

## Bundled Plugins

### Airwindows

This module bundles [Airwindows](https://github.com/airwindows/airwindows) by Chris Johnson — 498 high-quality open source audio effects as a single CLAP plugin. The CLAP build is from [airwin2rack](https://github.com/baconpaul/airwin2rack) by Bacon Pauls.

Airwindows is licensed under the MIT License. See [LICENSES/AIRWINDOWS.txt](LICENSES/AIRWINDOWS.txt) for full license text.

## License

MIT License - see LICENSE file

## AI Assistance Disclaimer

This module is part of Move Everything and was developed with AI assistance, including Claude, Codex, and other AI assistants.

All architecture, implementation, and release decisions are reviewed by human maintainers.  
AI-assisted content may still contain errors, so please validate functionality, security, and license compatibility before production use.
