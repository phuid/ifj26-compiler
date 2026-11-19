#!/usr/bin/env bash
#
# AI-generated test block — end-to-end test runner.
# Model: claude-opus-5-5 (Claude Code).
# Prompt:
#   Write a bash 3.2 compatible runner for tests/e2e/cases/<group>/<name>.ifj26.
#   Compile each case with the compiler (source on stdin, IFJcode26 on stdout),
#   read the expected exit code from <name>.rc (default 0). Codes 1-10 and 99
#   are compile errors and only the compiler exit code is compared. Otherwise
#   the compiler must succeed, ic26int runs the code with <name>.in on stdin,
#   and both its exit code and stdout (against <name>.out, empty when missing)
#   must match. Without an interpreter, only check that the case compiles and
#   report SKIP. Options: -k <group> (repeatable), -v, -h. Nonzero exit on any
#   failure.
#
# Usage: tests/e2e/run_e2e.sh [-v] [-k group]...
# Environment:
#   IFJ26_COMPILER  compiler binary   (default: ./ifj26-compiler)
#   IC26INT         interpreter       (default: ic26int)
#   E2E_TIMEOUT     seconds per step  (default: 10)

set -u

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
CASES_DIR="$SCRIPT_DIR/cases"
COMPILER=${IFJ26_COMPILER:-./ifj26-compiler}
INTERPRETER=${IC26INT:-ic26int}
STEP_TIMEOUT=${E2E_TIMEOUT:-10}
TIMEOUT_EXIT=142

verbose=0
groups=""

usage() {
  sed -n '16,21p' "$0" | sed 's/^# \{0,1\}//'
}

while getopts "vk:h" option; do
  case "$option" in
  v) verbose=1 ;;
  k) groups="$groups $OPTARG" ;;
  h)
    usage
    exit 0
    ;;
  *)
    usage >&2
    exit 2
    ;;
  esac
done

if [ ! -x "$COMPILER" ] && ! command -v "$COMPILER" >/dev/null 2>&1; then
  echo "error: compiler '$COMPILER' not found (set IFJ26_COMPILER)" >&2
  exit 2
fi

have_interpreter=1
if ! command -v "$INTERPRETER" >/dev/null 2>&1; then
  have_interpreter=0
  echo "warning: interpreter '$INTERPRETER' not found, run cases only compile" >&2
fi

work_dir=$(mktemp -d "${TMPDIR:-/tmp}/ifj26-e2e.XXXXXX") || exit 2
trap 'rm -rf "$work_dir"' EXIT

# Runs a command with a time limit; exit code 142 means it timed out.
with_timeout() {
  perl -e 'alarm shift; exec @ARGV or exit 127' "$STEP_TIMEOUT" "$@"
}

is_compile_error() {
  case "$1" in
  1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 99) return 0 ;;
  *) return 1 ;;
  esac
}

pass_count=0
fail_count=0
skip_count=0

report() {
  local status=$1 name=$2 detail=$3
  case "$status" in
  PASS) pass_count=$((pass_count + 1)) ;;
  SKIP) skip_count=$((skip_count + 1)) ;;
  FAIL) fail_count=$((fail_count + 1)) ;;
  esac
  if [ "$status" = FAIL ] || [ "$verbose" -eq 1 ]; then
    printf '%-4s %s%s\n' "$status" "$name" "${detail:+  ($detail)}"
  fi
}

run_case() {
  local source=$1 group=$2
  local base=${source%.ifj26}
  local name
  name="$group/$(basename "$base")"
  local code="$work_dir/case.code" actual="$work_dir/case.stdout"
  local compile_err="$work_dir/compile.stderr"
  local expected=0 compile_rc run_rc input=/dev/null expected_out=/dev/null

  [ -f "$base.rc" ] && expected=$(tr -d ' \t\r\n' <"$base.rc")
  [ -f "$base.in" ] && input="$base.in"
  [ -f "$base.out" ] && expected_out="$base.out"

  with_timeout "$COMPILER" <"$source" >"$code" 2>"$compile_err"
  compile_rc=$?

  if is_compile_error "$expected"; then
    if [ "$compile_rc" -eq "$expected" ]; then
      report PASS "$name" ""
    else
      report FAIL "$name" "compiler exit $compile_rc, expected $expected"
      [ "$verbose" -eq 1 ] && sed 's/^/     | /' "$compile_err"
    fi
    return
  fi

  if [ "$compile_rc" -ne 0 ]; then
    report FAIL "$name" "compiler exit $compile_rc, expected 0"
    [ "$verbose" -eq 1 ] && sed 's/^/     | /' "$compile_err"
    return
  fi

  if [ "$have_interpreter" -eq 0 ]; then
    report SKIP "$name" "compiled, no interpreter"
    return
  fi

  with_timeout "$INTERPRETER" "$code" <"$input" >"$actual" 2>/dev/null
  run_rc=$?

  if [ "$run_rc" -eq "$TIMEOUT_EXIT" ]; then
    report FAIL "$name" "interpreter timed out after ${STEP_TIMEOUT}s"
  elif [ "$run_rc" -ne "$expected" ]; then
    report FAIL "$name" "interpreter exit $run_rc, expected $expected"
  elif ! cmp -s "$actual" "$expected_out"; then
    report FAIL "$name" "stdout differs"
    [ "$verbose" -eq 1 ] && diff "$expected_out" "$actual" | sed 's/^/     | /'
  else
    report PASS "$name" ""
  fi
}

selected_group() {
  [ -z "$groups" ] && return 0
  case " $groups " in
  *" $1 "*) return 0 ;;
  *) return 1 ;;
  esac
}

for group_dir in "$CASES_DIR"/*/; do
  group=$(basename "$group_dir")
  selected_group "$group" || continue
  for source in "$group_dir"*.ifj26; do
    [ -f "$source" ] || continue
    run_case "$source" "$group"
  done
done

total=$((pass_count + fail_count + skip_count))
echo "E2E: $total cases, $pass_count passed, $fail_count failed, $skip_count skipped"
[ "$fail_count" -eq 0 ]
