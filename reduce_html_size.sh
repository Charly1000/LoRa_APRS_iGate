#!/bin/bash
# Remove large sections to reduce HTML file size

echo "Original size:"
ls -lh data_embed/index.html | awk '{print "  " $5}'

# Remove Air Quality section (lines 1340-1420)
sed -i '1340,1420d' data_embed/index.html

# Adjust line numbers after deletion (-80 lines)
# Remove Custom Text section (was 2181-2284, now 2101-2204)
sed -i '2101,2204d' data_embed/index.html

echo ""
echo "New size:"
ls -lh data_embed/index.html | awk '{print "  " $5}'

echo ""
wc -l data_embed/index.html | awk '{print "Lines: " $1}'
