// [TIME] | [TEST] | main_test.go | Protocol integration test: simulates two Anchor clients
package main

import (
	"bufio"
	"encoding/json"
	"net"
	"os"
	"sync"
	"testing"
	"time"
)

type testClient struct {
	tcp *net.Conn
	rd  *bufio.Reader
	id  uint32
	t   *testing.T
}

func dial(t *testing.T) *testClient {
	tcp, err := net.Dial("tcp", testAddress)
	if err != nil {
		t.Fatalf("server not running: %v", err)
	}
	t.Cleanup(func() { tcp.Close() })
	return &testClient{tcp: &tcp, rd: bufio.NewReader(tcp), t: t}
}

func (c *testClient) send(obj map[string]interface{}) {
	b, _ := json.Marshal(obj)
	b = append(b, 0)
	(*c.tcp).Write(b)
}

// recv waits for the next packet of the given type (skipping others).
func (c *testClient) recv(want string, timeout time.Duration) map[string]interface{} {
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		(*c.tcp).SetReadDeadline(time.Now().Add(time.Until(deadline)))
		line, err := c.rd.ReadBytes(0)
		if err != nil {
			c.t.Fatalf("recv(%s): %v", want, err)
		}
		var p map[string]interface{}
		if json.Unmarshal(line[:len(line)-1], &p) != nil {
			continue
		}
		if p["type"] == want {
			return p
		}
	}
	c.t.Fatalf("timeout waiting for %s", want)
	return nil
}

var testAddress string
var testListener net.Listener
var startServerOnce sync.Once

func startServer(t *testing.T) {
	t.Helper()
	startServerOnce.Do(func() {
		var err error
		testListener, err = net.Listen("tcp", "127.0.0.1:0")
		if err != nil {
			t.Fatal(err)
		}
		testAddress = testListener.Addr().String()
		go serveTCP(testListener)
	})
}
func TestMain(m *testing.M) {
	result := m.Run()
	if testListener != nil {
		testListener.Close()
	}
	os.Exit(result)
}

func TestTwoClientFlow(t *testing.T) {
	// Override port via flag for the test
	*portFlag = 43390
	startServer(t)

	// --- Client 1 connects and creates the room
	a := dial(t)
	a.send(map[string]interface{}{
		"type":        "HANDSHAKE",
		"roomId":      "test-room",
		"roomState":   map[string]interface{}{"ownerClientId": 0, "pvpMode": 2, "syncItemsAndFlags": 1},
		"clientState": map[string]interface{}{"name": "A", "teamId": "red", "online": true, "isSaveLoaded": true, "sceneNum": 81},
	})

	// A gets its own roster with self=true and the assigned clientId
	roster := a.recv("ALL_CLIENT_STATE", 3*time.Second)
	st := roster["state"].([]interface{})[0].(map[string]interface{})
	if st["self"] != true {
		t.Fatalf("expected self=true for own roster entry")
	}
	if _, ok := st["clientId"].(float64); !ok {
		t.Fatalf("expected clientId assigned, got %v", st["clientId"])
	}
	aID := uint32(st["clientId"].(float64))

	// A receives the authoritative room state back
	rs := a.recv("UPDATE_ROOM_STATE", 2*time.Second)
	if rs["state"].(map[string]interface{})["syncItemsAndFlags"].(float64) != 1 {
		t.Fatalf("room state not echoed back")
	}

	// --- Client 2 joins the same room/team
	b := dial(t)
	b.send(map[string]interface{}{
		"type":        "HANDSHAKE",
		"roomId":      "test-room",
		"clientState": map[string]interface{}{"name": "B", "teamId": "red", "online": true, "isSaveLoaded": true, "sceneNum": 81},
	})

	// Both clients see 2-player rosters
	rosterA := a.recv("ALL_CLIENT_STATE", 3*time.Second)
	if len(rosterA["state"].([]interface{})) != 2 {
		t.Fatalf("A expected 2 clients, got %v", rosterA["state"])
	}
	rosterB := b.recv("ALL_CLIENT_STATE", 3*time.Second)
	if len(rosterB["state"].([]interface{})) != 2 {
		t.Fatalf("B expected 2 clients, got %v", rosterB["state"])
	}
	// B's own entry has self=true, A's doesn't
	for _, e := range rosterB["state"].([]interface{}) {
		m := e.(map[string]interface{})
		if m["name"] == "B" && m["self"] != true {
			t.Fatalf("B's own roster entry should have self=true")
		}
		if m["name"] == "A" && m["self"] == true {
			t.Fatalf("A's entry in B's roster should have self=false")
		}
	}

	// B requests team state -> server asks A (save-loaded) -> A sends UPDATE_TEAM_STATE -> B receives it
	b.send(map[string]interface{}{"type": "REQUEST_TEAM_STATE", "targetTeamId": "red"})
	a.recv("REQUEST_TEAM_STATE", 3*time.Second)
	a.send(map[string]interface{}{
		"type": "UPDATE_TEAM_STATE", "targetTeamId": "red", "state": map[string]interface{}{"healthCapacity": 480},
	})
	uts := b.recv("UPDATE_TEAM_STATE", 3*time.Second)
	if uts["state"].(map[string]interface{})["healthCapacity"].(float64) != 480 {
		t.Fatalf("team state not relayed to B")
	}

	// Queued GIVE_ITEM for the team is replayed when B reloads (queue consumed by UPDATE_TEAM_STATE)
	a.send(map[string]interface{}{
		"type": "GIVE_ITEM", "targetTeamId": "red", "addToQueue": true, "modId": 0, "getItemId": 10,
	})
	b.recv("GIVE_ITEM", 3*time.Second)

	// Targeted packet routing: A damages B
	for _, e := range rosterB["state"].([]interface{}) {
		m := e.(map[string]interface{})
		if m["name"] == "B" {
			bID := uint32(m["clientId"].(float64))
			a.send(map[string]interface{}{
				"type": "DAMAGE_PLAYER", "targetClientId": bID, "damageEffect": 7, "damage": 2,
			})
		}
	}
	b.recv("DAMAGE_PLAYER", 3*time.Second)

	// A leaves -> B gets a 1-player roster
	(*a.tcp).Close()
	final := b.recv("ALL_CLIENT_STATE", 3*time.Second)
	if len(final["state"].([]interface{})) != 1 {
		t.Fatalf("expected 1 client after disconnect, got %v", final["state"])
	}
	_ = aID
}

func TestRoomListing(t *testing.T) {
	*portFlag = 43390
	startServer(t)

	// Unjoined browser connection: LIST_ROOMS without handshake
	c := dial(t)
	c.send(map[string]interface{}{"type": "LIST_ROOMS"})
	resp := c.recv("ROOM_LIST", 3*time.Second)
	rooms, ok := resp["rooms"].([]interface{})
	if !ok {
		t.Fatalf("expected rooms array, got %v", resp)
	}
	// test-room may or may not exist depending on test order; listing must be an array
	for _, rr := range rooms {
		m := rr.(map[string]interface{})
		if m["roomId"] == nil {
			t.Fatalf("room entry missing roomId: %v", m)
		}
	}

	// Joined client can also list (sees its own room)
	j := dial(t)
	j.send(map[string]interface{}{
		"type":        "HANDSHAKE",
		"roomId":      "list-test-room",
		"roomState":   map[string]interface{}{"listed": true},
		"clientState": map[string]interface{}{"name": "L", "teamId": "t", "online": true, "isSaveLoaded": true},
	})
	j.recv("ALL_CLIENT_STATE", 3*time.Second)
	j.send(map[string]interface{}{"type": "LIST_ROOMS"})
	resp2 := j.recv("ROOM_LIST", 3*time.Second)
	found := false
	for _, rr := range resp2["rooms"].([]interface{}) {
		m := rr.(map[string]interface{})
		if m["roomId"] == "list-test-room" {
			found = true
			if m["players"].(float64) != 1 || m["playersInGame"].(float64) != 1 {
				t.Fatalf("room counts wrong: %v", m)
			}
		}
	}
	if !found {
		t.Fatalf("joined room missing from listing: %v", resp2["rooms"])
	}
}
