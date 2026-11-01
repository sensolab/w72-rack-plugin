// brumista-mcp is the MCP server for Brumista, W72's bridge module. It speaks
// MCP over stdio to the client and Brumista's line-based JSON protocol over TCP
// to Rack. Its tools are generated from protocol/commands.json; no patch logic
// lives here.
//
//	brumista-mcp                       connect to 127.0.0.1:7200
//	brumista-mcp -host H -port P       connect elsewhere
//	brumista-mcp -version              print the version and exit
package main

import (
	"context"
	"flag"
	"fmt"
	"log"
	"os"
	"strconv"

	"github.com/modelcontextprotocol/go-sdk/mcp"
)

func main() {
	host := flag.String("host", "127.0.0.1", "Brumista's host")
	port := flag.Int("port", 7200, "Brumista's port")
	showVersion := flag.Bool("version", false, "print the version and exit")
	flag.Parse()

	if *showVersion {
		fmt.Println("brumista-mcp", version)
		return
	}

	// stdout carries MCP; diagnostics go to stderr, which clients log.
	log.SetFlags(0)
	log.SetPrefix("brumista-mcp: ")

	spec, err := loadSpec()
	if err != nil {
		log.Fatal(err)
	}
	bridge := &Bridge{Addr: *host + ":" + strconv.Itoa(*port)}
	server := newServer(spec, bridge)
	if err := server.Run(context.Background(), &mcp.StdioTransport{}); err != nil {
		log.Print(err)
		os.Exit(1)
	}
}
