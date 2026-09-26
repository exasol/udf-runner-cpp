#!/bin/bash
clang_tidy="${CLANG_TIDY:-clang-tidy-22}"
removed_arg="${CLANG_TIDY_REMOVED_ARG--fno-canonical-system-headers}"
clang_tidy_args=()

for arg in "$@"; do
    if [[ -n "$removed_arg" && "$arg" == "$removed_arg" ]]; then
        continue
    fi
    clang_tidy_args+=("$arg")
done

exec "$clang_tidy" "${clang_tidy_args[@]}"
