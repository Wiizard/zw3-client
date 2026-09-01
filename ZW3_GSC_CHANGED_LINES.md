# ZW3 GSC changes and exact active locations

Snapshot: 2026-08-24  
Active GSC root: `C:\ZWNET\game\zw3\core`  
Purpose: exact inventory of the ZW3-specific GSC changes currently active on the VPS.

## How to read the locations

- Every location points to the current active file under `C:\ZWNET\game\zw3\core`.
- Line and column numbers are 1-based.
- Several inherited ZW3 scripts are minified. Their complete executable content is physically on line 1. For those files, the column is included so the exact statement can still be found.
- `zw\zombies.gsc` and `zw\utility\challenge_progress.gsc` are formatted and therefore use normal line ranges.
- Line numbers will naturally change if one of the files is formatted or edited later.
- The list does not classify the hundreds of original map/FX GSC files as changes. It records only the ZW3 overlay files and integration points introduced for matchmaking lifecycle, player state, Barracks data and challenges.

## Active changed GSC files

| Active relative path | Relevant active code lines | Active SHA-256 |
|---|---:|---|
| `zw\main.gsc` | line 1 | `95DE4800C6A046B87841A074C38D1E24CE7ED547825A8AADDF6883B3E194E5E8` |
| `zw\player.gsc` | line 1 | `E32993DE119670DF250EED92C0A45C68D624700A51C20800EB0DCB8DC7BBCEC5` |
| `zw\aether.gsc` | line 1 | `F4FB01199D885D7110820540D0DF4613EEEC81E1E8FBFAB0C499168EE78A1ABA` |
| `zw\mapedits\mapedits.gsc` | line 1 | `55368C62643179FD2FF16680CCFF484745A211EF5C6CAAA23820A79AE49F0A33` |
| `zw\utility\hud.gsc` | line 1 | `CE31BED2D8F4F4F5BBD6378037618A9F5017C600F43159CF47816901AFFC3BD4` |
| `zw\utility\powerups.gsc` | line 1 | `644BF0883F5D35599501F3559FA078D6EF7E0129EA7E737E55675521BC6DCB08` |
| `zw\utility\quests.gsc` | line 1 | `5CB0C804764A039B7F5B6EC28C1C411701E540B426A7ACA1747E0765FFCC539C` |
| `zw\utility\revivesystem.gsc` | line 1 | `35D60EDE2C4392FC070E74ECBCFA5EF6FA0EBDD0E56C988FBF402DBD9A03278E` |
| `zw\zombies.gsc` | lines 1-4934 | `DD73E6A43EB6E50F68A0677F9623F86D9A7DB4B21E2BA90657756A56DB0E9681` |
| `zw\utility\challenge_progress.gsc` | lines 1-917 | `4A1AB20C7CE979043835E4369F6A37F97A63ADE3BDEDC8720DAFD0F95AB64A56` |

## 1. Matchmaking lifecycle and Game Over

### `zw\main.gsc`

The file is minified; all code is on physical line 1.

| Exact location | Change |
|---|---|
| line 1, columns 3650-3906 | `eventuallyRestart()` now recognizes a managed ZWNET match. When `zwnet_match_id` is set, an empty-server state no longer blindly reloads the current map. It returns only after `zwnet_match_ended == 1`; otherwise it waits for the managed lifecycle. |
| line 1, column 3773 | Start of the `if(getDvar("zwnet_match_id") != "")` managed-match guard inside `eventuallyRestart()`. |
| line 1, column 3810 | `if(getDvarInt("zwnet_match_ended") == 1) return;` prevents the ordinary map-restart path after the managed match has ended. |
| line 1, columns 14270-15322 | `startEndGame(lastplayer,pos)` is the normal zombie Game Over entry and performs the existing one-shot end sequence. |
| line 1, column 17873 | `if(getDvar("zwnet_match_id") != "") setDvar("zwnet_match_ended", 1);` publishes the final managed-match marker after the Game Over presentation, before the normal `endGame` call. |

This logic deliberately does not mark loading, a round transition, spectating or a temporary disconnect as Game Over.

## 2. Matchmaking character state, per-player initialization and weapon counters

### `zw\player.gsc`

The file is minified; all code is on physical line 1.

| Exact location | Change |
|---|---|
| line 1, columns 162-1901 | `setupPlayer()` contains the complete added player initialization block. |
| line 1, column 204 | The matchmaking-only branch begins with `if(getDvar("zwnet_match_id") != "")`. |
| line 1, column 242 | A managed player uses `self GetZW3Character()` so the server keeps the ZW3 lobby character choice instead of replacing it with an unrelated default. |
| line 1, column 662 | Initializes `self.zw3WeaponKillNames = [];`. |
| line 1, column 692 | Initializes `self.zw3WeaponKillCounts = [];`. |
| line 1, column 1577 | Starts `zw\utility\challenge_progress::initializeChallenges()` once the normal player setup data exists. |

The non-matchmaking branch continues to use the existing normal `getCharacter()` flow.

### `zw\zombies.gsc`

| Exact location | Change |
|---|---|
| line 3539 | Calls `zw3TrackWeaponKillForApi(attacker, sWeapon)` after an authenticated player kill. |
| line 3540 | Calls `trackZombieKill(...)` with headshot, damage type, weapon, zombie type and hero/boss information. |
| lines 4905-4934 | Adds `zw3TrackWeaponKillForApi(player, weapon)`, which increments an existing weapon bucket or creates a new one. |

### `zw\main.gsc` weapon result serialization

| Exact location | Change |
|---|---|
| line 1, column 25813 | Adds the `weapon_kills` array to the existing match-result JSON by calling `self getWeaponKills()`. |
| line 1, columns 32060-32582 | Adds `getWeaponKills()`, which serializes only defined weapon entries whose kill count is greater than zero. |

## 3. Scoreboard and Barracks-supporting zombie values

### `zw\main.gsc`

| Exact location | Change |
|---|---|
| line 1, columns 18308-18729 | `clock()` maintains the survived-time value used by the zombie scoreboard. |
| line 1, column 18426 | Initializes `zw3_ui_sb_survived_time` to `00:00:00`. |
| line 1, column 18668 | Updates `zw3_ui_sb_survived_time` once per active game second. |

### `zw\utility\revivesystem.gsc`

The file is minified; all code is on physical line 1.

| Exact location | Change |
|---|---|
| line 1, columns 129-307 | Adds `scoreboardSetDownState(value, progress)`. |
| line 1, column 206 | Publishes `zw3_sb_down_<clientNum>`. |
| line 1, column 250 | Publishes `zw3_sb_down_progress_<clientNum>`. |
| line 1, columns 310-497 | Adds `scoreboardTrackDownState()`, clearing the down state on revive, death or bleed-out destruction. |
| line 1, column 5734 | A completed revive calls `trackRevive()` for the challenge and lifetime-revive counters. |

### Lifetime zombie counters in `zw\utility\challenge_progress.gsc`

| Exact lines | Change |
|---|---|
| 411-413 | Saves lifetime zombie kills, deaths and revives in the existing GUID-bound challenge file. |
| 423-428 | Initializes the three counters and migration flags. |
| 454-471 | Reads `stat_zw3_zombie_kills`, `stat_zw3_zombie_deaths` and `stat_zw3_zombie_revives`. |
| 482-487 | Migrates older files by taking the maximum of the lifetime value and the matching challenge progress; marks old data dirty so it is rewritten safely. |
| 643-654 | Adds `challengeDeathMonitor()` and counts only deaths during the real `game` state. |
| 678-682 | Starts the challenge save, session, round, Game Over and death monitors. |
| 702-708 | Increments the lifetime kill counter on a real zombie kill. |
| 728-734 | Increments the lifetime revive counter after a completed revive. |
| 741-748 | Implements the lifetime zombie-death increment. |

These counters are local ZW3 gameplay/Barracks data. They do not read or write Stats API database tables.

## 4. Challenge runtime

### New file: `zw\utility\challenge_progress.gsc`

| Exact lines | Function or block | Purpose |
|---:|---|---|
| 4-35 | `getChallengeIds()` | Declares the 26 supported challenge IDs. |
| 36-156 | `getChallengeTierTarget()` | Supplies the active target for every supported tier. |
| 157-277 | `getChallengeTierReward()` | Supplies the XP reward for every supported tier. |
| 278-289 | `getChallengeTierCount()` | Counts valid tiers for one challenge. |
| 290-302 | `getChallengeMaxTarget()` | Finds the final cap used for safe progress clamping. |
| 303-307 | `isKnownChallenge()` | Rejects unknown challenge identifiers. |
| 308-341 | `getChallengeTitle()` | Maps IDs to their ZW3 display names. |
| 342-382 | `getChallengeCategoryTitle()` | Maps challenges to Survival, Combat, Team Play, Bosses, Economy or Mastery. |
| 383-398 | `publishChallengeProgress()` | Publishes progress, tier, target and reward to client DVARs. |
| 399-418 | `saveChallengeData()` | Serializes GUID-bound challenge and lifetime-zombie data. |
| 419-489 | `loadChallengeData()` | Loads, validates, clamps and migrates saved progress. |
| 490-522 | `evaluateChallengeTiers()` | Advances completed tiers exactly once and awards the configured XP. |
| 523-540 | `addChallengeProgress()` | Adds bounded incremental progress. |
| 541-557 | `setChallengeProgress()` | Applies monotonic absolute progress, used for rounds/time. |
| 558-569 | `challengeSaveLoop()` | Persists dirty progress every 10 seconds. |
| 570-592 | `challengeSessionMonitor()` | Tracks active, unpaused session minutes. |
| 593-631 | `challengeRoundMonitor()` | Tracks rounds, no-down rounds, modes and last-stand state. |
| 632-642 | `challengeGameOverMonitor()` | Handles final full-clear evaluation and final save. |
| 643-654 | `challengeDeathMonitor()` | Tracks actual in-match deaths. |
| 655-684 | `initializeChallenges()` | Waits for GUID/rank data, skips bots, loads data and starts all monitors. |
| 685-701 | `trackScoreChange()` | Separates earned score from spent score. |
| 702-727 | `trackZombieKill()` | Tracks kills, headshots, melee, explosives, weapons and boss types. |
| 728-740 | `trackRevive()` | Tracks revives, support and late-round saves. |
| 741-749 | `trackZombieDeath()` | Tracks lifetime zombie deaths. |
| 750-757 | `trackPowerup()` | Tracks power-up and support usage. |
| 758-777 | `isSupportedQuestline()` | Whitelists the ten supported quest maps. |
| 778-802 | quest read helpers | Reads one map completion count without changing other map entries. |
| 803-834 | `setQuestlineCount()` | Rewrites the per-GUID quest file while retaining all other map values. |
| 835-851 | `setQuestlineClientDvar()` | Publishes one of the ten quest completion counters. |
| 852-882 | `publishQuestlineProgress()` | Loads and publishes all questline counters, clamped to 0-10. |
| 883-893 | `getQuestlineBonus()` | Stages the repeated-completion XP bonuses. |
| 894-917 | `completeQuestline()` | Increments at most to 10, awards bonus XP and updates Map Master. |

### Important CSV/runtime note

The active GSC does not call `tableLookup()` for the challenge definitions. The target/reward/title/category values are deliberately embedded in lines 36-382 because the server runtime did not reliably expose the new CSV tables to GSC during the tested startup path. The native Barracks UI reader consumes `zw3/core/mp/zw3_challenge_ui.csv`; gameplay progress uses the matching guarded constants above. This avoids a missing-table script failure during map start.

## 5. Exact challenge integration points

The following calls connect existing gameplay events to the single challenge runtime. No duplicate challenge system was added.

| Active file | Exact location | Added call/effect |
|---|---:|---|
| `zw\player.gsc` | line 1, column 1577 | Initializes the player challenge runtime. |
| `zw\aether.gsc` | line 1, column 3989 | Adds one `ch_zw3_aether_crystals` point when a player destroys a small Aether crystal. |
| `zw\mapedits\mapedits.gsc` | line 1, column 32947 | Adds one `ch_zw3_perk_collector` point only for a newly acquired perk. |
| `zw\mapedits\mapedits.gsc` | line 1, column 44194 | Adds one `ch_zw3_packapunch_user` point after Pack-a-Punch use. |
| `zw\utility\hud.gsc` | line 1, column 23824 | Sends the local score delta to `trackScoreChange()`. |
| `zw\utility\powerups.gsc` | line 1, column 3734 | Sends the collected power-up type to `trackPowerup()`. |
| `zw\utility\quests.gsc` | line 1, column 19962 | Publishes saved questline progress when the quest flow initializes. |
| `zw\utility\quests.gsc` | line 1, column 20571 | Calls `completeQuestline(level.currentMap)` after the existing quest completion. |
| `zw\utility\revivesystem.gsc` | line 1, column 5734 | Calls `trackRevive()` only after the revive counter is committed. |
| `zw\zombies.gsc` | line 3540 | Sends the complete confirmed zombie-kill context to `trackZombieKill()`. |

## 6. Persistence and ownership

- Challenge file: `challenges_<guid>` in the existing ZW3 scriptdata filesystem.
- Quest mastery file: `quest_challenges_<guid>` in the existing ZW3 scriptdata filesystem.
- Matchmaking end signal: server DVAR `zwnet_match_ended` tied to non-empty `zwnet_match_id`.
- No GSC code directly accesses Stats API database tables.
- No Stats API source, schema, migration or endpoint is changed by these GSC files.

## Verification used for this inventory

- Active files were read only from `C:\ZWNET\game\zw3\core`.
- Current files were compared with the fixed package snapshots:
  - `ZW3-Complete-Package-20260823-0250-lifecycle`
  - `ZW3-Complete-Package-20260823-2325-challenges`
  - `ZW3-Complete-Package-20260823-2342-timeout-fix`
  - `ZW3-Complete-Package-20260823-2358-challenge-runtime-fix`
- The active challenge runtime was additionally diffed against the last package snapshot to capture the later Barracks lifetime-counter additions.
- This documentation task itself did not modify any `.gsc` file.
