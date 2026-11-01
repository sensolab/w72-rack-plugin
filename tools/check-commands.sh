#!/usr/bin/env bash
# Checks the C++ dispatch table in src/BrumistaCommands.cpp against protocol/commands.json:
# the same commands, in the same order, on the same thread. Needs jq.
set -euo pipefail
cd "$(dirname "$0")/.."

spec=$(jq -r '.commands[] | "\(.name) \(.thread)"' protocol/commands.json | tr -d '\r')
code=$(sed -n '/^const Command COMMANDS\[\] = {/,/^};/p' src/BrumistaCommands.cpp |
	sed -nE 's/^[[:space:]]*\{"([a-z_]+)", Thread::([A-Z]+),.*/\1 \2/p' |
	awk '{ print $1, tolower($2) }')

if [ "$spec" != "$code" ]; then
	echo "protocol/commands.json and src/BrumistaCommands.cpp disagree:" >&2
	diff <(echo "$spec") <(echo "$code") --label commands.json --label BrumistaCommands.cpp >&2 || true
	exit 1
fi
echo "commands match: $(echo "$code" | wc -l | tr -d ' ')"
