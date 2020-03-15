#!/usr/bin/env bash

export npm_config_target=11.3.0
export npm_config_disturl=https://atom.io/download/atom-shell
export JOBS=$(nproc)
npm --build-from-source "$@"
