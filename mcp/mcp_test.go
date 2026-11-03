package main

import (
	"bufio"
	"context"
	"encoding/json"
	"net"
	"os"
	"slices"
	"strings"
	"testing"

	"github.com/modelcontextprotocol/go-sdk/mcp"
)

// fakeRack answers like Brumista: hello with the given result, and every other
// command by echoing its request back as the result. A command named fail
// gets an error reply.
func fakeRack(t *testing.T, hello string) string {
	t.Helper()
	ln, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { ln.Close() })
	go func() {
		for {
			conn, err := ln.Accept()
			if err != nil {
				return
			}
			go func() {
				defer conn.Close()
				s := bufio.NewScanner(conn)
				for s.Scan() {
					var req map[string]any
					json.Unmarshal(s.Bytes(), &req)
					switch req["cmd"] {
					case "hello":
						conn.Write([]byte(`{"ok":true,"result":` + hello + "}\n"))
					case "fail":
						conn.Write([]byte(`{"ok":false,"error":"module not found"}` + "\n"))
					default:
						conn.Write([]byte(`{"ok":true,"result":` + s.Text() + "}\n"))
					}
				}
			}()
		}
	}()
	return ln.Addr().String()
}

const brumistaHello = `{"server":"Brumista","version":"` + version + `","commands":["hello"]}`

func session(t *testing.T, addr string) *mcp.ClientSession {
	t.Helper()
	spec, err := loadSpec()
	if err != nil {
		t.Fatal(err)
	}
	spec.Commands = append(spec.Commands, Command{Name: "fail", Params: map[string]Param{}})
	ctx := context.Background()
	st, ct := mcp.NewInMemoryTransports()
	if _, err := newServer(spec, &Bridge{Addr: addr}).Connect(ctx, st, nil); err != nil {
		t.Fatal(err)
	}
	cs, err := mcp.NewClient(&mcp.Implementation{Name: "test", Version: "0"}, nil).Connect(ctx, ct, nil)
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { cs.Close() })
	return cs
}

func call(t *testing.T, cs *mcp.ClientSession, name string, args map[string]any) (string, bool) {
	t.Helper()
	res, err := cs.CallTool(context.Background(), &mcp.CallToolParams{Name: name, Arguments: args})
	if err != nil {
		t.Fatal(err)
	}
	return res.Content[0].(*mcp.TextContent).Text, res.IsError
}

func TestToolsMatchCommands(t *testing.T) {
	cs := session(t, "127.0.0.1:1")
	res, err := cs.ListTools(context.Background(), nil)
	if err != nil {
		t.Fatal(err)
	}
	spec, _ := loadSpec()
	byName := map[string]*mcp.Tool{}
	for _, tool := range res.Tools {
		byName[tool.Name] = tool
	}
	for _, c := range spec.Commands {
		tool := byName[c.Name]
		if tool == nil {
			t.Errorf("no tool for command %s", c.Name)
			continue
		}
		schema := tool.InputSchema.(map[string]any)
		props := schema["properties"].(map[string]any)
		for name, p := range c.Params {
			if props[name] == nil {
				t.Errorf("%s: no property %s", c.Name, name)
			}
			required := slices.Contains(schema["required"].([]any), any(name))
			if required != p.Required {
				t.Errorf("%s.%s: required is %v, want %v", c.Name, name, required, p.Required)
			}
			if p.Enum != nil {
				enum, _ := props[name].(map[string]any)["enum"].([]any)
				if len(enum) != len(p.Enum) {
					t.Errorf("%s.%s: enum is %v, want %v", c.Name, name, enum, p.Enum)
				}
			}
		}
	}
}

func TestForwardsArguments(t *testing.T) {
	cs := session(t, fakeRack(t, brumistaHello))
	text, isErr := call(t, cs, "get_param", map[string]any{"id": 9007199254740993, "param": 2})
	if isErr {
		t.Fatal(text)
	}
	// 2^53+1 has no float64: the arguments must pass through as written.
	if text != `{"cmd":"get_param","id":9007199254740993,"param":2}` {
		t.Errorf("got %s", text)
	}
}

func TestBatchForwardsCommands(t *testing.T) {
	cs := session(t, fakeRack(t, brumistaHello))
	text, isErr := call(t, cs, "batch", map[string]any{
		"commands":    []any{map[string]any{"cmd": "set_param", "id": 1, "param": 0, "value": 0.5}},
		"stopOnError": false,
	})
	if isErr {
		t.Fatal(text)
	}
	var got struct {
		Cmd         string           `json:"cmd"`
		Commands    []map[string]any `json:"commands"`
		StopOnError bool             `json:"stopOnError"`
	}
	if err := json.Unmarshal([]byte(text), &got); err != nil {
		t.Fatal(err)
	}
	if got.Cmd != "batch" || len(got.Commands) != 1 || got.Commands[0]["cmd"] != "set_param" || got.StopOnError {
		t.Errorf("got %s", text)
	}
}

func TestHello(t *testing.T) {
	cs := session(t, fakeRack(t, brumistaHello))
	text, isErr := call(t, cs, "hello", nil)
	if isErr || text != brumistaHello {
		t.Errorf("got %s (error %v)", text, isErr)
	}
}

func TestModuleErrorIsToolError(t *testing.T) {
	cs := session(t, fakeRack(t, brumistaHello))
	text, isErr := call(t, cs, "fail", nil)
	if !isErr || text != "Brumista: module not found" {
		t.Errorf("got %s (error %v)", text, isErr)
	}
}

func TestRefusesLimen(t *testing.T) {
	cs := session(t, fakeRack(t, `{"protocol":2,"commands":["hello"]}`))
	text, isErr := call(t, cs, "list_modules", nil)
	if !isErr || !strings.Contains(text, "isn't Brumista") {
		t.Errorf("got %s (error %v)", text, isErr)
	}
}

func TestRefusesOtherMajor(t *testing.T) {
	cs := session(t, fakeRack(t, `{"server":"Brumista","version":"3.0.0","commands":[]}`))
	text, isErr := call(t, cs, "list_modules", nil)
	if !isErr || !strings.Contains(text, "install matching versions") {
		t.Errorf("got %s (error %v)", text, isErr)
	}
}

func TestNoRack(t *testing.T) {
	ln, _ := net.Listen("tcp", "127.0.0.1:0")
	addr := ln.Addr().String()
	ln.Close()
	text, isErr := call(t, session(t, addr), "list_modules", nil)
	if !isErr || !strings.Contains(text, "can't reach Brumista") {
		t.Errorf("got %s (error %v)", text, isErr)
	}
}

func TestVersionMatchesPlugin(t *testing.T) {
	data, err := os.ReadFile("../plugin.json")
	if err != nil {
		t.Fatal(err)
	}
	var p struct{ Version string }
	if err := json.Unmarshal(data, &p); err != nil {
		t.Fatal(err)
	}
	if p.Version != version {
		t.Errorf("plugin.json is %s, mcp/version.go is %s", p.Version, version)
	}
}

func TestTrimFloats(t *testing.T) {
	in := `{"value":0.30000001192092896,"n":16,"name":"a 0.123456789012 \"x\"","v":[-4.999999523162842,1e-07,0.5,-0.0]}`
	want := `{"value":0.3,"n":16,"name":"a 0.123456789012 \"x\"","v":[-5,1e-07,0.5,-0]}`
	if got := string(trimFloats(json.RawMessage(in))); got != want {
		t.Errorf("got %s\nwant %s", got, want)
	}
}
