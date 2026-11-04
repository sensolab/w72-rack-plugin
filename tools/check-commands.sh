#!/usr/bin/env bash
# Checks the C++ dispatch table in src/BrumistaCommands.cpp against protocol/commands.json:
# the same commands, in the same order, on the same thread, with the same effect (read or
# edit). Needs jq.
set -euo pipefail
cd "$(dirname "$0")/.."

spec=$(jq -r '.commands[] | "\(.name) \(.thread) \(if .edits then "edit" else "read" end)"' protocol/commands.json | tr -d '\r')
code=$(sed -n '/^const Command COMMANDS\[\] = {/,/^};/p' src/BrumistaCommands.cpp |
	sed -nE 's/^[[:space:]]*\{"([a-z_]+)", Thread::([A-Z]+), Effect::([A-Z]+),.*/\1 \2 \3/p' |
	awk '{ print $1, tolower($2), tolower($3) }')

if [ "$spec" != "$code" ]; then
	echo "protocol/commands.json and src/BrumistaCommands.cpp disagree:" >&2
	diff <(echo "$spec") <(echo "$code") --label commands.json --label BrumistaCommands.cpp >&2 || true
	exit 1
fi
echo "commands match: $(echo "$code" | wc -l | tr -d ' ')"
