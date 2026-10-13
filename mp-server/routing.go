package main

import (
	"encoding/json"
	"time"
)

// This enforces session permissions. Combat remains client simulated; these
// guards are not a substitute for authoritative hit validation.
func (c *conn) routeTargeted(p *packet) {
	r := c.room
	r.mu.Lock()
	target := r.clients[p.TargetClientID]
	if target == nil || target == c || !c.state.IsSaveLoaded || !target.state.IsSaveLoaded ||
		c.state.ClientVersion != target.state.ClientVersion || string(c.state.Seed) != string(target.state.Seed) {
		r.mu.Unlock()
		return
	}
	var rules struct {
		PVP      int `json:"pvpMode"`
		Teleport int `json:"teleportMode"`
	}
	json.Unmarshal(r.roomState, &rules)
	sameTeam := c.state.TeamID == target.state.TeamID
	allowed := false
	switch p.Type {
	case "DAMAGE_PLAYER":
		var hit struct {
			Damage int `json:"damage"`
			Effect int `json:"damageEffect"`
		}
		allowed = json.Unmarshal(p.raw, &hit) == nil && hit.Damage > 0 && hit.Damage <= 31 && hit.Effect >= 5 && hit.Effect <= 7 &&
			(rules.PVP == 2 || (rules.PVP == 1 && !sameTeam)) && string(c.state.SceneNum) == string(target.state.SceneNum) &&
			string(c.state.CurRoomNum) == string(target.state.CurRoomNum)
	case "REQUEST_TELEPORT":
		allowed = rules.Teleport == 2 || (rules.Teleport == 1 && sameTeam)
		if allowed {
			c.teleport[target.clientID] = time.Now().Add(10 * time.Second)
		}
	case "TELEPORT_TO":
		allowed = (rules.Teleport == 2 || (rules.Teleport == 1 && sameTeam)) && time.Now().Before(target.teleport[c.clientID])
		delete(target.teleport, c.clientID)
	}
	r.mu.Unlock()
	if allowed {
		target.sendRaw(func() ([]byte, error) { return p.raw, nil })
	}
}
