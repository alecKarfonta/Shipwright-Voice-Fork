package main

import (
	"encoding/json"
	"testing"
)

func TestCampaignBoundsAndCanonicalTitles(t *testing.T) {
	for _, stage := range []int{-1, 12, 100} {
		s := ClientState{IsSaveLoaded: true, Campaign: &campaignState{StoryStage: stage}}
		normalizeClient(&s)
		if s.Campaign != nil {
			t.Fatalf("invalid stage %d retained", stage)
		}
	}
	s := ClientState{IsSaveLoaded: true, Name: "Hero", TeamID: "crew", Campaign: &campaignState{StoryStage: 3, QuestID: 22, QuestState: 1, CompletedObjectives: 100, TotalObjectives: 99}}
	normalizeClient(&s)
	c := describeCampaign(s)
	if c.StoryTitle != "Awaken the Forest Sage" || c.QuestTitle != questCatalog[22].Title || c.TotalObjectives != 1 || c.CompletedObjectives != 1 || c.Player != "Hero" {
		t.Fatalf("bad canonical summary: %+v", c)
	}
	s.Campaign.QuestID = 999
	normalizeClient(&s)
	if s.Campaign.QuestID != 0 || s.Campaign.StoryStage != 3 {
		t.Fatal("invalid quest should preserve story")
	}
	s.IsSaveLoaded = false
	normalizeClient(&s)
	if s.Campaign != nil {
		t.Fatal("unloaded save leaked campaign")
	}
}
func TestDirectoryCampaignOwnerAndFallback(t *testing.T) {
	owner := &conn{clientID: 7, state: ClientState{Name: "Host", TeamID: "crew", IsSaveLoaded: true, Campaign: &campaignState{StoryStage: 1}}}
	peer := &conn{clientID: 2, state: ClientState{Name: "Peer", TeamID: "other", IsSaveLoaded: true, Campaign: &campaignState{StoryStage: 5}}}
	r := &room{id: "adventure", ownerID: 7, clients: map[uint32]*conn{7: owner, 2: peer}, roomState: json.RawMessage(`{"listed":true}`)}
	s := &server{rooms: map[string]*room{"adventure": r}}
	for i := 0; i < 20; i++ {
		if c := s.buildRoomList()[0].Campaign; c.Player != "Host" || c.StoryStage != 1 {
			t.Fatal(c)
		}
	}
	owner.state.IsSaveLoaded = false
	if c := s.buildRoomList()[0].Campaign; c.Player != "Peer" || c.Team != "other" {
		t.Fatal(c)
	}
	peer.state.Campaign = nil
	if s.buildRoomList()[0].Campaign != nil {
		t.Fatal("legacy clients must not invent progress")
	}
	owner.state.IsSaveLoaded = true
	owner.state.Campaign.StoryStage = 8
	if s.buildRoomList()[0].Campaign.StoryStage != 8 {
		t.Fatal("campaign updates stale")
	}
	delete(r.clients, 7)
	if s.buildRoomList()[0].Campaign != nil {
		t.Fatal("departed host leaked campaign")
	}
}
