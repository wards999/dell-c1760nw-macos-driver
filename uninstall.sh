#!/bin/bash
# Remove the Dell C1760nw HBPLv1 driver and its printer queue.
set -u
sudo lpadmin -x Dell_C1760nw 2>/dev/null
sudo rm -rf /Library/Printers/DellHBPL1
sudo rm -f "/Library/Printers/PPDs/Contents/Resources/Dell C1760nw HBPLv1.ppd"
echo "Removed."
