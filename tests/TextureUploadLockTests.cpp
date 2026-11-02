#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
using UINT = unsigned;
using DWORD = unsigned long;
using HRESULT = std::int32_t;
constexpr HRESULT D3D_OK=0, D3DERR_INVALIDCALL=static_cast<HRESULT>(0x8876086c), E_FAIL=static_cast<HRESULT>(0x80004005);
#define SUCCEEDED(x) ((x)>=0)
#define FAILED(x) ((x)<0)
constexpr int D3DPOOL_SYSTEMMEM=2;
struct RECT { int left,top,right,bottom; };
struct D3DLOCKED_RECT { int Pitch; void* pBits; };
struct D3DSURFACE_DESC { UINT Width=8,Height=8; int Format=0x35545844; };
struct IDirect3DDevice9;
struct IDirect3DSurface9
{
    int refs=1;
    std::vector<unsigned char> pixels=std::vector<unsigned char>(96,0);
    void AddRef(){++refs;}
    void Release(){--refs;}
};
struct IDirect3DTexture9
{
    IDirect3DDevice9* device=nullptr;
    HRESULT lockResult=D3D_OK,unlockResult=D3D_OK,surfaceResult=D3D_OK;
    bool nullBits=false;
    int pitch=48,refs=1,locks=0,unlocks=0;
    UINT lastLevel=0;
    D3DSURFACE_DESC desc;
    IDirect3DSurface9 surface;
    HRESULT LockRect(UINT level,D3DLOCKED_RECT* out,const RECT*,DWORD)
    {
        ++locks; lastLevel=level;
        out->Pitch=FAILED(lockResult)?0:pitch;
        out->pBits=FAILED(lockResult)||nullBits?nullptr:surface.pixels.data();
        return lockResult;
    }
    HRESULT UnlockRect(UINT level){++unlocks;lastLevel=level;return unlockResult;}
    HRESULT GetLevelDesc(UINT,D3DSURFACE_DESC* out){*out=desc;return D3D_OK;}
    HRESULT GetDevice(IDirect3DDevice9** out);
    HRESULT GetSurfaceLevel(UINT level,IDirect3DSurface9** out)
    {
        lastLevel=level;
        if(FAILED(surfaceResult))return surfaceResult;
        surface.AddRef();*out=&surface;return D3D_OK;
    }
    void AddRef(){++refs;}
    void Release(){--refs;}
};
struct IDirect3DDevice9
{
    IDirect3DTexture9 staging;
    HRESULT createResult=D3D_OK,updateResult=D3D_OK;
    int refs=1,creates=0,updates=0;
    UINT width=0,height=0,levels=0;
    int pool=-1;
    DWORD usage=999;
    void Release(){--refs;}
    HRESULT CreateTexture(UINT w,UINT h,UINT l,DWORD u,int format,int p,IDirect3DTexture9** out,void*)
    {
        ++creates;width=w;height=h;levels=l;usage=u;pool=p;staging.desc.Format=format;
        if(FAILED(createResult))return createResult;
        *out=&staging;return D3D_OK;
    }
    HRESULT UpdateSurface(IDirect3DSurface9* from,const RECT*,IDirect3DSurface9* to,const void*)
    {
        ++updates;
        if(FAILED(updateResult))return updateResult;
        to->pixels=from->pixels;return D3D_OK;
    }
};
HRESULT IDirect3DTexture9::GetDevice(IDirect3DDevice9** out){++device->refs;*out=device;return D3D_OK;}
#include "Utils/TextureUploadLock.hpp"
static void Require(bool v,const char* message){if(!v)throw std::runtime_error(message);}
int main()
{
    try
    {
        {
            IDirect3DDevice9 device;IDirect3DTexture9 texture;texture.device=&device;
            Utils::TextureUploadLock upload;D3DLOCKED_RECT lock{};
            Require(upload.Lock(&texture,0,&lock,nullptr,0)==D3D_OK&&lock.pBits,"Normal texture lock failed");
            Require(upload.Unlock(&texture,0)==D3D_OK&&device.creates==0&&texture.unlocks==1,"Direct path used staging");
        }
        for(bool invalidSuccess:{false,true})
        {
            IDirect3DDevice9 device;IDirect3DTexture9 texture;texture.device=&device;
            texture.lockResult=invalidSuccess?D3D_OK:D3DERR_INVALIDCALL;texture.nullBits=invalidSuccess;
            Utils::TextureUploadLock upload;D3DLOCKED_RECT lock{};
            Require(upload.Lock(&texture,2,&lock,nullptr,0)==D3D_OK&&lock.pBits&&lock.Pitch==48,"Failed lock was not recovered");
            Require(device.pool==D3DPOOL_SYSTEMMEM&&device.levels==1&&device.usage==0,"Wrong staging resource");
            Require(device.width==8&&device.height==8&&device.staging.desc.Format==0x35545844,"DXT5 mip shape changed");
            // Two compressed rows with driver padding must be preserved intact.
            for(unsigned i=0;i<96;++i)static_cast<unsigned char*>(lock.pBits)[i]=static_cast<unsigned char>(i+1);
            Require(upload.Unlock(&texture,2)==D3D_OK&&device.updates==1&&texture.lastLevel==2,"Staged pixels were not uploaded to the requested mip");
            Require(texture.surface.pixels[0]==1&&texture.surface.pixels[95]==96,"Texture pixels were dropped");
            Require(texture.unlocks==int(invalidSuccess)&&device.staging.unlocks==1,"Unlock balance was wrong");
            Require(texture.refs==1&&device.refs==1&&device.staging.refs==0&&texture.surface.refs==1,"Fallback leaked COM references");
        }
        for(int failure=0;failure<4;++failure)
        {
            IDirect3DDevice9 device;IDirect3DTexture9 texture;texture.device=&device;texture.lockResult=E_FAIL;
            if(failure==0)device.createResult=E_FAIL;
            if(failure==1)device.staging.lockResult=E_FAIL;
            if(failure==2)device.staging.nullBits=true;
            if(failure==3)device.updateResult=E_FAIL;
            Utils::TextureUploadLock upload;D3DLOCKED_RECT lock{};
            const auto result=upload.Lock(&texture,0,&lock,nullptr,0);
            if(failure<3)Require(FAILED(result)&&!lock.pBits&&!upload.UsesStaging(),"Unusable fallback reached the native memcpy");
            else Require(FAILED(upload.Unlock(&texture,0)),"Failed GPU upload was reported as success");
            Require(texture.refs==1&&device.refs==1,"Failure leaked references");
        }
        {
            IDirect3DDevice9 device;IDirect3DTexture9 texture;texture.device=&device;texture.lockResult=E_FAIL;
            Utils::TextureUploadLock upload;D3DLOCKED_RECT lock{};
            Require(FAILED(upload.Lock(&texture,0,&lock,nullptr,1))&&device.creates==0,"Read/subrect operation got a blank fallback");
        }
        std::cout<<"Texture upload: failed/null direct locks, DXT5 padded mip transfer, failures and COM cleanup passed\n";
        return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
