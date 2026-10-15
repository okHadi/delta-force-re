#include "dfre/game.hpp"
#include "dfre/platform.hpp"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <set>
#include <stdexcept>
namespace {
std::vector<std::uint64_t> frames;
class FlowWindow final:public dfre::GameWindow {
public:
    std::vector<dfre::InputEvent> poll() override {
        switch(step_++) {
        case 1:case 5:return {{dfre::InputKind::key_down,13}};
        case 3:return {{dfre::InputKind::pointer_down,0,320,144},{dfre::InputKind::pointer_up,0,320,144}};
        case 7:case 9:return {{dfre::InputKind::key_down,27}};
        case 11:return {{dfre::InputKind::quit}};
        default:if(step_>20)throw std::runtime_error("Game flow exceeded its update limit");return {};
        }
    }
    void present(const std::vector<std::uint32_t>& pixels) override {
        if(pixels.size()!=640*480)throw std::runtime_error("Invalid game frame size");
        std::uint64_t hash=14695981039346656037ull;
        for(auto pixel:pixels)for(unsigned shift:{0,8,16,24}){hash^=(pixel>>shift)&255;hash*=1099511628211ull;}
        frames.push_back(hash);
    }
    std::uint32_t ticks() const override{return ticks_;}
    void yield() const override{ticks_+=16;}
private:
    unsigned step_=0;
    mutable std::uint32_t ticks_=0;
};
}
namespace dfre {
std::unique_ptr<GameWindow> create_game_window(const std::string&,int width,int height){
    if(width!=640||height!=480)throw std::runtime_error("Unexpected game display size");
    return std::make_unique<FlowWindow>();
}
}
int main(int argc,char** argv){
    if(argc!=2)return 2;
    try {
        const auto folder=std::filesystem::temp_directory_path()/("dfre-flow-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        if(!std::filesystem::create_directory(folder))throw std::runtime_error("Cannot create game flow save folder");
        struct Cleanup {std::filesystem::path path;~Cleanup(){std::error_code ignored;std::filesystem::remove_all(path,ignored);}}cleanup{folder};
        dfre::GameOptions options;options.game_folder=argv[1];options.save_folder=folder;dfre::run_game(options);
        if(frames.size()!=6||std::set<std::uint64_t>(frames.begin(),frames.end()).size()<4)throw std::runtime_error("Soldier/main/campaign/briefing flow did not draw the expected screens");
        unsigned saves=0;for(const auto& entry:std::filesystem::directory_iterator(folder))if(entry.path().extension()==".ply"&&entry.file_size()==2704)++saves;
        if(saves!=9)throw std::runtime_error("Game flow did not save all soldier slots");
        std::cout<<"6 frames; soldier -> main -> campaign -> briefing -> campaign -> main; 9 separate saves\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
