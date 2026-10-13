package main

import (
	"bufio"
	"context"
	"encoding/json"
	"github.com/coder/websocket"
	"net"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"
)

func TestBridgeListingChoiceAndTraffic(t *testing.T) {
	for _, tc := range []struct {
		name         string
		state        map[string]interface{}
		listed, want bool
	}{
		{"legacy public", map[string]interface{}{}, true, true},
		{"legacy unlisted", map[string]interface{}{}, false, false},
		{"explicit private", map[string]interface{}{"listed": false}, true, false},
	} {
		t.Run(tc.name, func(t *testing.T) {
			result := make(chan bool, 1)
			h := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
				ws, err := websocket.Accept(w, r, nil)
				if err != nil {
					return
				}
				defer ws.CloseNow()
				ctx, cancel := context.WithTimeout(r.Context(), time.Second)
				defer cancel()
				_, payload, err := ws.Read(ctx)
				if err != nil {
					return
				}
				var packet map[string]interface{}
				json.Unmarshal(payload[:len(payload)-1], &packet)
				result <- packet["roomState"].(map[string]interface{})["listed"] == tc.want
				ws.Write(ctx, websocket.MessageBinary, []byte("{\"type\":\"ROOM_LIST\",\"rooms\":[]}\x00"))
			}))
			defer h.Close()
			local, game := net.Pipe()
			defer game.Close()
			ctx, cancel := context.WithCancel(context.Background())
			defer cancel()
			go bridge(ctx, local, "ws"+strings.TrimPrefix(h.URL, "http"), tc.listed)
			packet, _ := json.Marshal(map[string]interface{}{"type": "HANDSHAKE", "roomState": tc.state})
			game.SetDeadline(time.Now().Add(3 * time.Second))
			game.Write(append(packet, 0))
			reply, err := bufio.NewReader(game).ReadBytes(0)
			if err != nil || !strings.Contains(string(reply), "ROOM_LIST") {
				t.Fatalf("reply %q: %v", reply, err)
			}
			select {
			case ok := <-result:
				if !ok {
					t.Fatal("visibility choice changed")
				}
			case <-time.After(time.Second):
				t.Fatal("no handshake")
			}
		})
	}
}
