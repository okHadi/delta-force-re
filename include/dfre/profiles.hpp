#pragma once

#include "dfre/assets.hpp"

#include <array>
#include <filesystem>
#include <string>

namespace dfre {

struct ProfileDefaults {
    AssetBytes controls;
    std::array<std::string,10> macros;
    std::string empty_name, first_name;
};
ProfileDefaults load_profile_defaults(const std::filesystem::path& game_folder);

struct ProfileStats {
    std::uint32_t campaign_kills=0, completed_missions=0;
    std::uint32_t multiplayer_kills=0, multiplayer_deaths=0, multiplayer_team_games_won=0;
};

class PlayerProfile {
public:
    static constexpr std::size_t byte_size=0xa90;
    static constexpr std::size_t portrait_count=11;
    PlayerProfile();
    static PlayerProfile decode(AssetBytes bytes,const ProfileDefaults& defaults);
    static PlayerProfile empty(const ProfileDefaults& defaults,std::string_view filename);
    const AssetBytes& raw() const noexcept{return raw_;}
    // Source profile routines use these current bytes. Keep byte_size bytes.
    AssetBytes& source_bytes() noexcept{return raw_;}
    std::string name() const;
    bool active() const noexcept;
    std::uint32_t portrait() const;
    ProfileStats stats() const;
    void set_name(std::string_view name);
    void set_active(bool active) noexcept;
    void set_portrait(std::uint32_t index);
    void set_filename(std::string_view filename);
    AssetBytes encoded() const;
private:
    friend class ProfileStore;
    AssetBytes raw_;
};

class ProfileStore {
public:
    ProfileStore(std::filesystem::path game_folder,std::filesystem::path save_folder);
    ProfileStore(std::filesystem::path game_folder,std::filesystem::path save_folder,ProfileDefaults defaults);
    PlayerProfile& at(std::size_t slot);
    const PlayerProfile& at(std::size_t slot) const;
    std::size_t first_active() const noexcept;
    void create(std::size_t slot,std::string_view name);
    void erase(std::size_t slot);
    void save(std::size_t slot) const;
    void save_all() const;
    const std::filesystem::path& save_folder() const noexcept{return save_folder_;}
private:
    std::filesystem::path game_folder_,save_folder_;
    ProfileDefaults defaults_;
    std::array<PlayerProfile,9> profiles_;
};

std::string portrait_filename(std::uint32_t index);

} // namespace dfre
