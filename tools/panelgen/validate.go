package main

import (
	"encoding/xml"
	"fmt"
	"io"
	"strings"
)

// What Rack's nanosvg draws (docs/design-system.md, "What Rack can draw").
// Anything else is ignored or drawn wrong, so it fails the build instead.
var drawable = map[string]bool{
	"svg": true, "g": true, "path": true, "rect": true, "circle": true,
	"ellipse": true, "line": true, "polyline": true, "polygon": true,
	"defs": true, "linearGradient": true, "radialGradient": true, "stop": true,
	"title": true, "desc": true,
}

var undrawableAttrs = map[string]string{
	"class":            "nanosvg ignores classes; use presentation attributes",
	"stroke-dasharray": "dashes are drawn solid",
	"clip-path":        "clip paths are ignored",
	"mask":             "masks are ignored",
	"filter":           "filters are ignored",
}

// validate checks an SVG against what nanosvg can draw.
func validate(svg string) error {
	dec := xml.NewDecoder(strings.NewReader(svg))
	root := true
	for {
		tok, err := dec.Token()
		if err == io.EOF {
			return nil
		}
		if err != nil {
			return err
		}
		el, ok := tok.(xml.StartElement)
		if !ok {
			continue
		}
		name := el.Name.Local
		if !drawable[name] {
			return fmt.Errorf("<%s> is not drawn by Rack", name)
		}
		if root {
			if name != "svg" {
				return fmt.Errorf("root element is <%s>, not <svg>", name)
			}
			root = false
		}
		for _, a := range el.Attr {
			if why, bad := undrawableAttrs[a.Name.Local]; bad {
				return fmt.Errorf("<%s %s>: %s", name, a.Name.Local, why)
			}
			if strings.Contains(a.Value, "url(") && a.Name.Local != "fill" && a.Name.Local != "stroke" {
				return fmt.Errorf("<%s %s>: references are only drawn in fill and stroke", name, a.Name.Local)
			}
			if a.Name.Local == "stroke" && strings.Contains(a.Value, "url(") {
				return fmt.Errorf("<%s stroke>: gradient strokes are not drawn", name)
			}
		}
	}
}
