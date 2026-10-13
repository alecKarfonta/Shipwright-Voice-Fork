// public-client gives existing TCP-only Anchor clients access to the public WSS relay.
package main

import (
	"bufio"
	"bytes"
	"context"
	"encoding/json"
	"flag"
	"fmt"
	"io"
	"log"
	"net"
	"os"
	"os/signal"
	"strings"
	"time"

	"github.com/coder/websocket"
)

func main() {
	address := flag.String("listen", "127.0.0.1:43383", "loopback TCP endpoint for the game")
	endpoint := flag.String("relay", "wss://mlapi.us/z64/relay/ws", "public relay URL")
	unlisted := flag.Bool("unlisted", false, "hide newly created rooms from public discovery")
	flag.Parse()
	host, _, err := net.SplitHostPort(*address)
	if err != nil || net.ParseIP(host) == nil || !net.ParseIP(host).IsLoopback() {
		log.Fatal("Listen address must be a loopback IP (127.0.0.1 or ::1).")
	}
	listener, err := net.Listen("tcp", *address)
	if err != nil {
		log.Fatalf("Cannot listen on %s: %v. Close any other co-op server first, or choose -listen 127.0.0.1:43384.", *address, err)
	}
	defer listener.Close()
	fmt.Printf("Hyrule Reborn public relay connector\n\nKeep this window open while playing.\nIn the game's Server Browser, browse %s and select Refresh.\nTo create a game, use this address in Network > Anchor with your player name, room name and team.\nPlayers using the same room and team adventure together.\nNew rooms are %s; unlisted rooms still allow anyone who knows their name.\nThe connector does not open a port on your router or local network.\n\n", *address, map[bool]string{true: "unlisted", false: "shown in the public game list"}[*unlisted])
	ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt)
	defer stop()
	go func() { <-ctx.Done(); listener.Close() }()
	slots := make(chan struct{}, 8)
	for {
		local, err := listener.Accept()
		if err != nil {
			return
		}
		select {
		case slots <- struct{}{}:
			go func() { defer func() { <-slots }(); bridge(ctx, local, *endpoint, !*unlisted) }()
		default:
			local.Close()
		}
	}
}
func bridge(parent context.Context, local net.Conn, endpoint string, listed bool) {
	defer local.Close()
	ctx, cancel := context.WithCancel(parent)
	defer cancel()
	dialCtx, stop := context.WithTimeout(ctx, 10*time.Second)
	ws, _, err := websocket.Dial(dialCtx, endpoint, nil)
	stop()
	if err != nil {
		log.Printf("Public relay unavailable: %v", err)
		return
	}
	defer ws.CloseNow()
	ws.SetReadLimit(2 * 1024 * 1024)
	remote := websocket.NetConn(ctx, ws, websocket.MessageBinary)
	defer remote.Close()
	go func() {
		ticker := time.NewTicker(25 * time.Second)
		defer ticker.Stop()
		for {
			select {
			case <-ctx.Done():
				return
			case <-ticker.C:
				pingCtx, done := context.WithTimeout(ctx, 5*time.Second)
				err := ws.Ping(pingCtx)
				done()
				if err != nil {
					cancel()
					local.Close()
					return
				}
			}
		}
	}()
	go func() { defer local.Close(); defer cancel(); io.Copy(local, remote) }()
	reader := bufio.NewScanner(local)
	reader.Buffer(make([]byte, 4096), 2*1024*1024)
	reader.Split(func(data []byte, atEOF bool) (int, []byte, error) {
		if n := bytes.IndexByte(data, 0); n >= 0 {
			return n + 1, data[:n], nil
		}
		return 0, nil, nil
	})
	for reader.Scan() {
		payload := reader.Bytes()
		var packet map[string]interface{}
		if json.Unmarshal(payload, &packet) == nil && packet["type"] == "HANDSHAKE" {
			state, ok := packet["roomState"].(map[string]interface{})
			if !ok {
				state = map[string]interface{}{}
				packet["roomState"] = state
			}
			if _, explicit := state["listed"]; !explicit {
				state["listed"] = listed
			}
			payload, _ = json.Marshal(packet)
		}
		remote.SetWriteDeadline(time.Now().Add(5 * time.Second))
		if _, err := io.Copy(remote, strings.NewReader(string(payload)+"\x00")); err != nil {
			return
		}
	}
}
