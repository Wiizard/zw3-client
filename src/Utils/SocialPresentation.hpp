#pragma once
#include <algorithm>
#include <cctype>
#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace Utils
{
 inline std::string SocialActivityText(std::string status)
 {
  std::ranges::transform(status,status.begin(),[](unsigned char c){return static_cast<char>(std::toupper(c));});
  if(status=="IN_PARTY"||status=="IN_LOBBY"||status=="LOBBY"||status=="MATCH_FOUND")return "IN LOBBY";
  if(status=="MAP_VOTE"||status=="VOTING")return "VOTING";
  if(status=="RESERVING_SERVER"||status=="STARTING_SERVER"||status=="SERVER_STARTING"||status=="STARTING")return "STARTING";
  if(status=="CONNECTING"||status=="JOINING"||status=="JOIN_PREVIEW")return "JOINING";
  if(status=="IN_MATCH"||status=="IN_GAME"||status=="INGAME"||status=="IN_PUBLIC_MATCH"||status=="IN_ZOMBIES")return "IN GAME";
  if(status=="PRIVATE_MATCH"||status=="IN_PRIVATE_MATCH")return "PRIVATE MATCH";
  if(status=="MAIN_MENU"||status=="IDLE"||status=="ONLINE")return "ONLINE";
  if(status.empty()||status=="OFFLINE")return "OFFLINE";
  return status;
 }
 inline bool SocialActivityOnline(const std::string_view status)
 {
  return !status.empty()&&status!="OFFLINE"&&status!="UNAVAILABLE";
 }
 inline bool ValidSocialRank(int level,int prestige)
 {
  return level>=1&&level<=54&&prestige>=0&&prestige<=20;
 }
 inline std::optional<std::pair<int,int>> ParseSocialRank(std::string_view levelText,std::string_view prestigeText)
 {
  if(levelText.empty()||prestigeText.empty())return std::nullopt;
  int level=0,prestige=0;
  const auto levelResult=std::from_chars(levelText.data(),levelText.data()+levelText.size(),level);
  const auto prestigeResult=std::from_chars(prestigeText.data(),prestigeText.data()+prestigeText.size(),prestige);
  if(levelResult.ec!=std::errc{}||levelResult.ptr!=levelText.data()+levelText.size()
    ||prestigeResult.ec!=std::errc{}||prestigeResult.ptr!=prestigeText.data()+prestigeText.size()
    ||!ValidSocialRank(level,prestige))return std::nullopt;
  return std::pair{level,prestige};
 }
}
