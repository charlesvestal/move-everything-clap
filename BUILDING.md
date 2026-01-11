# Building CLAP Plugins for Move

The Move requires ARM64 Linux CLAP plugins. Most distributed plugins are x86_64, so you'll need to build from source.

## Move System Constraints

**Critical:** The Move runs an older Linux with:
- glibc 2.35 (NOT 2.36+)
- GLIBCXX up to 3.4.29 (NOT 3.4.30+)

Plugins built on newer systems (Debian 12/Bookworm, Ubuntu 23.04+) may fail with errors like:
```
GLIBC_2.36 not found
GLIBCXX_3.4.30 not found
```

**Solutions:**
1. Build on/for older systems (Debian 11, Ubuntu 22.04)
2. Use `-static-libstdc++ -static-libgcc` linker flags
3. Use the `move-anything-arm64` Docker image (native ARM64)

## Bundling Shared Libraries

If plugins need additional libraries, copy `.so` files to the plugins directory:
```bash
scp libfoo.so.1 ableton@move.local:/data/UserData/move-anything/modules/clap/plugins/
```

The CLAP host sets `LD_LIBRARY_PATH` to include the plugins directory.

---

## SA_Toolkit (Recommended - Easy)

Lightweight plugin framework with built-in headless support. **Best option for simple synths/effects.**

```bash
git clone https://github.com/skei/SA_Toolkit.git
cd SA_Toolkit

# Build with nogui flag
docker run --rm -v $(pwd):/src move-anything-arm64 bash -c '
  cd /src/build
  ./compile -i ../plugins/sa_mael.cpp -o sa_mael.clap -f clap -g nogui
'

# Copy to Move (no dependencies needed!)
scp build/sa_mael.clap ableton@move.local:/data/UserData/move-anything/modules/clap/plugins/
```

Available plugins: `sa_mael` (synth), `sa_demo`, and others in `plugins/` directory.

---

## LSP Plugins (194 Effects - Headless Build)

LSP provides many high-quality effects. Build without UI using `FEATURES="clap"`.

```bash
git clone https://github.com/lsp-plugins/lsp-plugins.git
cd lsp-plugins

# Configure for headless CLAP-only build
make config FEATURES="clap"
make fetch  # Downloads ~50 submodules

# Build (native ARM64 in Docker)
docker run --rm -v $(pwd):/src -w /src move-anything-arm64 bash -c '
  make -j4
'

# Copy plugin and audio codec dependencies
scp .build/target/lsp-plugins.clap ableton@move.local:/data/UserData/move-anything/modules/clap/plugins/

# LSP needs audio codec libs (extract from Docker or build static)
docker run --rm move-anything-arm64 bash -c 'cat /lib/aarch64-linux-gnu/libFLAC.so.12' > libFLAC.so.12
docker run --rm move-anything-arm64 bash -c 'cat /lib/aarch64-linux-gnu/libvorbis.so.0' > libvorbis.so.0
docker run --rm move-anything-arm64 bash -c 'cat /lib/aarch64-linux-gnu/libvorbisenc.so.2' > libvorbisenc.so.2
docker run --rm move-anything-arm64 bash -c 'cat /lib/aarch64-linux-gnu/libogg.so.0' > libogg.so.0
docker run --rm move-anything-arm64 bash -c 'cat /lib/aarch64-linux-gnu/libopus.so.0' > libopus.so.0
scp lib*.so.* ableton@move.local:/data/UserData/move-anything/modules/clap/plugins/
```

---

## clap-plugins (Official Examples)

The official CLAP example plugins include synthesizers, effects, and test utilities.

```bash
git clone --recursive https://github.com/free-audio/clap-plugins.git
cd clap-plugins

# Build with Docker (use native ARM64 image)
docker run --rm -v $(pwd):/src -w /src move-anything-arm64 bash -c '
  cmake -B build-arm64 \
    -DCMAKE_BUILD_TYPE=Release \
    -DCLAP_PLUGINS_HEADLESS=ON
  cmake --build build-arm64 --config Release
'

# Copy to Move
scp build-arm64/clap-plugins.clap ableton@move.local:/data/UserData/move-anything/modules/clap/plugins/
```

This provides ~20 plugins including **CLAP Synth** (a working synthesizer).

---

## Known Working Plugins

| Plugin | Source | Type | Notes |
|--------|--------|------|-------|
| sa_mael | SA_Toolkit | Synth | `-g nogui`, no deps |
| sa_demo | SA_Toolkit | Synth | `-g nogui`, no deps |
| LSP Plugins (194) | lsp-plugins | Effects | Headless build, needs audio libs |
| CLAP Synth | clap-plugins | Synth | Full synth, 25 params |
| CLAP SVF | clap-plugins | Effect | Filter |
| Gain | clap-plugins | Effect | Simple gain |

## Known Problematic Plugins

| Plugin | Issue | Workaround |
|--------|-------|------------|
| Surge XT | JUCE, needs GLIBCXX 3.4.30+ | Static link or native port |
| Vital | JUCE, complex deps | Native port recommended |
| ChowTapeModel | JUCE, glibc mismatch | Static link or native port |
| Any GUI plugin | X11/Cairo/OpenGL deps | Find headless build option |
| Commercial plugins | x86_64 only, no source | None |

## JUCE-Based Plugins (Difficult)

JUCE plugins (Surge XT, Vital, ChowTapeModel) have challenges:
1. JUCE has GUI code deeply integrated
2. Build environments typically have newer glibc than Move
3. Even headless builds may link against newer system libs

**Options:**
1. Build with `-static-libstdc++ -static-libgcc` (may still fail on glibc)
2. Build in older Docker image (Debian 11 / Ubuntu 20.04)
3. Port DSP core to native Move Anything plugin format (recommended for important synths)

---

## Building the CLAP Host Module

To build this module (the host, not plugins):

```bash
./scripts/build.sh      # Build with Docker (recommended)
./scripts/install.sh    # Deploy to Move
```
