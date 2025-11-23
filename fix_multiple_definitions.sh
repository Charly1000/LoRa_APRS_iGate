#!/bin/bash
echo "======================================"
echo "Fixing Multiple Definition Errors"
echo "======================================"
echo ""

# Backup directory for alternative implementations
mkdir -p backup_wx_implementations

echo "Creating backup of alternative wx_utils implementations..."

# Move alternative implementations to backup
if [ -f src/wx_utils_robust.cpp ]; then
    mv src/wx_utils_robust.cpp backup_wx_implementations/
    echo "  ✓ Moved wx_utils_robust.cpp to backup/"
fi

if [ -f src/wx_utils_safe.cpp ]; then
    mv src/wx_utils_safe.cpp backup_wx_implementations/
    echo "  ✓ Moved wx_utils_safe.cpp to backup/"
fi

if [ -f src/wx_utils_with_airquality.cpp ]; then
    # This is the version WITH air quality - we want to keep this one!
    echo "  ℹ  Keeping wx_utils_with_airquality.cpp (has air quality support)"
    
    # Backup original wx_utils.cpp
    if [ -f src/wx_utils.cpp ]; then
        mv src/wx_utils.cpp backup_wx_implementations/wx_utils_original.cpp
        echo "  ✓ Backed up original wx_utils.cpp"
    fi
    
    # Rename air quality version to be the main implementation
    mv src/wx_utils_with_airquality.cpp src/wx_utils.cpp
    echo "  ✓ Renamed wx_utils_with_airquality.cpp → wx_utils.cpp"
fi

echo ""
echo "======================================"
echo "✅ Fixed! Now you have:"
echo "  - src/wx_utils.cpp (with AIR QUALITY support)"
echo "  - backup_wx_implementations/ (other versions)"
echo ""
echo "Run: rm -rf .pio && pio run"
echo "======================================"
