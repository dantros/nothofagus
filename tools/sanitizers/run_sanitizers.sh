#!/usr/bin/env bash
# Run TSan and ASan+UBSan on the threaded sim/render path against SwiftShader (deterministic
# CPU Vulkan, headless). Mesa is buggy under sanitizers, so we never use the system GPU here.
#
# Usage:  tools/sanitizers/run_sanitizers.sh [tsan|asan|all]   (default: all)
#
# What it does, per sanitizer:
#   1. Configure a headless-Vulkan build with NOTHOFAGUS_FETCH_SWIFTSHADER=ON (downloads +
#      verifies a pinned SwiftShader ICD into the build dir) and NOTHOFAGUS_BUILD_SANITIZER_FIXTURE=ON.
#   2. Build the sanitizer_threaded_smoke fixture with the sanitizer flags.
#   3. Run it against the SwiftShader ICD, with the validation layer disabled (its own worker
#      thread/locks are sanitizer noise), and TSan pointed at tools/sanitizers/tsan.supp.
#   4. Report PASS only if the fixture completed AND the sanitizer reported no (non-suppressed) issues.
#
# Requires: clang/clang++, ninja, cmake, and network access on first run (SwiftShader fetch).
set -uo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/../.." && pwd)"
mode="${1:-all}"
supp="$here/tsan.supp"

configure() {
    local build_dir="$1"; shift
    local cxx_flags="$1"; shift
    local link_flags="$1"; shift
    cmake -S "$root" -G Ninja -B "$build_dir" \
        -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
        -DCMAKE_BUILD_TYPE=Debug \
        -DNOTHOFAGUS_BACKEND_VULKAN=ON -DNOTHOFAGUS_HEADLESS_VULKAN=ON \
        -DNOTHOFAGUS_FETCH_SWIFTSHADER=ON \
        -DNOTHOFAGUS_BUILD_SANITIZER_FIXTURE=ON \
        -DCMAKE_CXX_FLAGS="-Wno-narrowing $cxx_flags" \
        -DCMAKE_EXE_LINKER_FLAGS="$link_flags" \
        >/dev/null || { echo "configure failed for $build_dir"; return 1; }
    cmake --build "$build_dir" --parallel --target sanitizer_threaded_smoke >/dev/null \
        || { echo "build failed for $build_dir"; return 1; }
}

run_fixture() {
    # $1 build_dir, $2 label. Sanitizer env (TSAN_OPTIONS/ASAN_OPTIONS/...) comes from the caller.
    local build_dir="$1" label="$2"
    local icd="$build_dir/swiftshader/swiftshader_icd.json"
    if [[ ! -f "$icd" ]]; then echo "[$label] SwiftShader ICD missing ($icd)"; return 1; fi
    local bin; bin="$(find "$build_dir" -type f -name sanitizer_threaded_smoke | head -1)"
    if [[ -z "$bin" ]]; then echo "[$label] fixture binary not found under $build_dir"; return 1; fi
    local out err; out="$(mktemp)"; err="$(mktemp)"
    VK_ICD_FILENAMES="$icd" VK_DRIVER_FILES="$icd" VK_LOADER_LAYERS_DISABLE='*' \
        timeout 600 "$bin" >"$out" 2>"$err"
    local rc=$?
    local done warnings
    done=$(grep -c 'smoke done' "$out")
    warnings=$(grep -cE 'WARNING: ThreadSanitizer|ERROR: AddressSanitizer|runtime error:' "$err")
    echo "[$label] rc=$rc done=$done sanitizer_reports=$warnings"
    if [[ "$warnings" -gt 0 ]]; then
        echo "----- $label reports -----"; grep -E 'WARNING: ThreadSanitizer|ERROR: AddressSanitizer|runtime error:|SUMMARY:' "$err" | head
    fi
    rm -f "$out" "$err"
    [[ "$done" -ge 1 && "$warnings" -eq 0 ]]
}

status=0

if [[ "$mode" == "tsan" || "$mode" == "all" ]]; then
    echo "== TSan (SwiftShader headless) =="
    configure "$root/build/sanitizer-tsan" "-fsanitize=thread -g -O1" "-fsanitize=thread" || status=1
    TSAN_OPTIONS="halt_on_error=0 suppressions=$supp" run_fixture "$root/build/sanitizer-tsan" "tsan" || status=1
fi

if [[ "$mode" == "asan" || "$mode" == "all" ]]; then
    echo "== ASan + UBSan (SwiftShader headless) =="
    configure "$root/build/sanitizer-asan" "-fsanitize=address,undefined -fno-omit-frame-pointer -g -O1" "-fsanitize=address,undefined" || status=1
    ASAN_OPTIONS="halt_on_error=1 detect_leaks=0" UBSAN_OPTIONS="print_stacktrace=1 halt_on_error=1" \
        run_fixture "$root/build/sanitizer-asan" "asan" || status=1
fi

echo
[[ "$status" -eq 0 ]] && echo "ALL SANITIZERS CLEAN" || echo "SANITIZER FAILURES (see above)"
exit "$status"
