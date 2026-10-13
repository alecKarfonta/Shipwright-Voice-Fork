package main

// The client reports bounded IDs/progress only. Titles come from the shipped lore catalog.
type campaignState struct {
	StoryStage          int `json:"storyStage"`
	QuestID             int `json:"questId"`
	QuestState          int `json:"questState"`
	CompletedObjectives int `json:"completedObjectives"`
	TotalObjectives     int `json:"totalObjectives"`
}
type campaignInfo struct {
	campaignState
	StoryTitle string `json:"storyTitle"`
	QuestTitle string `json:"questTitle,omitempty"`
	Chapter    string `json:"chapter,omitempty"`
	Player     string `json:"player"`
	Team       string `json:"team"`
}

var storyTitles = [...]string{
	"Inside the Great Deku Tree", "Dodongo's Cavern", "Inside Jabu-Jabu's Belly",
	"Awaken the Forest Sage", "Awaken the Fire Sage", "Awaken the Water Sage",
	"Awaken the Shadow Sage", "Awaken the Spirit Sage", "Break the Castle's Barrier",
	"Defeat Ganondorf and Ganon", "Hyrule Saved", "Randomized Adventure",
}

func normalizeCampaign(s *ClientState) {
	c := s.Campaign
	if c == nil {
		return
	}
	if !s.IsSaveLoaded || c.StoryStage < 0 || c.StoryStage >= len(storyTitles) {
		s.Campaign = nil
		return
	}
	d, ok := questCatalog[c.QuestID]
	if !ok || c.QuestState < 1 || c.QuestState > 2 {
		*c = campaignState{StoryStage: c.StoryStage}
		return
	}
	total := 0
	for _, o := range d.Objectives {
		if o.Kind != 0 {
			total++
		}
	}
	c.TotalObjectives = total
	c.CompletedObjectives = max(0, min(total, c.CompletedObjectives))
	if c.QuestState == 2 {
		c.CompletedObjectives = total
	}
}
func describeCampaign(s ClientState) *campaignInfo {
	if s.Campaign == nil {
		return nil
	}
	c := &campaignInfo{campaignState: *s.Campaign, StoryTitle: storyTitles[s.Campaign.StoryStage], Player: s.Name, Team: s.TeamID}
	if d, ok := questCatalog[c.QuestID]; ok {
		c.QuestTitle = d.Title
		c.Chapter = d.Chapter
	}
	return c
}
