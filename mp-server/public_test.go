package main

import (
	"bufio"
	"context"
	"encoding/json"
	"fmt"
	"net/http/httptest"
	"strings"
	"sync"
	"testing"
	"time"

	"github.com/coder/websocket"
)

func wsTestClient(t *testing.T, endpoint string) *testClient {
	t.Helper()
	ws, _, err := websocket.Dial(context.Background(), endpoint, nil)
	if err != nil {
		t.Fatal(err)
	}
	ws.SetReadLimit(maxFrameBytes)
	tcp := websocket.NetConn(context.Background(), ws, websocket.MessageBinary)
	t.Cleanup(func() { tcp.Close() })
	return &testClient{tcp: &tcp, rd: bufio.NewReader(tcp), t: t}
}
func publicJoin(c *testClient, room, name string, listed bool) uint32 {
	c.send(map[string]interface{}{"type": "HANDSHAKE", "roomId": room, "roomState": map[string]interface{}{"listed": listed}, "clientState": map[string]interface{}{"name": name, "teamId": "campaign", "seed": 0, "isSaveLoaded": true, "sceneNum": 85, "curRoomNum": 0, "clientVersion": "public-test"}})
	roster := c.recv("ALL_CLIENT_STATE", time.Second)
	for _, entry := range roster["state"].([]interface{}) {
		m := entry.(map[string]interface{})
		if m["self"] == true {
			return uint32(m["clientId"].(float64))
		}
	}
	c.t.Fatal("no own identity")
	return 0
}
func TestPublicWebSocketDirectoryAndQuest(t *testing.T) {
	*portFlag = 43390
	startServer(t)
	h := httptest.NewServer(relayHTTPHandler())
	defer h.Close()
	endpoint := "ws" + strings.TrimPrefix(h.URL, "http") + "/ws"
	a, b, browser := wsTestClient(t, endpoint), wsTestClient(t, endpoint), wsTestClient(t, endpoint)
	room := fmt.Sprintf("public-%d", time.Now().UnixNano())
	aID := publicJoin(a, room, "A", true)
	bID := publicJoin(b, room, "B", true)
	if aID == bID {
		t.Fatal("duplicate identity")
	}
	browser.send(map[string]interface{}{"type": "LIST_ROOMS"})
	list := browser.recv("ROOM_LIST", time.Second)
	found := false
	for _, entry := range list["rooms"].([]interface{}) {
		r := entry.(map[string]interface{})
		if r["roomId"] == room {
			found = true
			if r["players"].(float64) != 2 || r["maxPlayers"].(float64) != 8 {
				t.Fatal(r)
			}
		}
	}
	if !found {
		t.Fatal("public room missing")
	}
	a.send(map[string]interface{}{"type": "ARPG_QUEST", "version": 1, "op": "share", "id": 1, "state": map[string]interface{}{"size": 1}})
	a.recv("ARPG_QUEST", time.Second)
	b.recv("ARPG_QUEST", time.Second)
	b.send(map[string]interface{}{"type": "ARPG_QUEST", "version": 1, "op": "join", "id": 1})
	q := b.recv("ARPG_QUEST", time.Second)
	if q["quest"].(map[string]interface{})["size"].(float64) != 2 {
		t.Fatal("party size not shared over WebSocket")
	}
	// Query the HTTPS-style directory without joining a room.
	response, err := h.Client().Get(h.URL + "/rooms")
	if err != nil {
		t.Fatal(err)
	}
	defer response.Body.Close()
	var directory map[string]interface{}
	if json.NewDecoder(response.Body).Decode(&directory) != nil || directory["type"] != "ROOM_LIST" {
		t.Fatal("HTTP directory failed")
	}
}
func TestUnlistedRoomsOwnershipAndSender(t *testing.T) {
	*portFlag = 43390
	startServer(t)
	a, b := dial(t), dial(t)
	defer (*a.tcp).Close()
	defer (*b.tcp).Close()
	room := fmt.Sprintf("hidden-%d", time.Now().UnixNano())
	aID := publicJoin(a, room, "A", false)
	bID := publicJoin(b, room, "B", true)
	b.recv("UPDATE_ROOM_STATE", time.Second)
	b.send(map[string]interface{}{"type": "UPDATE_ROOM_STATE", "state": map[string]interface{}{"listed": true, "ownerClientId": bID}})
	b.send(map[string]interface{}{"type": "LIST_ROOMS"})
	for _, entry := range b.recv("ROOM_LIST", time.Second)["rooms"].([]interface{}) {
		if entry.(map[string]interface{})["roomId"] == room {
			t.Fatal("guest exposed private room")
		}
	}
	b.send(map[string]interface{}{"type": "PLAYER_UPDATE", "clientId": aID, "state": map[string]interface{}{"marker": "sender"}})
	if a.recv("PLAYER_UPDATE", time.Second)["clientId"].(float64) != float64(bID) {
		t.Fatal("sender identity spoofed")
	}
	(*a.tcp).Close()
	state := b.recv("UPDATE_ROOM_STATE", time.Second)["state"].(map[string]interface{})
	if state["ownerClientId"].(float64) != float64(bID) {
		t.Fatal("ownership did not transfer")
	}
	b.send(map[string]interface{}{"type": "UPDATE_ROOM_STATE", "state": map[string]interface{}{"listed": true}})
	b.send(map[string]interface{}{"type": "LIST_ROOMS"})
	found := false
	for _, entry := range b.recv("ROOM_LIST", time.Second)["rooms"].([]interface{}) {
		if entry.(map[string]interface{})["roomId"] == room {
			found = true
		}
	}
	if !found {
		t.Fatal("new owner could not publish room")
	}
}
func TestConcurrentDirectoryAndRoomLifecycle(t *testing.T) {
	s := &server{rooms: map[string]*room{}}
	var wg sync.WaitGroup
	for worker := 0; worker < 8; worker++ {
		wg.Add(1)
		go func(worker int) {
			defer wg.Done()
			for i := 0; i < 100; i++ {
				id := fmt.Sprintf("%d-%d", worker, i)
				s.roomFor(id)
				s.buildRoomList()
				s.mu.Lock()
				delete(s.rooms, id)
				s.mu.Unlock()
			}
		}(worker)
	}
	wg.Wait()
}
func TestBoundedFramingAndLimiter(t *testing.T) {
	scanner := bufio.NewScanner(strings.NewReader(strings.Repeat("x", maxFrameBytes+1)))
	scanner.Buffer(make([]byte, 4096), maxFrameBytes)
	scanner.Split(splitFrames)
	if scanner.Scan() || scanner.Err() == nil {
		t.Fatal("oversized frame accepted")
	}
	limiter := newPacketLimiter()
	if limiter.allow(9 * 1024 * 1024) {
		t.Fatal("oversized burst accepted")
	}
	state := normalizeRoomState(json.RawMessage(`{"ownerClientId":999,"listed":true,"difficultyMode":99}`), 7)
	var decoded map[string]interface{}
	json.Unmarshal(state, &decoded)
	if decoded["ownerClientId"].(float64) != 7 || decoded["difficultyMode"].(float64) != 1 {
		t.Fatal("invalid settings trusted")
	}
}

func TestNullPacketAndFullRoom(t *testing.T) {
	*portFlag = 43390
	startServer(t)
	room := fmt.Sprintf("capacity-%d", time.Now().UnixNano())
	clients := []*testClient{}
	for i := 0; i < maxRoomPlayers; i++ {
		c := dial(t)
		clients = append(clients, c)
		defer (*c.tcp).Close()
		publicJoin(c, room, "Capacity", true)
	}
	a := clients[0]
	(*a.tcp).Write([]byte("null\x00"))
	a.send(map[string]interface{}{"type": "LIST_ROOMS"})
	a.recv("ROOM_LIST", time.Second)
	extra := dial(t)
	defer (*extra.tcp).Close()
	extra.send(map[string]interface{}{"type": "HANDSHAKE", "roomId": room})
	if extra.recv("SERVER_MESSAGE", time.Second)["message"] != "This room is full." {
		t.Fatal("ninth player not rejected")
	}
}
