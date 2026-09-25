#!/usr/bin/env bash
# Smoke tests for the OS simulator.
#
# Multi-CPU runs interleave their logs differently on every run, so instead of
# diffing against output/ this checks invariants that must always hold:
#   - the simulator exits with status 0 before the timeout,
#   - every loaded process either finishes or is killed,
#   - every CPU stops,
# plus a few functional checks for killall and swapping.
#
# Usage: tests/run_tests.sh [path/to/os]   (run from the project root)

OS=${1:-./os}
TIMEOUT=${TIMEOUT:-60}
pass=0
fail=0

run() {
	perl -e "alarm $TIMEOUT; exec @ARGV" "$OS" "$1" 2>&1
}

ok()  { echo "PASS  $1"; pass=$((pass + 1)); }
bad() { echo "FAIL  $1: $2"; fail=$((fail + 1)); }

for cfg in input/*; do
	[ -f "$cfg" ] || continue
	name=$(basename "$cfg")
	out=$(run "$name")
	status=$?
	if [ $status -ne 0 ]; then
		bad "$name" "exit status $status"
		continue
	fi
	nproc=$(head -1 "$cfg" | awk '{print $3}')
	ncpu=$(head -1 "$cfg" | awk '{print $2}')
	loaded=$(grep -c "Loaded a process" <<<"$out")
	ended=$(grep -cE "has finished|has been killed|Terminating process" <<<"$out")
	stopped=$(grep -c "stopped" <<<"$out")
	if [ "$loaded" -ne "$nproc" ]; then
		bad "$name" "loaded $loaded/$nproc processes"
	elif [ "$ended" -ne "$nproc" ]; then
		bad "$name" "$ended/$nproc processes ended"
	elif [ "$stopped" -ne "$ncpu" ]; then
		bad "$name" "$stopped/$ncpu CPUs stopped"
	elif grep -qE "AddressSanitizer|ThreadSanitizer|runtime error" <<<"$out"; then
		bad "$name" "sanitizer report"
	else
		ok "$name"
	fi
done

# killall must terminate the three P0 processes (two waiting, one running)
out=$(run os_killall)
if grep -q "Total processes terminated by killall: 3" <<<"$out"; then
	ok "killall terminates every matching process"
else
	bad "killall terminates every matching process" "wrong count"
fi

# 6 pages through a 2-frame RAM: values must survive swap-out/swap-in
out=$(run os_swap)
if grep -q "read region=0 offset=5 value=11" <<<"$out" &&
   grep -q "read region=1 offset=5 value=22" <<<"$out" &&
   grep -q "read region=2 offset=5 value=33" <<<"$out" &&
   ! grep -q "OOM" <<<"$out"; then
	ok "paging swaps pages without losing data"
else
	bad "paging swaps pages without losing data" "wrong values or OOM"
fi

echo
echo "$pass passed, $fail failed"
[ $fail -eq 0 ]
