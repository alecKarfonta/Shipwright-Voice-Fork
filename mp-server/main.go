// [TIME] | [INFO] | main.go | Anchor-protocol relay server for Ship of Harkinian co-op
//
// Implements the Anchor wire protocol observed in soh/soh/soh/Network/:
//   - TCP, JSON packets, null-byte (\0) delimited frames
//   - Rooms keyed by roomId; clients identified by server-assigned clientId
//   - ALL_CLIENT_STATE broadcast on join/leave/state change
//   - Team-targeted packets (targetTeamId) with addToQueue server-side queueing
//   - Client-targeted packets (targetClientId) routing
//   - UPDATE_ROOM_STATE relay + owner enforcement
//
// Run from mp-server: go run .  (default port 43383)
package main

import (
	"bufio"
	"encoding/json"
	"flag"
	"fmt"
	"log"
	"net"
	"sort"
	"strings"
	"sync"
	"time"
)

// ---------------------------------------------------------------------------
// Domain types
// ---------------------------------------------------------------------------

// clientState mirrors the fields of Anchor::PrepClientState() (UpdateClientState.cpp).
// Kept as raw JSON so cross-version extra fields survive a relay round trip.
type ClientState struct {
	Campaign       *campaignState  `json:"campaign,omitempty"`
	ClientID       uint32          `json:"clientId"`
	Name           string          `json:"name"`
	Color          json.RawMessage `json:"color"`
	ClientVersion  string          `json:"clientVersion"`
	TeamID         string          `json:"teamId"`
	Online         bool            `json:"online"`
	Seed           json.RawMessage `json:"seed"`
	IsSaveLoaded   bool            `json:"isSaveLoaded"`
	IsGameComplete bool            `json:"isGameComplete"`
	SceneNum       json.RawMessage `json:"sceneNum"`
	CurRoomNum     json.RawMessage `json:"curRoomNum"`
	EntranceIndex  json.RawMessage `json:"entranceIndex"`
	Self           bool            `json:"self"`
}

type conn struct {
	tcp       net.Conn
	clientID  uint32
	room      *room
	state     ClientState
	handshook bool
	outbox    *outbox
	social    bool
	chatLimit *chatLimiter
	teleport  map[uint32]time.Time
}

type room struct {
	id        string
	social    roomSocial
	quests    map[string]*questTeam
	mu        sync.RWMutex
	clients   map[uint32]*conn
	nextID    uint32
	ownerID   uint32
	roomState json.RawMessage // authoritative room state (owner's latest)
	// teamQueues holds queued packets per team, replayed with the next UPDATE_TEAM_STATE
	teamQueues map[string][]json.RawMessage
	lastTeam   map[string]json.RawMessage // last known full team state per team
}

type server struct {
	mu    sync.RWMutex
	rooms map[string]*room
}

var (
	srv      = &server{rooms: map[string]*room{}}
	globalID uint32
	idMu     sync.Mutex
	bindFlag = flag.String("bind", "", "TCP listen host (empty listens on all interfaces)")
	portFlag = flag.Int("port", 43383, "TCP listen port (0 disables TCP)")
	httpFlag = flag.String("http", "", "HTTP/WebSocket listen address (empty disables)")
)

func nextClientID() uint32 {
	idMu.Lock()
	defer idMu.Unlock()
	globalID++
	return globalID
}

// ---------------------------------------------------------------------------
// Frame I/O: JSON + \0 delimiter (mirrors Network.cpp:112-124)
// ---------------------------------------------------------------------------

func (c *conn) send(payload map[string]interface{}) {
	c.sendRaw(func() ([]byte, error) { return json.Marshal(payload) })
}

// sendRaw forwards a packet exactly as received (preserving unknown fields).
func (c *conn) sendRaw(marshal func() ([]byte, error)) {
	b, err := marshal()
	if err != nil {
		log.Printf("[client %d] marshal error: %v", c.clientID, err)
		return
	}
	if len(b) >= maxFrameBytes {
		return
	}
	c.outbox.enqueue(append(b, 0))
}

// ---------------------------------------------------------------------------
// Room roster: ALL_CLIENT_STATE broadcast
// ---------------------------------------------------------------------------

// rosterPacket builds {type: ALL_CLIENT_STATE, state: [client...]}.
// "self" is true only on the recipient's own entry (see AllClientState.cpp:26).
func (r *room) rosterPacket(forClient uint32) map[string]interface{} {
	r.mu.RLock()
	defer r.mu.RUnlock()
	roster := make([]ClientState, 0, len(r.clients))
	for _, c := range r.clients {
		st := c.state
		st.ClientID = c.clientID
		st.Online = true
		st.Self = c.clientID == forClient
		roster = append(roster, st)
	}
	return map[string]interface{}{"type": "ALL_CLIENT_STATE", "state": roster}
}

func (r *room) broadcastRoster() {
	r.mu.RLock()
	targets := make([]*conn, 0, len(r.clients))
	for _, c := range r.clients {
		targets = append(targets, c)
	}
	r.mu.RUnlock()
	for _, c := range targets {
		c.send(r.rosterPacket(c.clientID))
	}
}

// relay sends a raw packet to every client in the room except the sender.
func (r *room) relay(sender *conn, raw json.RawMessage) {
	r.mu.RLock()
	targets := make([]*conn, 0, len(r.clients))
	for _, c := range r.clients {
		if c != sender {
			targets = append(targets, c)
		}
	}
	r.mu.RUnlock()
	for _, c := range targets {
		c.sendRaw(func() ([]byte, error) { return append(json.RawMessage(nil), raw...), nil })
	}
}

func (r *room) teamExcept(sender *conn, teamID string, raw json.RawMessage) {
	r.mu.RLock()
	targets := []*conn{}
	for _, c := range r.clients {
		if c != sender && c.state.TeamID == teamID {
			targets = append(targets, c)
		}
	}
	r.mu.RUnlock()
	for _, c := range targets {
		c.sendRaw(func() ([]byte, error) { return append(json.RawMessage(nil), raw...), nil })
	}
}

func (c *conn) serverMessage(msg string) {
	c.send(map[string]interface{}{"type": "SERVER_MESSAGE", "message": msg})
}

// roomInfo is one entry of a ROOM_LIST response: joinable room summary.
type roomInfo struct {
	Campaign      *campaignInfo   `json:"campaign,omitempty"`
	RoomID        string          `json:"roomId"`
	Players       int             `json:"players"`
	PlayersInGame int             `json:"playersInGame"`
	Teams         []string        `json:"teams"`
	RoomState     json.RawMessage `json:"roomState,omitempty"`
	MaxPlayers    int             `json:"maxPlayers"`
	Versions      []string        `json:"versions"`
}

// Snapshot pointers under the map lock; rooms may disappear while browsing.
func (s *server) buildRoomList() []roomInfo {
	s.mu.RLock()
	rooms := make([]*room, 0, len(s.rooms))
	for _, r := range s.rooms {
		rooms = append(rooms, r)
	}
	s.mu.RUnlock()
	list := make([]roomInfo, 0, len(rooms))
	for _, r := range rooms {
		r.mu.RLock()
		var settings map[string]interface{}
		json.Unmarshal(r.roomState, &settings)
		if settings["listed"] != true || len(r.clients) == 0 {
			r.mu.RUnlock()
			continue
		}
		info := roomInfo{RoomID: r.id, MaxPlayers: maxRoomPlayers, RoomState: append(json.RawMessage(nil), r.roomState...), Teams: []string{}, Versions: []string{}}
		teams, versions := map[string]bool{}, map[string]bool{}
		var campaignClient *conn
		for _, c := range r.clients {
			info.Players++
			if c.state.IsSaveLoaded && c.state.Campaign != nil &&
				(campaignClient == nil || c.clientID == r.ownerID || (campaignClient.clientID != r.ownerID && c.clientID < campaignClient.clientID)) {
				campaignClient = c
			}
			if c.state.IsSaveLoaded {
				info.PlayersInGame++
			}
			teams[c.state.TeamID] = true
			versions[c.state.ClientVersion] = true
		}
		if campaignClient != nil {
			info.Campaign = describeCampaign(campaignClient.state)
		}
		for t := range teams {
			info.Teams = append(info.Teams, t)
		}
		for v := range versions {
			info.Versions = append(info.Versions, v)
		}
		sort.Strings(info.Teams)
		sort.Strings(info.Versions)
		r.mu.RUnlock()
		list = append(list, info)
	}
	sort.Slice(list, func(i, j int) bool { return list[i].RoomID < list[j].RoomID })
	return list
}

// ---------------------------------------------------------------------------
// Packet dispatch
// ---------------------------------------------------------------------------

type packet struct {
	Type           string          `json:"type"`
	RoomID         string          `json:"roomId"`
	RoomState      json.RawMessage `json:"roomState"`
	ClientState    json.RawMessage `json:"clientState"`
	State          json.RawMessage `json:"state"`
	TargetTeamID   string          `json:"targetTeamId"`
	TargetClientID uint32          `json:"targetClientId"`
	AddToQueue     bool            `json:"addToQueue"`
	ClientID       uint32          `json:"clientId"`
	Quiet          bool            `json:"quiet"`
	raw            json.RawMessage
}

func (s *server) roomFor(id string) *room {
	s.mu.Lock()
	defer s.mu.Unlock()
	r, ok := s.rooms[id]
	if !ok {
		r = &room{
			id:         id,
			clients:    map[uint32]*conn{},
			teamQueues: map[string][]json.RawMessage{},
			lastTeam:   map[string]json.RawMessage{},
		}
		s.rooms[id] = r
		log.Printf("[room %q] created", id)
	}
	return r
}

func handleHandshake(c *conn, p *packet) {
	roomID := strings.TrimSpace(shortText(p.RoomID, 65))
	if roomID == "" {
		roomID = fmt.Sprintf("anon-%d", time.Now().UnixNano()%100000)
	}
	if len(roomID) > 64 {
		c.reject("Room names must be at most 64 bytes.")
		return
	}
	srv.mu.Lock()
	r := srv.rooms[roomID]
	if r == nil {
		if len(srv.rooms) >= maxRooms {
			srv.mu.Unlock()
			c.reject("Relay is full. Try again later.")
			return
		}
		r = &room{id: roomID, clients: map[uint32]*conn{}, teamQueues: map[string][]json.RawMessage{}, lastTeam: map[string]json.RawMessage{}}
		srv.rooms[roomID] = r
	}
	r.mu.Lock()
	if len(r.clients) >= maxRoomPlayers {
		r.mu.Unlock()
		srv.mu.Unlock()
		c.reject("This room is full.")
		return
	}
	r.nextID++
	c.clientID = r.nextID
	c.room = r
	if p.ClientState != nil {
		json.Unmarshal(p.ClientState, &c.state)
	}
	normalizeClient(&c.state)
	c.state.ClientID = c.clientID
	c.state.Online = true
	// First handshaker owns the room and sets initial room state
	if len(r.clients) == 0 {
		r.ownerID = c.clientID
		if p.RoomState != nil {
			r.roomState = normalizeRoomState(p.RoomState, c.clientID)
		}
	}
	if r.roomState == nil {
		r.roomState = normalizeRoomState(nil, c.clientID)
	}
	r.clients[c.clientID] = c
	own := r.ownerID
	rs := append(json.RawMessage(nil), r.roomState...)
	teamID := c.state.TeamID
	queued := r.teamQueues[teamID]
	r.teamQueues[teamID] = nil
	joinedCount := len(r.clients)
	r.mu.Unlock()
	srv.mu.Unlock()

	r.mu.Lock()
	r.socialPublish()
	r.mu.Unlock()
	c.handshook = true
	log.Printf("[%s] client %d (%s) joined; %d in room", roomID, c.clientID, c.state.Name, joinedCount)

	// Roster first, then authoritative room state, then welcome
	r.broadcastRoster()
	if rs != nil {
		c.send(map[string]interface{}{"type": "UPDATE_ROOM_STATE", "state": rs})
	}
	c.serverMessage("Welcome to Hyrule.")
	c.send(map[string]interface{}{"type": "SERVER_CAPABILITIES", "protocol": 1, "socialProtocol": 1, "maxPlayers": maxRoomPlayers})

	// Replay any queued team packets to the newcomer
	for _, q := range queued {
		c.sendRaw(func() ([]byte, error) { return append(json.RawMessage(nil), q...), nil })
	}

	// If teammates hold a save, ask one for fresh team state so the newcomer catches up
	sent := false
	r.mu.RLock()
	for _, o := range r.clients {
		if o != c && o.state.TeamID == teamID && o.state.IsSaveLoaded {
			o.send(map[string]interface{}{"type": "REQUEST_TEAM_STATE", "targetTeamId": teamID})
			sent = true
			break
		}
	}
	lastState := r.lastTeam[teamID]
	r.mu.RUnlock()
	if !sent && lastState != nil {
		c.sendRaw(func() ([]byte, error) { return append(json.RawMessage(nil), lastState...), nil })
	}
	_ = own
}

func (c *conn) handlePacket(p *packet) {
	if !c.handshook {
		switch p.Type {
		case "HANDSHAKE":
			handleHandshake(c, p)
		case "LIST_ROOMS":
			// [TIME] | [INFO] | main.handlePacket | Unjoined browse connection: list rooms without joining
			c.send(map[string]interface{}{"type": "ROOM_LIST", "rooms": srv.buildRoomList()})
		}
		return
	}

	r := c.room
	// The relay assigns sender identity; clients cannot impersonate peers or the server.
	if p.Type == "ALL_CLIENT_STATE" || p.Type == "ROOM_LIST" || p.Type == "SERVER_MESSAGE" || p.Type == "SERVER_CAPABILITIES" {
		return
	}
	var raw map[string]interface{}
	if json.Unmarshal(p.raw, &raw) != nil || raw == nil || p.Type == "" || len(p.Type) > 64 {
		return
	}
	raw["clientId"] = c.clientID
	p.raw, _ = json.Marshal(raw)
	switch p.Type {
	case "HANDSHAKE":
		// duplicate handshake: ignore

	case "LIST_ROOMS":
		// Already joined; still allow listing (e.g. browsing from inside a room)
		c.send(map[string]interface{}{"type": "ROOM_LIST", "rooms": srv.buildRoomList()})

	case "UPDATE_CLIENT_STATE":
		var st ClientState
		if len(p.State) == 0 || string(p.State) == "null" || json.Unmarshal(p.State, &st) != nil {
			return
		}
		normalizeClient(&st)
		r.mu.Lock()
		if st.TeamID != c.state.TeamID || (!st.IsSaveLoaded && c.state.IsSaveLoaded) || string(st.Seed) != string(c.state.Seed) || st.ClientVersion != c.state.ClientVersion {
			r.removeQuestMember(c)
			r.socialDepart(c.clientID)
		}
		st.ClientID = c.clientID
		c.state = st
		r.socialPublish()
		raw["state"] = st
		p.raw, _ = json.Marshal(raw)
		r.mu.Unlock()
		r.relay(c, p.raw)

	case "UPDATE_ROOM_STATE":
		r.mu.Lock()
		if r.ownerID != c.clientID {
			r.mu.Unlock()
			return
		}
		r.roomState = normalizeRoomState(p.State, c.clientID)
		state := append(json.RawMessage(nil), r.roomState...)
		r.mu.Unlock()
		msg, _ := json.Marshal(map[string]interface{}{"type": "UPDATE_ROOM_STATE", "state": state})
		r.relay(c, msg)

	case "PLAYER_UPDATE", "PLAYER_SFX", "OCARINA_SFX":
		r.relay(c, p.raw)

	case "GIVE_ITEM", "SET_FLAG", "UNSET_FLAG", "ENTRANCE_DISCOVERED",
		"SET_CHECK_STATUS", "GAME_COMPLETE", "UPDATE_BEANS_COUNT", "UPDATE_DUNGEON_ITEMS":
		// Team-targeted; optionally queued for players who join later
		if p.TargetTeamID != c.state.TeamID {
			return
		}
		if p.AddToQueue && p.TargetTeamID != "" {
			r.mu.Lock()
			if len(p.raw) > 8192 || len(r.teamQueues[p.TargetTeamID]) >= 256 || roomCacheBytes(r)+len(p.raw) > 4*1024*1024 {
				r.mu.Unlock()
				return
			}
			r.teamQueues[p.TargetTeamID] = append(r.teamQueues[p.TargetTeamID], append(json.RawMessage(nil), p.raw...))
			r.mu.Unlock()
		}
		r.teamExcept(c, p.TargetTeamID, p.raw)

	case "SOCIAL":
		c.handleSocial(p.raw)

	case "PING":
		c.send(map[string]interface{}{"type": "PONG"})

	case "ARPG_QUEST":
		c.handleQuest(p.raw)

	case "ARPG_ENEMY_KILL":
		if p.TargetTeamID != c.state.TeamID {
			return
		}
		// [TIME] | [INFO] | ARPGEnemyKill | Team-only party kill-share relay (no queue: stale XP grants are noise)
		r.teamExcept(c, p.TargetTeamID, p.raw)

	case "REQUEST_TEAM_STATE":
		if p.TargetTeamID != c.state.TeamID {
			return
		}
		// Ask any save-loaded teammate for their state; fall back to cache
		asked := false
		r.mu.RLock()
		for _, o := range r.clients {
			if o != c && o.state.TeamID == p.TargetTeamID && o.state.IsSaveLoaded {
				o.send(map[string]interface{}{"type": "REQUEST_TEAM_STATE", "targetTeamId": p.TargetTeamID})
				asked = true
				break
			}
		}
		last := r.lastTeam[p.TargetTeamID]
		r.mu.RUnlock()
		if !asked && last != nil {
			c.sendRaw(func() ([]byte, error) { return append(json.RawMessage(nil), last...), nil })
		}

	case "UPDATE_TEAM_STATE":
		if p.TargetTeamID != c.state.TeamID {
			return
		}
		// Cache it; attach the queued packets for that team, then relay
		r.mu.Lock()
		if len(p.raw) > 512*1024 || roomCacheBytes(r)+len(p.raw)-len(r.lastTeam[p.TargetTeamID]) > 4*1024*1024 {
			r.mu.Unlock()
			return
		}
		r.lastTeam[p.TargetTeamID] = append(json.RawMessage(nil), p.raw...)
		r.teamQueues[p.TargetTeamID] = nil
		r.mu.Unlock()
		r.teamExcept(c, p.TargetTeamID, p.raw)

	case "REQUEST_TELEPORT", "DAMAGE_PLAYER", "TELEPORT_TO":
		c.routeTargeted(p)

	default:
		// New message types must define an explicit authorization and routing rule.
		return
	}
}

func (c *conn) leave() {
	if c.room == nil || !c.handshook {
		return
	}
	r := c.room
	srv.mu.Lock()
	r.mu.Lock()
	r.removeQuestMember(c)
	r.socialDepart(c.clientID)
	delete(r.clients, c.clientID)
	remaining := len(r.clients)
	ownerChanged := r.ownerID == c.clientID
	if remaining == 0 {
		if srv.rooms[r.id] == r {
			delete(srv.rooms, r.id)
		}
	} else if ownerChanged {
		r.ownerID = 0
		for id := range r.clients {
			if r.ownerID == 0 || id < r.ownerID {
				r.ownerID = id
			}
		}
		r.roomState = normalizeRoomState(r.roomState, r.ownerID)
	}
	state := append(json.RawMessage(nil), r.roomState...)
	r.socialPublish()
	r.mu.Unlock()
	srv.mu.Unlock()
	log.Printf("[%s] client %d left; %d in room", r.id, c.clientID, remaining)
	if remaining > 0 {
		r.broadcastRoster()
		if ownerChanged {
			msg, _ := json.Marshal(map[string]interface{}{"type": "UPDATE_ROOM_STATE", "state": state})
			r.relay(c, msg)
		}
	}
}

// ---------------------------------------------------------------------------
// TCP loop
// ---------------------------------------------------------------------------

func handleTCP(tcp net.Conn) {
	if !trackConnection(tcp) {
		tcp.Close()
		return
	}
	defer untrackConnection(tcp)
	c := &conn{tcp: tcp, teleport: map[uint32]time.Time{}}
	c.outbox = newOutbox(tcp)
	defer c.outbox.stop()
	defer c.leave()

	reader := bufio.NewScanner(tcp)
	reader.Buffer(make([]byte, 4096), maxFrameBytes)
	reader.Split(splitFrames)
	limiter := newPacketLimiter()
	for {
		// [TIME] | [INFO] | handleTCP | Long idle deadline only as a backstop; TCP keepalive (below) is the
		// primary liveness mechanism so quiet clients at the file-select screen are never dropped.
		timeout := 24 * time.Hour
		if !c.handshook {
			timeout = 15 * time.Second
		}
		tcp.SetReadDeadline(time.Now().Add(timeout))
		if !reader.Scan() {
			return
		}
		line := reader.Bytes()
		if !limiter.allow(len(line)) {
			return
		}
		if len(line) == 0 { // just the delimiter
			continue
		}
		var p packet
		if err := json.Unmarshal(line, &p); err != nil {
			log.Printf("[conn] bad json: %v", err)
			continue
		}
		p.raw = append(json.RawMessage(nil), line...)
		c.handlePacket(&p)
	}
}
