#include "dfre/profiles.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
void require(bool x,const char* m){if(!x)throw std::runtime_error(m);}
template<class F> void rejects(F f){bool yes=false;try{f();}catch(const dfre::AssetError&){yes=true;}require(yes,"Expected profile error");}
void put(dfre::AssetBytes& b,std::size_t at,std::uint32_t v){for(unsigned i=0;i<4;++i)b.at(at+i)=std::uint8_t(v>>(8*i));}
}
int main(){try{
 dfre::ProfileDefaults defaults;defaults.controls.resize(552);for(unsigned i=0;i<552;++i)defaults.controls[i]=std::uint8_t(i);defaults.empty_name="New Operative";defaults.first_name="Delete Me";
 for(auto& m:defaults.macros)m="Ready";
 auto p=dfre::PlayerProfile::empty(defaults,"player01.ply");require(!p.active()&&p.name().empty(),"Empty slot inactive with zeroed stored name");p.set_name("test soldier");p.set_active(true);p.set_portrait(10);
 require(p.name()=="TEST SOLDIER"&&p.portrait()==10,"Name and portrait");rejects([&]{p.set_name("1234567890123456");});rejects([&]{p.set_portrait(11);});
 auto bytes=p.encoded();bytes[0x300]=71;put(bytes,0x664,7);put(bytes,0x74,1);put(bytes,0x74+4*0x44+15*4,2);
 const auto q=dfre::PlayerProfile::decode(bytes,defaults);require(q.encoded()==bytes,"Unknown profile bytes preserved");require(q.stats().campaign_kills==7&&q.stats().completed_missions==2,"Stats offsets");
 bytes[3]=0x1a;std::fill(bytes.begin()+0x8b0,bytes.begin()+0x8c8,0);
 const auto old=dfre::PlayerProfile::decode(bytes,defaults);require(std::equal(defaults.controls.begin()+0x210,defaults.controls.end(),old.raw().begin()+0x8b0),"Legacy controls upgrade");
 bytes[3]=0x20;require(dfre::PlayerProfile::decode(bytes,defaults).raw()[3]==0x1b,"Original future-version acceptance");
 rejects([&]{dfre::PlayerProfile::decode({},defaults);});
 const auto root=std::filesystem::temp_directory_path()/("dfre-profile-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 const auto game=root/"original",save=root/"saves";std::filesystem::create_directories(game);
 rejects([&]{dfre::ProfileStore s(game,game/"nested",defaults);});
#ifdef _WIN32
 auto case_game=game.wstring();for(auto& c:case_game)if(c>=L'a'&&c<=L'z')c-=32;
 rejects([&]{dfre::ProfileStore s(game,std::filesystem::path(case_game)/"nested",defaults);});
#endif
 dfre::ProfileStore store(game,save,defaults);require(store.at(0).active(),"First-run profile");store.create(2,"new name");store.save(2);
 require(std::filesystem::file_size(save/"player03.ply")==2704&&!std::filesystem::exists(game/"player03.ply"),"Separate saved profile");
 dfre::ProfileStore loaded(game,save,defaults);require(loaded.at(2).name()=="NEW NAME"&&loaded.first_active()==2,"Saved profile reload");loaded.erase(2);require(!loaded.at(2).active(),"Slot reset");std::filesystem::remove_all(root);
 std::cout<<"profile checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
