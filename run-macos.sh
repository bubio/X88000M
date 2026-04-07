#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

has_roms() {
	dir=$1
	[ -d "$dir" ] || return 1
	for name in \
		pc88.rom PC88.ROM \
		n88.rom N88.ROM \
		disk.rom DISK.ROM
	do
		if [ -f "$dir/$name" ]; then
			return 0
		fi
	done
	return 1
}

ROM_DIR=${X88_ROM_DIR:-}
if [ -n "$ROM_DIR" ]; then
	if ! has_roms "$ROM_DIR"; then
		echo "X88_ROM_DIR does not contain expected ROM files: $ROM_DIR" >&2
		exit 1
	fi
else
	for candidate in \
		"$HOME/Library/Application Support/Bubilator88" \
		"$HOME/Library/Application Support/quasi88/rom" \
		"$HOME/Library/Application Support/retro_pc_pi/xm8" \
		"/Volumes/CrucialX6/roms/PC88/bios" \
		"/Volumes/CrucialX6/roms/PC88/M88 ROMS"
	do
		if has_roms "$candidate"; then
			ROM_DIR=$candidate
			break
		fi
	done
fi

if [ -z "$ROM_DIR" ]; then
	echo "No PC-88 ROM directory found." >&2
	echo "Set X88_ROM_DIR to a directory containing files like pc88.rom and DISK.ROM." >&2
	exit 1
fi

cd "$ROM_DIR"
exec "$SCRIPT_DIR/X88000" "$@"
