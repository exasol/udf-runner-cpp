#!/bin/bash

#Reason: .env file does not exist in repo. Need to disable shellcheck rule.
#shellcheck disable=SC1091
source .env
export VERBOSE_BUILD="--subcommands --verbose_failures"
bash run.sh --define bash=true --define benchmark=true "$@"
