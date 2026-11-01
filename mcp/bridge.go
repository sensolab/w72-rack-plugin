package main

import (
	"bufio"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"log"
	"net"
	"strings"
	"sync"
	"time"
)

const (
	dialTimeout = 2 * time.Second
	// Brumista waits 2 s for the main thread; the rest is slack for the reply.
	replyTimeout = 5 * time.Second
)

// Bridge sends commands to Brumista. Each call opens its own connection, checks
// it with hello and closes it, because Brumista serves one client at a time and
// a held connection would lock out everyone else. Calls run one at a time:
// Brumista's listen backlog is 1, so a burst of parallel connections is refused.
type Bridge struct {
	Addr string

	mu       sync.Mutex
	warnOnce sync.Once
}

// ModuleError is an error reply from Brumista: the command ran, or was refused,
// and the module said why.
type ModuleError struct{ Msg string }

func (e *ModuleError) Error() string { return e.Msg }

type reply struct {
	OK     bool            `json:"ok"`
	Result json.RawMessage `json:"result"`
	Error  string          `json:"error"`
}

type helloResult struct {
	Server   string   `json:"server"`
	Version  string   `json:"version"`
	Commands []string `json:"commands"`
}

// Call runs one command and returns its result.
func (b *Bridge) Call(ctx context.Context, cmd string, args map[string]json.RawMessage) (json.RawMessage, error) {
	b.mu.Lock()
	defer b.mu.Unlock()
	d := net.Dialer{Timeout: dialTimeout}
	conn, err := d.DialContext(ctx, "tcp", b.Addr)
	if err != nil {
		return nil, fmt.Errorf("can't reach Brumista at %s: is Rack running with a Brumista module in the patch? (%v)", b.Addr, err)
	}
	defer conn.Close()
	r := bufio.NewReader(conn)

	hello, err := b.roundTrip(ctx, conn, r, map[string]any{"cmd": "hello"})
	if err != nil {
		return nil, err
	}
	if err := b.checkHello(hello); err != nil {
		return nil, err
	}
	if cmd == "hello" {
		return hello, nil
	}

	req := make(map[string]any, len(args)+1)
	for k, v := range args {
		req[k] = v
	}
	req["cmd"] = cmd
	return b.roundTrip(ctx, conn, r, req)
}

func (b *Bridge) roundTrip(ctx context.Context, conn net.Conn, r *bufio.Reader, req map[string]any) (json.RawMessage, error) {
	deadline := time.Now().Add(replyTimeout)
	if d, ok := ctx.Deadline(); ok && d.Before(deadline) {
		deadline = d
	}
	conn.SetDeadline(deadline)

	line, err := json.Marshal(req)
	if err != nil {
		return nil, err
	}
	if _, err := conn.Write(append(line, '\n')); err != nil {
		return nil, fmt.Errorf("sending %s to Brumista: %v", req["cmd"], err)
	}
	data, err := r.ReadBytes('\n')
	if err != nil {
		var ne net.Error
		if errors.As(err, &ne) && ne.Timeout() {
			return nil, fmt.Errorf("no reply from Brumista to %s within %s", req["cmd"], replyTimeout)
		}
		return nil, fmt.Errorf("reading Brumista's reply to %s: %v", req["cmd"], err)
	}
	var rep reply
	if err := json.Unmarshal(data, &rep); err != nil {
		return nil, fmt.Errorf("Brumista's reply to %s isn't JSON: %v", req["cmd"], err)
	}
	if !rep.OK {
		return nil, &ModuleError{rep.Error}
	}
	if len(rep.Result) == 0 {
		return json.RawMessage("null"), nil
	}
	return rep.Result, nil
}

// checkHello refuses anything that isn't Brumista (limen answers hello too, on
// its own port) and a module on another major version. A different minor or
// patch version still works, with a warning once on stderr.
func (b *Bridge) checkHello(raw json.RawMessage) error {
	var h helloResult
	if err := json.Unmarshal(raw, &h); err != nil || h.Server != "Brumista" {
		return fmt.Errorf("the server at %s isn't Brumista", b.Addr)
	}
	if major(h.Version) != major(version) {
		return fmt.Errorf("Brumista %s doesn't speak brumista-mcp %s's protocol: install matching versions", h.Version, version)
	}
	if h.Version != version {
		b.warnOnce.Do(func() {
			log.Printf("Brumista is %s and brumista-mcp is %s: some tools may be missing or unknown to the module", h.Version, version)
		})
	}
	return nil
}

func major(v string) string {
	m, _, _ := strings.Cut(strings.TrimPrefix(v, "v"), ".")
	return m
}
