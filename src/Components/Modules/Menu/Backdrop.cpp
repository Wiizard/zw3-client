#include "STDInclude.hpp"
#include "../AssetHandler.hpp"
#include "../Dedicated.hpp"
#include "../ZoneBuilder.hpp"
#include "../Logger.hpp"
#include "../FastFiles.hpp"
#include "../Scheduler.hpp"
#include "../Command.hpp"
#include "../Maps.hpp"
#include "../Dvar.hpp"
#include "../Materials.hpp"
#include "../FileSystem.hpp"
#include "../Renderer.hpp"
#include "Backdrop.hpp"
#include "Actors.hpp"
#include "Engine.hpp"
#include "Hooks.hpp"
#include "Cast.hpp"
#include "Mask.hpp"
#include "Stage.hpp"
#include "ClipMapState.hpp"
#include "Budget.hpp"
#include "Camera.hpp"
#include "Director.hpp"
#include "Transition.hpp"
#include "Selection.hpp"
#include "WorldEffects.hpp"


#include <windows.h>
#include <intrin.h> // _ReturnAddress ()

#include <cstdlib>      // calloc(), free()
#include <cstring>      // memset()
#include <format>
#include <cmath>        // cos(), sin()
#include <numbers>      // pi_v
#include <stdexcept>    // invalid_argument
#include <array>
#include <algorithm>    // min(), ranges::{any_of,contains,find,sort}()
#include <deque>
#include <filesystem>
#include <string_view>
#include <regex>
#include <mutex>




using namespace std;

namespace Components
{
  namespace Menu
  {
    using namespace Engine;
    using namespace BackdropData;
    static bool hooksInstalled = false;
    static std::atomic_bool enabled = false;
    namespace
    {
      using namespace BackdropData;

      // Engine addresses the generated headers don't give us, or give us
      // with a type we can't use. Every data address here was read from
      // the executable's own instructions (objdump of iw4mp.exe), not from
      // the disassembler's annotations, which are 0x10 low in .data.

      // SCR_UpdateScreen's call to SCR_DrawScreenField (0x1401018A0), made
      // every frame after R_ToggleSmpFrame. It clears the screen and calls
      // UI_RefreshViewport to paint the menus, so overlays must precede it.
      //
      constexpr uintptr_t screen_field_call (0x140101D15);

      using screen_field_fn = void (*) (unsigned int, int);

      // SCR_DrawScreenField's clear of the screen behind fullscreen menus,
      // the one the front end takes when not connected, and its clear
      // behind the connect screen, while connecting and loading.
      //
      constexpr uintptr_t menu_clear_call (0x140101B81);
      constexpr uintptr_t connect_clear_call (0x1401019CB);

      // The renderer's scene path calls into cgame and cgame's effects:
      // R_RenderScene's view setup (0x140024750) draws the shellshock
      // blend, and R_GenerateSortedDrawSurfs asks whether the motion
      // tracker is up, generates the effect marks' vertices (the models'
      // through a worker command it fills, the glass's itself), and draws
      // the 2D. These are those calls (see cgame_draw_2d () for why we hook
      // them).
      //
      constexpr uintptr_t shellshock_blend_call (0x14002481D);
      constexpr uintptr_t motion_tracker_call (0x14002334B);
      constexpr uintptr_t model_marks_call (0x1400230B8);
      constexpr uintptr_t glass_marks_call (0x140023555);
      constexpr uintptr_t draw_2d_call (0x140022896);

      // The engine's own signatures at these calls, which the generated
      // declarations lack or get wrong: each takes the local client.
      //
      using motion_tracker_fn = bool (*) (int local_client);
      using model_marks_fn = void (*) (int local_client, unsigned char* cmd);
      using glass_marks_fn = void (*) (int local_client);

      // In the model marks command, whether the worker should generate
      // anything at all (it returns at once if not).
      //
      constexpr size_t model_marks_enabled (0x54);

      // The client's rendererStarted and uiStarted, as Com_StartHunkUsers
      // and PreloadMap test them. Note that clientStatic_t in the generated
      // headers places both 4 bytes too low.
      //
      const volatile int* renderer_started = nullptr;

      const volatile int* ui_started = nullptr;

      // The two ends of the zone memory region still free (see
      // zone/engine/zone-memory.hxx). PMem_AllocFromSource allocates up from
      // the first and down from the second and fails when they would cross.
      //
      const volatile int32_t* pmem_low = nullptr;

      const volatile int32_t* pmem_high = nullptr;

      // The head of the free XAssetEntry list, which DB_AllocXAssetEntry
      // (0x14012CDE0) pops and which is a fatal error when empty. Each free
      // node starts with the pointer to the next one.
      //
      void* const* free_entry_head = nullptr;

      // CL_RenderScene, R_InitPrimaryLights, and SL_FindString, with the
      // types they actually have.
      //
      using render_scene_fn = void (*) (refdef_t*);
      using init_primary_lights_fn = GfxWorld* (*) (GfxLight*);
      using find_string_fn = unsigned int (*) (const char*);

      render_scene_fn render_scene = nullptr;

      init_primary_lights_fn init_primary_lights = nullptr;

      find_string_fn find_string = nullptr;

      // CG_DrawActiveFrame tells the renderer that this is the first local
      // client's scene right before it toggles the frame (a single store
      // into the scene's visibility state). We are that client's scene.
      //
      using first_view_fn = void (*) ();

      first_view_fn first_view = nullptr;

      // Zone release goes through the observed engine entry. Map requests
      // use Maps::LoadMapZones so team, patch and dependency routing is shared.
      //
      using load_xassets_fn = void (*) (XZoneInfo*, unsigned int, int);

      load_xassets_fn load_xassets = nullptr;

      // The part of R_Shutdown that lets go of the world's renderer
      // resources (model lighting, the static model cache, and the world's
      // buffers), which R_Shutdown follows by forgetting the world itself.
      // It is what a level load does to the renderer short of shutting the
      // rest of it down too, so it is what we do to swap one backdrop map
      // for another.
      //
      using release_world_fn = void (*) ();

      release_world_fn release_world = nullptr;

      // The allocation flag of a level's zones. The engine frees a level
      // with it (DB_LoadLevelXAssets) and gives it its own memory group, so
      // a map loaded with it is freed by the next level load like any other.
      //
      constexpr int zone_flag_level (0x4);

      // The calls to DB_LoadLevelXAssets (0x14012EB40), one of them a tail
      // call (PreloadMap's jmp at 0x1400FE489), one of which every level
      // load goes through. CL_ConnectionlessPacket's loadingnewmap no longer
      // inlines PreloadMap, so its call goes through that jmp too.
      //
      constexpr uintptr_t level_load_calls[] {0x1400F4D26,
                                              0x1400FE489,
                                              0x140112036,
                                              0x14023B6A2};

      // The party's check that enough of the level is in to have the
      // members load it too (it starts them at a quarter), and the load
      // bar's measure of it (UI_DrawLoadBar), both of which ask
      // DB_GetLoadedFraction.
      //
      constexpr uintptr_t party_fraction_call (0x140109A5D);
      constexpr uintptr_t load_bar_fraction_call (0x140251045);



      // Free the level's zones the way a level load frees them.
      //
      // Each asset of theirs is left as its type's default, so that whatever
      // outside them still points at it points at something valid (a weapon
      // of common_mp whose sound only the map had, say), and the load's
      // DB_FreeUnusedResources() then returns every such default that
      // nothing points at to the pool. Returning them all without asking
      // would leave those pointers to free entries, which the next load
      // either hands to unrelated assets or the next mark pass reads.
      //
      // Except that DB_FreeUnusedResources() also keeps every default whose
      // asset the main thread has asked for by name (bit 1 of the entry's
      // in-use mask). DB_FindXAssetHeader() marks the asset it answers and
      // everything the asset points at, so asking for the world marks the
      // whole map. A level load clears those marks before it frees the
      // level (Com_Restart() calls DB_ReleaseXAssets()), and we have to as
      // well. Otherwise the map's thousands of defaults stay, and loading
      // the next map over them overflows the 2048 copies DB_AddXAsset()
      // can queue for assets that are still in use (g_copyInfo exceeded).
      //
      // We clear the marks of the level's assets only, those of the zones
      // we loaded (the map's and those loaded with every map), which we
      // know by name. Everything else stays loaded, and its marks keep
      // meaning what they mean.
      //
      void
      free_level_zones (const string& map, const std::deque<string>& extra)
      {
        Sys_SyncDatabase ();

        // The zones' slots, by the name each was loaded under.
        //
        constexpr size_t zone_slots (33);
        array<bool, zone_slots> ours {};
        string names;

        for (size_t z (1); z != zone_slots; ++z)
        {
          const char* n (reinterpret_cast<const char*> (Utils::Hook::Rebase (0x1415FA320) + z * 0xF8 + 8));
          const int flags (Utils::Hook::Get<int> (0x1415FA368 + z * 0xF8));

          if (n[0] == '\0')
            continue;

          // Include the team, dependency and localized zones loaded by Maps.
          // DB_LoadXAssets tests this exact allocation mask when freeing them.
          if ((flags & zone_flag_level) != 0 ||
              (!map.empty () && I_stricmp (n, map.c_str ()) == 0) ||
              ranges::any_of (extra,
                              [n] (const string& x)
                              {
                                return I_stricmp (n, x.c_str ()) == 0;
                              }))
          {
            ours[z] = true;
            names += names.empty () ? "" : ", ";
            names += format ("{} (slot {})", n, z);
          }
        }

        const size_t released (AssetHandler::ReleaseZoneReferences (ours));
        LogLine{} << "backdrop: freeing " << names << ": " << released
                  << " asset references released";

        XZoneInfo z {nullptr, 0, zone_flag_level};
        load_xassets (&z, 1, 0);

        // The clip map keeps its name when its zone goes (CM_Unload clears
        // everything else), and the name was in the zone. So it would now
        // point into freed memory, and the next CM_LoadMap would take the
        // same name for the same map.
        //
        cm->name = nullptr;
      }

      // How long the front end has to be up before we load anything. When
      // the menus are on their way somewhere (a map is about to load, the
      // renderer is about to restart), whatever we loaded would be freed
      // again right away.
      //
      constexpr int settle_msec (250);

      // The near plane, as the engine's own r_znear default.
      //
      constexpr float z_near (4.0f);

      // The most models a figure may have, and the entity number its DObj
      // is drawn as, the character being entity 0. The renderer caches
      // skeletons by entity, so the two must differ.
      //
      constexpr size_t max_figure_models (8);
      constexpr unsigned int figure_entity (1);

      // The materials the main menus paint their background with
      // (ui_mp/main_text.menu and the menus that share its look). They are
      // what the backdrop replaces, so they are not drawn while it shows.
      //
      constexpr string_view hidden_materials[]
      {
        "mw2_main_background",
        "mw2_main_cloud_overlay",
        "mw2_main_sp_image",
        "mw2_main_mp_image",
        "mw2_main_co_image",
        "animbg_blur_back",
        "animbg_blur_fogscroll",
        "animbg_blur_front"
      };

      // Memory for the animation trees.
      //
      // The engine allocates a tree's memory through a callback and the
      // client's own is the level's hunk, which a level load clears from
      // under us. So each tree gets heap blocks of its own, kept with what
      // it animates (see character below) so that whatever the engine
      // doesn't hand back is freed with it. The callbacks have no context,
      // so the blocks of the tree being made or freed are pointed to here
      // while it is.
      //
      vector<void*>* anim_blocks (nullptr);

      void*
      anim_allocate (int n)
      {
        if (anim_blocks == nullptr)
        {
          LogLine{} << "backdrop: animation memory asked for outside of a "
                     << "tree's making";
          Logger::Fatal ("menu backdrop: animation allocation outside tree construction");
        }

        void* p (calloc (1, static_cast<size_t> (n)));

        if (p == nullptr)
        {
          LogLine{} << "backdrop: out of memory allocating " << n
                     << " bytes for the animation tree";
          Logger::Fatal ("menu backdrop: out of animation memory");
        }

        anim_blocks->push_back (p);
        return p;
      }

      void
      anim_free (void* p, int)
      {
        if (anim_blocks == nullptr)
          return;

        auto i (ranges::find (*anim_blocks, p));

        if (i != anim_blocks->end ())
        {
          free (p);
          anim_blocks->erase (i);
        }
      }

      // Where the backdrop is.
      //
      //   idle -> loading -> ready
      //     ^        |
      //     |        v
      //     +---- failed (until the renderer restarts)
      //
      enum class phase
      {
        idle,    // Nothing loaded by us, nothing shown.
        loading, // The map's zone is on its way.
        ready,   // The world is the renderer's and the scene is shown.
        failed   // Something was missing or short; we don't retry.
      };

      // The character, put together for the scene.
      //
      struct character
      {
        XAnim_s* anims {nullptr};
        XAnimTree_s* tree {nullptr};
        vector<void*> blocks; // The tree's memory (see anim_allocate()).

        // The models shown in part, which the DObj points to.
        //
        vector<unique_ptr<masked_model>> masked;

        alignas (16) unsigned char obj[sizeof (DObj)];
        bool created {false};

        cpose_t pose;
        array<float, 3> lighting_origin;

        DObj*
        dobj () noexcept {return reinterpret_cast<DObj*> (obj);}
      };

      struct state
      {
        phase current {phase::idle};

        const stage* shown_stage {nullptr};
        string map;     // As the zone is named: mp_rust.
        string world;   // As the world is named: maps/mp/mp_rust.d3dbsp.
        spawn standing {};

        // The zone budget when the load was asked for, to log what the map
        // really took against what we estimated.
        //
        int64_t zone_free_before {0};
        uint32_t entries_free_before {0};
        uint64_t zone_estimate {0};

        int front_end_since {-1}; // Milliseconds, -1 when not in it.
        int loading_since {0};
        int start_time {0};
        int last_time {0};

        unique_ptr<refdef_t> refdef;
        unique_ptr<character> cast_member;

        // The figure a menu has stand in its map, and the one it was put
        // together for, to tell when that changes.
        //
        unique_ptr<character> figure;
        optional<BackdropFigure> figure_for;

        // Whether this frame's scene was rendered, which is what hides the
        // menus' background.
        //
        bool shown {false};
        bool in_scene {false};      // Our scene is being submitted.

        // The dvar value the current stage was chosen for, to say once that
        // a change waits for the next load rather than repeating it.
        //
        string chosen_for;

        uint64_t seed {0};
        backdrop_selector selector;

        // The direction for the frame being drawn, if a menu gave one, and
        // the map of the last direction that was refused, to say once why.
        //
        optional<BackdropDirection> direction;
        string refused_map;

        // The direction of the last scene rendered in the front end, which
        // stays up while a match is joined, the same kept through the
        // renderer's restart for the level load, and whether the scene is
        // being kept up for a match (as last logged).
        //
        optional<BackdropDirection> last_direction;
        optional<BackdropDirection> held_direction;
        bool joining {false};
        bool remote_updates_logged {false}; // While the match is joined.

        // The zones loaded with every map (see Backdrop::AddZone()). The
        // load is given their names, which therefore stay put.
        //
        std::deque<string> zones;

        // The map whose zones the last renderer shutdown kept for the level
        // load that may follow, and the level that a level load found
        // loaded that way, until the next level load (see level_load()).
        //
        string held;
        string resident_level;

        // The map last published in ui_backdrop_map.
        //
        string published_map;

        // Maps a random pick tried this session that could not be shown,
        // and how many tries the current pick has had (see retry_or_fail()).
        //
        vector<string> rejected;
        unsigned int retries {0};
        bool random_pick {false};

        // The camera on the map shown (see Director.hpp).
        //
        director scene;

        // The map a lobby has moved on to, and since when (see show()).
        //
        string wanted_map;
        int wanted_since {0};

        backdrop_transition transition;
        bool main_menu_transition {true}; // Only the main menu loads its background without an overlay.
        bool quitting {false};  // The game is on its way out (see Backdrop::BeginShutdown ()).
        bool startup_presented {false};
        int startup_since {-1};
        string prefetched_map;  // The lobby map last read ahead.

        // What the log has already been told, so that per-frame conditions
        // are said once when they start rather than every frame.
        //
        bool in_front_end {false};  // As last logged.
        string last_value;          // The ui_backdrop value last logged.
        string failure;
        bool settling {false};
        bool database_busy {false};
        int loading_reported {0};   // When loading progress was last logged.
        uint64_t frames {0};        // Rendered since the stage was shown.
        int heartbeat {0};          // When the last heartbeat was logged.
        uint64_t hidden {0};        // Background draws left out.
        vector<string> hidden_logged;
        bool bad_screen {false};
      };

      state s;

      // The game is on its way out: by the quit command (see
      // Backdrop::BeginShutdown ()) or Com_Quit_f. Nothing more is loaded,
      // held or taken back up from then on, since the engine is freeing the
      // zones under us.
      //
      bool
      quitting () noexcept
      {
        return s.quitting || (Game::g_quitRequested != nullptr && *Game::g_quitRequested);
      }

      Material* teleport_arc = nullptr;
      Material* teleport_veil = nullptr;
      Material* spinner_icon = nullptr; // skullicon_old, at the spinner's middle (see draw_spinner ()).
      Material* spinner_light = nullptr; // The same image, added, to light it up.
      vector<Game::GfxStateBits> spinner_light_blend;
      Material* spinner_lines = nullptr; // Own additive state; the shared white UI material stays unchanged.
      vector<Game::GfxStateBits> spinner_lines_blend;
      vector<Game::GfxStateBits> teleport_blend;
      atomic_bool teleport_needs_load {true};

      // An IWI image file decoded and shrunk, smoothly, to `target` pixels
      // square (or its own size, if smaller), as an image of our own. Our
      // icons come as one large block-compressed level with no mipmaps, so
      // the renderer shrinking them on the fly leaves them jagged and
      // shimmering; shrunk here by averaging every source pixel, they stay
      // sharp. Handles the IW4 (6) and IW5 (8) headers and the ARGB8, DXT1,
      // DXT3 and DXT5 formats; nullptr for anything else.
      //
      Game::GfxImage*
      load_icon (const string& name, unsigned int target)
      {
        FileSystem::File file ("images/" + name + ".iwi");
        if (!file.Exists ()) return nullptr;

        const string& d (file.GetBuffer ());
        const auto u8 = [&d] (size_t i) { return static_cast<unsigned int> (static_cast<unsigned char> (d[i])); };
        const auto u16 = [&u8] (size_t i) { return u8 (i) | (u8 (i + 1) << 8); };

        if (d.size () < 32 || d.compare (0, 3, "IWi") != 0) return nullptr;

        unsigned int format, w, h;
        if (u8 (3) == 8) { format = u8 (8); w = u16 (10); h = u16 (12); }
        else if (u8 (3) == 6) { format = u8 (4); w = u16 (6); h = u16 (8); }
        else return nullptr;

        if (w == 0 || h == 0 || w > 4096 || h > 4096) return nullptr;

        // The largest level comes last.
        const size_t blocks (static_cast<size_t> ((w + 3) / 4) * ((h + 3) / 4));
        size_t bytes;
        switch (format)
        {
          case 0x01: bytes = static_cast<size_t> (w) * h * 4; break; // ARGB8
          case 0x0B: bytes = blocks * 8; break;                       // DXT1
          case 0x0C: case 0x0D: bytes = blocks * 16; break;           // DXT3, DXT5
          default: return nullptr;
        }
        if (d.size () < 32 + bytes) return nullptr;
        const auto* src (reinterpret_cast<const unsigned char*> (d.data ()) + d.size () - bytes);

        // Decode to RGBA.
        vector<unsigned char> rgba (static_cast<size_t> (w) * h * 4);
        const auto put = [&] (unsigned int x, unsigned int y, unsigned int r, unsigned int g, unsigned int b, unsigned int a)
        {
          if (x >= w || y >= h) return;
          unsigned char* p (&rgba[(static_cast<size_t> (y) * w + x) * 4]);
          p[0] = static_cast<unsigned char> (r); p[1] = static_cast<unsigned char> (g);
          p[2] = static_cast<unsigned char> (b); p[3] = static_cast<unsigned char> (a);
        };

        if (format == 0x01)
        {
          for (unsigned int y (0); y != h; ++y)
            for (unsigned int x (0); x != w; ++x)
            {
              const unsigned char* p (src + (static_cast<size_t> (y) * w + x) * 4); // B, G, R, A
              put (x, y, p[2], p[1], p[0], p[3]);
            }
        }
        else
        {
          const size_t stride (format == 0x0B ? 8 : 16);
          for (unsigned int by (0); by < h; by += 4)
            for (unsigned int bx (0); bx < w; bx += 4)
            {
              const unsigned char* b (src + ((by / 4) * ((w + 3) / 4) + bx / 4) * stride);
              unsigned int alpha[16];
              for (unsigned int& a: alpha) a = 255;

              if (format == 0x0C) // DXT3: 4 bits each.
              {
                for (int i (0); i != 16; ++i)
                  alpha[i] = ((b[i / 2] >> ((i & 1) * 4)) & 0xF) * 17;
                b += 8;
              }
              else if (format == 0x0D) // DXT5: two ends and 3-bit steps between.
              {
                unsigned int a[8] {b[0], b[1]};
                if (a[0] > a[1])
                  for (unsigned int i (1); i != 7; ++i) a[i + 1] = ((7 - i) * a[0] + i * a[1]) / 7;
                else
                {
                  for (unsigned int i (1); i != 5; ++i) a[i + 1] = ((5 - i) * a[0] + i * a[1]) / 5;
                  a[6] = 0; a[7] = 255;
                }
                uint64_t bits (0);
                for (int i (0); i != 6; ++i) bits |= static_cast<uint64_t> (b[2 + i]) << (8 * i);
                for (int i (0); i != 16; ++i) alpha[i] = a[(bits >> (3 * i)) & 7];
                b += 8;
              }

              const unsigned int c0 (b[0] | (b[1] << 8)), c1 (b[2] | (b[3] << 8));
              unsigned int c[4][4];
              const auto expand = [] (unsigned int v, unsigned int* o)
              {
                o[0] = ((v >> 11) & 31) * 255 / 31;
                o[1] = ((v >> 5) & 63) * 255 / 63;
                o[2] = (v & 31) * 255 / 31;
                o[3] = 255;
              };
              expand (c0, c[0]);
              expand (c1, c[1]);
              const bool four (format != 0x0B || c0 > c1);
              for (int k (0); k != 3; ++k)
              {
                c[2][k] = four ? (2 * c[0][k] + c[1][k]) / 3 : (c[0][k] + c[1][k]) / 2;
                c[3][k] = four ? (c[0][k] + 2 * c[1][k]) / 3 : 0;
              }
              c[2][3] = 255;
              c[3][3] = four ? 255 : 0;

              const unsigned int idx (b[4] | (b[5] << 8) | (b[6] << 16) | (static_cast<unsigned int> (b[7]) << 24));
              for (int i (0); i != 16; ++i)
              {
                const unsigned int* col (c[(idx >> (2 * i)) & 3]);
                put (bx + static_cast<unsigned int> (i % 4), by + static_cast<unsigned int> (i / 4),
                     col[0], col[1], col[2], format == 0x0B ? col[3] : alpha[i]);
              }
            }
        }

        // Shrink, averaging with alpha weighting so edges don't fringe.
        const unsigned int ow (min (w, target)), oh (min (h, target));
        Game::GfxImage* image (Materials::CreateImage ("zw3_icon_" + name, ow, oh, 1, 0x1000003, D3DFMT_A8R8G8B8));
        D3DLOCKED_RECT pixels {};
        if (image == nullptr || image->texture.map == nullptr ||
            FAILED (image->texture.map->LockRect (0, &pixels, nullptr, 0)))
        {
          if (image != nullptr) Materials::DeleteImage (image);
          return nullptr;
        }

        for (unsigned int y (0); y != oh; ++y)
          for (unsigned int x (0); x != ow; ++x)
          {
            const unsigned int x0 (x * w / ow), x1 (max (x0 + 1, (x + 1) * w / ow));
            const unsigned int y0 (y * h / oh), y1 (max (y0 + 1, (y + 1) * h / oh));
            double r (0), g (0), b (0), a (0);
            for (unsigned int sy (y0); sy != y1; ++sy)
              for (unsigned int sx (x0); sx != x1; ++sx)
              {
                const unsigned char* p (&rgba[(static_cast<size_t> (sy) * w + sx) * 4]);
                r += p[0] * p[3]; g += p[1] * p[3]; b += p[2] * p[3]; a += p[3];
              }
            const double n (static_cast<double> ((x1 - x0) * (y1 - y0)));
            auto* o (static_cast<unsigned char*> (pixels.pBits) + y * pixels.Pitch + x * 4);
            o[0] = static_cast<unsigned char> (a > 0 ? b / a + 0.5 : 0);
            o[1] = static_cast<unsigned char> (a > 0 ? g / a + 0.5 : 0);
            o[2] = static_cast<unsigned char> (a > 0 ? r / a + 0.5 : 0);
            o[3] = static_cast<unsigned char> (a / n + 0.5);
          }

        image->texture.map->UnlockRect (0);
        return image;
      }

      // Own the veil's image and material, made here rather than taken from
      // any zone, so they are there for every map and survive zone unloads.
      void
      prepare_teleport ()
      {
        if (!teleport_needs_load.exchange (false)) return;
        R_SyncRenderThread ();
        if (teleport_arc != nullptr) Materials::Delete (teleport_arc, true);
        if (teleport_veil != nullptr) Materials::Delete (teleport_veil, true);
        if (spinner_light != nullptr) Materials::Delete (spinner_light, false); // Shares the icon's image.
        if (spinner_icon != nullptr) Materials::Delete (spinner_icon, true);
        if (spinner_lines != nullptr) Materials::Delete (spinner_lines, false); // Borrows the engine's white image.
        teleport_arc = teleport_veil = spinner_icon = spinner_light = nullptr;
        spinner_lines = nullptr;
        teleport_blend.clear ();
        spinner_light_blend.clear ();
        spinner_lines_blend.clear ();

        const auto additive = [] (Material* material, vector<Game::GfxStateBits>& states)
        {
          if (material == nullptr || material->stateBitsTable == nullptr || material->stateBitsCount == 0)
            return false;
          states.assign (material->stateBitsTable, material->stateBitsTable + material->stateBitsCount);
          for (Game::GfxStateBits& bits: states)
            bits.loadBits[0] = spinner_glow_bits (bits.loadBits[0]);
          material->stateBitsTable = states.data ();
          return true;
        };

        // Ring quads must add their glow over the splash logo rather than alpha-blend dark red over it.
        const auto* white = static_cast<const Material*> (Game::DB_FindXAssetHeader (Game::ASSET_TYPE_MATERIAL, "white"));
        if (white != nullptr && white->textureTable != nullptr && white->textureCount > 0)
          spinner_lines = Materials::Create ("zw3_menu_spinner_lines", white->textureTable[0].u.image);
        if (spinner_lines != nullptr && !additive (spinner_lines, spinner_lines_blend))
        {
          Materials::Delete (spinner_lines, false);
          spinner_lines = nullptr;
        }

        // The spinner's skull: decoded and shrunk here (see load_icon ()),
        // or, failing that, the image as the game would load it. Our own
        // copy either way, so it outlives any zone.
        // Made at the size it is drawn at for this resolution (72 at 720p,
        // see draw_spinner ()), so it is neither shrunk nor stretched; made
        // again when the renderer restarts (a resolution change).
        unsigned int icon_size (256);
        {
          int sw, sh;
          float aspect;
          CL_GetScreenDimensions (&sw, &sh, &aspect);
          if (sh > 0)
            icon_size = static_cast<unsigned int> (clamp (static_cast<float> (sh) * 72.0f / 720.0f, 48.0f, 1024.0f));
        }
        Game::GfxImage* skull (load_icon ("skullicon_old", icon_size));
        if (skull == nullptr) skull = Materials::LoadPreviewImage ("skullicon_old");
        if (Game::GfxImage* image = skull)
        {
          spinner_icon = Materials::Create ("zw3_menu_spinner_icon", image);
          if (spinner_icon == nullptr) Materials::DeleteImage (image);
          else
          {
            spinner_light = Materials::Create ("zw3_menu_spinner_light", image);
            if (spinner_light != nullptr && !additive (spinner_light, spinner_light_blend))
            {
              Materials::Delete (spinner_light, false);
              spinner_light = nullptr;
            }
          }
        }
        if (spinner_icon == nullptr)
          LogLine{} << "backdrop: skullicon_old couldn't be loaded; the spinner goes without it";

        // The veil over the unloaded world: black, with a dark red vignette
        // creeping in from the screen's edges. A low-frequency gradient, so
        // it scales cleanly.
        constexpr unsigned int size = 256;
        Game::GfxImage* image (Materials::CreateImage ("zw3_menu_teleport_veil", size, size, 1,
                                                      0x1000003, D3DFMT_A8R8G8B8));
        D3DLOCKED_RECT pixels {};
        if (image->texture.map != nullptr && SUCCEEDED (image->texture.map->LockRect (0, &pixels, nullptr, 0)))
        {
          for (unsigned int y = 0; y < size; ++y)
            for (unsigned int x = 0; x < size; ++x)
            {
              const float u ((static_cast<float> (x) + 0.5f) / size * 2 - 1);
              const float v ((static_cast<float> (y) + 0.5f) / size * 2 - 1);
              // A rounded square, so the red hugs all four edges evenly,
              // strongest in the corners.
              const float r (pow (pow (abs (u), 4.0f) + pow (abs (v), 4.0f), 0.25f));
              const float e (clamp ((r - 0.35f) / 0.75f, 0.0f, 1.0f));
              const float edge (e * e * (3.0f - 2.0f * e));
              auto* pixel (static_cast<unsigned char*> (pixels.pBits) + y * pixels.Pitch + x * 4);
              // Sub-byte ordered dithering avoids visible bands in dark tones.
              const float dither (((x & 1) + 2 * (y & 1)) * 0.25f);
              pixel[0] = static_cast<unsigned char> (2 * edge + dither);  // B
              pixel[1] = static_cast<unsigned char> (3 * edge + dither);  // G
              pixel[2] = static_cast<unsigned char> (62 * edge + dither); // R
              pixel[3] = 255;
            }
          image->texture.map->UnlockRect (0);
          teleport_veil = Materials::Create ("zw3_menu_teleport_veil", image);
        }
        if (teleport_veil == nullptr) Materials::DeleteImage (image);
      }

      // The ui_backdrop switch. Legacy map-name values still enable it;
      // the lobby and the five-map random pool now choose the actual map.
      //
      // Note that it cannot be a row of the declaration table: being a
      // string, its registration copies the value into the engine's script
      // string memory, which is not up yet when the table is registered (see
      // con_filter in console/output.cxx for the details). So we register it
      // after Com_Init through the initialization scheduler. A
      // `seta ui_backdrop` from a config that ran earlier left an external
      // dvar, which the registration adopts. The dvar is never freed, so the
      // handle is kept.
      //
      const dvar_t*
      backdrop_dvar () noexcept
      {
        static const dvar_t* d (nullptr);

        if (d == nullptr)
        {
          // The backdrop is always on: the dvar only reports it, and a
          // value left in a config by an older build (`seta ui_backdrop
          // off`) is overridden and no longer saved.
          d = Dvar_RegisterString (
            "ui_backdrop",
            "random",
            DVAR_ROM,
            "Menu backdrop: always on. Lobbies follow the selected match map.");

          if (d != nullptr)
          {
            const_cast<dvar_t*> (d)->flags &= ~static_cast<unsigned int> (DVAR_ARCHIVE);
            const_cast<dvar_t*> (d)->flags |= DVAR_ROM;

            if (d->current.string == nullptr || string_view (d->current.string) != "random")
              Game::Dvar_SetString (d, "random");
          }

          LogLine{} << "backdrop: ui_backdrop registered as '"
                    << (d != nullptr && d->current.string != nullptr
                        ? d->current.string
                        : "")
                    << "'";
        }

        return d;
      }

      // The ui_backdrop_map dvar: the map the backdrop is showing, empty if
      // none, for a menu to tell whether the map it would show as a picture
      // is already behind it. It is registered on the first frame for the
      // same reason as ui_backdrop.
      //
      const dvar_t*
      shown_map_dvar () noexcept
      {
        static const dvar_t* d (nullptr);

        if (d == nullptr)
          d = Dvar_RegisterString (
            "ui_backdrop_map",
            "",
            DVAR_ROM,
            "The map the backdrop is showing, empty if none.");

        return d;
      }

      // Publish the map shown this frame, if that changed.
      //
      void
      publish_shown_map ()
      {
        const string m (s.shown ? s.map : string ());

        if (m == s.published_map)
          return;

        if (const dvar_t* d = shown_map_dvar ())
          Dvar_SetString (d, m.c_str ());

        LogLine{} << "backdrop: ui_backdrop_map is '" << m << "'";
        s.published_map = m;
      }

      // The backdrop's dvars are read-only: nothing a user types in the
      // console or leaves in a config changes them. They are registered
      // read-only and never archived, and if anything still writes them
      // (a config that ran before they were registered, a `set` that got
      // through), they are put back the next frame.
      //
      bool dvars_registered (false);

      void
      lock_backdrop_dvars ()
      {
        if (!dvars_registered) return; // Not before Com_Init (see backdrop_dvar ()).

        const dvar_t* setting (backdrop_dvar ());
        const dvar_t* shown (shown_map_dvar ());

        for (const dvar_t* d : {setting, shown})
        {
          if (d == nullptr) continue;
          auto* w (const_cast<dvar_t*> (d));
          w->flags = (w->flags & ~static_cast<unsigned int> (DVAR_ARCHIVE)) | DVAR_ROM;
        }

        if (setting != nullptr &&
            (setting->current.string == nullptr || string_view (setting->current.string) != "random"))
        {
          LogLine{} << "backdrop: ui_backdrop is read-only, putting it back";
          Dvar_SetString (setting, "random");
        }

        if (shown != nullptr &&
            (shown->current.string == nullptr || shown->current.string != s.published_map))
        {
          LogLine{} << "backdrop: ui_backdrop_map is read-only, putting it back";
          Dvar_SetString (shown, s.published_map.c_str ());
        }
      }

      // Return true if the menu is open in the front end's UI context.
      //
      bool
      menu_open (string_view name)
      {
        if (Game::uiContext == nullptr) return false;
        const UiContext& c (*Game::uiContext);

        const int n (min (c.openMenuCount,
                          static_cast<int> (size (c.menuStack))));

        for (int i (0); i < n; ++i)
        {
          const menuDef_t* m (c.menuStack[i]);

          if (m != nullptr && m->window.name != nullptr &&
              m->window.name == name)
            return true;
        }

        return false;
      }

      // The most blur any open menu asks for (its blurWorld).
      //
      float
      open_menu_blur ()
      {
        if (Game::uiContext == nullptr) return 0.0f;
        const UiContext& c (*Game::uiContext);

        const int n (min (c.openMenuCount,
                          static_cast<int> (size (c.menuStack))));

        float r (0.0f);
        for (int i (0); i < n; ++i)
          if (const menuDef_t* m = c.menuStack[i])
            r = max (r, m->blurRadius);

        return r;
      }

      void
      prefetch_map (string_view map);

      // The map zones installed: every map's fast file in usermaps/<map>/
      // and anywhere under zone/ (english, dlc, the mod's own dlc...), the
      // ones a lobby can name. Read once, the first time a name is needed
      // that isn't installed as it is.
      //
      const vector<string>&
      installed_maps ()
      {
        static optional<vector<string>> r;

        if (r)
          return *r;

        r.emplace ();

        using install_path_fn = const char* (*) ();
        const char* base (Engine::Bind<install_path_fn> (0x14028E230) ());
        if (base == nullptr)
          return *r;

        const filesystem::path root (base);
        std::error_code ec;

        auto consider = [] (const filesystem::path& f, bool usermap)
        {
          if (f.extension () != ".ff") return;
          const string stem (f.stem ().string ());
          if (stem.ends_with ("_load")) return;

          std::error_code e;
          const bool mp (stem.starts_with ("mp_") || stem.starts_with ("zm_"));
          if (!usermap && !mp &&
              !filesystem::is_regular_file (f.parent_path () / (stem + "_load.ff"), e))
            return;

          string name;
          for (char c: stem) name.push_back (c >= 'A' && c <= 'Z' ? static_cast<char> (c - 'A' + 'a') : c);
          if (ranges::find (*r, name) == r->end ()) r->push_back (move (name));
        };

        for (filesystem::directory_iterator i (root / "usermaps", ec), e; !ec && i != e; i.increment (ec))
        {
          std::error_code e2;
          if (!i->is_directory (e2)) continue;
          const filesystem::path f (i->path () / (i->path ().filename ().string () + ".ff"));
          if (filesystem::is_regular_file (f, e2)) consider (f, true);
        }

        ec.clear ();
        for (filesystem::recursive_directory_iterator i (root / "zone", filesystem::directory_options::skip_permission_denied, ec), e;
             !ec && i != e; i.increment (ec))
        {
          if (i.depth () > 4) { i.disable_recursion_pending (); continue; }
          std::error_code e2;
          if (i->is_regular_file (e2)) consider (i->path (), false);
        }

        // Only what the engine itself finds by name.
        erase_if (*r, [] (const string& m) { return !Engine::ZoneFile (m); });

        LogLine{} << "backdrop: " << r->size () << " map zones are installed";
        return *r;
      }

      // The installed map a lobby's map name stands for.
      //
      // Usually the same name. But the lobby's lists don't always spell a
      // map the way its files are named (Hijacked is mp_hijacked in the
      // arena and mp_box_hijacked in maps.json, and its files are
      // usermaps/mp_box_hjacked), and a map shouldn't lose its backdrop
      // over that. A name that isn't installed is matched against the
      // installed maps by its words (mp_, and the cod5/sh/ss/box style tags
      // aside), then by its consonants (hijacked, hjacked), and taken only
      // if exactly one installed map matches best.
      //
      map<string, string, less<>> lobby_alias; // Installed map -> lobby's name.

      string
      installed_map (string_view requested)
      {
        if (requested.empty ())
          return {};

        static map<string, string, less<>> cache;

        if (auto i (cache.find (requested)); i != cache.end ())
          return i->second;

        string lower;
        for (char c: requested) lower.push_back (c >= 'A' && c <= 'Z' ? static_cast<char> (c - 'A' + 'a') : c);

        string r (lower);

        if (!Engine::ZoneFile (lower))
        {
          static const string_view tags[] {"mp", "zm", "cod5", "waw", "sh", "ss", "box", "bo2", "zombie"};

          auto words = [] (string_view n)
          {
            vector<string> w;
            size_t b (0);
            while (b <= n.size ())
            {
              const size_t e (min (n.find ('_', b), n.size ()));
              const string_view t (n.substr (b, e - b));
              if (!t.empty () && ranges::find (tags, t) == ranges::end (tags)) w.emplace_back (t);
              b = e + 1;
            }
            return w;
          };

          auto consonants = [] (const vector<string>& w)
          {
            string c;
            for (const string& t: w)
              for (char x: t)
                if (string_view ("aeiou").find (x) == string_view::npos) c.push_back (x);
            return c;
          };

          const vector<string> want (words (lower));
          const string want_c (consonants (want));

          int best (0);
          vector<string> found;

          for (const string& m: installed_maps ())
          {
            const vector<string> have (words (m));
            const string have_c (consonants (have));

            int score (0);
            if (!want.empty () && have == want)                               score = 4;
            else if (!want_c.empty () && have_c == want_c)                    score = 3;
            else if (want_c.size () >= 4 && have_c.find (want_c) != string::npos) score = 2;
            else if (have_c.size () >= 4 && want_c.find (have_c) != string::npos) score = 1;

            if (score == 0) continue;

            // Between equals, prefer the one sharing more of the full name
            // (mp_cod5_prototype over mp_prototype_sh for mp_cod5_prototype_waw).
            if (score > best) { best = score; found.clear (); }
            if (score == best) found.push_back (m);
          }

          if (found.size () > 1)
          {
            auto shared = [&lower] (const string& m)
            {
              size_t n (0);
              while (n < m.size () && n < lower.size () && m[n] == lower[n]) ++n;
              return n;
            };
            ranges::sort (found, greater<> {}, shared);
            if (shared (found[0]) == shared (found[1])) found.clear ();
            else found.resize (1);
          }

          if (found.size () == 1)
          {
            r = found.front ();
            LogLine{} << "backdrop: the lobby's " << lower << " is installed as " << r;
          }
          else
            LogLine{} << "backdrop: the lobby's " << lower << " is not installed under that or a similar name";
        }

        if (r != lower)
          lobby_alias.insert_or_assign (r, lower);

        cache.emplace (string (requested), r);
        return r;
      }

      backdrop_request
      requested_backdrop ()
      {
        const auto text = [] (const char* name) -> string_view
        {
          const char* value (Dvar_GetString (name));
          return value != nullptr ? value : "";
        };

        // The lobby's names, as the installed maps they stand for.
        static string lobby_map, vote_map;
        lobby_map = installed_map (text ("ui_mapname"));
        vote_map = installed_map (text ("zwnet_vote_winner_id"));

        const dvar_t* vote (Game::Dvar_FindVar ("zwnet_vote_active"));
        return s.selector.choose ({
          menu_open ("menu_xboxlive_privatelobby") || menu_open ("zwnet_party_lobby"),
          menu_open ("zwnet_matchmaking"), vote != nullptr && vote->current.enabled,
          text ("ui_zwnet_state"), lobby_map, vote_map});
      }

      // Return true if we are in the front end proper: the renderer and the
      // menus are up and there is no connection, not even one being made.
      //
      bool
      front_end () noexcept
      {
        return !headless ()                                      &&
               *renderer_started != 0                            &&
               *ui_started != 0                                  &&
               Game::CL_GetLocalClientConnectionState (0) == CA_DISCONNECTED &&
               !Com_ErrorEntered ();
      }

      // Return true if a match is being joined from the front end: the
      // renderer and the menus are up and the connection is past the front
      // end but short of the match being up (CA_ACTIVE), through the level
      // load.
      //
      bool
      joining () noexcept
      {
        const connstate_t c (Game::CL_GetLocalClientConnectionState (0));

        return !headless ()          &&
               *renderer_started != 0 &&
               *ui_started != 0       &&
               c >= CA_CONNECTING     &&
               c <= CA_PRIMED         &&
               !Com_ErrorEntered ();
      }

      string
      world_name (string_view map)
      {
        const bool multiplayer = map.starts_with ("mp_") || map.starts_with ("zm_") || Maps::IsUserMap (string (map));
        return multiplayer ? format ("maps/mp/{}.d3dbsp", map) : format ("maps/{}.d3dbsp", map);
      }

      // Return the map a world name names, or an empty string if it isn't
      // one of the form world_name() produces.
      //
      string
      map_name (string_view w)
      {
        const string_view p (w.starts_with ("maps/mp/") ? "maps/mp/" : "maps/");
        constexpr string_view x (".d3dbsp");

        if (!w.starts_with (p) || !w.ends_with (x))
          return {};

        return string (w.substr (p.size (), w.size () - p.size () - x.size ()));
      }

      // Return the zone's fast file, or nullopt if it isn't installed.
      //
      optional<filesystem::path>
      zone_file (const string& map)
      {
        return Engine::ZoneFile (map);
      }

      // Measure what there is room for. Only valid while the database is
      // idle, since its thread is what allocates zone memory and entries.
      //
      resources
      measure ()
      {
        resources r {};

        const int64_t z (static_cast<int64_t> (*pmem_high) - *pmem_low);
        r.zone_free = z > 0 ? static_cast<uint64_t> (z) : 0;

        MEMORYSTATUSEX m {};
        m.dwLength = sizeof (m);

        if (GlobalMemoryStatusEx (&m))
          r.physical_free = m.ullAvailPhys;

        // Count the free list, bounded in case it is ever not a list.
        //
        uint32_t n (0);

        for (void* p (*free_entry_head);
             p != nullptr && n != (1u << 20);
             p = *static_cast<void* const*> (p))
          ++n;

        r.entries_free = n;

        return r;
      }

      // Give the animation to the DObj that is about to be created: an
      // animation tree of one, the root blending a single leaf. Return the
      // animation, or nullptr if it isn't loaded.
      //
      const XAnimParts*
      animate (character& c, const char* anim)
      {
        anim_blocks = &c.blocks;

        c.anims = XAnimCreateAnims ("backdrop", 2, &anim_allocate);
        XAnimBlend (c.anims, 0, "root", 1, 1, 0);

        const XAnimParts* r (XAnimCreate (c.anims, 1, anim));

        c.tree = XAnimCreateTree (c.anims, &anim_allocate);

        anim_blocks = nullptr;
        return r;
      }

      // Play the animation the DObj was created with, from its start.
      //
      void
      play (character& c)
      {
        XAnimSetCompleteGoalWeight (c.dobj (),
                                    1,
                                    1.0f /* goal weight */,
                                    0.0f /* goal time */,
                                    1.0f /* rate */,
                                    0,
                                    0,
                                    0);
      }

      // Free the DObj, its animation tree, and the tree's memory, including
      // the anims themselves, which the engine never frees. Safe at any
      // point, including half-built.
      //
      void
      release (character& c)
      {
        if (c.created)
        {
          DObjFree (c.dobj ());
          c.created = false;
        }

        if (c.tree != nullptr)
        {
          anim_blocks = &c.blocks;
          XAnimFreeTree (c.tree, &anim_free);
          anim_blocks = nullptr;

          c.tree = nullptr;
        }

        for (void* p: c.blocks)
          free (p);

        c.blocks.clear ();
        c.anims = nullptr;
      }

      // Free the character.
      //
      void
      release_character ()
      {
        if (!s.cast_member)
          return;

        release (*s.cast_member);
        s.cast_member.reset ();
      }

      // Free the figure. The render thread may still be drawing it, so the
      // caller syncs with it first if a scene with it was submitted.
      //
      void
      release_figure ()
      {
        if (s.figure)
          release (*s.figure);

        s.figure.reset ();
        s.figure_for.reset ();
      }

      // Forget the backdrop, for when the renderer's world is gone.
      //
      const char*
      to_string (phase p) noexcept
      {
        switch (p)
        {
          case phase::idle:    return "idle";
          case phase::loading: return "loading";
          case phase::ready:   return "ready";
          case phase::failed:  return "failed";
        }

        return "?";
      }

      void
      enter (phase p)
      {

        if (s.current != p)
          LogLine{} << "backdrop: " << to_string (s.current) << " -> "
                    << to_string (p);

        s.current = p;
      }

      void
      reset ()
      {
        WorldEffects::Reset ();
        Actors::Reset ();
        release_character ();
        release_figure ();

        enter (phase::idle);
        s.shown_stage = nullptr;
        s.map.clear ();
        s.world.clear ();
        s.front_end_since = -1;
        s.shown = false;
        s.last_direction.reset ();
        s.failure.clear ();
      }

      void
      fail (const string& why)
      {
        LogLine{} << "backdrop: " << why;

        release_character ();
        release_figure ();
        enter (phase::failed);
        s.failure = why;
        s.shown = false;
        s.transition.reset ();
      }

      // Return the figure's placement: where its first model's origin is in
      // the map, its pivot being at the figure's origin, and the yaw's sine
      // and cosine.
      //
      struct placement
      {
        array<float, 3> origin;
        float sine;
        float cosine;
      };

      placement
      place (const BackdropFigure& f)
      {
        const float a (f.yaw * numbers::pi_v<float> / 180);

        placement r {{}, sin (a), cos (a)};

        r.origin = {f.origin[0] - (r.cosine * f.pivot[0] - r.sine * f.pivot[1]),
                    f.origin[1] - (r.sine * f.pivot[0] + r.cosine * f.pivot[1]),
                    f.origin[2] - f.pivot[2]};

        return r;
      }

      // Return true if the two figures are the same models with the same
      // tags hidden playing the same animation, that is, if one's DObj will
      // do for the other.
      //
      bool
      same_figure (const BackdropFigure& x, const BackdropFigure& y)
      {
        return x.models == y.models           &&
               x.hidden_tags == y.hidden_tags &&
               x.animation == y.animation;
      }

      // Put the figure together.
      //
      // The models are the menu's to choose, but they come from the zones,
      // so they are checked the same way as the character's (see below).
      //
      void
      build_figure (const BackdropFigure& f)
      {
        if (f.models.empty ()                       ||
            f.models.size () > max_figure_models    ||
            f.models.front ().tag                   ||
            ranges::any_of (f.models, [] (const BackdropModel& m) { return m.model == nullptr; }))
        {
          LogLine{} << "backdrop: a figure of " << f.models.size ()
                       << " models was asked for, which isn't shown";
          return;
        }

        auto m (make_unique<character> ());

        // The first model, then the rest merged onto its skeleton by bone
        // name, as a head is merged onto a body, or attached to its tag, as
        // a weapon is to a hand. A model shown in part is replaced by one
        // with only those surfaces.
        //
        DObjModel_s ms[max_figure_models] {};
        unsigned int n (0);

        for (const BackdropModel& x: f.models)
        {
          XModel* xm (x.model);

          if (x.surfaces || !x.grafts.empty ())
          {
            // The model's own surfaces, all of them unless it says which,
            // and those it wears of other models.
            //
            vector<uint16_t> own;

            if (x.surfaces)
              own = *x.surfaces;
            else if (xm->numLods > 0)
            {
              for (uint16_t i (0); i != xm->lodInfo[0].numsurfs; ++i)
                own.push_back (i);
            }

            vector<surface_set> ss {surface_set {xm, own}};

            for (const BackdropGraft& g: x.grafts)
              ss.push_back (surface_set {g.model, g.surfaces});

            const bool shown (ranges::any_of (ss, [] (const surface_set& surfaces)
            {
              return !surfaces.surfaces.empty ();
            }));

            const surface_set* bad (nullptr);

            for (const surface_set& surfaces: ss)
            {
              const XModel& sm (*surfaces.model);

              if (sm.numLods <= 0 ||
                  sm.lodInfo[0].surfs == nullptr ||
                  ranges::any_of (surfaces.surfaces, [&sm] (uint16_t i)
                  {
                    return i >= sm.lodInfo[0].numsurfs;
                  }))
              {
                bad = &surfaces;
                break;
              }
            }

            if (!shown || bad != nullptr)
            {
              LogLine{} << "backdrop: a figure shows surfaces that "
                           << (bad != nullptr ? bad->model : xm)->name
                           << " doesn't have, so it isn't shown";
              return;
            }

            try
            {
              m->masked.push_back (mask (*xm, ss));
            }
            catch (const invalid_argument& e)
            {
              LogLine{} << "backdrop: " << e.what () << ", so the "
                           << "figure isn't shown";
              return;
            }

            xm = &m->masked.back ()->model ();
          }

          unsigned int tag (0);

          if (x.tag)
          {
            // As for the character's weapon, the tag exists as a script
            // string once a model with the bone is loaded.
            //
            if ((tag = find_string (x.tag->c_str ())) == 0)
            {
              LogLine{} << "backdrop: a figure is attached to tag "
                           << *x.tag << ", which no model has, so it isn't "
                           << "shown";
              return;
            }
          }

          ms[n] = DObjModel_s {xm, static_cast<uint16_t> (tag), n != 0, {}};
          ++n;
        }

        // The animation, if it has one. Without, or if it isn't loaded, the
        // figure stands in its bind pose.
        //
        if (f.animation && animate (*m, f.animation->c_str ()) == nullptr)
        {
          LogLine{} << "backdrop: the figure's animation "
                       << *f.animation << " isn't loaded, so it is shown "
                       << "in its bind pose";

          release (*m);
        }

        memset (m->obj, 0, sizeof (m->obj));
        DObjCreate (ms, n, m->tree, reinterpret_cast<char*> (m->obj),
                    figure_entity);
        m->created = true;

        if (m->tree != nullptr)
          play (*m);

        // Hide the tags and everything under them, the way the engine hides
        // a weapon's missing attachments (BG_UpdateWeaponHidePartBitsForDObj
        // at 0x14009DEA0): the bits run from the top of each word.
        //
        unsigned int bits[6] {};

        auto hide = [&bits] (unsigned char b)
        {
          bits[b >> 5] |= 0x80000000u >> (b & 31);
        };

        for (const string& t: f.hidden_tags)
        {
          const unsigned int name (find_string (t.c_str ()));

          if (name == 0)
            continue;

          unsigned char b (0xFE);

          if (DObjGetBoneIndex (m->dobj (), name, &b) == 0)
            continue;

          hide (b);

          for (unsigned char c (0xFE);
               DObjGetNextChildBoneIndex (m->dobj (), b, &c) != 0; )
            hide (c);
        }

        DObjSetHidePartBits (m->dobj (), bits);

        // A general entity's pose, as for the character.
        //
        const placement p (place (f));

        memset (&m->pose, 0, sizeof (m->pose));
        // Zeroed pose uses ET_GENERAL, avoiding player controllers.

        for (size_t i (0); i != 3; ++i)
          m->pose.origin[i] = p.origin[i];

        m->pose.angles[1] = f.yaw;

        m->lighting_origin = f.origin;
        m->lighting_origin[2] += 8.0f;

        LogLine{} << format ("backdrop: figure of {} models with {} tags "
                             "hidden, {} bones, at ({}, {}, {}) facing {}",
                             n,
                             f.hidden_tags.size (),
                             m->dobj ()->numBones,
                             f.origin[0], f.origin[1], f.origin[2],
                             f.yaw);

        s.figure = move (m);
      }

      // Let go of the map we loaded, for another one to be loaded in its
      // place: the character, the world, which the renderer has to forget
      // before its zone can go, and the level's zones themselves, which
      // take the clip map with them. The backdrop is idle after this, and
      // the next frame starts over.
      //
      void
      unload ()
      {
        LogLine{} << "backdrop: unloading " << s.map;

        R_SyncRenderThread ();
        WorldEffects::Reset ();
        Actors::Reset ();
        release_character ();
        release_figure ();

        if ((*rendererWorld) != nullptr)
        {
          release_world ();
          (*rendererWorld) = nullptr;
        }

        free_level_zones (s.map, s.zones);
        s.held.clear ();

        reset ();
      }

      // A map that loaded but can't be shown (no clip map, a world the
      // renderer won't take, too many lights...). A map named by the
      // player or a menu fails as before; a random pick is put aside for
      // the session and another one picked, so a random backdrop always
      // ends up showing something whatever is installed.
      //
      void
      retry_or_fail (const string& why)
      {
        constexpr unsigned int max_retries (6);

        // A map a lobby chose that can't be shown gives way to the usual
        // backdrop rather than leaving the lobby without one.
        //
        const bool from_menu (s.chosen_for.starts_with ("menu "));

        if ((!s.random_pick && !from_menu) || s.retries >= max_retries)
        {
          fail (why);
          return;
        }

        LogLine{} << "backdrop: " << why << (from_menu ? "; showing the usual backdrop instead" : "; picking another map");

        s.rejected.push_back (s.map);
        s.selector.reject (s.map);
        ++s.retries;
        s.seed = (s.seed * 6364136223846793005ull + 1442695040888963407ull) | 1;

        const int settled (s.front_end_since);
        unload ();
        s.front_end_since = settled; // Already settled: don't wait again.
      }

      constexpr int reveal_msec (1400);
      constexpr int swap_delay_msec (350);

      void
      lift_curtain () noexcept
      {
        s.transition.reset ();
      }

      // The world collision the director fits its shots against.
      //
      float
      scene_clearance (const vec3& from, const vec3& to)
      {
        return Actors::Clearance (from, to);
      }

      // The mod's own spawn points, from zw/mapedits/spawns.gsc: under each
      // map's case of setSpawns()'s switch, addSpawn ("humans", origin,
      // angles) places the players' start. The backdrop stands the crew on
      // the first of them, where the mod starts a match on that map, rather
      // than on one of the map's own spawns (which zombie maps often leave
      // out of the playable area). Read from the game's files, so an edit
      // to the script takes effect without a rebuild. Cases that fall
      // through (case "a": case "b": ...) share their spawns.
      //
      map<string, vector<spawn>, less<>>
      parse_mod_spawns (string text)
      {
        // Comments out.
        {
          string out;
          out.reserve (text.size ());
          for (size_t i (0); i < text.size (); )
          {
            if (text.compare (i, 2, "//") == 0)
            {
              while (i < text.size () && text[i] != '\n') ++i;
            }
            else if (text.compare (i, 2, "/*") == 0)
            {
              const size_t e (text.find ("*/", i + 2));
              i = e == string::npos ? text.size () : e + 2;
            }
            else
              out += text[i++];
          }
          text = move (out);
        }

        map<string, vector<spawn>, less<>> spawns;
        vector<string> active;
        bool after_case (false);

        static const regex token (
          R"re(case\s*"([^"]+)"\s*:|\bbreak\s*;|\bdefault\s*:|addSpawn\s*\(\s*"([a-z]+)"\s*,\s*\(([^()]*)\)\s*,\s*\(([^()]*)\)\s*\))re",
          regex::optimize);

        const auto numbers = [] (const string& str)
        {
          vector<float> v;
          const char* p (str.c_str ());
          while (*p != '\0')
          {
            char* e;
            const float f (strtof (p, &e));
            if (e == p) { ++p; continue; }
            v.push_back (f);
            p = e;
          }
          return v;
        };

        for (sregex_iterator i (text.begin (), text.end (), token), end; i != end; ++i)
        {
          const smatch& m (*i);
          const string whole (m.str (0));

          if (m[1].matched)
          {
            if (!after_case) active.clear ();
            active.push_back (m[1].str ());
            after_case = true;
            continue;
          }

          after_case = false;

          if (whole.starts_with ("break") || whole.starts_with ("default"))
          {
            active.clear ();
            continue;
          }

          if (m[2].str () != "humans") continue;

          const vector<float> o (numbers (m[3].str ())), a (numbers (m[4].str ()));
          if (o.size () != 3 || a.size () < 2) continue;

          for (const string& label: active)
            spawns[label].push_back (spawn {{o[0], o[1], o[2]}, a[1]});
        }

        return spawns;
      }

      optional<spawn>
      mod_spawn (const string& name)
      {
        static optional<map<string, vector<spawn>, less<>>> table;

        if (!table)
        {
          FileSystem::File file ("zw/mapedits/spawns.gsc");
          if (!file.Exists ())
          {
            LogLine{} << "backdrop: zw/mapedits/spawns.gsc not found; using the maps' own spawns";
            table.emplace ();
          }
          else
          {
            try { table = parse_mod_spawns (file.GetBuffer ()); }
            catch (const exception& e)
            {
              LogLine{} << "backdrop: couldn't read zw/mapedits/spawns.gsc: " << e.what ();
              table.emplace ();
            }
            LogLine{} << "backdrop: the mod's spawns cover " << table->size () << " maps";
          }
        }

        const auto i (ranges::find_if (*table, [&name] (const auto& e) { return I_stricmp (e.first.c_str (), name.c_str ()) == 0; }));
        if (i == table->end () || i->second.empty ()) return nullopt;
        return i->second.front ();
      }

      // Plan the map's cinematography. Needs the clip map, which finish()
      // has set up by the time it calls this.
      //
      void
      plan_scene ()
      {
        // Every map gets the same treatment: the crew posing together,
        // filmed from the front (see BackdropData::lineup_script).
        //
        scene_script script (lineup_script (s.seed ^ std::hash<string> {} (s.map)));

        // Keep the camera inside the rendered world, a little in from its
        // edges. A world without usable bounds leaves it unbounded.
        //
        s.scene.unbound ();

        if (const GfxWorld* w = *rendererWorld;
            w != nullptr && w->bounds.halfSize[0] > 64.0f &&
            w->bounds.halfSize[1] > 64.0f && w->bounds.halfSize[2] > 32.0f)
        {
          const Game::Bounds& b (w->bounds);
          vec3 lo, hi;

          for (size_t i (0); i != 3; ++i)
          {
            const float inset (min (b.halfSize[i] * 0.5f, 24.0f));
            lo[i] = b.midPoint[i] - b.halfSize[i] + inset;
            hi[i] = b.midPoint[i] + b.halfSize[i] - inset;
          }

          s.scene.bound (lo, hi);
        }

        s.scene.plan (s.standing, move (script), clearance (&scene_clearance));
        s.standing = s.scene.subject ();
        Actors::SetLineup (true, s.scene.room ());

        LogLine{} << format ("backdrop: filming {} at ({:.0f}, {:.0f}, {:.0f}) facing {:.0f}, {:.0f}% of the room wanted",
                             s.map,
                             s.standing.origin[0], s.standing.origin[1], s.standing.origin[2],
                             s.standing.yaw, s.scene.room () * 100.0f);
      }

      // The maps a random backdrop may be: the curated stock ones, every map
      // of the lobby's map list, and the custom maps found installed. Which
      // of them are installed and fit is decided when one is picked.
      //
      // Start reading a map's files into the system's file cache in the
      // background, at low I/O priority, so that loading it afterwards
      // doesn't wait on the disk. Asking again is cheap: the prefetcher
      // skips files it has queued or read in the last minute.
      //
      void
      prefetch_map (string_view map)
      {
        const string m (map);

        if (optional<filesystem::path> f = zone_file (m))
          FastFiles::PrefetchPath (*f);

        Maps::PrefetchMapAssets (m);
      }

      // The maps the backdrop shows when no lobby has chosen one: the
      // classic zombies maps. In a lobby it shows the lobby's map.
      //
      vector<const stage*>
      random_pool ()
      {
        vector<const stage*> r;

        for (string_view m: default_backdrop_maps)
          if (const stage* st = find_stage (m))
            r.push_back (st);

        return r;
      }

      // Pick and request the map.
      //
      void
      start (const selection& sel)
      {
        // Cover the load, the first one too, so the map fades in out of the
        // loading screen when it is up (see finish ()) rather than the
        // screen dropping away at once.
        if (!s.transition.active ())
        {
          s.transition.hold (Sys_Milliseconds ());
        }

        s.random_pick = sel.kind == selection_kind::random || s.chosen_for.starts_with ("random");
        const string held (exchange (s.held, {}));

        // A resident level may only be reused when it is the requested map.
        // Otherwise free it before loading another clip map into the engine.
        //
        // CM_Unload leaves the old name pointing into the released zone. Check
        // the remaining clip-map state before reading that string at all.
        //
        const char* resident_world (loaded_clip_map_name (cm));
        if (resident_world != nullptr || (!held.empty () && DB_IsFileLoaded (held.c_str ())))
        {
          const string m (!held.empty () ? held :
                          (!s.resident_level.empty () && DB_IsFileLoaded (s.resident_level.c_str ())
                           ? s.resident_level : map_name (resident_world)));
          const stage* st (find_stage (m));

          if (resident_world != nullptr && st != nullptr && sel.kind == selection_kind::map && sel.stage == st && DB_IsFileLoaded (st->map))
          {
            s.shown_stage = st;
            s.map = st->map;
            s.world = resident_world;
            enter (phase::loading);
            s.loading_since = Sys_Milliseconds ();
            LogLine{} << "backdrop: using the requested resident " << s.map;
            return;
          }

          LogLine{} << "backdrop: freeing resident " << m << " for the requested backdrop";
          const int settled (s.front_end_since);
          s.map = m;
          unload ();
          Sys_SyncDatabase ();
          s.front_end_since = settled;
        }

        // Choose among the stages that are installed.
        //
        vector<const stage*> cs;

        if (sel.kind == selection_kind::map)
          cs.push_back (sel.stage);
        else
        {
          cs = random_pool ();
          erase_if (cs, [] (const stage* st)
          {
            return ranges::find (s.rejected, string (st->map)) != s.rejected.end ();
          });
        }

        vector<const stage*> installed;
        vector<uint64_t> sizes;

        for (const stage* st: cs)
        {
          if (optional<filesystem::path> f = zone_file (st->map))
          {
            error_code ec;
            const uintmax_t n (filesystem::file_size (*f, ec));

            if (!ec)
            {
              installed.push_back (st);
              sizes.push_back (n);

              LogLine{} << format ("backdrop:   {} at {} ({} MiB)",
                                   st->map,
                                   f->string (),
                                   n / (1024 * 1024));
            }
            else
              LogLine{} << "backdrop:   " << st->map << " at " << f->string ()
                        << " is unreadable: " << ec.message ();
          }
          else
            LogLine{} << "backdrop:   " << st->map << " is not installed";
        }

        LogLine{} << "backdrop: " << installed.size () << " of " << cs.size ()
                  << " stages are installed, seed " << s.seed;

        // A lobby's map that can't be shown here is passed over for the
        // usual backdrop (show() drops directions to rejected maps).
        //
        auto reject_selection = [&sel] ()
        {
          if (sel.kind == selection_kind::map && sel.stage != nullptr &&
              (s.random_pick || s.chosen_for.starts_with ("menu ")))
          {
            s.rejected.emplace_back (sel.stage->map);
            s.selector.reject (sel.stage->map);
          }
        };

        if (installed.empty ())
        {
          reject_selection ();
          fail (sel.kind == selection_kind::map
                ? format ("{} is not installed", sel.stage->map)
                : string ("no backdrop stage is installed"));
          return;
        }

        // With a hundred maps to pick from, some won't fit beside the menus.
        // Pick among those that do rather than refuse the one picked.
        //
        if (sel.kind != selection_kind::map && installed.size () > 1)
        {
          uint64_t extra_size (0);

          for (const string& n: s.zones)
            if (optional<filesystem::path> f = zone_file (n))
            {
              error_code ec;
              const uintmax_t z (filesystem::file_size (*f, ec));
              if (!ec) extra_size += z;
            }

          const resources room (measure ());
          vector<const stage*> fitting;
          vector<uint64_t> fitting_sizes;

          for (size_t i (0); i != installed.size (); ++i)
            if (!shortfall (room, require (sizes[i] + extra_size)))
            {
              fitting.push_back (installed[i]);
              fitting_sizes.push_back (sizes[i]);
            }

          LogLine{} << "backdrop: " << fitting.size () << " of "
                    << installed.size () << " installed stages fit";

          if (!fitting.empty ())
          {
            installed = move (fitting);
            sizes = move (fitting_sizes);
          }
        }

        const stage& st (pick_stage (installed, s.seed));
        uint64_t file_size (
          sizes[static_cast<size_t> (ranges::find (installed, &st) -
                                     installed.begin ())]);

        // The zones that go with every map, those installed, which count
        // toward what the map needs as if they were part of it.
        //
        vector<const string*> extra;

        for (const string& n: s.zones)
        {
          if (optional<filesystem::path> f = zone_file (n))
          {
            error_code ec;
            const uintmax_t z (filesystem::file_size (*f, ec));

            if (!ec)
            {
              extra.push_back (&n);
              file_size += z;
            }
          }
          else
            LogLine{} << "backdrop:   " << n << " is not installed";
        }

        // Make sure it fits (see backdrop/budget.hxx).
        //
        const resources r (measure ());
        const requirement q (require (file_size));

        LogLine{} << format ("backdrop: {} is {} MiB on disk; {} MiB of zone "
                             "memory, {} MiB of physical memory, and {} asset "
                             "entries are free, {} MiB, {} MiB, and {} are "
                             "needed",
                             st.map,
                             file_size / (1024 * 1024),
                             r.zone_free / (1024 * 1024),
                             r.physical_free / (1024 * 1024),
                             r.entries_free,
                             q.zone / (1024 * 1024),
                             q.physical / (1024 * 1024),
                             q.entries);

        if (optional<string> why = shortfall (r, q))
        {
          reject_selection ();
          fail (format ("not loading {}: {}", st.map, *why));
          return;
        }

        s.shown_stage = &st;
        s.map = st.map;
        s.world = world_name (s.map);
        s.zone_free_before = static_cast<int64_t> (r.zone_free);
        s.entries_free_before = r.entries_free;
        s.zone_estimate = q.zone;

        // Ask for it without waiting: a map takes a while to load and the
        // menus keep running meanwhile. The name has to outlive the load,
        // which s.map does.
        //
        vector<XZoneInfo> zs {XZoneInfo {s.map.c_str (), zone_flag_level, 0}};

        for (const string* n: extra)
          zs.push_back (XZoneInfo {n->c_str (), zone_flag_level, 0});

        Engine::LoadMap (zs.data (), static_cast<unsigned int> (zs.size ()), 0);

        enter (phase::loading);
        s.loading_since = Sys_Milliseconds ();

        LogLine{} << "backdrop: loading " << s.map;

        for (const string* n: extra)
          LogLine{} << "backdrop: loading " << *n << " with it";
      }

      // Finish once the database is done, then hand the world over and
      // build the scene.
      //
      void
      finish (bool spread = true)
      {
        // Don't ask about the zone while the database is still working:
        // DB_IsFileLoaded waits for it if it is. Give the request a frame
        // to reach the database thread, too.
        //
        const int elapsed (Sys_Milliseconds () - s.loading_since);

        if (elapsed < 50 || !Sys_IsDatabaseReady2 ())
        {
          if (elapsed - s.loading_reported >= 2000)
          {
            LogLine{} << "backdrop: still loading " << s.map << " after "
                      << elapsed << " ms";
            s.loading_reported = elapsed;
          }

          return;
        }

        LogLine{} << "backdrop: database idle " << elapsed << " ms after "
                  << "asking for " << s.map;
        s.loading_reported = 0;

        if (!DB_IsFileLoaded (s.map.c_str ()))
        {
          retry_or_fail (format ("{} did not load", s.map));
          return;
        }

        if (s.zone_estimate != 0)
        {
          const resources r (measure ());

          LogLine{} << format (
            "backdrop: {} took {} MiB of zone memory (estimated at most {} "
            "MiB) and {} asset entries; {} MiB and {} entries remain",
            s.map,
            (s.zone_free_before - static_cast<int64_t> (r.zone_free)) /
              (1024 * 1024),
            s.zone_estimate / (1024 * 1024),
            static_cast<int64_t> (s.entries_free_before) - r.entries_free,
            r.zone_free / (1024 * 1024),
            r.entries_free);

          s.zone_estimate = 0;
        }

        // The clip map is the zone's (its pool is the cm global itself), and
        // the renderer reads the map's entities from it every scene, to
        // find the stage the view is in.
        //
        // A map's zone may name its world differently from the zone (a
        // shared base map, a converted campaign level): take the world the
        // zone actually brought.
        //
        const char* loaded_world (loaded_clip_map_name (cm));
        if (loaded_world != nullptr && s.world != loaded_world &&
            string_view (loaded_world).ends_with (".d3dbsp") &&
            cm->mapEnts->entityString != nullptr)
        {
          LogLine{} << "backdrop: " << s.map << "'s world is " << loaded_world;
          s.world = loaded_world;
        }

        if (loaded_world == nullptr || s.world != loaded_world ||
            cm->mapEnts->entityString == nullptr)
        {
          retry_or_fail (format ("{} loaded without its clip map", s.map));
          return;
        }

        // Find where the character stands.
        //
        try
        {
          optional<spawn> sp (mod_spawn (s.map));

          // The mod's spawns are keyed by the lobby's name for a map, which
          // may not be its files' (see installed_map ()).
          if (!sp)
            if (auto a (lobby_alias.find (s.map)); a != lobby_alias.end ())
              sp = mod_spawn (a->second);

          if (sp)
            LogLine{} << format ("backdrop: standing on the mod's spawn for {} at ({:.0f}, {:.0f}, {:.0f})",
                                 s.map, sp->origin[0], sp->origin[1], sp->origin[2]);
          else
            sp = find_spawn (cm->mapEnts->entityString,
                             static_cast<size_t> (std::hash<string>{}(s.map)));

          if (!sp)
          {
            retry_or_fail (format ("{} has no spawn to stand on", s.map));
            return;
          }

          s.standing = *sp;
        }
        catch (const invalid_argument& e)
        {
          retry_or_fail (format ("{} has an invalid entity string: {}", s.map,
                        e.what ()));
          return;
        }

        // Hand the world to the renderer, as CG_Init does once a map's zone
        // is in. It is the renderer's from now until R_Shutdown, which every
        // level load and renderer restart goes through.
        //
        LogLine{} << format ("backdrop: clip map {}, {} entity characters, "
                             "{} stages",
                             cm->name,
                             cm->mapEnts->numEntityChars,
                             static_cast<int> (cm->mapEnts->stageCount));

        if ((*rendererWorld) == nullptr)
        {
          // Set the collision side up first, as a level load does before
          // the world. The renderer lights the world's static models by
          // tracing through the clip map, and the traces keep their check
          // counts in per-thread arrays that CM_LoadMap allocates (on the
          // hunk, which the next level load clears and reallocates). Without
          // them the first trace reads through a null array.
          //
          int checksum (0);
          CM_LoadMap (s.world.c_str (), &checksum);

          LogLine{} << format ("backdrop: collision for {} set up, checksum "
                               "{:#x}",
                               s.world,
                               static_cast<uint32_t> (checksum));

          LogLine{} << "backdrop: handing " << s.world << " to the renderer";
          LoadWorld (s.world.c_str ());

          // Build the scene on the next frame: handing the world over is the
          // longest step on the main thread, and the menus get a frame in
          // between rather than one long stall.
          //
          if (spread && (*rendererWorld) != nullptr && (*rendererWorld)->name != nullptr &&
              s.world == (*rendererWorld)->name)
            return;
        }
        else
          LogLine{} << "backdrop: the renderer already has "
                    << ((*rendererWorld)->name != nullptr ? (*rendererWorld)->name : "?");

        if ((*rendererWorld) == nullptr || (*rendererWorld)->name == nullptr ||
            s.world != (*rendererWorld)->name)
        {
          retry_or_fail (format ("the renderer did not take {}'s world", s.map));
          return;
        }

        if ((*rendererWorld)->primaryLightCount > 248)
        {
          retry_or_fail ("world exceeds the renderer's primary-light limit");
          return;
        }

        if (!s.refdef)
          s.refdef = make_unique<refdef_t> ();

        memset (s.refdef.get (), 0, sizeof (refdef_t));
        init_primary_lights (s.refdef->primaryLights);

        WorldEffects::Load (s.map);

        plan_scene ();
        Actors::Prepare ();
        s.retries = 0;
        if (s.random_pick) s.selector.remember_default (s.map);

        enter (phase::ready);
        s.start_time = s.last_time = s.heartbeat = Sys_Milliseconds ();
        s.frames = 0;
        s.hidden = 0;

        s.transition.open (s.start_time);

        LogLine{} << format ("backdrop: showing {} from ({}, {}, {}) facing "
                             "{}",
                             s.map,
                             s.standing.origin[0],
                             s.standing.origin[1],
                             s.standing.origin[2],
                             s.standing.yaw);
      }

      // Render the scene, as CG_DrawActiveFrame and CG_DrawActive do for a
      // match, less everything a match has and we don't: entities other
      // than ours, effects, the view model, and the player's view.
      //
      void
      render (const SceneView* directed,
              const BackdropFigure* figure)
      {
        if ((*rendererWorld) == nullptr)
        {
          // The renderer let go of the world without us being told, which
          // R_Shutdown would have. Start over rather than draw nothing.
          //
          LogLine{} << "backdrop: the renderer has no world, starting over";
          reset ();
          return;
        }

        const int now (Sys_Milliseconds ());
        const float dt (clamp (static_cast<float> (now - s.last_time) / 1000.0f,
                               0.0f,
                               0.1f));
        s.last_time = now;

        int w, h;
        float aspect;
        CL_GetScreenDimensions (&w, &h, &aspect);

        if (w <= 0 || h <= 0)
        {
          if (!s.bad_screen)
            LogLine{} << "backdrop: screen is " << w << 'x' << h
                         << ", not rendering until it has a size";

          s.bad_screen = true;
          return;
        }

        s.bad_screen = false;

        // Keep the map's primary lights and the backdrop-only exposure grade
        // prepared by finish(). Depth of field is configured below.
        //
        refdef_t& r (*s.refdef);

        r.x = 0;
        r.y = 0;
        r.width = static_cast<unsigned int> (w);
        r.height = static_cast<unsigned int> (h);

        if (directed != nullptr)
        {
          const SceneView& v (*directed);

          r.view.tanHalfFovX = v.tan_half_fov_x;
          r.view.tanHalfFovY = v.tan_half_fov_y;

          r.view.org[0] = v.origin.x;
          r.view.org[1] = v.origin.y;
          r.view.org[2] = v.origin.z;

          for (size_t i (0); i != 3; ++i)
          {
            r.view.axis[i][0] = v.axis[i].x;
            r.view.axis[i][1] = v.axis[i].y;
            r.view.axis[i][2] = v.axis[i].z;
          }
        }
        else
        {
          const double seconds ((now - s.start_time) / 1000.0);
          const clearance clear (&scene_clearance);

          // Keep the crew where they actually stand in the frame, wherever
          // the floor and the room put them (see director::track).
          //
          if (const optional<array<float, 3>> c = Actors::CrewCenter ())
            s.scene.track (*c);

          const view v (s.scene.at (seconds,
                                    static_cast<float> (w) / static_cast<float> (h),
                                    &clear));

          r.view.tanHalfFovX = v.fov.x;
          r.view.tanHalfFovY = v.fov.y;

          for (size_t i (0); i != 3; ++i)
          {
            r.view.org[i] = v.origin[i];

            for (size_t j (0); j != 3; ++j)
              r.view.axis[i][j] = v.axis[i][j];
          }
        }

        r.view.zNear = z_near;

        // Push the near plane out toward the crew, so the leaves, grass and
        // bushes the camera stands among (foliage has no collision for it to
        // avoid) are clipped away instead of filling the frame.
        //
        if (directed == nullptr)
          if (const optional<array<float, 3>> c = Actors::CrewCenter ())
          {
            const float d (static_cast<float> (hypot (r.view.org[0] - (*c)[0], hypot (r.view.org[1] - (*c)[1], r.view.org[2] - (*c)[2]))));
            r.view.zNear = clamp (d * 0.32f, z_near, 110.0f);
          }
        r.time = now;

        // A popup that blurs the world in a match blurs the backdrop too.
        //
        // But only as much as an open menu asks for: the UI's blur output
        // (UiContext::blurRadiusOut) is only ever raised by the menus that
        // blur, and after a vid_restart (which an audio device change also
        // causes) it can be left at a stale value with no such menu open,
        // which blurred the backdrop for good.
        //
        r.blurRadius = min (CL_GetMenuBlurRadius (0), open_menu_blur ());

        // Reveal the scene by bringing it into focus over its first second
        // or so, rather than popping it in over the still image.
        //
        if (const int shown_for (now - s.start_time); shown_for < reveal_msec)
        {
          const float t (static_cast<float> (shown_for) / reveal_msec);
          r.blurRadius = max (r.blurRadius, 6.0f * (1.0f - t * t * (3.0f - 2.0f * t)));
        }

        // Blur what lies behind a figure that asks for it: from just past
        // its farthest reach along the view, fading in over a few of its
        // sizes.
        //
        r.dof = {};

        if (directed != nullptr            &&
            figure != nullptr              &&
            figure->background_blur > 0    &&
            !figure->models.empty ())
        {
          const float* f (r.view.axis[0]);
          const array<float, 3>& o (figure->origin);

          const float d ((o[0] - r.view.org[0]) * f[0] +
                         (o[1] - r.view.org[1]) * f[1] +
                         (o[2] - r.view.org[2]) * f[2]);

          const float reach (max (figure->models.front ().model->radius,
                                  1.0f));

          // Note that the engine takes the near blur to be at least 4 and
          // otherwise ignores the depth of field altogether, so the near
          // blur is there but over the first unit only, well short of the
          // figure.
          //
          r.dof.nearStart = 0;
          r.dof.nearEnd = 1;
          r.dof.nearBlur = 4;
          r.dof.farStart = d + reach;
          r.dof.farEnd = d + reach * 4;
          r.dof.farBlur = figure->background_blur;
        }

        // Light the crew a little brighter than the map would, so they read
        // in dark maps as well as lit ones.
        //
        r.charPrimaryLightScale.diffuseScale = 1.8f;
        r.charPrimaryLightScale.specularScale = 1.4f;
        r.localClientNum = 0;

        // Build the scene.
        //
        R_ClearScene ();
        R_ClearSceneView (0);
        first_view ();
        CL_ResetSkeletonCache ();
        R_SetLodOrigin (&r, 0);

        // The figure stands where the menu puts it, in its view. Put it
        // together anew when it changed, which a menu does as one of its
        // choices changes, after the render thread is done with the last
        // one.
        //
        if (directed != nullptr && figure != nullptr)
        {
          if (!s.figure_for || !same_figure (*s.figure_for, *figure))
          {
            if (s.figure)
            {
              R_SyncRenderThread ();
              release_figure ();
            }

            build_figure (*figure);
            s.figure_for = *figure;
          }

          if (s.figure)
          {
            character& c (*s.figure);

            if (c.tree != nullptr)
              DObjUpdateClientInfo (c.dobj (), dt, false);

            // The menu may move it without changing what it is.
            //
            const placement p (place (*figure));

            for (size_t i (0); i != 3; ++i)
              c.pose.origin[i] = p.origin[i];

            c.pose.angles[1] = figure->yaw;
            c.lighting_origin = figure->origin;
            c.lighting_origin[2] += 8.0f;

            // The zeroed general pose has no cgame culling state.
            R_AddDObjToScene (c.dobj (),
                              &c.pose,
                              figure_entity,
                              0 /* renderFxFlags */,
                              c.lighting_origin.data (),
                              0.0f /* materialTime */);
          }
        }

        if (directed == nullptr && figure == nullptr) Actors::DrawScene (s.standing, s.map, dt);

        R_EndDObjScene ();
        WorldEffects::Draw (r);

        if (s.frames == 0)
          LogLine{} << format ("backdrop: first scene, {}x{}, tangents {} {}, "
                               "eye ({}, {}, {}), blur {}",
                               w, h,
                               r.view.tanHalfFovX, r.view.tanHalfFovY,
                               r.view.org[0], r.view.org[1], r.view.org[2],
                               r.blurRadius);

        // The scene path calls back into cgame (see cgame_draw_2d () below),
        // which has nothing to draw from in the front end, so it is told
        // that this scene is ours.
        //
        s.in_scene = true;
        render_scene (&r);
        s.in_scene = false;

        if (s.frames == 0)
          LogLine{} << "backdrop: first scene submitted";

        ++s.frames;
        s.shown = true;

        if (now - s.heartbeat >= 10000)
        {
          LogLine{} << format ("backdrop: {} scenes of {} in {} s, {} "
                               "background draws hidden",
                               s.frames,
                               s.map,
                               (now - s.start_time) / 1000,
                               s.hidden);
          s.heartbeat = now;
        }
      }

      // The frame.
      //
      screen_field_fn screen_field (nullptr);

      // Start the actual zone load while the intro is still playing, then
      // finish collision, world and actors before the first menu scene.
      void
      startup_tick ()
      {
        if (!hooksInstalled || s.startup_presented || !Backdrop::IsEnabled () || quitting () ||
            *renderer_started == 0 || *ui_started == 0 || Com_ErrorEntered ()) return;

        const connstate_t connection (Game::CL_GetLocalClientConnectionState (0));
        if (connection != CA_DISCONNECTED && connection != Game::CA_CINEMATIC &&
            connection != Game::CA_LOGO) return;

        if (s.startup_since < 0) s.startup_since = Sys_Milliseconds ();
        if (!FastFiles::Ready ()) return;

        if (s.current == phase::idle)
        {
          prepare_teleport ();
          const selection sel (s.selector.choose ({}).choice);
          if (s.seed == 0) s.seed = random_device {} () | 1;
          s.chosen_for = sel.stage != nullptr ? "random " + string (sel.stage->map) : "random";
          start (sel);
        }
        else if (s.current == phase::loading)
          finish (false);
      }

      // Show the backdrop for this frame, if there is one to show.
      //
      void
      show ()
      {
        s.shown = false;

        // Take the direction the menus just gave, if any, which only holds
        // for this frame.
        //
        optional<BackdropDirection> dir (move (s.direction));
        s.direction.reset ();

        if (!Game::Sys_IsMainThread () || quitting ()) return;

        // A preloaded world belongs to the upcoming menu through the intro.
        const connstate_t connection (Game::CL_GetLocalClientConnectionState (0));
        if (!s.startup_presented &&
            (connection == Game::CA_CINEMATIC || connection == Game::CA_LOGO)) return;

        if (!front_end ())
        {
          // While a match is joined, the scene the front end showed last
          // stays up under the connect screen: while connecting, until the
          // level load shuts the renderer down, and, when the level is the
          // map we show (see level_load()), on through the load until the
          // match is up.
          //
          if (joining () && s.current == phase::ready && (*rendererWorld) != nullptr)
          {
            if (!s.joining)
            {
              LogLine{} << "backdrop: keeping " << s.map
                        << " up while the match is joined";
              s.joining = true;
            }

            const optional<BackdropDirection>& d (s.last_direction);

            render (d && d->view ? &*d->view : nullptr,
                    d && d->figure ? &*d->figure : nullptr);
            return;
          }

          s.joining = false;
          s.remote_updates_logged = false;
          lift_curtain ();

          // Once the match is up, the world is the match's (see
          // match_load_world()), and only the character is ours to let go.
          //
          if (s.current != phase::idle && !joining ())
          {
            LogLine{} << "backdrop: the match is up, letting go of "
                      << s.map;

            R_SyncRenderThread ();
            reset ();
          }

          if (s.in_front_end)
          {
            LogLine{} << format ("backdrop: left the front end (connection "
                                 "state {}, renderer {}, ui {})",
                                 static_cast<int> (
                                   Game::CL_GetLocalClientConnectionState (0)),
                                 static_cast<int> (*renderer_started),
                                 static_cast<int> (*ui_started));
            s.in_front_end = false;
          }

          s.front_end_since = -1;
          return;
        }

        s.joining = false;

        if (!s.in_front_end)
        {
          LogLine{} << "backdrop: in the front end, " << to_string (s.current);
          s.in_front_end = true;
        }

        backdrop_dvar (); // Always on; registered for menus that read it.

        // Only a private lobby or a confirmed matchmaking map can override
        // the session's random backdrop. Preserve directed figures on that map.
        const backdrop_request request (requested_backdrop ());
        s.main_menu_transition = !request.follows_lobby;
        if (!request.follows_lobby)
          dir.reset ();
        else if (!dir || dir->map != request.choice.stage->map)
          dir = BackdropDirection {request.choice.stage->map, nullopt, nullopt};

        // A rejected lobby map falls back to the session's default.
        //
        if (dir && ranges::find (s.rejected, dir->map) != s.rejected.end ())
          dir.reset ();

        if (dir)
        {
          if (const stage* st = find_stage (dir->map))
            dir->map = st->map;
          else
          {
            if (dir->map != s.refused_map)
            {
              LogLine{} << "backdrop: a menu asks for " << dir->map
                           << ", which is not a backdrop stage";
              s.refused_map = dir->map;
            }

            dir.reset ();
          }
        }

        string value;
        selection sel {selection_kind::off};

        if (dir)
        {
          value = "menu " + dir->map;
          sel = selection {selection_kind::map, find_stage (dir->map)};
        }
        else
        {
          sel = s.selector.choose ({}).choice;
          value = sel.stage != nullptr ? "random " + string (sel.stage->map) : "random";
        }

        // Read the requested map ahead during the selection debounce, including
        // the return to the default map when a lobby closes.
        if (sel.stage != nullptr && sel.stage->map != s.map && sel.stage->map != s.prefetched_map)
        {
          prefetch_map (sel.stage->map);
          s.prefetched_map = sel.stage->map;
        }

        if (value != s.last_value)
        {
          LogLine{} << "backdrop: showing for '" << value << "'";
          s.last_value = value;
        }

        if (s.current == phase::failed && value != s.chosen_for)
          unload ();

        if (sel.kind == selection_kind::off)
        {
          lift_curtain ();

          if (s.current == phase::loading && Sys_IsDatabaseReady2 ()) finish ();
          if (s.current == phase::ready || s.current == phase::failed) unload ();
          return;
        }

        const int now (Sys_Milliseconds ());

        if (s.front_end_since < 0)
          s.front_end_since = now;

        if (s.startup_presented && now - s.front_end_since < settle_msec)
        {
          if (!s.settling && s.current == phase::idle)
            LogLine{} << "backdrop: waiting " << settle_msec
                      << " ms for the front end to settle";

          s.settling = true;
          return;
        }

        s.settling = false;

        switch (s.current)
        {
          case phase::idle:
          {
            // Measuring needs the database idle (see measure()).
            //
            if (!Sys_IsDatabaseReady2 ())
            {
              if (!s.database_busy)
                LogLine{} << "backdrop: waiting for the database to be idle";

              s.database_busy = true;
              return;
            }

            if (s.database_busy)
              LogLine{} << "backdrop: database idle";

            s.database_busy = false;
            prepare_teleport ();

            if (s.seed == 0)
              s.seed = random_device {} () | 1;

            s.chosen_for = value;
            start (sel);
            break;
          }
          case phase::loading:
          {
            finish ();
            break;
          }
          case phase::ready:
          {
            // unload() synchronizes the render thread before a map swap.
            //
            // Wait for a lobby's choice to settle before swapping maps:
            // scrolling through the map list shouldn't load every map it
            // passes over.
            //
            if (sel.kind == selection_kind::random || (sel.stage != nullptr && sel.stage->map != s.map))
            {
              const string wanted (sel.stage != nullptr ? sel.stage->map : "random");
              if (s.wanted_map != wanted)
              {
                s.wanted_map = wanted;
                s.wanted_since = now;
              }

              if (now - s.wanted_since < swap_delay_msec)
              {
                render (dir && dir->view ? &*dir->view : nullptr,
                        dir && dir->figure ? &*dir->figure : nullptr);
                s.last_direction = move (dir);
                break;
              }

              // Put a full transport frame on screen before releasing the
              // old world. Keep animating it until the next scene is ready.
              if (!s.transition.closing ())
              {
                s.transition.close (now);
              }

              if (!s.transition.covered (now))
              {
                render (dir && dir->view ? &*dir->view : nullptr,
                        dir && dir->figure ? &*dir->figure : nullptr);
                s.last_direction = move (dir);
                break;
              }

              LogLine{} << "backdrop: asked for '" << value << "' with "
                        << s.map << " loaded, swapping it";
              const int settled (s.front_end_since);
              unload ();
              s.front_end_since = settled; // Already settled: load at once.
              return;
            }

            s.wanted_map.clear (); // Back on the map shown: nothing pending.

            // Asked for the shown map again while it was fading out: fade
            // it back in from wherever the fade had got to.
            //
            if (s.transition.closing ()) s.transition.open (now);

            if (value != s.chosen_for)
            {
              LogLine{} << "backdrop: asked for '" << value << "' with "
                        << s.map << " loaded, which stays until the next "
                        << "time a backdrop is loaded";
              s.chosen_for = value;
            }

            render (dir && dir->view ? &*dir->view : nullptr,
                    dir && dir->figure ? &*dir->figure : nullptr);

            s.last_direction = move (dir);
            if (s.shown) s.startup_presented = true;
            break;
          }
          case phase::failed:
            break;
        }
      }

      // A loading spinner in the middle of the screen, in the veil's red,
      // around the skull: a faint track, a bright head sweeping round it
      // with a tail fading out behind (its length breathing as it goes,
      // tipped with a spark), and an outer ring of three thin arcs turning
      // slowly the other way. It eases in, growing slightly as it fades up. Joined, slanted pieces
      // (R_AddCmdDrawQuadPic) with a soft glow under a thin core, so it is
      // smooth at any resolution; plain white quads, tinted, so nothing is
      // loaded for it.
      //
      void
      draw_spinner (int w, int h, int elapsed, float cover, Material* lines)
      {
        using quad_fn = void (*) (const float (*)[2], const float*, Material*);
        const auto quad4 (Engine::Bind<quad_fn> (0x14001CCE0));

        // A line from a to b, th pixels thick. Wound clockwise on screen,
        // or it is culled.
        const auto line = [&] (float ax, float ay, float bx, float by, float th, float r, float g, float b, float a)
        {
          const float dx (bx - ax), dy (by - ay);
          const float len (sqrt (dx * dx + dy * dy));
          if (lines == nullptr || len < 0.001f || a <= 0.0f) return;
          const float nx (-dy / len * th * 0.5f), ny (dx / len * th * 0.5f);
          const float v[4][2] {{ax - nx, ay - ny}, {bx - nx, by - ny}, {bx + nx, by + ny}, {ax + nx, ay + ny}};
          const float c[4] {r, g, b, clamp (a * cover, 0.0f, 1.0f)};
          quad4 (v, c, lines);
        };

        constexpr float tau (6.28318530718f);
        const float fw (static_cast<float> (w)), fh (static_cast<float> (h));
        const float tt (static_cast<float> (max (0, elapsed)) / 1000.0f);
        const float px (fh / 720.0f);

        // Fade in over its first moment, so it doesn't pop.
        //
        const float shown (clamp (tt / 0.35f, 0.0f, 1.0f));

        const float grow (0.9f + 0.1f * (1.0f - (1.0f - shown) * (1.0f - shown)));
        const float radius (52.0f * px * grow);
        const float cx (fw * 0.5f), cy (fh * 0.5f);

        // The skull at its middle, breathing a little.
        //
        if (spinner_icon != nullptr)
        {
          const auto pic (Engine::Bind<Game::CL_DrawStretchPicPhysical_t> (0x14001BE90));
          const float breath (0.5f + 0.5f * static_cast<float> (sin (tt * 3.0)));
          const float size (72.0f * px * grow * (0.97f + 0.03f * breath));
          const float c[4] {1.0f, 1.0f, 1.0f, clamp (shown * cover, 0.0f, 1.0f)};
          pic (cx - size * 0.5f, cy - size * 0.5f, size, size, 0.0f, 0.0f, 1.0f, 1.0f, c, spinner_icon);

          // Lit up: the image added over itself, and a little larger in red
          // for a glow round it.
          if (spinner_light != nullptr)
          {
            const float lift[4] {1.0f, 0.95f, 0.92f, clamp ((0.55f + 0.2f * breath) * shown * cover, 0.0f, 1.0f)};
            pic (cx - size * 0.5f, cy - size * 0.5f, size, size, 0.0f, 0.0f, 1.0f, 1.0f, lift, spinner_light);
            const float halo (size * 1.12f);
            const float red[4] {1.0f, 0.15f, 0.08f, clamp ((0.30f + 0.2f * breath) * shown * cover, 0.0f, 1.0f)};
            pic (cx - halo * 0.5f, cy - halo * 0.5f, halo, halo, 0.0f, 0.0f, 1.0f, 1.0f, red, spinner_light);
          }
        }
        constexpr int pieces (96);

        const auto at = [&] (float angle, float* x, float* y)
        {
          *x = cx + radius * static_cast<float> (cos (angle));
          *y = cy + radius * static_cast<float> (sin (angle));
        };

        // The track.
        //
        for (int i (0); i != pieces; ++i)
        {
          float ax, ay, bx, by;
          at (tau * static_cast<float> (i) / pieces, &ax, &ay);
          at (tau * static_cast<float> (i + 1) / pieces, &bx, &by);
          line (ax, ay, bx, by, 2.5f * px, 0.55f, 0.06f, 0.05f, 0.40f * shown);
        }

        // The outer ring: three thin arcs, a sixth of a turn each, turning
        // slowly against the head.
        //
        {
          const float outer (radius * 1.3f);
          const float spin (-tt * 0.6f);
          for (int arc (0); arc != 3; ++arc)
          {
            const float start (spin + tau * static_cast<float> (arc) / 3.0f);
            constexpr int steps (24);
            for (int i (0); i != steps; ++i)
            {
              const float a0 (start + tau / 6.0f * static_cast<float> (i) / steps);
              const float a1 (start + tau / 6.0f * static_cast<float> (i + 1) / steps);
              // Fading at both ends of each arc.
              const float m (static_cast<float> (sin (3.14159f * (static_cast<float> (i) + 0.5f) / steps)));
              line (cx + outer * static_cast<float> (cos (a0)), cy + outer * static_cast<float> (sin (a0)),
                    cx + outer * static_cast<float> (cos (a1)), cy + outer * static_cast<float> (sin (a1)),
                    1.6f * px, 0.9f, 0.12f, 0.08f, 0.45f * m * shown);
            }
          }
        }

        // The head, eased so it surges and settles each turn, and its tail,
        // between a quarter and three quarters of a turn long.
        //
        const float turn (tt * 0.9f);
        const float frac (turn - floor (turn));
        const float head (tau * (floor (turn) + frac - 0.12f * static_cast<float> (sin (frac * tau))));
        const float tail (tau * (0.25f + 0.5f * (0.5f + 0.5f * static_cast<float> (sin (tt * 2.2)))));

        for (int pass (0); pass != 2; ++pass)
        {
          const float th ((pass == 0 ? 11.0f : 3.5f) * px);

          for (int i (0); i != pieces; ++i)
          {
            const float f0 (static_cast<float> (i) / pieces), f1 (static_cast<float> (i + 1) / pieces);
            float ax, ay, bx, by;
            at (head - tail * f1, &ax, &ay);
            at (head - tail * f0, &bx, &by);

            const float fade ((1.0f - f0) * (1.0f - f0));
            if (pass == 0) line (ax, ay, bx, by, th, 1.0f, 0.10f, 0.06f, 0.18f * fade * shown);
            else line (ax, ay, bx, by, th, 1.0f, 0.22f + 0.4f * fade, 0.16f + 0.3f * fade, (0.15f + 0.85f * fade) * shown);
          }
        }

        // The spark at the head: a short bright stroke along the ring.
        //
        {
          float ax, ay, bx, by;
          at (head - 0.06f, &ax, &ay);
          at (head + 0.02f, &bx, &by);
          line (ax, ay, bx, by, 14.0f * px, 1.0f, 0.35f, 0.25f, 0.25f * shown);
          line (ax, ay, bx, by, 5.0f * px, 1.0f, 0.85f, 0.8f, 0.95f * shown);
        }
      }

      // Cover only a changing backdrop. A ready scene keeps its own lighting;
      // no permanent black quad is drawn over it.
      void
      draw_transport ()
      {
        const int now (Sys_Milliseconds ());
        if (!front_end ()) lift_curtain ();
        const bool startup (Backdrop::IsStartupLoading ());
        // Hide the overlay when returning to the main menu, but retain it for lobby map loads.
        if (s.main_menu_transition)
        {
          if (s.transition.finished (now)) lift_curtain ();
          return;
        }
        const float cover (startup ? 1.0f : s.transition.amount (now));
        if (cover <= 0.0f)
        {
          if (s.transition.finished (now)) lift_curtain ();
          return;
        }

        // Looked up every frame (a hash lookup): a cached handle would not
        // survive the material system restarting with the renderer.
        //
        Material* const white (Game::Material_RegisterHandle != nullptr
                               ? Game::Material_RegisterHandle ("white", 0)
                               : nullptr);

        int w, h;
        float aspect;
        CL_GetScreenDimensions (&w, &h, &aspect);

        if (white == nullptr || w <= 0 || h <= 0)
          return;

        const auto draw = [w, h] (Material* material, const float* color)
        {
          Engine::Bind<Game::CL_DrawStretchPicPhysical_t> (0x14001BE90) (
            0.0f, 0.0f, static_cast<float> (w), static_cast<float> (h),
            0.0f, 0.0f, 1.0f, 1.0f, color, material);
        };

        const float veil[4] {1.0f, 1.0f, 1.0f, cover};
        if (teleport_veil != nullptr) draw (teleport_veil, veil);
        else
        {
          const float fallback[4] {0.0f, 0.0f, 0.0f, cover};
          draw (white, fallback);
        }

        draw_spinner (w, h, now - s.transition.started_at (s.startup_since), cover, spinner_lines);
        if (s.transition.finished (now)) lift_curtain ();
      }

      void
      draw_screen_field (unsigned int local_client_num, int refresh_ui)
      {
        // screen_field starts the shared UI command list. menu_clear queues
        // the transport inside that list, before UI_RefreshViewport paints.
        screen_field (local_client_num, refresh_ui);
        if (!Game::Sys_IsMainThread ()) return;

        show ();
        publish_shown_map ();
      }

      // The menus are no longer held back while the first map loads: they
      // come up at once over the loading screen (see draw_transport ()),
      // which is a loading indicator in its own right.
      //
      void
      draw_menu_viewport (int local_client)
      {
        Engine::Bind<void (*) (int)> (0x140270C60) (local_client);
      }

      // The renderer is going down, and the world with it, whether for a
      // level load, a renderer restart, or the game exiting.
      //
      void renderer_release_world ()
      {
        R_SyncRenderThread ();
        const string map (s.map);
        optional<BackdropDirection> direction (move (s.last_direction));
        reset ();
        release_world ();
        if (!map.empty () && !quitting ())
        {
          s.held = map;
          s.held_direction = move (direction);
        }
      }

      // Take the level that a level load found loaded back up for the
      // scene, as the front end takes a resident level (see start() and
      // finish()): the renderer is up again by now, with no world.
      //
      void
      resume ()
      {
        const stage* st (find_stage (s.resident_level));

        if (st == nullptr)
          return;

        LogLine{} << "backdrop: taking " << st->map << " back up for the "
                  << "level load";

        s.shown_stage = st;
        s.map = st->map;
        s.world = world_name (s.map);
        s.last_direction = move (s.held_direction);
        s.held_direction.reset ();

        enter (phase::loading);
        s.loading_since = Sys_Milliseconds () - 1000;

        finish (false); // All at once: the level load is waiting on it.
      }

      // CG_Init's call to LoadWorld, which hands the match's world to the
      // renderer, and which the renderer only takes with none. The world it
      // has then is the level's own, which we took back up for the scene
      // (see resume()), so we let go of it and the match takes it again.
      //
      constexpr uintptr_t match_load_world_call (0x1400D6B5A);

      LoadWorld_t match_load_world_original (nullptr);

      __int64
      match_load_world (const char* name)
      {
        // Release our FX before CG_Init creates the match's native FX system.
        WorldEffects::Reset ();
        if ((*rendererWorld) != nullptr)
        {
          LogLine{} << "backdrop: handing " << name << " over to the match";

          R_SyncRenderThread ();
          release_world ();
          (*rendererWorld) = nullptr;
        }

        return match_load_world_original (name);
      }

      // R_BeginRemoteScreenUpdate and R_EndRemoteScreenUpdate.
      //
      // While the main thread is busy for long (a server spawning its game,
      // a zone being waited for), the render thread draws the screen in its
      // stead. Those frames can't skin models: the skinned vertex cache is
      // there for the main thread's frames (R_SkinXModelCmd faults writing
      // to it), so the character can't be in them. Keep the last scene or
      // transport frame during those stalls; a remote frame's clear would
      // otherwise flash black over the transport while the world changes.
      //
      R_BeginRemoteScreenUpdate_t begin_remote_update (nullptr);
      R_EndRemoteScreenUpdate_t end_remote_update (nullptr);

      thread_local vector<bool> remote_updates;

      void
      begin_remote_screen_update ()
      {
        const bool suppressed (Game::Sys_IsMainThread () &&
                               ((s.current == phase::ready && (*rendererWorld) != nullptr && joining ()) ||
                                (front_end () && (s.transition.active () || Backdrop::IsStartupLoading ()))));
        remote_updates.push_back (suppressed);
        if (!suppressed) begin_remote_update ();
      }

      void end_remote_screen_update ()
      {
        const bool suppressed (!remote_updates.empty () && remote_updates.back ());
        if (!remote_updates.empty ()) remote_updates.pop_back ();
        if (!suppressed) end_remote_update ();
      }

      // DB_LoadLevelXAssets, through its callers.
      //
      using level_load_fn = void (*) (const char*);

      level_load_fn level_load_original = nullptr;

      // Load the level, unless it is the map we kept, which is in already.
      //
      // The engine skips the load of a map that is loaded, as it does when a
      // match restarts its map, and so the front end's menus, which a load
      // frees first, stay loaded through the match; the return to the front
      // end then finds them in. Freeing them by themselves would leave the
      // defaults of those the menus still ask for by name, whose names were
      // in the zones, for the renderer's next sort of the materials to read.
      // And the party moves on once the level is a quarter in, as
      // DB_GetLoadedFraction has it, which says nothing is with nothing
      // loading, so we answer for it (see level_fraction()).
      //
      // Any other map frees ours first, the way a level load frees a level
      // (see free_level_zones()): the load screen's zone came in while ours
      // was there and points at its assets, and it stays loaded through the
      // level load.
      //
      void
      level_load (const char* map)
      {
        s.resident_level.clear ();

        if (quitting ())
        {
          s.held.clear ();
          level_load_original (map);
          return;
        }

        if (!s.held.empty ())
        {
          const string h (move (s.held));
          s.held.clear ();

          if (map != nullptr                   &&
              I_stricmp (h.c_str (), map) == 0 &&
              DB_IsFileLoaded (map))
          {
            LogLine{} << "backdrop: the level load is for " << h << ", which "
                      << "is loaded already";

            s.resident_level = h;
            level_load_original (map);

            resume ();
            return;
          }

          LogLine{} << "backdrop: the level load is for "
                    << (map != nullptr ? map : "?") << ", freeing " << h;

          free_level_zones (h, s.zones);
        }

        level_load_original (map);
      }

      // DB_GetLoadedFraction, for the level load's progress: all of it for
      // a level that was in already.
      //
      static float (*party_fraction_original) ();
      static float (*loadbar_fraction_original) ();

      float
      level_fraction (float f)
      {
        return f == 0.0f && !s.resident_level.empty () ? 1.0f : f;
      }

      float party_fraction () { return level_fraction (party_fraction_original ()); }
      float loadbar_fraction () { return level_fraction (loadbar_fraction_original ()); }

      // A zone load that frees the level's zones.
      //
      // Every engine path that does goes through R_Shutdown first, which
      // has already released everything. But the character points into
      // those zones, so if one ever doesn't, the character goes before the
      // zones do, and the world, which we can't take back from the
      // renderer, is at least named in the log before the engine's own
      // error about it.
      //
      void track_level_zones (int allocated, int freed);

      void
      zone_load (const XZoneInfo* zones, int count, int)
      {
        int freed (0);
        int allocated (0);

        for (int i (0); i != count; ++i)
        {
          freed |= zones[i].freeFlags;
          allocated |= zones[i].allocFlags;
        }

        track_level_zones (allocated, freed);



        if ((freed & zone_flag_level) != 0)
          s.resident_level.clear ();

        if ((freed & zone_flag_level) == 0 || s.current == phase::idle)
          return;

        WorldEffects::Reset ();

        LogLine{} << format ("backdrop: a zone load frees flags {:#x} while "
                             "{}, releasing the character",
                             freed,
                             to_string (s.current));

        if (s.cast_member || s.figure || Actors::IsReady ())
        {
          R_SyncRenderThread ();
          Actors::Reset ();
          release_character ();
          release_figure ();
        }

        if ((*rendererWorld) != nullptr && s.current == phase::ready)
          LogLine{} << "backdrop: level zones are being freed while "
                     << s.map << "'s world is still the renderer's";

        enter (phase::idle);
        s.shown = false;
      }

      R_AddCmdClearScreen_t clear_screen (nullptr);

      // The clear behind the menus.
      //
      // SCR_DrawScreenField issues it on the frame's shared command list,
      // which the render thread runs after the frame's scenes. So it would
      // clear our scene away right after drawing it. While the stage is up,
      // the scene covers the whole screen anyway, so the clear goes. Behind
      // the connect screen, the stage is up only while a match is joined.
      //
      void
      menu_clear (int which, const float* color, float depth,
                  unsigned char total)
      {
        // SCR_DrawScreenField has started its shared command list by now.
        // Drawing before screen_field would leave the transport outside it.
        if (front_end () && (s.transition.active () || Backdrop::IsStartupLoading ()))
        {
          if (Game::Sys_IsMainThread ()) draw_transport ();
          // The normal title background remains visible until its first map scene is drawn.
          if (!s.shown && s.main_menu_transition)
            clear_screen (which, color, depth, total);
          return;
        }

        if (s.current == phase::ready && (*rendererWorld) != nullptr &&
            (Backdrop::IsEnabled () && (front_end () || joining ())))
          return;

        clear_screen (which, color, depth, total);
      }

      // SCR_DrawScreenField, disconnected with no fullscreen menu open,
      // clears the screen and stops every sound (SND_StopSounds (0) at
      // 0x1401019A6), every frame: with nothing on screen, nothing should
      // be heard. With the backdrop up, the front end's menus are drawn
      // over the scene and the menu music is meant to play, but it was
      // stopped as soon as it started, so it was only heard while a map was
      // loading. In the front end with the backdrop on, the stop goes.
      //
      using stop_sounds_fn = void (*) (int);
      stop_sounds_fn stop_sounds (nullptr);
      stop_sounds_fn stop_stream (nullptr);
      std::atomic<int> menu_music (0);

      // Zone frees in progress (see Backdrop::BeginZoneFree ()), and
      // whether the one in progress frees only level zones.
      std::atomic<int> zone_frees (0);
      std::atomic<bool> level_only_free (false);

      int
      music_kind (const char* name)
      {
        if (name == nullptr) return 0;
        if (string_view (name) == "music_mainmenu_mp") return 1;
        if (string_view (name) == "music_mainmenu") return 2;
        return 0;
      }

      // The menu tracks as the front end's own zones (ui_mp, common_mp)
      // define them, as opposed to a backdrop map's copy of the same alias,
      // which overrides them while the map is loaded and goes with it.
      //
      // The music is always started from these, so the track that plays
      // lives in memory a backdrop map change never frees, and it can keep
      // playing straight through the change instead of being stopped and
      // started over. They are looked up only while no level zone is
      // loaded (nothing can override them then), and forgotten whenever a
      // zone other than a level's is freed (the front end's own may be).
      //
      using find_alias_fn = const Game::snd_alias_t* (*) (const char*);
      using play_alias_fn = int (*) (int, const Game::snd_alias_t*, int);

      std::atomic<bool> level_zones (false);
      std::atomic<const Game::snd_alias_t*> resident_music[3] {};

      // The alias the asset table holds for a name right now, without
      // waiting for loads in progress (DB_FindXAssetHeader waits for a
      // missing asset, and this runs as a zone load starts). It reads the
      // table under the database's own reader protocol, as
      // DB_FindXAssetHeader does (0x14012D6F6).
      //
      const Game::snd_alias_t*
      current_alias (const char* name)
      {
        auto* readers (reinterpret_cast<volatile long*> (Utils::Hook::Rebase (0x14151D630)));
        auto* writing (reinterpret_cast<volatile long*> (Utils::Hook::Rebase (0x14151D634)));

        InterlockedIncrement (readers);
        while (*writing != 0) Sleep (1);
        const auto* entry (Game::DB_FindXAssetEntry (Game::ASSET_TYPE_SOUND, name));
        const Game::snd_alias_list_t* list (entry != nullptr ? entry->asset.header.sound : nullptr);
        InterlockedDecrement (readers);

        return list != nullptr && list->count != 0 ? list->head : nullptr;
      }

      void
      capture_resident_music ()
      {
        if (level_zones.load () || !Game::Sys_IsMainThread ())
          return;

        for (const char* name : {"music_mainmenu_mp", "music_mainmenu"})
        {
          const int kind (music_kind (name));

          if (resident_music[kind].load () != nullptr)
            continue;

          const auto* alias (current_alias (name));

          if (alias != nullptr && music_kind (alias->aliasName) == kind)
          {
            resident_music[kind].store (alias);
            LogLine{} << "backdrop: the menu music '" << name
                      << "' is the front end's own";
          }
        }
      }

      void
      forget_resident_music ()
      {
        for (auto& a : resident_music)
          a.store (nullptr);
      }

      bool
      resident (const Game::snd_alias_t* alias, int kind)
      {
        return alias != nullptr && kind > 0 && kind < 3 &&
               alias == resident_music[kind].load ();
      }

      void
      track_level_zones (int allocated, int freed)
      {
        if ((freed & ~zone_flag_level) != 0)
          forget_resident_music ();

        if ((freed & zone_flag_level) != 0)
          level_zones.store (false);

        // A level is about to be loaded over the front end: last chance to
        // see the front end's own tracks before the level's copies (if it
        // has any) override them.
        if ((allocated & zone_flag_level) != 0)
        {
          if (freed == 0)
            capture_resident_music ();

          level_zones.store (true);
        }
      }

      const Game::snd_alias_t*
      stream_alias (int channel)
      {
        if (*reinterpret_cast<void* const*> (Utils::Hook::Rebase (0x148BCEA68) + static_cast<uintptr_t> (channel) * sizeof (void*)) == nullptr)
          return nullptr;
        const auto info (Utils::Hook::Rebase (0x14651C808) + static_cast<uintptr_t> (channel) * 0xA8);
        return *reinterpret_cast<const Game::snd_alias_t* const*> (info + 0x68);
      }

      int
      play_menu_loop (int local_client, const char* name, int system)
      {
        const int kind (music_kind (name));
        if (kind != 0 && Backdrop::PreserveMenuMusic ())
        {
          // Follow the visible menu's requested track. An intro track must
          // still stop when the main menu requests a different one.
          menu_music.store (kind);
          for (int channel (40); channel < 52; ++channel)
          {
            const auto* alias (stream_alias (channel));
            if (alias != nullptr && music_kind (alias->aliasName) == kind)
              return 0; // Menu_Paint ignores the playback ID.
          }

          capture_resident_music ();

          if (const auto* alias = resident_music[kind].load ())
          {
            static const auto play (Engine::Bind<play_alias_fn> (0x140246CC0));
            return play (local_client, alias, system);
          }
        }
        return Game::SND_PlayLocalSoundAliasByName (local_client, name, system);
      }

      void
      stop_menu_stream (int channel)
      {
        // SND_StopSounds' music/ambient keep flags cover streams 40..43.
        // The UI's soundLoop actually plays music_mainmenu_mp on stream 44.
        // Keep only the track the visible menu requested, across both zone
        // cleanup and native stale-loop cleanup. Other streams stop normally.
        //
        // Never while zones are being freed, though: the track may be the
        // map's own (its zone's music_mainmenu_mp), whose sample data goes
        // with the zone, and kept playing XAudio2 reads it after it is
        // freed (the crash on changing the backdrop's map). The visible
        // menu asks for it again afterwards (see play_menu_loop ()), and it
        // starts over from what is still loaded.
        //
        // A track started from the front end's own zones (see
        // capture_resident_music ()) is the exception: a backdrop map change
        // frees only level zones, which it doesn't live in, so it plays on.
        //
        // And never for the stops that tear the channel down rather than
        // just silence it: the sound driver shutting down (a sound restart,
        // as when the audio device changes or the video restarts) releases
        // XAudio2 and every voice after stopping the streams, so a stream
        // left playing there keeps a pointer to a destroyed voice, and the
        // next stop of that channel crashes on it (0x1402C2859). Starting a
        // new stream on the channel, or reusing it for another sound, stops
        // it first for the same reason.
        const auto caller (reinterpret_cast<uintptr_t> (_ReturnAddress ()));
        const bool teardown (caller == Utils::Hook::Rebase (0x1402C23A9) || // driver shutdown
                             caller == Utils::Hook::Rebase (0x1402C267A) || // stream start
                             caller == Utils::Hook::Rebase (0x14024739C));  // channel reuse

        if (!teardown && channel >= 40 && channel < 52 && Backdrop::PreserveMenuMusic ())
        {
          const auto* alias (stream_alias (channel));
          const int wanted (menu_music.load ());
          if (wanted != 0 && alias != nullptr && music_kind (alias->aliasName) == wanted &&
              (zone_frees.load () == 0 || (level_only_free.load () && resident (alias, wanted))))
            return;
        }
        stop_stream (channel);
      }

      void
      menu_stop_sounds (int flags)
      {
        static bool kept (false);

        if (front_end () && Backdrop::IsEnabled ())
        {
          if (!kept)
          {
            LogLine{} << "backdrop: keeping the front end's sounds playing";
            kept = true;
          }
          return;
        }

        kept = false;
        stop_sounds (flags);
      }

      // Keep cgame out of our scene.
      //
      // The renderer's scene path draws the local client's shellshock blend
      // while it sets the view up, and while it sorts the draw surfaces asks
      // whether the motion tracker is up, has a worker generate the effect
      // marks' vertices, and draws the 2D (the HUD), all unconditionally.
      // That is right for a match, but in the front end there is no
      // snapshot or player state for cgame to read, and the marks system is
      // not set up until CG_Init, so each would read through a null pointer
      // (the marks one on a worker thread). Our scene has no HUD, tracker,
      // shellshock, or marks, so for it these do nothing.
      //
      // Note that we repoint the calls rather than detour the functions:
      // nothing else needs to change, and the blend's prologue reads through
      // rip, which a detour would have to relocate.
      //
      CG_DrawShellshockBlend_t draw_shellshock_blend (nullptr);
      motion_tracker_fn motion_tracker (nullptr);
      model_marks_fn model_marks (nullptr);
      glass_marks_fn glass_marks (nullptr);
      CG_Draw2D_t draw_2d (nullptr);

      void
      cgame_shellshock_blend (unsigned int local_client)
      {
        if (!s.in_scene)
          draw_shellshock_blend (local_client);
      }

      bool
      cgame_motion_tracker (int local_client)
      {
        return !s.in_scene && motion_tracker (local_client);
      }

      // The command is still filled, since the worker reads the rest of it,
      // but told to generate nothing.
      //
      void
      fx_model_marks (int local_client, unsigned char* cmd)
      {
        model_marks (local_client, cmd);

        if (s.in_scene)
          cmd[model_marks_enabled] = 0;
      }

      void
      fx_glass_marks (int local_client)
      {
        if (!s.in_scene)
          glass_marks (local_client);
      }

      void
      cgame_draw_2d (int local_client)
      {
        if (!s.in_scene)
          draw_2d (local_client);
      }
    }

    namespace
    {
      void draw_menu_picture (float x, float y, float w, float h,
                              float s0, float t0, float s1, float t1,
                              const float* color, Material* material)
      {
        if (!Backdrop::Hides (material))
          Engine::Bind<Game::CL_DrawStretchPicPhysical_t> (0x14001BE90) (
            x, y, w, h, s0, t0, s1, t1, color, material);
      }
    }

    bool Backdrop::IsEnabled ()
    {
      if (!hooksInstalled || headless ()) return false;
      if (!Game::Sys_IsMainThread ()) return enabled.load (std::memory_order_acquire);
      // Always on; users can't turn it off. Only a failed map stops it.
      enabled = s.current != phase::failed;
      return enabled;
    }

    bool Backdrop::IsShown () { return hooksInstalled && s.shown; }

    bool Backdrop::IsStartupLoading ()
    {
      return hooksInstalled && !s.startup_presented && front_end () && IsEnabled () && !quitting ();
    }

    // The quit command, before the engine shuts down: it doesn't go through
    // Com_Quit_f, so g_quitRequested isn't set (see quitting ()).
    //
    void Backdrop::BeginShutdown ()
    {
      if (s.quitting) return;
      s.quitting = true;
      s.held.clear ();
      s.held_direction.reset ();
      LogLine{} << "backdrop: the game is quitting";
    }

    // A zone load that frees zones is about to run (see LoadAssets in
    // Hooks.cpp). Stop the menu music first, the native way, so none of
    // its sample data is in use when its zone goes, and keep it stopped
    // until the free is done.
    //
    //
    // Except a track started from the front end's own zones, when only
    // level zones go: that is a backdrop map change, and the track is
    // nowhere in what it frees, so the music carries on through it.
    //
    void Backdrop::BeginZoneFree (int freed)
    {
      if (zone_frees.fetch_add (1) != 0 || stop_stream == nullptr || !Game::Sys_IsMainThread ())
        return;

      const bool level_only ((freed & ~zone_flag_level) == 0 && PreserveMenuMusic ());
      level_only_free.store (level_only);

      for (int channel (40); channel < 52; ++channel)
      {
        const auto* alias (stream_alias (channel));
        if (alias == nullptr) continue;
        const int kind (music_kind (alias->aliasName));
        if (kind == 0) continue;
        const bool keep (level_only && resident (alias, kind));
        LogLine{} << "backdrop: " << (keep ? "keeping" : "stopping") << " the menu music '"
                  << alias->aliasName << "' (stream " << channel << ") through a zone free";
        if (!keep)
          stop_stream (channel);
      }
    }

    void Backdrop::EndZoneFree ()
    {
      if (zone_frees.fetch_sub (1) == 1)
      {
        level_only_free.store (false);

        // With the level gone nothing overrides the front end's tracks,
        // so this is when they can be looked up if they weren't already.
        if (PreserveMenuMusic ())
          capture_resident_music ();
      }
    }

    bool Backdrop::PreserveMenuMusic ()
    {
      return hooksInstalled && !quitting () && !Renderer::IsDeviceRecoveryActive () && front_end () && IsEnabled ();
    }

    void Backdrop::BeforeZoneLoad (const XZoneInfo* zones, unsigned int count)
    {
      if (hooksInstalled && zones != nullptr) zone_load (zones, static_cast<int> (count), 0);
    }

    bool
    Backdrop::Hides (const Material* material)
    {
      if (!s.shown && s.main_menu_transition)
        return false;

      if ((!s.shown && !s.transition.active () && !IsStartupLoading ()) ||
          material == nullptr || material->info.name == nullptr)
        return false;

      const string_view n (material->info.name);

      if (ranges::find (hidden_materials, n) == ranges::end (hidden_materials))
        return false;

      ++s.hidden;

      if (ranges::find (s.hidden_logged, n) == s.hidden_logged.end ())
      {
        LogLine{} << "backdrop: hiding the menu background " << n;
        s.hidden_logged.emplace_back (n);
      }

      return true;
    }

    void
    Backdrop::Direct (BackdropDirection d)
    {
      s.direction = move (d);
    }

    namespace
    {
      vector<pair<string, std::function<std::optional<BackdropFigure>()>>> subjects;
    }

    void
    Backdrop::AddZone (string n)
    {
      s.zones.push_back (move (n));
    }

    void
    Backdrop::AddSubject (string n, std::function<std::optional<BackdropFigure>()> f)
    {
      using entry = pair<string, std::function<std::optional<BackdropFigure>()>>;

      auto i (ranges::find (subjects, n, &entry::first));

      if (i != subjects.end ())
        i->second = move (f);
      else
        subjects.emplace_back (move (n), move (f));
    }

    optional<BackdropFigure>
    Backdrop::FindFigure (string_view n)
    {
      using entry = pair<string, std::function<std::optional<BackdropFigure>()>>;

      auto i (ranges::find (subjects, n, &entry::first));
      return i != subjects.end () ? i->second () : nullopt;
    }

    optional<Position>
    Backdrop::FindTag (const BackdropFigure& f, string_view t)
    {
      // Script strings exist as soon as a model with the bone is loaded,
      // so a tag that isn't one is on none of the models.
      //
      const unsigned int name (find_string (string (t).c_str ()));

      if (name == 0)
        return nullopt;

      for (const BackdropModel& x: f.models)
      {
        // A model attached to a tag is posed from it, which we don't follow
        // here.
        //
        const XModel* m (x.model);

        if (x.tag || m == nullptr || m->boneNames == nullptr ||
            m->baseMat == nullptr)
          continue;

        for (size_t i (0); i != m->numBones; ++i)
        {
          if (m->boneNames[i] != name)
            continue;

          // The bone's bind pose is in the model's space, which the
          // figure's placement takes to the map.
          //
          const placement p (place (f));
          const float* b (m->baseMat[i].trans);

          return Position {
            p.origin[0] + p.cosine * b[0] - p.sine * b[1],
            p.origin[1] + p.sine * b[0] + p.cosine * b[1],
            p.origin[2] + b[2]};
        }
      }

      return nullopt;
    }

    Backdrop::Backdrop ()
    {
      if (headless ()) return;
      Engine::Initialize ();
      Scheduler::OnGameInitialized ([]
      {
        backdrop_dvar ();
        shown_map_dvar ();
        dvars_registered = true;

        // startup_tick loads the chosen map; avoid reading every other map
        // into the file cache while that load is in progress.
        if (s.seed == 0)
          s.seed = random_device {} () | 1;
      }, Scheduler::Pipeline::MAIN);
      Scheduler::Loop (startup_tick, Scheduler::Pipeline::MAIN);
      Renderer::OnDeviceRecoveryBegin ([] { teleport_needs_load = true; });
      Command::Add ("ui_backdrop_status", []
      {
        const dvar_t* setting (backdrop_dvar ());
        Logger::Print ("menu backdrop: hooks={}, phase={}, setting='{}', map='{}', renderer={}, ui={}, connection={}, actors={}\n",
                       hooksInstalled, to_string (s.current), setting->current.string,
                       s.map, static_cast<int> (*renderer_started), static_cast<int> (*ui_started),
                       static_cast<int> (Game::CL_GetLocalClientConnectionState (0)), Actors::IsReady ());
        if (!s.failure.empty ()) Logger::Print ("menu backdrop: {}\n", s.failure);
      });
      Scheduler::Loop ([] { if (!headless ()) lock_backdrop_dvars (); }, Scheduler::Pipeline::MAIN);
      renderer_started = reinterpret_cast<const volatile int*> (Utils::Hook::Rebase (0x140C5CEA8));
      ui_started = reinterpret_cast<const volatile int*> (Utils::Hook::Rebase (0x140C5CEB0));
      pmem_low = reinterpret_cast<const volatile int32_t*> (Utils::Hook::Rebase (0x146707B74));
      pmem_high = reinterpret_cast<const volatile int32_t*> (Utils::Hook::Rebase (0x146707D84));
      free_entry_head = reinterpret_cast<void* const*> (Utils::Hook::Rebase (0x141521658));
      render_scene = reinterpret_cast<render_scene_fn> (Utils::Hook::Rebase (0x1400F4EE0));
      init_primary_lights = reinterpret_cast<init_primary_lights_fn> (Utils::Hook::Rebase (0x140033BA0));
      find_string = reinterpret_cast<find_string_fn> (Utils::Hook::Rebase (0x140221910));
      first_view = reinterpret_cast<first_view_fn> (Utils::Hook::Rebase (0x140024370));
      load_xassets = reinterpret_cast<load_xassets_fn> (Utils::Hook::Rebase (0x14012EC40));
      release_world = reinterpret_cast<release_world_fn> (Utils::Hook::Rebase (0x140037550));
      level_load_original = reinterpret_cast<level_load_fn> (Utils::Hook::Rebase (0x14012EB40));
      screen_field = Engine::Bind<screen_field_fn> (0x1401018A0);
      clear_screen = Engine::Bind<R_AddCmdClearScreen_t> (0x14001D630);
      stop_sounds = Engine::Bind<stop_sounds_fn> (0x14024A2E0);
      draw_shellshock_blend = Engine::Bind<CG_DrawShellshockBlend_t> (0x1400B8F80);
      motion_tracker = Engine::Bind<motion_tracker_fn> (0x1400B42F0);
      model_marks = Engine::Bind<model_marks_fn> (0x14013C370);
      glass_marks = Engine::Bind<glass_marks_fn> (0x1401505F0);
      draw_2d = Engine::Bind<CG_Draw2D_t> (0x1400D0D80);
      match_load_world_original = LoadWorld;
      begin_remote_update = R_BeginRemoteScreenUpdate;
      end_remote_update = R_EndRemoteScreenUpdate;
      const HookRequest requests[] =
      {
        {0x140101D15, 0x1401018a0, reinterpret_cast<void*> (&draw_screen_field), false},
        {0x1401019D2, 0x140270c60, reinterpret_cast<void*> (&draw_menu_viewport), false},
        {0x140101BAA, 0x140270c60, reinterpret_cast<void*> (&draw_menu_viewport), false},
        {0x140101D63, 0x140270c60, reinterpret_cast<void*> (&draw_menu_viewport), false},
        {0x140101B81, 0x14001d630, reinterpret_cast<void*> (&menu_clear), false},
        {0x14010199F, 0x14001d630, reinterpret_cast<void*> (&menu_clear), false},
        {0x1401019A6, 0x14024a2e0, reinterpret_cast<void*> (&menu_stop_sounds), false},
        {0x140265E1E, 0x140246D20, reinterpret_cast<void*> (&play_menu_loop), false},
        {0x1401019CB, 0x14001d630, reinterpret_cast<void*> (&menu_clear), false},
        {0x14002481D, 0x1400b8f80, reinterpret_cast<void*> (&cgame_shellshock_blend), false},
        {0x14002334B, 0x1400b42f0, reinterpret_cast<void*> (&cgame_motion_tracker), false},
        {0x1400230B8, 0x14013c370, reinterpret_cast<void*> (&fx_model_marks), false},
        {0x140023555, 0x1401505f0, reinterpret_cast<void*> (&fx_glass_marks), false},
        {0x140022896, 0x1400d0d80, reinterpret_cast<void*> (&cgame_draw_2d), false, reinterpret_cast<void**> (&draw_2d)},
        {0x1400D6B5A, 0x1400f5b10, reinterpret_cast<void*> (&match_load_world), false},
        {0x140109A5D, 0x140117ce0, reinterpret_cast<void*> (&party_fraction), false, reinterpret_cast<void**> (&party_fraction_original)},
        {0x140251045, 0x140117ce0, reinterpret_cast<void*> (&loadbar_fraction), false, reinterpret_cast<void**> (&loadbar_fraction_original)},
        {0x1400F4D26, 0x14012eb40, reinterpret_cast<void*> (&level_load), false},
        {0x1400FE489, 0x14012eb40, reinterpret_cast<void*> (&level_load), true},
        {0x140112036, 0x14012eb40, reinterpret_cast<void*> (&level_load), false},
        {0x14023B6A2, 0x14012eb40, reinterpret_cast<void*> (&level_load), false},
        {0x140032D22, 0x140037550, reinterpret_cast<void*> (&renderer_release_world), false},
        {0x1400F4293, 0x14001be90, reinterpret_cast<void*> (&draw_menu_picture), false},
        {0x1400F449D, 0x14001be90, reinterpret_cast<void*> (&draw_menu_picture), false},
        {0x1400d6690, 0x14001dac0, reinterpret_cast<void*> (&begin_remote_screen_update), false},
        {0x1400f0b9c, 0x14001dac0, reinterpret_cast<void*> (&begin_remote_screen_update), false},
        {0x1400f4d72, 0x14001dac0, reinterpret_cast<void*> (&begin_remote_screen_update), false},
        {0x1400f8286, 0x14001dac0, reinterpret_cast<void*> (&begin_remote_screen_update), false},
        {0x14012d907, 0x14001dac0, reinterpret_cast<void*> (&begin_remote_screen_update), false},
        {0x14012e0f6, 0x14001dac0, reinterpret_cast<void*> (&begin_remote_screen_update), false},
        {0x14012ebcb, 0x14001dac0, reinterpret_cast<void*> (&begin_remote_screen_update), false},
        {0x14012ec7c, 0x14001dac0, reinterpret_cast<void*> (&begin_remote_screen_update), false},
        {0x14012f761, 0x14001dac0, reinterpret_cast<void*> (&begin_remote_screen_update), false},
        {0x14012f99d, 0x14001dac0, reinterpret_cast<void*> (&begin_remote_screen_update), false},
        {0x1401ade59, 0x14001dac0, reinterpret_cast<void*> (&begin_remote_screen_update), false},
        {0x1401f372e, 0x14001dac0, reinterpret_cast<void*> (&begin_remote_screen_update), false},
        {0x1401f70e4, 0x14001dac0, reinterpret_cast<void*> (&begin_remote_screen_update), false},
        {0x1401f77e2, 0x14001dac0, reinterpret_cast<void*> (&begin_remote_screen_update), false},
        {0x14023b371, 0x14001dac0, reinterpret_cast<void*> (&begin_remote_screen_update), false},
        {0x14023b4fe, 0x14001dac0, reinterpret_cast<void*> (&begin_remote_screen_update), false},
        {0x14023b6a7, 0x14001dac0, reinterpret_cast<void*> (&begin_remote_screen_update), false},
        {0x1402432ac, 0x14001dac0, reinterpret_cast<void*> (&begin_remote_screen_update), false},
        {0x1400d6dda, 0x14001db30, reinterpret_cast<void*> (&end_remote_screen_update), false},
        {0x1400f0c93, 0x14001db30, reinterpret_cast<void*> (&end_remote_screen_update), false},
        {0x1400f4d9d, 0x14001db30, reinterpret_cast<void*> (&end_remote_screen_update), false},
        {0x1400f83ba, 0x14001db30, reinterpret_cast<void*> (&end_remote_screen_update), false},
        {0x14012d916, 0x14001db30, reinterpret_cast<void*> (&end_remote_screen_update), false},
        {0x14012e100, 0x14001db30, reinterpret_cast<void*> (&end_remote_screen_update), false},
        {0x14012ebd5, 0x14001db30, reinterpret_cast<void*> (&end_remote_screen_update), false},
        {0x14012ec86, 0x14001db30, reinterpret_cast<void*> (&end_remote_screen_update), false},
        {0x14012f76b, 0x14001db30, reinterpret_cast<void*> (&end_remote_screen_update), false},
        {0x14012f9a7, 0x14001db30, reinterpret_cast<void*> (&end_remote_screen_update), false},
        {0x1401adede, 0x14001db30, reinterpret_cast<void*> (&end_remote_screen_update), false},
        {0x1401f373f, 0x14001db30, reinterpret_cast<void*> (&end_remote_screen_update), true},
        {0x1401f70f0, 0x14001db30, reinterpret_cast<void*> (&end_remote_screen_update), false},
        {0x1401f77ee, 0x14001db30, reinterpret_cast<void*> (&end_remote_screen_update), false},
        {0x14023b4e1, 0x14001db30, reinterpret_cast<void*> (&end_remote_screen_update), false},
        {0x14023b503, 0x14001db30, reinterpret_cast<void*> (&end_remote_screen_update), false},
        {0x14023ba8b, 0x14001db30, reinterpret_cast<void*> (&end_remote_screen_update), false},
        {0x14024330b, 0x14001db30, reinterpret_cast<void*> (&end_remote_screen_update), false},
      };
      if (!InstallHooks (requests)) return;
      if (!ObserveZoneLoads ())
      {
        RemoveHooks ();
        Logger::Error ("menu backdrop: could not observe zone loads; backdrop disabled\n");
        return;
      }
      if (!ObserveStreamStops (&stop_menu_stream, &stop_stream))
        Logger::Error ("backdrop: stream-stop entry did not match; music preservation unavailable\n");
      hooksInstalled = true;
      Logger::Print ("menu backdrop: iw4mp 2.0.13 hooks installed\n");
    }
  }
}
