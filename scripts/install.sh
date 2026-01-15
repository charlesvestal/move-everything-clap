#!/bin/bash
# Install CLAP Host module to Move
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"

cd "$REPO_ROOT"

if [ ! -d "dist/clap" ]; then
    echo "Error: dist/clap not found. Run ./scripts/build.sh first."
    exit 1
fi

echo "=== Installing CLAP Host Module ==="

# Deploy main module to Move
echo "Copying module to Move..."
scp -r dist/clap ableton@move.local:/data/UserData/move-anything/modules/

# Deploy audio FX plugin for chain
if [ -f "dist/chain_audio_fx/clap/clap.so" ]; then
    echo "Installing CLAP audio FX for Signal Chain..."
    ssh ableton@move.local "mkdir -p /data/UserData/move-anything/modules/chain/audio_fx/clap"
    scp dist/chain_audio_fx/clap/clap.so ableton@move.local:/data/UserData/move-anything/modules/chain/audio_fx/clap/
fi

# Install chain presets if they exist
if [ -d "src/chain_patches" ] && ls src/chain_patches/*.json 1> /dev/null 2>&1; then
    echo "Installing chain presets..."
    scp src/chain_patches/*.json ableton@move.local:/data/UserData/move-anything/modules/chain/patches/
fi

# Create plugins directory on device
echo "Creating plugins directory..."
ssh ableton@move.local "mkdir -p /data/UserData/move-anything/modules/clap/plugins"

# Set permissions so Module Store can update later
echo "Setting permissions..."
ssh ableton@move.local "chmod -R a+rw /data/UserData/move-anything/modules/clap"
ssh ableton@move.local "chmod -R a+rw /data/UserData/move-anything/modules/chain/audio_fx/clap" 2>/dev/null || true

echo ""
echo "=== Install Complete ==="
echo "Module installed to: /data/UserData/move-anything/modules/clap/"
echo "Audio FX installed to: /data/UserData/move-anything/modules/chain/audio_fx/clap/"
echo ""
echo "Add your .clap plugin files to:"
echo "  /data/UserData/move-anything/modules/clap/plugins/"
echo ""
echo "Restart Move Anything to load the new module."
