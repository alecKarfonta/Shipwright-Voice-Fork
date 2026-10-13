package main

import (
	"encoding/json"
	"fmt"
	"testing"
	"time"
)

func partyClient(t *testing.T, room, team, name string) *testClient {
	*portFlag = 43390
	startServer(t)
	c := dial(t)
	t.Cleanup(func() { (*c.tcp).Close() })
	c.send(map[string]interface{}{"type": "HANDSHAKE", "roomId": room, "clientState": map[string]interface{}{"name": name, "teamId": team, "isSaveLoaded": true, "seed": 0, "sceneNum": 85, "curRoomNum": 0}})
	roster := c.recv("ALL_CLIENT_STATE", time.Second)
	for _, v := range roster["state"].([]interface{}) {
		m := v.(map[string]interface{})
		if m["self"] == true {
			c.id = uint32(m["clientId"].(float64))
		}
	}
	return c
}
func questSend(c *testClient, op string, id int, extra map[string]interface{}) {
	p := map[string]interface{}{"type": "ARPG_QUEST", "version": 1, "op": op, "id": id}
	for k, v := range extra {
		p[k] = v
	}
	c.send(p)
}
func questSnapshot(c *testClient) map[int]partyQuest {
	questSend(c, "hello", 0, nil)
	for {
		p := c.recv("ARPG_QUEST", time.Second)
		if p["op"] != "hello" {
			continue
		}
		b, _ := json.Marshal(p["quests"])
		var list []partyQuest
		if err := json.Unmarshal(b, &list); err != nil {
			c.t.Fatal(err)
		}
		out := map[int]partyQuest{}
		for _, q := range list {
			out[q.ID] = q
		}
		return out
	}
}
func setQuestScene(c *testClient, team string, scene int) {
	c.send(map[string]interface{}{"type": "UPDATE_CLIENT_STATE", "clientId": c.id, "state": map[string]interface{}{"name": "Tester", "teamId": team, "isSaveLoaded": true, "seed": 0, "sceneNum": scene, "curRoomNum": 0}})
}
func TestPartyQuestConcurrentProgressAndRejoin(t *testing.T) {
	room := fmt.Sprint("quests-", time.Now().UnixNano())
	a := partyClient(t, room, "red", "A")
	b := partyClient(t, room, "red", "B")
	other := partyClient(t, room, "blue", "Other")
	empty := questProgress{Size: 1}
	questSend(a, "share", 1, map[string]interface{}{"state": empty})
	questSnapshot(a)
	questSend(b, "join", 1, map[string]interface{}{"state": empty})
	q := questSnapshot(b)[1]
	if len(q.Members) != 2 || q.Size != 2 || questTarget(&q, questCatalog[1].Objectives[0]) != 6 {
		t.Fatalf("two-player bounty did not grow: %+v", q)
	}
	questSend(a, "advance", 1, map[string]interface{}{"seq": 1, "objective": 0, "amount": 1})
	questSnapshot(a)
	questSend(b, "advance", 1, map[string]interface{}{"seq": 1, "objective": 0, "amount": 1})
	q = questSnapshot(b)[1]
	if q.Progress[0] != 2 {
		t.Fatalf("simultaneous contributors lost: %+v", q)
	}
	questSend(a, "advance", 1, map[string]interface{}{"seq": 1, "objective": 0, "amount": 1})
	q = questSnapshot(a)[1]
	if q.Progress[0] != 2 {
		t.Fatal("replayed kill counted twice")
	}
	questSend(a, "advance", 1, map[string]interface{}{"seq": 2, "objective": 0, "amount": 999})
	q = questSnapshot(a)[1]
	if q.Progress[0] != 2 {
		t.Fatal("unbounded objective accepted")
	}
	questSend(other, "advance", 1, map[string]interface{}{"seq": 1, "objective": 0, "amount": 4, "targetTeamId": "red"})
	if len(questSnapshot(other)) != 0 {
		t.Fatal("cross-team quest leaked")
	}
	setQuestScene(a, "red", 81)
	questSend(a, "advance", 1, map[string]interface{}{"seq": 3, "objective": 0, "amount": 1})
	q = questSnapshot(a)[1]
	if q.Progress[0] != 2 {
		t.Fatal("wrong-region kill accepted")
	}
	(*a.tcp).Close()
	c := partyClient(t, room, "red", "Rejoined")
	q = questSnapshot(c)[1]
	if q.Progress[0] != 2 || q.Size != 2 {
		t.Fatal("disconnect lost progress or shrank bounty")
	}
	questSend(c, "join", 1, map[string]interface{}{"state": empty})
	q = questSnapshot(c)[1]
	if q.Progress[0] != 2 || q.Size != 2 {
		t.Fatal("rejoin duplicated progress or inflated party size")
	}
	questSend(c, "advance", 1, map[string]interface{}{"seq": 1, "objective": 0, "amount": 4})
	q = questSnapshot(c)[1]
	if q.Progress[0] != 6 {
		t.Fatal("shared target not completed")
	}
	questSend(c, "reward", 1, nil)
	q = questSnapshot(c)[1]
	if !q.Members[c.id] || q.Members[b.id] {
		t.Fatal("reward claim leaked to teammate")
	}
}
func TestPartyQuestSitesAndContinuousWatches(t *testing.T) {
	room := fmt.Sprint("sites-", time.Now().UnixNano())
	a := partyClient(t, room, "red", "A")
	b := partyClient(t, room, "red", "B")
	questSend(a, "share", 2, map[string]interface{}{"state": questProgress{Size: 1}})
	questSnapshot(a)
	questSend(b, "join", 2, map[string]interface{}{"state": questProgress{Size: 1}})
	questSnapshot(b)
	point := questSite{X: 120, Y: 0, Z: 160, Room: 0, Adult: 0}
	alternate := point
	alternate.X = 900
	questSend(a, "site", 2, map[string]interface{}{"seq": 1, "objective": 0, "node": 0, "point": point})
	questSnapshot(a)
	questSend(b, "site", 2, map[string]interface{}{"seq": 1, "objective": 0, "node": 0, "point": alternate})
	q := questSnapshot(b)[2]
	if q.Sites[0][0].X != 120 {
		t.Fatal("conflicting placement moved shared site")
	}
	for _, c := range []*testClient{a, b} {
		questSend(c, "advance", 2, map[string]interface{}{"seq": 2, "objective": 0, "node": 0})
		questSnapshot(c)
	}
	q = questSnapshot(a)[2]
	if q.Progress[0] != 1 || q.Nodes[0] != 1 {
		t.Fatal("same pickup counted twice")
	}
	questSend(a, "advance", 2, map[string]interface{}{"seq": 3, "objective": 0, "node": 2})
	q = questSnapshot(a)[2]
	if q.Progress[0] != 1 {
		t.Fatal("unplaced node completed")
	}
}
func TestPartyQuestAllCatalogObjectives(t *testing.T) {
	room := fmt.Sprint("catalog-", time.Now().UnixNano())
	a := partyClient(t, room, "red", "A")
	b := partyClient(t, room, "red", "B")
	var seq uint64
	for id := 1; id <= len(questCatalog); id++ {
		d := questCatalog[id]
		questSend(a, "share", id, map[string]interface{}{"state": questProgress{Size: 1}})
		questSnapshot(a)
		questSend(b, "join", id, map[string]interface{}{"state": questProgress{Size: 1}})
		questSnapshot(b)
		for i, o := range d.Objectives {
			if o.Count == 0 {
				continue
			}
			scene := o.Scene
			if scene < 0 {
				scene = 85
			}
			setQuestScene(a, "red", scene)
			q := questSnapshot(a)[id]
			if o.Nodes {
				for n := 0; n < o.Count; n++ {
					seq++
					questSend(a, "site", id, map[string]interface{}{"seq": seq, "objective": i, "node": n, "point": questSite{X: float64(150 * n), Room: 0}})
					seq++
					questSend(a, "advance", id, map[string]interface{}{"seq": seq, "objective": i, "node": n})
				}
			} else {
				if o.Kind == 11 {
					seq++
					questSend(a, "advance", id, map[string]interface{}{"seq": seq, "objective": i, "amount": o.Count - 1})
					watch := questSnapshot(a)[id]
					if watch.Progress[i] != 0 {
						t.Fatal("partial watches pooled")
					}
				}
				seq++
				questSend(a, "advance", id, map[string]interface{}{"seq": seq, "objective": i, "amount": questTarget(&q, o)})
			}
			q = questSnapshot(a)[id]
			peer := questSnapshot(b)[id]
			if q.Progress[i] != questTarget(&q, o) || q.Progress != peer.Progress || q.Nodes != peer.Nodes {
				t.Fatalf("quest %d objective %d did not converge: %+v / %+v", id, i, q, peer)
			}
		}
	}
}

func TestPartyQuestActionsBeforeJoinAcknowledgement(t *testing.T) {
	room := fmt.Sprint("inflight-", time.Now().UnixNano())
	a := partyClient(t, room, "red", "A")
	questSend(a, "share", 2, map[string]interface{}{"state": questProgress{Size: 1}})
	// Do not read the acknowledgement: the game keeps moving during this interval.
	questSend(a, "site", 2, map[string]interface{}{"seq": 1, "objective": 0, "node": 0, "point": questSite{X: 123, Room: 0}})
	questSend(a, "advance", 2, map[string]interface{}{"seq": 2, "objective": 0, "node": 0})
	q := questSnapshot(a)[2]
	if q.Sites[0][0] == nil || q.Nodes[0] != 1 {
		t.Fatal("in-flight site/action was lost")
	}
}
