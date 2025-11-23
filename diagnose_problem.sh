#!/bin/bash
echo "======================================"
echo "DIAGNOSTIC REPORT"
echo "======================================"
echo ""
echo "Current directory:"
pwd
echo ""
echo "Git branch:"
git branch --show-current
echo ""
echo "Last commit:"
git log --oneline -1
echo ""
echo "Git status:"
git status -s
echo ""
echo "======================================"
echo "Checking problematic files:"
echo "======================================"
echo ""
echo "1. query_utils.cpp line 24:"
sed -n '24p' src/query_utils.cpp
echo ""
echo "2. syslog_utils.cpp line 24:"
sed -n '24p' src/syslog_utils.cpp
echo ""
echo "3. wx_utils_robust.cpp line 335:"
sed -n '335p' src/wx_utils_robust.cpp
echo ""
echo "4. wx_utils_airquality.h lines 220-222:"
sed -n '220,222p' include/wx_utils_airquality.h
echo ""
echo "======================================"
echo "Checking for errors:"
echo "======================================"
echo ""
errors=0

if grep -q '#include "utils.h"' src/query_utils.cpp; then
    echo "✓ query_utils.cpp has #include"
else
    echo "✗ query_utils.cpp MISSING #include"
    ((errors++))
fi

if grep -q '#include "utils.h"' src/syslog_utils.cpp; then
    echo "✓ syslog_utils.cpp has #include"
else
    echo "✗ syslog_utils.cpp MISSING #include"
    ((errors++))
fi

if grep -q '%u.*failures' src/wx_utils_robust.cpp; then
    echo "✓ wx_utils_robust.cpp has %u"
else
    echo "✗ wx_utils_robust.cpp still has %lu"
    ((errors++))
fi

if grep -q 'float ratio' include/wx_utils_airquality.h; then
    echo "✗ wx_utils_airquality.h has unused ratio"
    ((errors++))
else
    echo "✓ wx_utils_airquality.h no ratio variable"
fi

echo ""
echo "======================================"
if [ $errors -eq 0 ]; then
    echo "FILES ARE CORRECT!"
    echo "Problem is .pio cache. Run: rm -rf .pio && pio run"
else
    echo "FOUND $errors PROBLEMS IN FILES"
    echo ""
    echo "YOUR FILES ARE OUT OF DATE!"
    echo ""
    echo "RUN THESE COMMANDS:"
    echo "  git fetch origin claude/analyze-esp32-aprs-optimization-011CUoLRVX1AuGViiNtaiSmv"
    echo "  git reset --hard origin/claude/analyze-esp32-aprs-optimization-011CUoLRVX1AuGViiNtaiSmv"
    echo "  rm -rf .pio"
    echo "  pio run"
fi
echo "======================================"
