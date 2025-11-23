#!/bin/bash
# Comprehensive PlatformIO cache cleanup script

echo "=== PlatformIO Complete Cache Cleanup ==="

# 1. Remove project build directory
echo "1. Removing .pio directory..."
rm -rf .pio

# 2. Remove PlatformIO global cache
echo "2. Removing PlatformIO global cache..."
rm -rf ~/.platformio/.cache
rm -rf ~/.platformio/packages/.cache

# 3. Touch all source files to force recompilation
echo "3. Touching all source files..."
find src include -type f \( -name "*.cpp" -o -name "*.h" \) -exec touch {} \;

# 4. Clean platformio
echo "4. Running platformio clean..."
pio run --target clean 2>/dev/null || echo "   (pio command not available, skipping)"

echo ""
echo "=== Cleanup Complete ==="
echo ""
echo "Now run: pio run"
echo "Or in your IDE: Clean All → Build"
