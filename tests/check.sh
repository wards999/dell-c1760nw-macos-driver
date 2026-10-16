#!/bin/bash
# Run tests/testpage.pdf through the installed driver with macOS's own
# print pipeline (cupsfilter) and check the HBPLv1 output by decoding it.
# Usage: tests/check.sh path/to/hbpldecode
set -u
cd "$(dirname "$0")"
DECODE=${1:?usage: check.sh path/to/hbpldecode}
PPD="${PPD:-/Library/Printers/PPDs/Contents/Resources/Dell C1760nw HBPLv1.ppd}"
OUT=out
rm -rf "$OUT" && mkdir -p "$OUT"
fail=0

check() {	# name, expected render mode, expected page dims, cupsfilter options...
    name=$1 mode=$2 dims=$3; shift 3
    cupsfilter -p "$PPD" -m printer/foo "$@" testpage.pdf \
	> "$OUT/$name.hbpl" 2> "$OUT/$name.log"
    "$DECODE" -d "$OUT/$name" "$OUT/$name.hbpl" > "$OUT/$name.decode.txt" 2>&1
    pages=$(ls "$OUT/$name"-*.p?m 2>/dev/null | wc -l | tr -d ' ')
    got=$(head -c 24 "$OUT/$name"-01.p?m 2>/dev/null | tr '\n' ' ' | awk '{print $2, $3}')
    if grep -aq "RENDERMODE=$mode" "$OUT/$name.hbpl" && [ "$pages" = 2 ] \
	&& [ "$got" = "$dims" ]; then
	echo "PASS $name ($(wc -c < "$OUT/$name.hbpl" | tr -d ' ') bytes, $pages pages, $got)"
    else
	echo "FAIL $name: pages=$pages dims='$got' (want '$dims')"
	tail -20 "$OUT/$name.log"
	fail=1
    fi
    rm -f "$OUT/$name"-*.p?m	# decoded pages are ~100 MB each
}

check color COLOR     "5104 6600" -o ColorModel=CMYK
check rgb   COLOR     "5104 6600" -o ColorModel=RGB
check gray  GRAYSCALE "5104 6600" -o ColorModel=Gray -o MediaType=Labels
check a4    COLOR     "4968 7016" -o PageSize=A4
grep -aq 'MSIP=LABEL' "$OUT/gray.hbpl" || { echo "FAIL paper type not sent"; fail=1; }
exit $fail
