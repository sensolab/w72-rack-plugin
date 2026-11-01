// panelgen builds W72's panel SVGs and their position headers from the module
// descriptions in panels/, following docs/design-system.md.
//
//	go run . -root ../..          write res/<Slug>.svg and src/<Slug>Panel.hpp
//	go run . -root ../.. -check   fail if a generated file is out of date
package main

import (
	"bytes"
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"sort"
)

func main() {
	root := flag.String("root", "../..", "repository root")
	check := flag.Bool("check", false, "check that generated files are current instead of writing them")
	flag.Parse()
	if err := run(*root, *check); err != nil {
		fmt.Fprintln(os.Stderr, "panelgen:", err)
		os.Exit(1)
	}
}

func loadFaces(dir string) (Faces, error) {
	semi, err := loadFace(filepath.Join(dir, "InterTight-SemiBold.ttf"))
	if err != nil {
		return Faces{}, err
	}
	bold, err := loadFace(filepath.Join(dir, "InterTight-Bold.ttf"))
	if err != nil {
		return Faces{}, err
	}
	return Faces{semi, bold}, nil
}

func run(root string, check bool) error {
	faces, err := loadFaces(filepath.Join(root, "tools", "panelgen", "fonts"))
	if err != nil {
		return err
	}
	descs, err := filepath.Glob(filepath.Join(root, "panels", "*.json"))
	if err != nil {
		return err
	}
	sort.Strings(descs)
	if len(descs) == 0 {
		return fmt.Errorf("no module descriptions in %s", filepath.Join(root, "panels"))
	}
	stale := 0
	for _, path := range descs {
		m, err := loadModule(path)
		if err != nil {
			return err
		}
		svg, err := panelSVG(m, faces)
		if err != nil {
			return fmt.Errorf("%s: %w", m.Slug, err)
		}
		if err := validate(svg); err != nil {
			return fmt.Errorf("%s: %w", m.Slug, err)
		}
		outputs := map[string]string{
			filepath.Join(root, "res", m.Slug+".svg"):      svg,
			filepath.Join(root, "src", m.Slug+"Panel.hpp"): panelHeader(m),
		}
		for out, content := range outputs {
			if check {
				old, err := os.ReadFile(out)
				if err != nil || !bytes.Equal(old, []byte(content)) {
					fmt.Fprintf(os.Stderr, "%s is out of date; run make panels\n", out)
					stale++
				}
				continue
			}
			if err := os.WriteFile(out, []byte(content), 0o644); err != nil {
				return err
			}
			fmt.Println("wrote", out)
		}
	}
	if stale > 0 {
		return fmt.Errorf("%d generated file(s) out of date", stale)
	}
	return nil
}
