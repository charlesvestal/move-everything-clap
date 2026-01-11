# CLAUDE.md

Instructions for Claude Code when working with this repository.

## Project Overview

CLAP Host module for Move Anything - hosts arbitrary CLAP audio plugins in-process, usable as a sound generator or audio FX in Signal Chain.

## Build Commands

```bash
./scripts/build.sh      # Build with Docker (recommended)
./scripts/install.sh    # Deploy to Move
```

## Architecture

```
src/
  dsp/
    clap_plugin.cpp     # Main plugin (plugin_api_v1), sound generator
    clap_host.c/h       # CLAP discovery, load, process helpers
  chain_audio_fx/
    clap_fx.cpp         # Audio FX wrapper (audio_fx_api_v1)
  ui.js                 # JavaScript UI for plugin selection
  module.json           # Module metadata
  chain_patches/        # Signal Chain presets
third_party/
  clap/include/         # Vendored CLAP headers
```

## Key Implementation Details

### Plugin API

Implements Move Anything plugin_api_v1:
- `on_load`: Scans plugins directory, loads default or stored plugin
- `on_midi`: Converts MIDI to CLAP events and forwards to plugin
- `set_param`: selected_plugin, param_* for CLAP parameters, refresh
- `get_param`: plugin_count, plugin_name_*, param_count, param_name_*, param_value_*
- `render_block`: Calls CLAP process, converts float to int16

### Audio FX API

Implements audio_fx_api_v1 for Signal Chain effects:
- Same CLAP host core as main plugin
- Processes audio input -> CLAP plugin -> audio output

### CLAP Host Core (clap_host.c)

Shared functions:
- `clap_scan_plugins()`: Discover .clap files and read metadata
- `clap_load_plugin()`: Load plugin instance from file
- `clap_process_block()`: Run CLAP process with float buffers
- `clap_param_*()`: Parameter enumeration and control
- `clap_unload_plugin()`: Clean up plugin instance

Host extensions implemented (stub/minimal):
- `thread-check`: Reports main/audio thread status
- `state`: Mark dirty callback (no-op)
- `latency`: Latency changed callback (no-op)
- `tail`: Tail changed callback (no-op)
- `params`: Rescan/clear/flush callbacks (no-op)
- `track-info`: Returns basic track info
- `gui`: GUI request callbacks (returns false, no GUI support)
- `note-name`: Note name changed callback (no-op)
- `audio-ports-config`: Rescan callback (no-op)

These extensions prevent crashes from plugins that expect host callbacks.

### Plugin Discovery

Plugins scanned from `/data/UserData/move-anything/modules/clap/plugins/`.
Each `.clap` file is opened with dlopen, factory enumerated, metadata extracted.

## Signal Chain Integration

Module declares `"chainable": true` and `"component_type": "sound_generator"` in module.json.
Audio FX wrapper installed to `modules/chain/audio_fx/clap/`.
Chain presets installed to `modules/chain/patches/` by install script.

## Move System Constraints

The Move runs glibc 2.35 and GLIBCXX up to 3.4.29. Plugins built on newer systems may fail to load. See BUILDING.md for details on building compatible plugins.

## Testing

```bash
# Header test
cc tests/test_clap_headers.c -Ithird_party/clap/include -o /tmp/test && /tmp/test

# Discovery test (requires fixtures)
cc tests/test_clap_scan.c -Isrc -Ithird_party/clap/include -ldl -o /tmp/test && /tmp/test

# Process test (requires fixtures)
cc tests/test_clap_process.c -Isrc -Ithird_party/clap/include -ldl -o /tmp/test && /tmp/test
```

### On-Device Testing

JavaScript test scripts for testing on Move hardware:

```bash
# Test all plugins (runs on device)
scp tests/test_all_plugins.js ableton@move.local:/data/UserData/move-anything/tests/
ssh ableton@move.local 'cd /data/UserData/move-anything && ./move-anything tests/test_all_plugins.js'

# Test specific plugin indices
# Edit test_batch.js to set test_indices array, then run same way
```
