package main

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"net/http"
	"strings"
	"sync/atomic"
	"time"
	"unicode"

	"github.com/coder/websocket"
)

const maxFrameBytes = 2 * 1024 * 1024
const maxRoomPlayers = 8
const maxRooms = 128

var connections atomic.Int32

func admitConnection() bool {
	if connections.Add(1) > 512 {
		connections.Add(-1)
		return false
	}
	return true
}
func releaseConnection() { connections.Add(-1) }
func splitFrames(data []byte, atEOF bool) (int, []byte, error) {
	if i := bytes.IndexByte(data, 0); i >= 0 {
		return i + 1, data[:i], nil
	}
	return 0, nil, nil
}

type packetLimiter struct {
	last           time.Time
	packets, bytes float64
}

func newPacketLimiter() *packetLimiter { return &packetLimiter{time.Now(), 600, 8 * 1024 * 1024} }
func (l *packetLimiter) allow(size int) bool {
	now := time.Now()
	elapsed := now.Sub(l.last).Seconds()
	l.last = now
	l.packets = min(600, l.packets+elapsed*180)
	l.bytes = min(8*1024*1024, l.bytes+elapsed*1024*1024)
	l.packets--
	l.bytes -= float64(size)
	return l.packets >= 0 && l.bytes >= 0
}
func shortText(value string, limit int) string {
	value = strings.Map(func(r rune) rune {
		if unicode.IsControl(r) || unicode.Is(unicode.Cf, r) {
			return -1
		}
		return r
	}, value)
	if len(value) > limit {
		value = value[:limit]
	}
	return strings.ToValidUTF8(value, "")
}
func normalizeClient(s *ClientState) {
	normalizeCampaign(s)
	s.Name = shortText(s.Name, 48)
	if s.Name == "" {
		s.Name = "Adventurer"
	}
	s.TeamID = shortText(s.TeamID, 48)
	if s.TeamID == "" {
		s.TeamID = "default"
	}
	s.ClientVersion = shortText(s.ClientVersion, 80)
	s.Self = false
	s.Online = true
	for _, field := range []struct {
		raw      *json.RawMessage
		fallback string
		min, max int64
	}{
		{&s.Seed, "0", 0, 4294967295}, {&s.SceneNum, "0", -1, 255}, {&s.CurRoomNum, "-1", -1, 127}, {&s.EntranceIndex, "0", 0, 65535},
	} {
		var n int64
		if json.Unmarshal(*field.raw, &n) != nil || n < field.min || n > field.max {
			*field.raw = json.RawMessage(field.fallback)
		}
	}
	var color map[string]int
	colorErr := json.Unmarshal(s.Color, &color)
	_, red := color["r"]
	_, green := color["g"]
	_, blue := color["b"]
	if colorErr != nil || !red || !green || !blue || len(color) != 3 || color["r"] < 0 || color["r"] > 255 || color["g"] < 0 || color["g"] > 255 || color["b"] < 0 || color["b"] > 255 {
		s.Color = json.RawMessage(`{"r":100,"g":255,"b":100}`)
	}
}
func normalizeRoomState(raw json.RawMessage, owner uint32) json.RawMessage {
	var input map[string]interface{}
	json.Unmarshal(raw, &input)
	out := map[string]interface{}{"ownerClientId": owner, "pvpMode": 0, "showLocationsMode": 1, "teleportMode": 1, "syncItemsAndFlags": 0, "difficultyMode": 1, "arpgSeed": 0, "listed": input["listed"] == true}
	for _, k := range []string{"pvpMode", "showLocationsMode", "teleportMode", "syncItemsAndFlags", "difficultyMode", "arpgSeed"} {
		if n, ok := input[k].(float64); ok && n >= 0 && n <= 4294967295 && n == float64(uint32(n)) {
			if k == "arpgSeed" || (k == "syncItemsAndFlags" && n <= 1) || (k != "syncItemsAndFlags" && n <= 2) {
				out[k] = uint32(n)
			}
		}
	}
	encoded, _ := json.Marshal(out)
	return encoded
}
func relayHTTPHandler() http.Handler {
	mux := http.NewServeMux()
	mux.HandleFunc("/healthz", func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		w.Write([]byte(`{"status":"ok","questProtocol":1}`))
	})
	mux.HandleFunc("/readyz", func(w http.ResponseWriter, r *http.Request) {
		if isDraining() {
			http.Error(w, "draining", http.StatusServiceUnavailable)
			return
		}
		w.Header().Set("Content-Type", "application/json")
		w.Write([]byte(`{"status":"ready"}`))
	})
	mux.HandleFunc("/metrics", func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "text/plain; version=0.0.4")
		srv.mu.RLock()
		rooms := len(srv.rooms)
		srv.mu.RUnlock()
		fmt.Fprintf(w, "# TYPE relay_connections gauge\nrelay_connections %d\n# TYPE relay_rooms gauge\nrelay_rooms %d\n# TYPE relay_pending_bytes gauge\nrelay_pending_bytes %d\n# TYPE relay_slow_disconnects_total counter\nrelay_slow_disconnects_total %d\n", connections.Load(), rooms, pendingBytes.Load(), slowDisconnects.Load())
	})
	mux.HandleFunc("/rooms", func(w http.ResponseWriter, r *http.Request) {
		if r.Method != http.MethodGet {
			http.Error(w, "GET required", 405)
			return
		}
		w.Header().Set("Content-Type", "application/json")
		w.Header().Set("Cache-Control", "no-store")
		json.NewEncoder(w).Encode(map[string]interface{}{"type": "ROOM_LIST", "rooms": srv.buildRoomList(), "maxPlayers": maxRoomPlayers, "questProtocol": 1})
	})
	mux.HandleFunc("/ws", func(w http.ResponseWriter, r *http.Request) {
		if isDraining() || !admitConnection() {
			http.Error(w, "Relay full", 503)
			return
		}
		defer releaseConnection()
		ws, err := websocket.Accept(w, r, &websocket.AcceptOptions{OriginPatterns: []string{"mlapi.us"}, CompressionMode: websocket.CompressionDisabled})
		if err != nil {
			return
		}
		defer ws.CloseNow()
		ws.SetReadLimit(maxFrameBytes)
		ctx, cancel := context.WithCancel(r.Context())
		defer cancel()
		// Binary WebSocket messages carry the unchanged null-delimited Anchor stream.
		handleTCP(websocket.NetConn(ctx, ws, websocket.MessageBinary))
	})
	return mux
}

// Bound retained data independently of active connections.
func roomCacheBytes(r *room) int {
	n := 0
	for _, state := range r.lastTeam {
		n += len(state)
	}
	for _, queue := range r.teamQueues {
		for _, p := range queue {
			n += len(p)
		}
	}
	return n
}
