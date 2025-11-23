#!/bin/bash
# Script to verify all compilation fixes are present

echo "================================"
echo "Verifying Compilation Fixes"
echo "================================"
echo ""

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

errors=0

# Check 1: utils.h include in query_utils.cpp
echo -n "1. Checking query_utils.cpp for #include utils.h... "
if grep -q '#include "utils.h"' src/query_utils.cpp; then
    echo -e "${GREEN}✓ PASS${NC}"
else
    echo -e "${RED}✗ FAIL${NC}"
    ((errors++))
fi

# Check 2: utils.h include in syslog_utils.cpp
echo -n "2. Checking syslog_utils.cpp for #include utils.h... "
if grep -q '#include "utils.h"' src/syslog_utils.cpp; then
    echo -e "${GREEN}✓ PASS${NC}"
else
    echo -e "${RED}✗ FAIL${NC}"
    ((errors++))
fi

# Check 3: No %lu in wx_utils files
echo -n "3. Checking for %lu format strings (should be %u)... "
if grep -q 'printf.*%lu.*failures' src/wx_utils_*.cpp; then
    echo -e "${RED}✗ FAIL - Found %lu (should be %u)${NC}"
    ((errors++))
else
    echo -e "${GREEN}✓ PASS${NC}"
fi

# Check 4: Verify %u is present
echo -n "4. Checking for %u format strings... "
if grep -q 'printf.*%u.*failures' src/wx_utils_*.cpp; then
    echo -e "${GREEN}✓ PASS${NC}"
else
    echo -e "${RED}✗ FAIL - Missing %u format strings${NC}"
    ((errors++))
fi

# Check 5: No unused ratio variable
echo -n "5. Checking for unused 'ratio' variable... "
if grep -q 'float ratio' include/wx_utils_airquality.h; then
    echo -e "${RED}✗ FAIL - Found unused variable${NC}"
    ((errors++))
else
    echo -e "${GREEN}✓ PASS${NC}"
fi

# Check 6: Git commit check
echo -n "6. Checking for fix commits... "
if git log --oneline -5 | grep -q "Fix ESP32 compiler warnings"; then
    echo -e "${GREEN}✓ PASS${NC}"
else
    echo -e "${RED}✗ FAIL - Missing fix commits${NC}"
    ((errors++))
fi

echo ""
echo "================================"
if [ $errors -eq 0 ]; then
    echo -e "${GREEN}All checks passed! ✓${NC}"
    echo "You can now compile with: pio run"
else
    echo -e "${RED}Found $errors errors ✗${NC}"
    echo ""
    echo "Action required:"
    echo "1. git pull origin claude/analyze-esp32-aprs-optimization-011CUoLRVX1AuGViiNtaiSmv"
    echo "2. rm -rf .pio"
    echo "3. Run this script again"
fi
echo "================================"

exit $errors
