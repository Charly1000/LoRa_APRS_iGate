#!/bin/bash
# Temporarily comment out large sections to reduce HTML size

# Comment out Air Quality section (lines 1340-1420)
sed -i '1340,1420 s/^/<!-- TEMP_DISABLED /' data_embed/index.html
sed -i '1420 a -->' data_embed/index.html

# Comment out Custom Text section (lines 2181-2284)  
sed -i '2181,2284 s/^/<!-- TEMP_DISABLED /' data_embed/index.html
sed -i '2284 a -->' data_embed/index.html

echo "✓ Große Sektionen auskommentiert"
echo "  - Air Quality (80 Zeilen)"
echo "  - Custom Text (103 Zeilen)"
echo ""
echo "Neue Dateigröße:"
ls -lh data_embed/index.html | awk '{print "  " $5 " (" $9 ")"}'
