#!/bin/bash
# avOS userspace utility functions

die() {
	echo "$@" >&2
	exit 1
}

info() {
	echo "[INFO] $*"
}

warn() {
	echo "[WARN] $*" >&2
}

check_dep() {
	command -v "$1" >/dev/null 2>&1 || die "missing required tool: $1"
}

check_deps() {
	local d
	for d in "$@"; do check_dep "$d"; done
}
