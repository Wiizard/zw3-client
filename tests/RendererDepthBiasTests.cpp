#define NOMINMAX
#include <windows.h>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
static std::uint8_t* image;
static unsigned attempted=0,failInstall=0;
constexpr bool HOOK_CALL=false;
namespace Game {
struct MaterialTechniqueSet{const char* name;};
struct Material{MaterialTechniqueSet* techniqueSet;};
struct GfxSurface{Material* material;};
struct GfxWorld{unsigned surfaceCount;struct{GfxSurface* surfaces;}dpvs;};
}
struct IDirect3DDevice9 {
 void** vtable=nullptr;
 std::vector<std::pair<unsigned,DWORD>> states;
 HRESULT SetRenderState(unsigned state,DWORD value){states.emplace_back(state,value);return S_OK;}
};
constexpr unsigned D3DRS_SLOPESCALEDEPTHBIAS=175,D3DRS_DEPTHBIAS=195;
static HRESULT Record(IDirect3DDevice9* device,unsigned state,DWORD value){return device->SetRenderState(state,value);}
namespace Utils {
struct Hook {
 bool installed=false;
 static std::uintptr_t Rebase(std::uintptr_t a){return reinterpret_cast<std::uintptr_t>(image)+(a-0x140000000);}
 template<class T>static T Get(std::uintptr_t a){T out;std::memcpy(&out,reinterpret_cast<void*>(Rebase(a)),sizeof(out));return out;}
 static bool MatchesBytes(std::uintptr_t a,const std::uint8_t* data,size_t bytes){return !std::memcmp(reinterpret_cast<void*>(Rebase(a)),data,bytes);}
 static bool BranchesTo(std::uintptr_t site,std::uintptr_t target,bool){const auto* p=reinterpret_cast<const std::uint8_t*>(Rebase(site));std::int32_t delta;std::memcpy(&delta,p+1,4);return p[0]==0xe8&&site+5+delta==target;}
 template<class T>Hook* Initialize(std::uintptr_t,T,bool){return this;}
 Hook* Install(){installed=++attempted!=failInstall;return this;}
 bool IsInstalled(){return installed;}
 void Uninstall(){installed=false;}
};
}
#include "Components/Modules/RendererDepthBias.hpp"
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>static void put(unsigned rva,const T& data){std::memcpy(image+rva,&data,sizeof(data));}
int main(int argc,char**argv){try{
 require(argc==2,"Native executable path required");std::ifstream file(argv[1],std::ios::binary);std::vector<char> bytes{std::istreambuf_iterator<char>(file),{}};
 require(bytes.size()>4096,"Missing native executable");auto* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(bytes.data());auto* pe=reinterpret_cast<IMAGE_NT_HEADERS64*>(bytes.data()+dos->e_lfanew);
 require(pe->OptionalHeader.SizeOfImage<0xa000000,"Unexpected image size");image=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,0xa000000,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));require(image,"Mapping failed");auto* s=IMAGE_FIRST_SECTION(pe);for(unsigned i=0;i<pe->FileHeader.NumberOfSections;++i)if(s[i].SizeOfRawData)std::memcpy(image+s[i].VirtualAddress,bytes.data()+s[i].PointerToRawData,s[i].SizeOfRawData);
 namespace P=Components::RendererDepthBiasPolicy;namespace H=Components::RendererDepthBias;
 require(!P::IsConvertedWorldTechnique(nullptr)&&!P::IsConvertedWorldTechnique("wc_lit")&&P::IsConvertedWorldTechnique("wc_t7_reveal_detail_lit"),"World classification");
 for(auto rva:{0x69670u,0x68d71u,0x6994cu}){auto old=image[rva];image[rva]^=1;require(!H::Install()&&attempted==0,"Changed native site was accepted");image[rva]=old;}
 failInstall=2;require(!H::Install()&&!H::hooks[0].installed&&!H::hooks[1].installed,"Partial hook not rolled back");failInstall=0;attempted=0;require(H::Install()&&attempted==2,"Both native call paths not hooked");
 std::array<std::array<std::uint8_t,0x60>,4> dvars{};const std::array<unsigned,4> pointers{0x8cc69c8,0x8cc6ab0,0x8cc6b70,0x8cc6940};
 for(unsigned i=0;i<4;++i){float value=-1.f;std::memcpy(dvars[i].data()+16,&value,4);auto* p=dvars[i].data();put(pointers[i],p);}
 Game::MaterialTechniqueSet technique{"wc_t7_reveal_detail_lit"};Game::Material material{&technique};Game::GfxSurface surface{&material};Game::GfxWorld world{1,{&surface}};auto* wp=&world;put(0x8ccc7a0,wp);
 std::array<void*,58> table{};table[57]=reinterpret_cast<void*>(Record);IDirect3DDevice9 device;device.vtable=table.data();unsigned cases=0;
 for(bool slopeSupport:{false,true})for(bool converted:{false,true})for(unsigned offset=0;offset<4;++offset){
  image[0x8ccc9dd]=slopeSupport;technique.name=converted?"wc_t7_reveal_lit":"wc_lit";device.states.clear();H::SetPolygonOffset(&device,offset<<4);
  bool precise=P::UsesPreciseDecalBias(converted,offset<<4);float nativeBias=-static_cast<float>(offset==3?1:offset)/65536.f*(slopeSupport?1:2);float expected=precise?P::DecalBias(offset<<4):nativeBias;
  require(!device.states.empty()&&device.states.back().first==195,"Missing depth state");require(std::bit_cast<float>(device.states.back().second)==expected,"Native/corrected depth bias differs");
  if(precise||slopeSupport)require(device.states.front().first==175&&std::bit_cast<float>(device.states.front().second)==(precise?0.f:-static_cast<float>(offset==3?1:offset)),"Incorrect slope state");
  ++cases;
 }
 wp=nullptr;put(0x8ccc7a0,wp);require(!H::IsConvertedWorld(),"Null world accepted");wp=&world;put(0x8ccc7a0,wp);world.surfaceCount=0;require(!H::IsConvertedWorld(),"Empty world accepted");world.surfaceCount=1;surface.material=nullptr;require(!H::IsConvertedWorld(),"Null material accepted");surface.material=&material;material.techniqueSet=nullptr;require(!H::IsConvertedWorld(),"Null technique accepted");
 std::cout<<"PASS "<<cases<<" actual native/corrected depth-bias cases; both native call sites guarded, all 3 mismatches rejected, partial hook rollback, null/empty worlds, shadow mode and stock materials preserved.\n";
 VirtualFree(image,0,MEM_RELEASE);return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
