#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>
#include "Components/Modules/Remix/Core.hpp"
namespace Utils::String
{
    static std::string ToLower(std::string text)
    {
        std::transform(text.begin(),text.end(),text.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
        return text;
    }
}
namespace Components::Remix
{
    LogLine::~LogLine() = default;
    LogLine& LogLine::operator<<(const std::wstring&){return *this;}
    LogLine& LogLine::operator<<(const wchar_t*){return *this;}
}
namespace Components::Remix::Core
{
    static std::filesystem::path testRoot;
    std::wstring ModuleDirectory(){return testRoot.wstring()+L"\\";}
#include "MapSettingsPersistence.inc"
}
static void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv)
{
    try
    {
        using namespace Components::Remix::Core;
        Require(argc==2,"Missing test output directory");
        testRoot=std::filesystem::absolute(argv[1])/("files-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
        std::filesystem::create_directories(testRoot/"zw3_remix"/"maps");
        const auto writeGlobal=[](const Config& values)
        {
            std::ofstream file(IniPath());
            for(const auto& setting:settingsTable)file<<setting.key<<" = "<<FormatSetting(values,setting.member)<<'\n';
        };
        const auto restart=[](const char* map)
        {
            // Discard all in-memory state as a new game process would.
            config={};savedConfig={};globalConfig={};activeMap.clear();hasMapConfig=false;
            LoadConfigForMap(map);
        };
        Config defaults;
        defaults.remixAutoExposure=true;defaults.remixPlusExposureTarget=0.18f;defaults.remixAreaExposureStrength=0.7f;
        writeGlobal(defaults);
        LoadConfigForMap("same_as_global");
        Require(!HasMapConfig(),"A new map unexpectedly had a profile");
        std::string message;
        SaveMapConfig(message);
        Require(HasMapConfig()&&std::filesystem::exists(MapIniPath("same_as_global")),
            "Save for this map deleted a profile matching global settings, allowing automatic look after restart");
        restart("same_as_global");
        Require(HasMapConfig()&&!HasUnsavedChanges(),"Saved map was lost after process restart");
        LoadConfigForMap("tuned");
        MutableConfig().remixExposureBiasEv=-1.53f;
        const Config expected=GetConfig();
        SaveMapConfig(message);
        Config changed=defaults;
        for(const auto& setting:settingsTable)std::visit([&](auto field)
        {
            using T=std::remove_cvref_t<decltype(changed.*field)>;
            if constexpr(std::is_same_v<T,bool>)changed.*field=!(changed.*field);
            else if constexpr(std::is_same_v<T,float>)changed.*field+=0.125f;
            else changed.*field+=1;
        },setting.member);
        writeGlobal(changed);
        restart("tuned");
        for(const auto& setting:settingsTable)Require(FormatSetting(GetConfig(),setting.member)==FormatSetting(expected,setting.member),
            "A saved map inherited changed global settings after restart");
        Require(GetActiveMap()=="tuned"&&HasMapConfig(),"Wrong map profile loaded");
        {
            std::ofstream legacy(MapIniPath("legacy"));legacy<<"remix_exposure_bias = -2.5\n";
        }
        restart("legacy");
        Require(GetConfig().remixExposureBiasEv==-2.5f,"An older differential profile no longer loads");
        const auto legacyExpected=GetConfig();
        SaveMapConfig(message);
        writeGlobal(defaults);
        restart("legacy");
        for(const auto& setting:settingsTable)Require(FormatSetting(GetConfig(),setting.member)==FormatSetting(legacyExpected,setting.member),
            "Saving an older map profile did not freeze its effective settings");
        MutableConfig().remixExposureBiasEv=-4.0f;
        SetFileAttributesW(MapIniPath("legacy").c_str(),FILE_ATTRIBUTE_READONLY);
        SaveMapConfig(message);
        SetFileAttributesW(MapIniPath("legacy").c_str(),FILE_ATTRIBUTE_NORMAL);
        Require(HasUnsavedChanges()&&message.find("Could not")!=std::string::npos,"Failed write was reported as saved");
        LoadConfigForMap("");
        SaveMapConfig(message);
        Require(message=="No map is loaded","Saved without a map");
        testRoot/="new-install";
        LoadConfigForMap("fresh_map");
        SaveMapConfig(message);
        Require(HasMapConfig()&&std::filesystem::exists(MapIniPath("fresh_map")),
            "First map save failed when zw3_remix and maps directories did not exist yet");
        std::cout<<"Map settings: restart, all fields pinned across global changes, identical defaults, legacy profiles and write failure passed\n";
        return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
