#include "STDInclude.hpp"

#include "Modules/ArenaLength.hpp"
#include "Modules/AssetHandler.hpp"
#include "Modules/Auth.hpp"
#include "Modules/Bans.hpp"
#include "Modules/Bullet.hpp"
#include "Modules/BotAI/BotAI.hpp"
#include "Modules/Bots.hpp"
#include "Modules/Console.hpp"
#include "Modules/ConnectProtocol.hpp"
#include "Modules/Branding.hpp"
#include "Modules/CardTitles.hpp"
#include "Modules/Ceg.hpp"
#include "Modules/Changelog.hpp"
#include "Modules/D3D9Ex.hpp"
#include "Modules/Debug.hpp"
#include "Modules/Dedicated.hpp"
#include "Modules/Discord.hpp"
#include "Modules/Discovery.hpp"
#include "Modules/Elevators.hpp"
#include "Modules/Chat.hpp"
#include "Modules/ClanTags.hpp"
#include "Modules/ClientCommand.hpp"
#include "Modules/ClientSlots.hpp"
#include "Modules/Command.hpp"
#include "Modules/ConfigStrings.hpp"
#include "Modules/Dvar.hpp"
#include "Modules/Events.hpp"
#include "Modules/Exception.hpp"
#include "Modules/FastFiles.hpp"
#include "Modules/FileSystem.hpp"
#include "Modules/Flags.hpp"
#include "Modules/Friends.hpp"
#include "Modules/Gamepad.hpp"
#include "Modules/GSC/GSC.hpp"
#include "Modules/Handshake.hpp"
#include "Modules/HudParallax.hpp"
#include "Modules/Huffman.hpp"
#include "Modules/IPCPipe.hpp"
#include "Modules/Lean.hpp"
#include "Modules/LobbyScene.hpp"
#include "Modules/Localization.hpp"
#include "Modules/Logger.hpp"
#include "Modules/MapDump.hpp"
#include "Modules/MapRotation.hpp"
#include "Modules/Maps.hpp"
#include "Modules/Materials.hpp"
#include "Modules/Menus.hpp"
#include "Modules/ModelCache.hpp"
#include "Modules/ModelSurfs.hpp"
#include "Modules/Download.hpp"
#include "Modules/ModList.hpp"
#include "Modules/Node.hpp"
#include "Modules/Network.hpp"
#include "Modules/NetworkDebug.hpp"
#include "Modules/News.hpp"
#include "Modules/Scheduler.hpp"
#include "Modules/Party.hpp"
#include "Modules/PlayerMovement.hpp"
#include "Modules/PlayerName.hpp"
#include "Modules/Playlist.hpp"
#include "Modules/QuickPatch.hpp"
#include "Modules/RawFiles.hpp"
#include "Modules/RawMouse.hpp"
#include "Modules/RCon.hpp"
#include "Modules/Renderer.hpp"
#include "Modules/Rumble.hpp"
#include "Modules/Security.hpp"
#include "Modules/ServerCommands.hpp"
#include "Modules/ServerInfo.hpp"
#include "Modules/ServerList.hpp"
#include "Modules/Session.hpp"
#include "Modules/Singleton.hpp"
#include "Modules/SlowMotion.hpp"
#include "Modules/Sound.hpp"
#include "Modules/SPLoadscreens.hpp"
#include "Modules/StartupMessages.hpp"
#include "Modules/Stats.hpp"
#include "Modules/Steam.hpp"
#include "Modules/StringTable.hpp"
#include "Modules/StructuredData.hpp"
#include "Modules/Theatre.hpp"
#include "Modules/Toast.hpp"
#include "Modules/Leaderboard.hpp"
#include "Modules/UPnP.hpp"
#include "Modules/ZW3Changelog.hpp"
#include "Modules/ZWNet.hpp"
#include "Modules/ViewModelFxSetup.hpp"
#include "Modules/VisionFile.hpp"
#include "Modules/TextRenderer.hpp"
#include "Modules/Threading.hpp"
#include "Modules/UIFeeder.hpp"
#include "Modules/UIScript.hpp"
#include "Modules/Updater.hpp"
#include "Modules/Vote.hpp"
#include "Modules/Voice.hpp"
#include "Modules/Weapon.hpp"
#include "Modules/Window.hpp"
#include "Modules/Zones.hpp"
#include "Modules/ZoneBuilder.hpp"
#include "Modules/ZoneConvert.hpp"

namespace Components
{
	bool Loader::pregame = true;
	std::vector<Component*> Loader::components;

	bool Loader::IsPregame()
	{
		return pregame;
	}

	void Loader::Initialize()
	{
		pregame = true;

		Register(new Flags());
		Register(new Exception());
		Register(new Command());
		Register(new Dvar());
		Register(new Logger());
		Register(new Scheduler());
		Register(new Events());
		Register(new Singleton());
		Register(new IPCPipe());
		Register(new Branding());
		Register(new Console());
		Register(new Steam());
		Register(new Ceg());
		Register(new Changelog());
		Register(new Vote());
		Register(new Huffman());
		Register(new Handshake());
		Register(new Network());
		Register(new ServerCommands());
		Register(new Party());
		Register(new Playlist());
		Register(new ConnectProtocol());
		Register(new ServerInfo());
		Register(new UIFeeder());
		Register(new UIScript());
		Register(new ZoneBuilder());
		Register(new Auth());
		Register(new ServerList());
		Register(new Session());
		Register(new AssetHandler());
		Register(new FastFiles());
		Register(new FileSystem());
		Register(new Localization());
		Register(new MapDump());
		Register(new TextRenderer());
		Register(new Chat());
		Register(new MapRotation());
		Register(new Menus());
		Register(new Stats());
		Register(new StringTable());
		Register(new StructuredData());
		Register(new QuickPatch());
		Register(new RawFiles());
		Register(new Security());
		Register(new ConfigStrings());
		Register(new ModelCache());
		Register(new ModelSurfs());
		Register(new Friends());
		Register(new Gamepad());
		Register(new Rumble());
		Register(new Weapon());
		Register(new ClientSlots());
		Register(new Threading());

		Register(new GSC::GSC());
		Register(new Bots());
		Register(new BotAI::BotAI());
		Register(new Maps());
		Register(new ClanTags());
		Register(new PlayerName());
		Register(new CardTitles());
		Register(new Bans());
		Register(new ClientCommand());
		Register(new RCon());
		Register(new Voice());
		Register(new ModList());
		Register(new Node());
		Register(new Download());
		Register(new Theatre());
		Register(new D3D9Ex());
		Register(new Debug());
		Register(new Dedicated());
		Register(new Discord());
		Register(new ArenaLength());
		Register(new Bullet());
		Register(new Discovery());
		Register(new Elevators());
		Register(new Lean());
		Register(new HudParallax());
		Register(new Materials());
		Register(new NetworkDebug());
		Register(new PlayerMovement());
		Register(new Window());
		Register(new RawMouse());
		Register(new Renderer());
		Register(new LobbyScene());
		Register(new SlowMotion());
		Register(new Sound());
		Register(new SPLoadscreens());
		Register(new StartupMessages());
		Register(new News());
		Register(new Toast());
		Register(new Leaderboard());
		Register(new UPnP());
		Register(new ZW3Changelog());
		Register(new ZWNet());
		Register(new ViewModelFxSetup::Setup());
		Register(new VisionFile());
		Register(new Zones());

		pregame = false;
	}

	void Loader::Register(Component* component)
	{
		if (!component)
		{
			return;
		}

		components.push_back(component);
	}
}
