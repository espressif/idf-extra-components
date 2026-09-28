#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
# Host conformance check for json_parser against JSONTestSuite.
#   ./run.sh [path/to/JSONTestSuite]   (cloned on demand into ./JSONTestSuite)
set -euo pipefail
cd "$(dirname "$0")"
SUITE=${1:-./JSONTestSuite}
[ -d "$SUITE/test_parsing" ] || git clone --depth 1 -q https://github.com/nst/JSONTestSuite.git "$SUITE"
CF="-std=gnu11 -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -I../include -I../../jsmn/include"
gcc $CF -o accept accept.c ../src/json_parser.c
gcc $CF -o api_walk api_walk.c ../src/json_parser.c
export ASAN_OPTIONS=detect_leaks=1

ya=0; yr=0; na=0; nr=0; fails=()
for f in "$SUITE"/test_parsing/*.json; do
    b=$(basename "$f" .json); rc=0; timeout 5 ./accept "$f" >/dev/null 2>&1 || rc=$?
    # accept exits 0 (accepted) or 2 (rejected); a sanitizer abort, signal or timeout is neither
    if [ $rc -ne 0 ] && [ $rc -ne 2 ]; then fails+=("crashed (exit $rc) on $b"); continue; fi
    case "$b" in
        y_*) if [ $rc -eq 0 ]; then ya=$((ya+1)); else yr=$((yr+1)); fails+=("rejected valid $b"); fi ;;
        n_*) if [ $rc -eq 0 ]; then na=$((na+1)); fails+=("accepted invalid $b"); else nr=$((nr+1)); fi ;;
    esac
done
echo "accept/reject: $ya/$((ya+yr)) valid accepted, $nr/$((na+nr)) invalid rejected"
printf '  %s\n' "${fails[@]:-}" | sed '/^  $/d'
python3 check_values.py ./api_walk "$SUITE"
[ ${#fails[@]} -eq 0 ]
