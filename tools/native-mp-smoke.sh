#!/usr/bin/env bash
set -euo pipefail

usage() {
	printf 'Usage: %s prepare|host|client\n' "${0##*/}" >&2
	exit 2
}

role=${1:-}
case "$role" in
	prepare|host|client) ;;
	*) usage ;;
esac

repo_root=$(cd -- "$(dirname -- "$0")/.." && pwd)
game_dir=${JA2_GAME_DIR:-"$repo_root/.local/wine-baseline/game"}
native_binary=${JA2_NATIVE_BINARY:-"$repo_root/build/native/JA2-native"}
base_vfs="$game_dir/vfs_config.JA2113.ini"
seed_profile="$game_dir/Profiles/UserProfile_JA2113"
test_root="$game_dir/.mp-smoke"

if [[ ! -f "$base_vfs" || ! -f "$seed_profile/Ja2_mp.ini" ]]; then
	printf 'Missing prepared game data under %s\n' "$game_dir" >&2
	exit 1
fi

prepare_role() {
	local current_role=$1
	local profile_name player_name profile_dir vfs_file
	case "$current_role" in
		host)
			profile_name=MPTestHost
			player_name='MP Host'
			;;
		client)
			profile_name=MPTestClient
			player_name='MP Client'
			;;
	esac

	profile_dir="$game_dir/Profiles/$profile_name"
	vfs_file="$test_root/vfs-$current_role.ini"
	mkdir -p "$profile_dir" "$test_root"

	awk -v root="Profiles/$profile_name" '
		/^\[PROFILE_UserProf\]\r?$/ { in_user_profile = 1; print; next }
		/^\[/ { in_user_profile = 0 }
		in_user_profile && /^PROFILE_ROOT[[:space:]]*=/ {
			print "PROFILE_ROOT = " root
			next
		}
		{ print }
	' "$base_vfs" > "$vfs_file.tmp"
	mv "$vfs_file.tmp" "$vfs_file"

	if [[ ! -f "$profile_dir/Ja2_mp.ini" ]]; then
		sed -E \
			-e "s/^(PLAYER_NAME[[:space:]]*=).*/\\1 $player_name/" \
			-e 's/^(SYNC_GAME_DIRECTORY[[:space:]]*=).*/\1 0/' \
			"$seed_profile/Ja2_mp.ini" > "$profile_dir/Ja2_mp.ini"
	fi
}

prepare_role host
prepare_role client

if [[ "$role" == prepare ]]; then
	printf 'Prepared isolated host and client profiles in %s\n' "$test_root"
	printf 'Run in two terminals:\n'
	printf '  %s host\n' "$0"
	printf '  %s client\n' "$0"
	exit 0
fi

if [[ ! -x "$native_binary" ]]; then
	printf 'Native executable is missing: %s\n' "$native_binary" >&2
	exit 1
fi

cd "$game_dir"
exec "$native_binary" \
	"-VFS_CONFIG_INI=.mp-smoke/vfs-$role.ini" \
	-SCREEN_MODE_WINDOWED=1 \
	-PLAY_INTRO=0
