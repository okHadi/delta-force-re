#pragma once
#include "dfre/menu.hpp"
#include "dfre/profiles.hpp"
#include <optional>
#include <functional>

namespace dfre {
struct CampaignMission {
    std::string filename, name, overview;
    std::uint32_t equipment_flags=0, prerequisites=0;
    int map_x=0,map_y=0;
};
struct CampaignDefinition {
    std::uint32_t index=0;
    std::string name, overview, text_file, map_image;
    std::vector<CampaignMission> missions;
};
class CampaignCatalog {
public:
    explicit CampaignCatalog(const AssetStore& assets);
    explicit CampaignCatalog(std::vector<CampaignDefinition> campaigns);
    const CampaignDefinition& at(std::uint32_t index) const;
    const std::vector<CampaignDefinition>& campaigns() const noexcept{return campaigns_;}
private:
    std::vector<CampaignDefinition> campaigns_;
};
struct MissionSelection {
    std::uint32_t campaign=0,mission=0,equipment_flags=0,prerequisites=0;
    std::string filename,text_file;
};
class CampaignState {
public:
    CampaignState(const CampaignCatalog& catalog,PlayerProfile& profile);
    void enter();
    std::uint32_t campaign() const noexcept{return campaign_;}
    std::uint32_t mission() const noexcept{return mission_;}
    const CampaignDefinition& definition() const;
    bool available(std::size_t mission) const;
    bool completed(std::size_t mission) const;
    void select_campaign(std::uint32_t index);
    bool select_mission(std::uint32_t index);
    MissionSelection accept();
private:
    const CampaignCatalog& catalog_;
    PlayerProfile& profile_;
    std::uint32_t campaign_=1,mission_=0;
    void write_selection();
};
struct CampaignResult {
    std::uint32_t next_state=0;
    std::optional<MissionSelection> mission;
};
struct BriefingLink {std::string key;std::size_t word=0;};
struct BriefingText {std::string plain;std::vector<BriefingLink> links;};
std::string expand_briefing(std::string text,const std::function<std::string(std::string_view)>& lookup,std::uint32_t completion_mask);
BriefingText parse_briefing(std::string_view expanded);
class CampaignMenu {
public:
    CampaignMenu(MenuRuntime& menu,const std::filesystem::path& game_folder,PlayerProfile& profile);
    void enter();
    void update() noexcept{++ticks_;}
    void pointer(int x,int y) noexcept{pointer_x_=x;pointer_y_=y;}
    std::optional<CampaignResult> actions(const MenuAction& action);
    const std::vector<std::uint32_t>& draw();
    const CampaignState& state() const noexcept{return state_;}
    const CampaignCatalog& catalog() const noexcept{return catalog_;}
private:
    MenuRuntime& menu_;
    AssetStore assets_;
    CampaignCatalog catalog_;
    CampaignState state_;
    std::uint32_t ticks_=0,last_click_=0;
    int pointer_x_=0,pointer_y_=0;
    void labels();
    void owner_draw(const MenuAction& action);
    void paragraph(int x,int y,int width,int height,std::string_view text);
};
class MissionSetupMenu {
public:
    MissionSetupMenu(MenuRuntime& menu,const std::filesystem::path& game_folder,PlayerProfile& profile);
    void enter(const MissionSelection& selection);
    void update() noexcept;
    void pointer(int x,int y) noexcept{pointer_x_=x;pointer_y_=y;}
    std::optional<CampaignResult> actions(const MenuAction& action);
    const std::vector<std::uint32_t>& draw();
    const BriefingText& briefing() const noexcept{return briefing_;}
    const MissionSelection& selection() const noexcept{return selection_;}
private:
    struct MapItem {std::uint32_t icon=0,team=0;int x=0,y=0;};
    struct LinkHit {std::size_t index=0;int x1=0,y1=0,x2=0,y2=0;};
    MenuRuntime& menu_;
    AssetStore assets_;
    PlayerProfile& profile_;
    MissionSelection selection_;
    std::unique_ptr<RtxtTable> text_;
    BriefingText briefing_;
    PcxImage map_;
    std::vector<MapItem> map_items_;
    std::vector<LinkHit> links_;
    int pointer_x_=0,pointer_y_=0,scroll_line_=0,lines_=0;
    int map_x_=0,map_y_=0,target_x_=0,target_y_=0,center_x_=512,center_y_=512;
    std::optional<std::pair<int,int>> link_center_;
    std::string link_text_;
    void center(int x,int y);
    void owner_draw(const MenuAction& action);
    void draw_map();
    void draw_briefing();
};
}
