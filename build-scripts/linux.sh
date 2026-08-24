#!/usr/bin/env bash
set -euo pipefail

repo_root="$(git -C "$(dirname "${BASH_SOURCE[0]}")" rev-parse --show-toplevel)"

git -C "${repo_root}" submodule update --init --recursive

cd "${repo_root}/examples/libdatachannel"
cmake --workflow --preset release
