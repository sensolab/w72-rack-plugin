package main

import (
	"encoding/json"
	"strconv"
)

// Rack stores parameters and voltages as float32; Brumista widens them to
// double, so 0.3 arrives as 0.30000001192092896. Seven significant digits is
// what a float32 carries, so the replies are trimmed to that before they reach
// the model.
const floatDigits = 7

// trimFloats rewrites every fractional or exponent number in a JSON document
// to floatDigits significant digits, leaving strings, integers and the
// document's layout untouched.
func trimFloats(doc json.RawMessage) json.RawMessage {
	out := make([]byte, 0, len(doc))
	for i := 0; i < len(doc); {
		c := doc[i]
		switch {
		case c == '"':
			j := i + 1
			for j < len(doc) && doc[j] != '"' {
				if doc[j] == '\\' {
					j++
				}
				j++
			}
			j++
			if j > len(doc) {
				j = len(doc)
			}
			out = append(out, doc[i:j]...)
			i = j
		case c == '-' || (c >= '0' && c <= '9'):
			j := i + 1
			frac := false
			for j < len(doc) && (doc[j] >= '0' && doc[j] <= '9' || doc[j] == '.' || doc[j] == 'e' || doc[j] == 'E' || doc[j] == '+' || doc[j] == '-') {
				if doc[j] == '.' || doc[j] == 'e' || doc[j] == 'E' {
					frac = true
				}
				j++
			}
			tok := doc[i:j]
			if v, err := strconv.ParseFloat(string(tok), 64); frac && err == nil {
				tok = []byte(strconv.FormatFloat(v, 'g', floatDigits, 64))
			}
			out = append(out, tok...)
			i = j
		default:
			out = append(out, c)
			i++
		}
	}
	return out
}
