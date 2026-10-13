#include "QuestPartyRules.h"
#include "ARPGSaveCodec.h"
#include <cassert>
#include <iostream>
using namespace ARPG::Quests;
using namespace ARPG::Quests::PartyRules;
int main() {
 int checks=0;
 for(const auto& d:Catalog) {
  ARPGSaveData a{},b{};
  a.campaign.quests[d.id-1].state=b.campaign.quests[d.id-1].state=uint8_t(State::Active);
  auto& p=a.campaign.quests[d.id-1];auto& r=b.campaign.quests[d.id-1];r.party=12345;
  for(int i=0;i<3;++i) {
   auto& o=d.objectives[i];p.progress[i]=o.count;
   if(NodeObjective(o))for(int n=0;n<o.count;++n){p.nodes[i]|=1<<n;p.sites[i][n]={float(n*200),0,300,0,1,0};}
  }
  auto snapshot=Encode(p);
  assert(Merge(b.campaign,d.id,snapshot));++checks;
  assert(Status(b.campaign,d.id)==State::Ready);++checks;
  assert(Merge(b.campaign,d.id,snapshot));assert(r.progress[0]==p.progress[0]);++checks;
  assert(b.level==0); // Merging progress never grants rewards.
  auto saved=ARPG::SaveCodec::Decode(ARPG::SaveCodec::Encode(b),1);
  assert(saved.campaign.quests[d.id-1].party==12345);++checks;
  assert(Claim(b,d.id,0,false));assert(!Claim(b,d.id,0,false));++checks;
  assert(Merge(b.campaign,d.id,snapshot)&&Status(b.campaign,d.id)==State::Rewarded);++checks;
  ARPGCampaignData unaccepted{};assert(!Merge(unaccepted,d.id,snapshot));++checks;
  auto invalid=snapshot;invalid["progress"][0]=256;
  assert(!Merge(b.campaign,d.id,invalid));++checks;
 }
 // Two clients converge on the same node mask; stale/repeated snapshots neither
 // duplicate collections nor lose a previously completed node.
 ARPGCampaignData c{};c.quests[1].state=1;auto p=c.quests[1];p.nodes[0]=1;p.progress[0]=1;
 assert(Merge(c,2,Encode(p)));p.nodes[0]=2;assert(Merge(c,2,Encode(p)));assert(c.quests[1].progress[0]==2);
 assert(Merge(c,2,Encode(p)));assert(c.quests[1].progress[0]==2);checks+=4;
 auto invalid=Encode(p);invalid["sites"][0][0]={{"x",1e50},{"y",0},{"z",0},{"room",0},{"adult",0}};
 Merge(c,2,invalid);assert(!c.quests[1].sites[0][0].placed);++checks;
 for(const auto& d:Catalog)for(int i=0;i<3;++i)if(d.objectives[i].kind==Kind::Vigil) {
  ARPGCampaignData watch{};watch.quests[d.id-1].state=1;
  for(int k=0;k<i;++k){watch.quests[d.id-1].progress[k]=d.objectives[k].count;if(NodeObjective(d.objectives[k]))watch.quests[d.id-1].nodes[k]=(1<<d.objectives[k].count)-1;}
  auto partial=watch.quests[d.id-1];partial.progress[i]=d.objectives[i].count-1;
  assert(Merge(watch,d.id,Encode(partial)));assert(watch.quests[d.id-1].progress[i]==0);
  partial.progress[i]=d.objectives[i].count;assert(Merge(watch,d.id,Encode(partial)));assert(watch.quests[d.id-1].progress[i]==partial.progress[i]);checks+=4;
 }
 ARPGCampaignData locked{};assert(JoinBlock(locked,1,false)==nullptr);assert(JoinBlock(locked,2,false)!=nullptr);assert(JoinBlock(locked,1,true)!=nullptr);checks+=3;
 ARPGSaveData scaled{};scaled.campaign.quests[0].state=1;
 auto& bounty=scaled.campaign.quests[0];bounty.partySize=2;
 assert(Target(bounty,Catalog[0].objectives[0])==6);assert(RewardXp(bounty,Catalog[0])==150);checks+=2;
 for(int i=0;i<4;++i)RecordKill(scaled.campaign,0x55,Foe::Plant);
 assert(Status(scaled.campaign,1)==State::Active);RecordKill(scaled.campaign,0x55,Foe::Plant);RecordKill(scaled.campaign,0x55,Foe::Plant);
 assert(Status(scaled.campaign,1)==State::Ready);checks+=2;
 auto restored=ARPG::SaveCodec::Decode(ARPG::SaveCodec::Encode(scaled),1);assert(restored.campaign.quests[0].progress[0]==6&&restored.campaign.quests[0].partySize==2);++checks;
 std::cout<<checks<<" party quest checks passed across all "<<ARPG_QUEST_COUNT<<" quests\n";
}
