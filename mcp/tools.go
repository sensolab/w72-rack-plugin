package main

import (
	"context"
	"encoding/base64"
	"encoding/json"
	"errors"
	"fmt"

	"github.com/modelcontextprotocol/go-sdk/mcp"
	"github.com/sensolab/w72-rack-plugin/protocol"
)

// Spec is the part of protocol/commands.json the tools are built from.
type Spec struct {
	Commands []Command `json:"commands"`
}

type Command struct {
	Name        string           `json:"name"`
	Description string           `json:"description"`
	Params      map[string]Param `json:"params"`
	// Image names the format of a picture the result carries in a field of the
	// same name, as base64.
	Image string `json:"image"`
}

type Param struct {
	Type        string   `json:"type"`
	Required    bool     `json:"required"`
	Default     any      `json:"default,omitempty"`
	Enum        []string `json:"enum,omitempty"`
	Items       any      `json:"items,omitempty"`
	Description string   `json:"description"`
}

func loadSpec() (*Spec, error) {
	var s Spec
	if err := json.Unmarshal(protocol.CommandsJSON, &s); err != nil {
		return nil, fmt.Errorf("protocol/commands.json: %v", err)
	}
	return &s, nil
}

// inputSchema turns a command's params into the tool's JSON Schema.
func (c Command) inputSchema() map[string]any {
	props := map[string]any{}
	required := []string{}
	for name, p := range c.Params {
		prop := map[string]any{"type": p.Type, "description": p.Description}
		if p.Default != nil {
			prop["default"] = p.Default
		}
		if p.Enum != nil {
			prop["enum"] = p.Enum
		}
		if p.Items != nil {
			prop["items"] = p.Items
		}
		props[name] = prop
		if p.Required {
			required = append(required, name)
		}
	}
	return map[string]any{
		"type":                 "object",
		"properties":           props,
		"required":             required,
		"additionalProperties": false,
	}
}

// instructions reach every client at connection, as limen's skill does for limen: call
// hello first. Its warning is the one sign that Rack's engine has stopped, since every
// other command still answers.
const instructions = `Brumista reads and edits a running Rack patch. Call hello before anything else, and again if Rack may have restarted: it reports Brumista's version, the commands this build supports and whether Rack's engine is running. If hello's reply carries a warning, tell the user before going on: while the engine is stopped, modules don't process, knob changes don't take effect and voltages and displays don't move, so what you read will mislead you.`

func newServer(spec *Spec, bridge *Bridge) *mcp.Server {
	server := mcp.NewServer(&mcp.Implementation{Name: "brumista", Version: version}, &mcp.ServerOptions{Instructions: instructions})
	for _, c := range spec.Commands {
		server.AddTool(&mcp.Tool{
			Name:        c.Name,
			Description: c.Description,
			InputSchema: c.inputSchema(),
		}, handler(c, bridge))
	}
	return server
}

// handler forwards a tool call to Brumista. The module validates the
// arguments; its error replies, like a failed connection, come back as tool
// errors so the model can read them and correct itself.
func handler(c Command, bridge *Bridge) mcp.ToolHandler {
	return func(ctx context.Context, req *mcp.CallToolRequest) (*mcp.CallToolResult, error) {
		args := map[string]json.RawMessage{}
		if len(req.Params.Arguments) > 0 {
			if err := json.Unmarshal(req.Params.Arguments, &args); err != nil {
				return errorResult(fmt.Errorf("arguments must be a JSON object: %v", err)), nil
			}
		}
		result, err := bridge.Call(ctx, c.Name, args)
		if err != nil {
			var me *ModuleError
			if errors.As(err, &me) {
				return errorResult(fmt.Errorf("Brumista: %s", me.Msg)), nil
			}
			return errorResult(err), nil
		}
		if c.Image != "" {
			return imageResult(result, c.Image)
		}
		return &mcp.CallToolResult{Content: []mcp.Content{&mcp.TextContent{Text: string(result)}}}, nil
	}
}

// imageResult returns the picture in result's field named format as an image
// the model can see, and the other fields as text.
func imageResult(result json.RawMessage, format string) (*mcp.CallToolResult, error) {
	fields := map[string]json.RawMessage{}
	var encoded string
	if err := json.Unmarshal(result, &fields); err != nil {
		return errorResult(fmt.Errorf("Brumista's result isn't an object: %v", err)), nil
	}
	if err := json.Unmarshal(fields[format], &encoded); err != nil {
		return errorResult(fmt.Errorf("Brumista's result has no %s", format)), nil
	}
	data, err := base64.StdEncoding.DecodeString(encoded)
	if err != nil {
		return errorResult(fmt.Errorf("Brumista's %s isn't base64: %v", format, err)), nil
	}
	delete(fields, format)
	text, err := json.Marshal(fields)
	if err != nil {
		return nil, err
	}
	return &mcp.CallToolResult{Content: []mcp.Content{
		&mcp.ImageContent{Data: data, MIMEType: "image/" + format},
		&mcp.TextContent{Text: string(text)},
	}}, nil
}

func errorResult(err error) *mcp.CallToolResult {
	return &mcp.CallToolResult{
		Content: []mcp.Content{&mcp.TextContent{Text: err.Error()}},
		IsError: true,
	}
}
