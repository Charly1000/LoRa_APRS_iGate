#!/bin/bash
echo "======================================"
echo "Web Server Diagnostic"
echo "======================================"
echo ""

# Check web_utils files
echo "1. Checking web_utils files:"
ls -lh src/web_utils.* 2>/dev/null || echo "  ⚠ web_utils files not found"
echo ""

# Check data_embed directory
echo "2. Checking web content files:"
if [ -d "data_embed" ]; then
    ls -lh data_embed/*.html 2>/dev/null | awk '{print "  " $5 " - " $9}'
    echo ""
    echo "  Total size:"
    du -sh data_embed/
else
    echo "  ⚠ data_embed directory not found"
fi
echo ""

# Check if we modified web_utils
echo "3. Checking for modifications in web_utils:"
git log --oneline --all -- src/web_utils.cpp | head -5
echo ""

# Check configuration.cpp for customText handling
echo "4. Checking configuration.cpp modifications:"
git log --oneline --all -- src/configuration.cpp | head -5
echo ""

# Check firmware size
echo "5. Checking firmware size:"
if [ -f ".pio/build/ttgo-lora32-v21/firmware.bin" ]; then
    ls -lh .pio/build/ttgo-lora32-v21/firmware.bin | awk '{print "  Firmware: " $5}'
    ls -lh .pio/build/ttgo-lora32-v21/firmware.elf | awk '{print "  ELF: " $5}'
else
    echo "  ℹ Firmware not built yet"
fi
echo ""

echo "======================================"
echo "Recommendations:"
echo "======================================"
echo "1. Check if web_utils.cpp was modified"
echo "2. Verify data_embed/index.html size"
echo "3. Check Serial output for errors"
echo "======================================"
