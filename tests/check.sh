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

# In GitHub Actions, also report results as annotations (readable via the API).
report() {	# level, title, message-file-or-text
    [ -n "${GITHUB_ACTIONS:-}" ] || return 0
    msg=$( { [ -f "$3" ] && { grep -iE 'error|warn|filter|rastertohbpl|unable|denied|sandbox|chain|mime' "$3" | head -40; echo ---; tail -5 "$3"; } || echo "$3"; } | sed 's/%/%25/g' | awk '{printf "%s%%0A", $0}')
    echo "::$1 title=$2::$msg"
}

check() {	# name, expected render mode, expected paper size, cupsfilter options...
    name=$1 mode=$2 paper=$3; shift 3
    cupsfilter -p "$PPD" -m printer/foo -e "$@" testpage.pdf \
	> "$OUT/$name.hbpl" 2> "$OUT/$name.log"
    "$DECODE" -d "$OUT/$name" "$OUT/$name.hbpl" > "$OUT/$name.decode.txt" 2>&1
    pages=$(ls "$OUT/$name"-*.p?m 2>/dev/null | wc -l | tr -d ' ')
    got=$(head -c 24 "$OUT/$name"-01.p?m 2>/dev/null | tr '\n' ' ' | awk '{print $2, $3}')
    if grep -aq "RENDERMODE=$mode" "$OUT/$name.hbpl" && [ "$pages" = 2 ] \
	&& grep -q "\[paper=$paper\]" "$OUT/$name.decode.txt"; then
	echo "PASS $name ($(wc -c < "$OUT/$name.hbpl" | tr -d ' ') bytes, $pages pages, $paper $got)"
	report notice "PASS $name" "$(wc -c < "$OUT/$name.hbpl" | tr -d ' ') bytes, $pages pages, $paper $got"
    else
	echo "FAIL $name: pages=$pages dims='$got' (want $paper)"
	tail -20 "$OUT/$name.log"
	report error "FAIL $name: pages=$pages dims='$got'" "$OUT/$name.log"
	fail=1
    fi
    rm -f "$OUT/$name"-*.p?m	# decoded pages are ~100 MB each
}

check color COLOR     Letter -o ColorModel=CMYK
check rgb   COLOR     Letter -o ColorModel=RGB
check gray  GRAYSCALE Letter -o ColorModel=Gray -o MediaType=Labels
check a4    COLOR     A4 -o PageSize=A4
grep -aq 'MSIP=LABEL' "$OUT/gray.hbpl" || { echo "FAIL paper type not sent"; fail=1; }
exit $fail
