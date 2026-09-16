#!/bin/bash
exec clang-tidy-22 --removed-arg=-fno-canonical-system-headers "$@"
