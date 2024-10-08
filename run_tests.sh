#!/bin/bash
#set -e

if [[ -z "$NO_BUILD" ]]; then
  make
fi

while [[ "$#" -gt 0 ]]; do case $1 in
    --no-pass) no_pass=1; shift 1;;
    --no-fail) no_fail=1; shift 1;;
    --quiet) no_pass=1; no_fail=1; shift 1;;
    *) break;;
  esac;
done

# default arg is test directory
[ $# -eq 0 ] && set -- test

cmd="python3 test/test.pyc"

PASS=0
FAIL=0
DEAD=0
for arg in "$@"; do
  while read -r file; do
    output="$($cmd "$file" 2>&1)";
    code="$?"

    if [[ "$code" == 0 ]]; then
      if [[ -z "$no_pass" ]]; then
        echo
        echo "$file";
        echo "$output"
      fi
      ((PASS++))
    elif [[ "$code" == 1 ]]; then
      if [[ -z "$no_fail" ]]; then
        echo
        echo "$file";
        echo "$output"
      fi
      ((FAIL++))
    else
      # exit code other than 0 or 1 - always print
      echo
      echo "$file";
      echo "$output"
      ((DEAD++))
    fi
  done < <(find "$arg" -type f -name '*.lox')
done

echo
echo "$PASS tests passed, $FAIL tests failed."
if (( DEAD > 0 )); then
  echo "$DEAD tests died."
  exit 2
elif (( FAIL > 0 )); then
  exit 1
fi
