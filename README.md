# Dell C1760nw driver for macOS (Apple Silicon)

A native macOS printer driver for the **Dell C1760nw**. Dell's own Mac
driver stopped working years ago and there is no AirPrint support. The
driver should also work with the printer's siblings, which use the same
HBPLv1 printer language: Dell 1250c / C1660w, Xerox Phaser 6010, Fuji Xerox
DocuPrint CP105b / CP205, and Epson AcuLaser C1700.

- Runs natively on Apple Silicon: no Rosetta, no Ghostscript, nothing else
  to install.
- Supports colour and black-and-white printing, paper type, and the common
  paper and envelope sizes.
- Works from any app, like any other printer.

> **Status:** the driver's output is byte-for-byte identical to the
> [foo2hbpl1](https://github.com/mikerr/foo2zjs) Linux driver, which users
> report working on these printers. It also passes automated tests on
> macOS. 

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

To make a release, push a version tag. Actions builds, tests, and publishes
the zip:

```bash
git tag v1.0.0 && git push origin v1.0.0
```

## Licence

GPL v2 or later (see [COPYING](COPYING)), the same as foo2zjs.
