First release of a native macOS driver for the Dell C1760nw (and other
HBPLv1 printers), built for Apple Silicon.

**Install:** download the zip below, unzip it, then in Terminal run
`bash install.sh` from the unzipped folder. Then add the printer in System
Settings. Full steps are in the README.

**Status:**
- The driver's output matches the Linux foo2hbpl1 driver byte for byte.
- On an Apple Silicon Mac, test pages printed through macOS's own print
  system come out correctly in colour, grayscale, Letter and A4, with the
  right paper type.
- It has **not yet been confirmed on a real printer**. Please report
  results, good or bad, in Issues.
