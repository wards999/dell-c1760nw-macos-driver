# Dell C1760nw printer driver for Mac (Apple Silicon)
<img width="200" height="200" alt="image" src="https://github.com/user-attachments/assets/d0a015f6-ed66-4748-be9f-1d9476df795c" />

**A working macOS driver for the Dell C1760nw color laser printer** on Apple
Silicon Macs (M1, M2, M3, M4 and later). Dell's own Mac driver stopped
working years ago, and the printer has no AirPrint. If your Dell C1760nw
won't print from your Mac, or macOS can't find a driver for it, this fixes
it.

It's a native CUPS driver: no Rosetta, Windows or Linux print server
needed. The driver should also work with the C1760nw's siblings, which use
the same HBPLv1 printer language:

- Dell 1250c, Dell C1660w
- Xerox Phaser 6010 / 6010N
- Fuji Xerox DocuPrint CP105b / CP205 / CP205w
- Epson AcuLaser C1700

**Features:**

- Runs natively on Apple Silicon: no Rosetta, no Ghostscript, nothing else
  to install.
- Supports colour and black-and-white printing, paper type, and the common
  paper and envelope sizes.
- Works from any app, like any other printer.

> **Status: tested and working on a real Dell C1760nw** from an Apple
> Silicon Mac, including toner levels in System Settings. The driver's
> output is byte-for-byte identical to the
> [foo2hbpl1](https://github.com/mikerr/foo2zjs) Linux driver, and it
> passes automated print tests on macOS on every change. Owners of the
> sibling printers listed above: please [report your results](../../issues).

## Problems this solves

- Dell C1760nw **won't print from a Mac**, or prints nothing at all.
- The Dell C1760nw Mac driver **won't install on recent macOS** (Big Sur
  and later), or doesn't work on Apple Silicon.
- The **"Dell C1760nw Color Printer" driver is missing** when you add the
  printer in System Settings → Printers & Scanners.
- Generic PostScript or PCL drivers print garbage or nothing. The printer
  only understands Dell's own host-based language (HBPL).

## Install

You'll need your printer's IP address. Print a configuration page from the
printer's front panel menu to find it. It's worth giving the printer a fixed
address in your router.

1. **Download** the zip from the [latest release](../../releases/latest) and
   unzip it.
2. **Install Apple's command line tools** (only needed once). Open
   Terminal and run:
   ```bash
   xcode-select --install
   ```
3. **Install the driver.** In Terminal, type `cd ` (with a space), drag the
   unzipped folder into the Terminal window, press Return, then run:
   ```bash
   bash install.sh
   ```
   Enter your Mac password when asked. This compiles the driver on your Mac
   and installs it.
4. **Add the printer** in System Settings → **Printers & Scanners** →
   **Add Printer, Scanner or Fax…**:
   - Click the **IP** tab (globe icon).
   - **Address:** your printer's IP, e.g. `192.168.1.50`
   - **Protocol:** **HP Jetdirect – Socket**
   - **Use:** **Select Software…**, search for **C1760**, and choose
     **Dell C1760nw HBPLv1**.
   - Click **Add**.

   Or skip this step by giving the IP to the installer:
   `bash install.sh 192.168.1.50`

## Printing in colour or black-and-white

In the Print dialog, open the printer options (in some apps under
*Printer Options* or *Printer Features*):

| Option | Choices |
|---|---|
| Color Mode | **Color** (default) · **Color (alternate conversion)**, try this if colours look off · **Grayscale**, black toner only |
| Paper Type | Plain · Bond · Lightweight Cardstock · Glossy Cardstock · Labels · Envelope · Recycled |
| Paper Size | Letter · Legal · A4 · A5 · Executive · B5 · Folio · Envelopes #10, Monarch, C5, DL |

To make Grayscale the default, go to System Settings → Printers & Scanners →
the printer → Options & Supplies → Options. Or save a preset in the Print
dialog.


## Troubleshooting

- **Nothing prints:** check the queue (System Settings → Printers &
  Scanners → the printer → Printer Queue) and the log:
  `tail -50 /var/log/cups/error_log`
- **Still nothing:** remove the printer and add it again with protocol
  **Line Printer Daemon – LPD**.
- **Garbled or wrong colours:** try *Color (alternate conversion)*, and
  open an issue with a photo of the page.
- **To test without printing,** capture what the driver would send to the
  printer:
  ```bash
  cupsfilter -p "/Library/Printers/PPDs/Contents/Resources/Dell C1760nw HBPLv1.ppd" \
      -m printer/foo ~/Desktop/some.pdf > out.hbpl
  ```
  A file of a hundred KB or more per page is normal. An empty file means
  something failed, and the reason appears in the Terminal output.
- **Detailed logs:** run `sudo cupsctl --debug-logging`, print again, and
  check the log. Turn logging back off with
  `sudo cupsctl --no-debug-logging`.

## Uninstall

```bash
bash uninstall.sh
```

## How it works

macOS renders each page itself. A small filter, `src/rastertohbpl1.c`,
encodes the page in the printer's HBPLv1 language. The encoder is Dave
Coffin's from [foo2zjs](https://github.com/mikerr/foo2zjs), unchanged
except for a bounds fix. `ppd/` holds the printer description file that
tells macOS about the printer and its options.

| Installed file | Location |
|---|---|
| Filter | `/Library/Printers/DellHBPL1/Filter/rastertohbpl1` |
| PPD | `/Library/Printers/PPDs/Contents/Resources/Dell C1760nw HBPLv1.ppd` |

### Tests

On every push, GitHub Actions builds and installs the driver on an Apple
Silicon Mac. It then prints `tests/testpage.pdf` through macOS's own print
pipeline (`tests/check.sh`), decodes the result with foo2zjs's
`hbpldecode`, and checks page count, page size, colour mode and paper type.

To make a release, publish a GitHub release with a new version tag
(e.g. `v1.0.1`). Actions builds, tests, and attaches the download zip.

## Licence

GPL v2 or later (see [COPYING](COPYING)), the same as foo2zjs.
