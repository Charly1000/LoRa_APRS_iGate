#!/bin/bash
# FORCE FIX - Directly patches all compilation errors
# Run this if git pull isn't working properly

echo "======================================"
echo "FORCE FIXING Compilation Errors"
echo "======================================"
echo ""

# Fix 1: Add #include "utils.h" to query_utils.cpp (after line 23)
echo "Fix 1: Adding #include to query_utils.cpp..."
if ! grep -q '#include "utils.h"' src/query_utils.cpp; then
    sed -i '23a#include "utils.h"' src/query_utils.cpp
    echo "  ✓ Added #include \"utils.h\""
else
    echo "  ✓ Already present"
fi

# Fix 2: Add #include "utils.h" to syslog_utils.cpp (after line 23)
echo "Fix 2: Adding #include to syslog_utils.cpp..."
if ! grep -q '#include "utils.h"' src/syslog_utils.cpp; then
    sed -i '23a#include "utils.h"' src/syslog_utils.cpp
    echo "  ✓ Added #include \"utils.h\""
else
    echo "  ✓ Already present"
fi

# Fix 3: Change %lu to %u in wx_utils_robust.cpp
echo "Fix 3: Fixing format strings in wx_utils_robust.cpp..."
sed -i 's/printf("\[WX\] Attempting recovery after %lu failures/printf("[WX] Attempting recovery after %u failures/g' src/wx_utils_robust.cpp
sed -i 's/printf("\[WX\] ✗ Sensor read failed (consecutive: %lu)/printf("[WX] ✗ Sensor read failed (consecutive: %u)/g' src/wx_utils_robust.cpp
echo "  ✓ Changed %lu to %u"

# Fix 4: Change %lu to %u in wx_utils_safe.cpp
echo "Fix 4: Fixing format strings in wx_utils_safe.cpp..."
sed -i 's/printf("\[WX\] Attempting recovery after %lu failures/printf("[WX] Attempting recovery after %u failures/g' src/wx_utils_safe.cpp
sed -i 's/printf("\[WX\] ✗ Sensor read failed (consecutive: %lu)/printf("[WX] ✗ Sensor read failed (consecutive: %u)/g' src/wx_utils_safe.cpp
echo "  ✓ Changed %lu to %u"

# Fix 5: Change %lu to %u in wx_utils_with_airquality.cpp
echo "Fix 5: Fixing format strings in wx_utils_with_airquality.cpp..."
sed -i 's/printf("\[WX\] Attempting recovery after %lu failures/printf("[WX] Attempting recovery after %u failures/g' src/wx_utils_with_airquality.cpp
sed -i 's/printf("\[WX\] ✗ Sensor read failed (consecutive: %lu)/printf("[WX] ✗ Sensor read failed (consecutive: %u)/g' src/wx_utils_with_airquality.cpp
echo "  ✓ Changed %lu to %u"

# Fix 6: Remove unused 'ratio' variable from wx_utils_airquality.h
echo "Fix 6: Removing unused variable from wx_utils_airquality.h..."
if grep -q 'float ratio = gasResistanceKOhm / baselineKOhm' include/wx_utils_airquality.h; then
    sed -i '/float ratio = gasResistanceKOhm \/ baselineKOhm;/d' include/wx_utils_airquality.h
    # Also remove the comment above it if it exists
    sed -i '/\/\/ Normalize to baseline/d' include/wx_utils_airquality.h
    echo "  ✓ Removed unused variable"
else
    echo "  ✓ Already removed"
fi

echo ""
echo "======================================"
echo "All fixes applied!"
echo "======================================"
echo ""
echo "Now run:"
echo "  rm -rf .pio"
echo "  pio run"
echo ""
