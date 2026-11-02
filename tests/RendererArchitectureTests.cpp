#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
namespace Game {
struct Material{struct{const char* name;}info;};
struct XModel{const char* name;unsigned char numsurfs;Material** materialHandles;};
struct GfxPackedPlacement{float origin[3],axis[3][3],scale;};
struct GfxStaticModelDrawInst{GfxPackedPlacement placement;XModel* model;std::uint16_t cullDist,lightingHandle;std::array<char,20> tail;};
struct GfxStaticModelInst{struct{float midPoint[3],halfSize[3];}bounds;float lightingOrigin[3];};
struct GfxWorld{struct{unsigned smodelCount;GfxStaticModelInst* smodelInsts;GfxStaticModelDrawInst* smodelDrawInsts;}dpvs;};
}
#include "Components/Modules/RendererArchitecture.hpp"
static_assert(offsetof(Game::GfxStaticModelDrawInst,cullDist)==0x40);
static void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
struct Model{std::string name,material;Game::Material mat{};Game::Material* handle=&mat;Game::XModel model{};};
template<class T>static T read(std::ifstream& f){T v{};f.read(reinterpret_cast<char*>(&v),sizeof(v));if(!f)throw std::runtime_error("Truncated fixture");return v;}
static std::string string(std::ifstream& f){auto n=read<unsigned>(f);check(n<4096,"Invalid name");std::string v(n,'\0');f.read(v.data(),n);check(bool(f),"Truncated name");return v;}
int main(int argc,char**argv){try{
 check(argc==2,"Actual placement fixture required");std::ifstream f(argv[1],std::ios::binary);auto count=read<unsigned>(f);check(count==34298 || count==59030,"Unexpected captured map");
 std::vector<Game::GfxStaticModelDrawInst> draws(count);std::vector<Game::GfxStaticModelInst> inst(count);std::vector<std::unique_ptr<Model>> models;
 for(unsigned i=0;i<count;++i){auto values=read<std::array<float,10>>(f);auto old=read<std::uint16_t>(f);auto m=std::make_unique<Model>();m->name=string(f);m->material=string(f);m->mat.info.name=m->material.c_str();m->model={m->name.c_str(),1,&m->handle};
  auto& dr=draws[i];std::memset(&dr,0xa5,sizeof(dr));std::memcpy(dr.placement.origin,values.data(),12);dr.placement.scale=values[9];dr.model=&m->model;dr.cullDist=old;std::memcpy(inst[i].bounds.midPoint,values.data()+3,24);models.push_back(std::move(m));}
 auto before=draws;const auto instancesBefore=inst;Game::GfxWorld world{{count,inst.data(),draws.data()}};
 check(!Components::RendererArchitecture::Raise(&world,false).changed,"Stock map was changed");check(!std::memcmp(draws.data(),before.data(),draws.size()*sizeof(draws[0])),"Stock draws differ");
 auto result=Components::RendererArchitecture::Raise(&world,true);check(result.changed>100,"Captured architecture was not corrected");check(result.clusters>0,"Captured masonry cluster was not corrected");
 unsigned changed=0;std::ofstream report("architecture-deltas.tsv");report<<"index\tmodel\told_cull\tnew_cull\n";
 for(unsigned i=0;i<count;++i){auto newCull=draws[i].cullDist;check(newCull>=before[i].cullDist,"Cull distance shrank");if(newCull!=before[i].cullDist){check(newCull>=3072,"Architecture floor missing");++changed;report<<i<<'\t'<<draws[i].model->name<<'\t'<<before[i].cullDist<<'\t'<<newCull<<'\n';}auto copy=draws[i];copy.cullDist=before[i].cullDist;check(!std::memcmp(&copy,&before[i],sizeof(copy)),"Non-cull draw field changed");}
 check(!std::memcmp(inst.data(),instancesBefore.data(),inst.size()*sizeof(inst[0])),"Bounds or lighting origin changed");check(changed==result.changed,"Incorrect change accounting");check(!Components::RendererArchitecture::Raise(&world,true).changed,"Repeated frame is not idempotent");
 auto& dr=draws[0];dr.model=&models[0]->model;models[0]->name="p7_zm_ctl_stone_wall_smooth_stone_single_03";models[0]->model.name=models[0]->name.c_str();dr.cullDist=0;world.dpvs.smodelCount=1;check(!Components::RendererArchitecture::Raise(&world,true).changed&&dr.cullDist==0,"Disabled test changed");dr.cullDist=512;inst[0].bounds.halfSize[0]=-1;check(!Components::RendererArchitecture::Raise(&world,true).changed,"Invalid bounds accepted");inst[0].bounds.halfSize[0]=1;dr.placement.scale=0;check(!Components::RendererArchitecture::Raise(&world,true).changed,"Invalid scale accepted");world.dpvs.smodelCount=65536;check(!Components::RendererArchitecture::Raise(&world,true).changed,"Oversize world accepted");check(!Components::RendererArchitecture::Raise(nullptr,true).changed,"Null world accepted");
 std::cout<<"PASS actual Castle placements="<<count<<", raised="<<result.changed<<", known masonry clusters="<<result.clusters<<"; only 16-bit cull fields changed; poses/bounds/lighting/materials/cache fields preserved; stock maps, disabled tests, malformed bounds and repeat frames retained.\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
