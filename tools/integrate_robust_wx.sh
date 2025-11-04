#!/bin/bash
# Quick Integration Script for Robust Weather Sensor Code
# This script backs up original files and integrates the robust version

set -e  # Exit on error

echo "╔═══════════════════════════════════════════════════════════╗"
echo "║  LoRa APRS iGate - Robust WX Sensor Integration Script   ║"
echo "╚═══════════════════════════════════════════════════════════╝"
echo ""

# Check if we're in the right directory
if [ ! -f "platformio.ini" ]; then
    echo "❌ Error: platformio.ini not found!"
    echo "   Please run this script from the LoRa_APRS_iGate root directory"
    exit 1
fi

echo "📁 Project directory: $(pwd)"
echo ""

# Create backup directory
BACKUP_DIR="backup_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$BACKUP_DIR"
echo "✓ Created backup directory: $BACKUP_DIR"

# Backup original files
echo ""
echo "📦 Backing up original files..."

if [ -f "src/wx_utils.cpp" ]; then
    cp src/wx_utils.cpp "$BACKUP_DIR/"
    echo "  ✓ Backed up src/wx_utils.cpp"
else
    echo "  ⚠️  src/wx_utils.cpp not found (okay if new install)"
fi

if [ -f "include/wx_utils.h" ]; then
    cp include/wx_utils.h "$BACKUP_DIR/"
    echo "  ✓ Backed up include/wx_utils.h"
else
    echo "  ⚠️  include/wx_utils.h not found (okay if new install)"
fi

# Integration method selection
echo ""
echo "Select integration method:"
echo "  1) Replace original files (recommended)"
echo "  2) Create parallel version (wx_utils_robust.*)"
echo "  3) Cancel"
echo ""
read -p "Choice [1-3]: " choice

case $choice in
    1)
        echo ""
        echo "🔄 Replacing original files with robust version..."

        if [ ! -f "src/wx_utils_robust.cpp" ]; then
            echo "❌ Error: src/wx_utils_robust.cpp not found!"
            echo "   Please ensure the robust version files are in the repository"
            exit 1
        fi

        cp src/wx_utils_robust.cpp src/wx_utils.cpp
        cp include/wx_utils_robust.h include/wx_utils.h

        echo "  ✓ Replaced src/wx_utils.cpp"
        echo "  ✓ Replaced include/wx_utils.h"
        ;;

    2)
        echo ""
        echo "📋 Keeping both versions (parallel)..."

        if [ ! -f "src/wx_utils_robust.cpp" ]; then
            echo "ℹ️  Robust version already exists"
        else
            echo "  ✓ Robust version available as wx_utils_robust.*"
        fi

        echo ""
        echo "Manual steps required:"
        echo "  1. Open src/LoRa_APRS_iGate.cpp"
        echo "  2. Change: #include \"wx_utils.h\""
        echo "     To:     #include \"wx_utils_robust.h\""
        echo "  3. Rebuild project"
        ;;

    3)
        echo ""
        echo "❌ Cancelled. No changes made."
        rmdir "$BACKUP_DIR" 2>/dev/null || true
        exit 0
        ;;

    *)
        echo ""
        echo "❌ Invalid choice. Cancelled."
        rmdir "$BACKUP_DIR" 2>/dev/null || true
        exit 1
        ;;
esac

# Test compilation
echo ""
read -p "Test compilation now? [y/N]: " test_compile

if [[ $test_compile =~ ^[Yy]$ ]]; then
    echo ""
    echo "🔨 Testing compilation..."

    if command -v pio &> /dev/null; then
        pio run --target clean
        pio run

        if [ $? -eq 0 ]; then
            echo ""
            echo "✓ Compilation successful!"
        else
            echo ""
            echo "❌ Compilation failed!"
            echo "   Restoring backup..."

            if [ -f "$BACKUP_DIR/wx_utils.cpp" ]; then
                cp "$BACKUP_DIR/wx_utils.cpp" src/
                cp "$BACKUP_DIR/wx_utils.h" include/
                echo "  ✓ Original files restored"
            fi

            exit 1
        fi
    else
        echo "⚠️  PlatformIO not found. Skipping compilation test."
    fi
fi

# Summary
echo ""
echo "╔═══════════════════════════════════════════════════════════╗"
echo "║                    Integration Complete                   ║"
echo "╚═══════════════════════════════════════════════════════════╝"
echo ""
echo "✓ Backup saved to: $BACKUP_DIR"
echo ""
echo "Next steps:"
echo "  1. Upload firmware to ESP32"
echo "  2. Monitor serial output for WX sensor status"
echo "  3. Test with disconnected sensor to verify fallback"
echo ""
echo "To restore original:"
echo "  cp $BACKUP_DIR/wx_utils.cpp src/"
echo "  cp $BACKUP_DIR/wx_utils.h include/"
echo "  pio run"
echo ""
echo "Documentation: See WEATHER_SENSOR_FALLBACK.md"
echo ""
