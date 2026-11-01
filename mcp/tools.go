package main

import (
	"context"
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
}

type Param struct {
	Type        string `json:"type"`
	Required    bool   `json:"required"`
	Default     any    `json:"default,omitempty"`
	Description string `json:"description"`
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

func newServer(spec *Spec, bridge *Bridge) *mcp.Server {
	server := mcp.NewServer(&mcp.Implementation{Name: "brumista", Version: version}, nil)
	for _, c := range spec.Commands {
		server.AddTool(&mcp.Tool{
			Name:        c.Name,
			Description: c.Description,
			InputSchema: c.inputSchema(),
		}, handler(c.Name, bridge))
	}
	return server
}

// handler forwards a tool call to Brumista. The module validates the
// arguments; its error replies, like a failed connection, come back as tool
// errors so the model can read them and correct itself.
func handler(cmd string, bridge *Bridge) mcp.ToolHandler {
	return func(ctx context.Context, req *mcp.CallToolRequest) (*mcp.CallToolResult, error) {
		args := map[string]json.RawMessage{}
		if len(req.Params.Arguments) > 0 {
			if err := json.Unmarshal(req.Params.Arguments, &args); err != nil {
				return errorResult(fmt.Errorf("arguments must be a JSON object: %v", err)), nil
			}
		}
		result, err := bridge.Call(ctx, cmd, args)
		if err != nil {
			var me *ModuleError
			if errors.As(err, &me) {
				return errorResult(fmt.Errorf("Brumista: %s", me.Msg)), nil
			}
			return errorResult(err), nil
		}
		return &mcp.CallToolResult{Content: []mcp.Content{&mcp.TextContent{Text: string(result)}}}, nil
	}
}

func errorResult(err error) *mcp.CallToolResult {
	return &mcp.CallToolResult{
		Content: []mcp.Content{&mcp.TextContent{Text: err.Error()}},
		IsError: true,
	}
}
