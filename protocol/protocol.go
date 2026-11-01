// Package protocol embeds commands.json, Brumista's command list, so Go code
// reads the one source of truth instead of a copy.
package protocol

import _ "embed"

//go:embed commands.json
var CommandsJSON []byte
