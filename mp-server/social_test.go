package main

import (
	"bufio"
	"encoding/json"
	"fmt"
	"net"
	"strings"
	"testing"
	"time"
)

func socialSend(c *testClient, op string, fields map[string]interface{}) {
	p := map[string]interface{}{"type": "SOCIAL", "version": 1, "op": op}
	for k, v := range fields {
		p[k] = v
	}
	c.send(p)
}
func socialRecv(c *testClient, op string) map[string]interface{} {
	for i := 0; i < 100; i++ {
		p := c.recv("SOCIAL", time.Second)
		if p["op"] == op {
			return p
		}
	}
	c.t.Fatal("missing social op", op)
	return nil
}
func socialState(c *testClient) map[string]interface{} {
	socialSend(c, "hello", nil)
	return socialRecv(c, "state")
}
func barrier(c *testClient, forbidden ...string) {
	c.send(map[string]interface{}{"type": "PING"})
	(*c.tcp).SetReadDeadline(time.Now().Add(time.Second))
	for {
		line, err := c.rd.ReadBytes(0)
		if err != nil {
			c.t.Fatal(err)
		}
		var p map[string]interface{}
		json.Unmarshal(line[:len(line)-1], &p)
		if p["type"] == "PONG" {
			return
		}
		for _, typ := range forbidden {
			if p["type"] == typ {
				c.t.Fatalf("unauthorized %s delivered", typ)
			}
		}
	}
}
func TestSocialPartyPrivacyReadyAndPromotion(t *testing.T) {
	room := fmt.Sprint("fellowship-", time.Now().UnixNano())
	a := partyClient(t, room, "red", "Link")
	b := partyClient(t, room, "red", "Zelda")
	other := partyClient(t, room, "blue", "Sheik")
	if socialState(a)["leader"] != float64(a.id) {
		t.Fatal("wrong leader")
	}
	socialState(b)
	socialState(other)
	socialSend(b, "chat", map[string]interface{}{"channel": "party", "text": "  Hello\nHyrule\u202e  ", "clientId": a.id, "name": "Fake", "requestId": 12})
	chat := socialRecv(a, "chat")["message"].(map[string]interface{})
	if chat["name"] != "Zelda" || chat["clientId"] != float64(b.id) || chat["text"] != "HelloHyrule" {
		t.Fatal("untrusted chat identity or text", chat)
	}
	socialRecv(b, "chat")
	if len(socialState(other)["history"].([]interface{})) != 0 {
		t.Fatal("party chat leaked")
	}
	socialSend(b, "ready_check", nil)
	socialRecv(b, "error")
	socialSend(a, "ready_check", nil)
	check := socialRecv(a, "state")["check"].(map[string]interface{})
	checkID := check["id"]
	if len(check["members"].(map[string]interface{})) != 2 {
		t.Fatal("wrong ready roster")
	}
	socialRecv(b, "state")
	socialSend(b, "ready", map[string]interface{}{"checkId": checkID, "ready": true})
	state := socialRecv(a, "state")
	if state["active"] != false {
		t.Fatal("completed ready check still active")
	}
	socialRecv(b, "state")
	socialSend(b, "promote", map[string]interface{}{"targetClientId": b.id})
	socialRecv(b, "error")
	socialSend(a, "promote", map[string]interface{}{"targetClientId": b.id})
	socialRecv(a, "state")
	if socialRecv(b, "state")["leader"] != float64(b.id) {
		t.Fatal("leadership did not transfer")
	}
	socialSend(b, "ready_check", nil)
	socialRecv(a, "state")
	socialRecv(b, "state")
	(*a.tcp).Close()
	state = socialRecv(b, "state")
	members := state["check"].(map[string]interface{})["members"].(map[string]interface{})
	if members[fmt.Sprint(a.id)] != "departed" {
		t.Fatal("departed player left pending", members)
	}
}
func TestSocialHistoryBoundedAndRateLimit(t *testing.T) {
	room := fmt.Sprint("chat-", time.Now().UnixNano())
	a := partyClient(t, room, "red", "Link")
	socialState(a)
	for i := 0; i < 4; i++ {
		socialSend(a, "chat", map[string]interface{}{"channel": "room", "text": "hello"})
		socialRecv(a, "chat")
	}
	socialSend(a, "chat", map[string]interface{}{"channel": "room", "text": "flood"})
	socialRecv(a, "error")
	if len(socialState(a)["history"].([]interface{})) != 4 {
		t.Fatal("rate limit failed")
	}
	srv.mu.RLock()
	r := srv.rooms[room]
	srv.mu.RUnlock()
	for i := 0; i < maxChatHistory+10; i++ {
		r.mu.Lock()
		r.clients[a.id].chatLimit.tokens = 4
		r.mu.Unlock()
		socialSend(a, "chat", map[string]interface{}{"channel": "room", "text": strings.Repeat("a", 600)})
		socialRecv(a, "chat")
	}
	history := socialState(a)["history"].([]interface{})
	if len(history) != maxChatHistory {
		t.Fatal("unbounded chat history")
	}
	if len(history[0].(map[string]interface{})["text"].(string)) != 480 {
		t.Fatal("unbounded chat text")
	}
}
func TestReadyExpiryAndLateJoin(t *testing.T) {
	room := fmt.Sprint("ready-", time.Now().UnixNano())
	a := partyClient(t, room, "red", "A")
	b := partyClient(t, room, "red", "B")
	socialState(a)
	socialState(b)
	socialSend(a, "ready_check", nil)
	state := socialRecv(a, "state")
	socialRecv(b, "state")
	id := state["check"].(map[string]interface{})["id"]
	late := partyClient(t, room, "red", "Late")
	socialState(late)
	socialSend(late, "ready", map[string]interface{}{"checkId": id, "ready": true})
	socialRecv(late, "error")
	srv.mu.RLock()
	r := srv.rooms[room]
	srv.mu.RUnlock()
	r.mu.Lock()
	r.social.checks[socialScope(r.clients[a.id])].ExpiresAt = time.Now().Add(-time.Second).UnixMilli()
	r.mu.Unlock()
	socialSend(b, "ready", map[string]interface{}{"checkId": id, "ready": true})
	socialRecv(b, "error")
	if socialState(b)["active"] != false {
		t.Fatal("expired ready check still active")
	}
}
func TestRelayRejectsUntrustedPacketClasses(t *testing.T) {
	room := fmt.Sprint("permissions-", time.Now().UnixNano())
	a := partyClient(t, room, "red", "A")
	b := partyClient(t, room, "blue", "B")
	barrier(a)
	barrier(b)
	for _, typ := range []string{"DISABLE_ANCHOR", "UNRECOGNIZED", "SERVER_CAPABILITIES", "SERVER_MESSAGE"} {
		a.send(map[string]interface{}{"type": typ})
	}
	a.send(map[string]interface{}{"type": "UPDATE_CLIENT_STATE", "state": "malformed"})
	a.send(map[string]interface{}{"type": "SET_FLAG", "targetTeamId": "blue", "flag": 1})
	a.send(map[string]interface{}{"type": "DAMAGE_PLAYER", "targetClientId": b.id, "damage": 2, "damageEffect": 7})
	a.send(map[string]interface{}{"type": "TELEPORT_TO", "targetClientId": b.id})
	barrier(a)
	barrier(b, "DISABLE_ANCHOR", "UNRECOGNIZED", "SERVER_CAPABILITIES", "SERVER_MESSAGE", "UPDATE_CLIENT_STATE", "SET_FLAG", "DAMAGE_PLAYER", "TELEPORT_TO")
	// An authorized request is required before a teleport response, once only.
	a.send(map[string]interface{}{"type": "UPDATE_ROOM_STATE", "state": map[string]interface{}{"teleportMode": 2}})
	b.recv("UPDATE_ROOM_STATE", time.Second)
	a.send(map[string]interface{}{"type": "REQUEST_TELEPORT", "targetClientId": b.id})
	b.recv("REQUEST_TELEPORT", time.Second)
	b.send(map[string]interface{}{"type": "TELEPORT_TO", "targetClientId": a.id})
	a.recv("TELEPORT_TO", time.Second)
	b.send(map[string]interface{}{"type": "TELEPORT_TO", "targetClientId": a.id})
	barrier(b)
	barrier(a, "TELEPORT_TO")
}
func TestSlowReaderDoesNotBlockRoomOrLeakQueue(t *testing.T) {
	before := pendingBytes.Load()
	slow, peer := net.Pipe()
	defer peer.Close()
	o := newOutbox(slow)
	started := time.Now()
	for i := 0; i < 200; i++ {
		o.enqueue(make([]byte, 32768))
	}
	if time.Since(started) > time.Second {
		t.Fatal("producer blocked on a slow socket")
	}
	o.stop()
	if pendingBytes.Load() > before {
		t.Fatal("outbox bytes leaked", pendingBytes.Load(), before)
	}
	// Verify ordering and complete writes with a normal reader.
	normal, reader := net.Pipe()
	defer reader.Close()
	healthy := newOutbox(normal)
	defer healthy.stop()
	healthy.enqueue([]byte("first\x00"))
	healthy.enqueue([]byte("second\x00"))
	rd := bufio.NewReader(reader)
	reader.SetReadDeadline(time.Now().Add(time.Second))
	one, _ := rd.ReadString(0)
	two, _ := rd.ReadString(0)
	if one != "first\x00" || two != "second\x00" {
		t.Fatal("outbox reordered writes", one, two)
	}
}
