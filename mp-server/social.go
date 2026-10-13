package main

import (
	"encoding/json"
	"fmt"
	"strings"
	"time"
)

const socialProtocol = 1
const maxChatHistory = 100

type chatMessage struct {
	ID        uint64 `json:"id"`
	ClientID  uint32 `json:"clientId"`
	Name      string `json:"name"`
	Channel   string `json:"channel"`
	Text      string `json:"text"`
	SentAt    int64  `json:"sentAt"`
	RequestID uint64 `json:"requestId"`
	scope     string
}
type readyCheck struct {
	ID        uint64            `json:"id"`
	Leader    uint32            `json:"leader"`
	ExpiresAt int64             `json:"expiresAt"`
	Members   map[uint32]string `json:"members"`
}
type roomSocial struct {
	sequence uint64
	history  []chatMessage
	checks   map[string]*readyCheck
}
type chatLimiter struct {
	last   time.Time
	tokens float64
}

func (l *chatLimiter) allow(now time.Time) bool {
	l.tokens = min(4, l.tokens+now.Sub(l.last).Seconds()/2)
	l.last = now
	if l.tokens < 1 {
		return false
	}
	l.tokens--
	return true
}
func socialScope(c *conn) string {
	return fmt.Sprintf("%s\x00%s\x00%s", c.state.TeamID, c.state.Seed, c.state.ClientVersion)
}
func (r *room) socialLeader(c *conn) uint32 {
	id := uint32(0)
	for _, peer := range r.clients {
		if socialScope(peer) != socialScope(c) {
			continue
		}
		if peer.clientID == r.ownerID {
			return peer.clientID
		}
		if id == 0 || peer.clientID < id {
			id = peer.clientID
		}
	}
	return id
}
func socialPacket(op string) map[string]interface{} {
	return map[string]interface{}{"type": "SOCIAL", "version": socialProtocol, "op": op}
}
func (c *conn) socialError(message string) {
	p := socialPacket("error")
	p["message"] = message
	c.send(p)
}
func checkActive(check *readyCheck, now time.Time) bool {
	if check == nil || now.UnixMilli() >= check.ExpiresAt {
		return false
	}
	for _, status := range check.Members {
		if status == "pending" {
			return true
		}
	}
	return false
}

// Called with r.mu held; enqueue never blocks on a socket.
func (r *room) socialSnapshot(c *conn) {
	scope := socialScope(c)
	history := make([]chatMessage, 0, len(r.social.history))
	for _, msg := range r.social.history {
		if msg.Channel == "room" || msg.scope == scope {
			history = append(history, msg)
		}
	}
	p := socialPacket("state")
	p["leader"] = r.socialLeader(c)
	p["roomId"] = r.id
	p["history"] = history
	if check := r.social.checks[scope]; check != nil {
		p["check"] = check
		p["remainingMs"] = max(int64(0), check.ExpiresAt-time.Now().UnixMilli())
		p["active"] = checkActive(check, time.Now())
	}
	c.send(p)
}
func (r *room) socialPublish() {
	for _, c := range r.clients {
		if c.social {
			r.socialSnapshot(c)
		}
	}
}
func (r *room) socialDepart(id uint32) {
	for _, check := range r.social.checks {
		if _, ok := check.Members[id]; ok {
			check.Members[id] = "departed"
		}
	}
}
func (c *conn) handleSocial(raw json.RawMessage) {
	if len(raw) > 4096 {
		return
	}
	var req struct {
		Version   int    `json:"version"`
		Op        string `json:"op"`
		Channel   string `json:"channel"`
		Text      string `json:"text"`
		CheckID   uint64 `json:"checkId"`
		Ready     *bool  `json:"ready"`
		Target    uint32 `json:"targetClientId"`
		RequestID uint64 `json:"requestId"`
	}
	if json.Unmarshal(raw, &req) != nil {
		return
	}
	if req.Version != socialProtocol {
		c.socialError("Update your game to use Fellowship.")
		return
	}
	r := c.room
	r.mu.Lock()
	defer r.mu.Unlock()
	if !c.social && req.Op != "hello" {
		return
	}
	now := time.Now()
	scope := socialScope(c)
	switch req.Op {
	case "hello":
		c.social = true
		r.socialSnapshot(c)
	case "chat":
		if req.Channel != "party" && req.Channel != "room" {
			c.socialError("Choose Party or Room chat.")
			return
		}
		text := strings.TrimSpace(shortText(req.Text, 480))
		if text == "" {
			c.socialError("Write a message first.")
			return
		}
		if c.chatLimit == nil {
			c.chatLimit = &chatLimiter{now, 4}
		}
		if !c.chatLimit.allow(now) {
			c.socialError("Please wait a moment before sending another message.")
			return
		}
		r.social.sequence++
		msg := chatMessage{r.social.sequence, c.clientID, c.state.Name, req.Channel, text, now.UnixMilli(), req.RequestID, scope}
		r.social.history = append(r.social.history, msg)
		if len(r.social.history) > maxChatHistory {
			r.social.history = append([]chatMessage(nil), r.social.history[len(r.social.history)-maxChatHistory:]...)
		}
		p := socialPacket("chat")
		p["message"] = msg
		for _, peer := range r.clients {
			if peer.social && (req.Channel == "room" || socialScope(peer) == scope) {
				peer.send(p)
			}
		}
	case "ready_check":
		if c.clientID != r.socialLeader(c) {
			c.socialError("Only your party leader can start a ready check.")
			return
		}
		if checkActive(r.social.checks[scope], now) {
			c.socialError("A ready check is already in progress.")
			return
		}
		if r.social.checks == nil {
			r.social.checks = map[string]*readyCheck{}
		}
		for key, check := range r.social.checks {
			if now.UnixMilli() > check.ExpiresAt+120000 {
				delete(r.social.checks, key)
			}
		}
		if len(r.social.checks) >= 64 && r.social.checks[scope] == nil {
			c.socialError("Please wait before starting a new ready check.")
			return
		}
		r.social.sequence++
		check := &readyCheck{r.social.sequence, c.clientID, now.Add(time.Minute).UnixMilli(), map[uint32]string{}}
		for _, peer := range r.clients {
			if socialScope(peer) == scope {
				check.Members[peer.clientID] = "pending"
			}
		}
		check.Members[c.clientID] = "ready"
		r.social.checks[scope] = check
		r.socialPublish()
	case "ready":
		check := r.social.checks[scope]
		if check == nil || check.ID != req.CheckID || req.Ready == nil || now.UnixMilli() >= check.ExpiresAt {
			c.socialError("This ready check has ended.")
			return
		}
		if _, ok := check.Members[c.clientID]; !ok {
			c.socialError("Join the next ready check.")
			return
		}
		if check.Members[c.clientID] == "departed" {
			return
		}
		if *req.Ready {
			check.Members[c.clientID] = "ready"
		} else {
			check.Members[c.clientID] = "away"
		}
		r.socialPublish()
	case "promote":
		target := r.clients[req.Target]
		if c.clientID != r.ownerID || target == nil || target == c {
			c.socialError("Only the room leader can pass leadership to another member.")
			return
		}
		r.ownerID = target.clientID
		r.roomState = normalizeRoomState(r.roomState, r.ownerID)
		for _, peer := range r.clients {
			peer.send(map[string]interface{}{"type": "UPDATE_ROOM_STATE", "state": r.roomState})
		}
		r.socialPublish()
	}
}
