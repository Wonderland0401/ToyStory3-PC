#include "Menu.h"
#include "Game.h"
#include <math.h>
#include <stdarg.h>

using namespace mem;

namespace menu
{
namespace
{
	constexpr int MaxRows = 64;
	constexpr int RowLength = 48;
	constexpr int MaxDepth = 8;
	constexpr int EntitiesPerPage = 48;
	constexpr int LocksPerPage = 48;
	constexpr int SlotCount = 8;
	constexpr int NormalSpeed = 4;

	enum Page
	{
		PAGE_MAIN, PAGE_PLAYER, PAGE_TELEPORT, PAGE_PROGRESS, PAGE_COLLECTIBLES, PAGE_UNLOCKABLES,
		PAGE_UNLOCKABLE, PAGE_CHEATS, PAGE_WORLD, PAGE_SKY, PAGE_ENTITIES, PAGE_ENTITY, PAGE_MISSIONS
	};

	enum Action
	{
		ACT_NONE, ACT_OPEN_PAGE, ACT_CLOSE_MENU,
		ACT_GOD_MODE, ACT_HEAL, ACT_FLY, ACT_FLY_SPEED, ACT_HOVER, ACT_RUN_SPEED, ACT_PLAYER_SIZE,
		ACT_SUPER_JUMP, ACT_SWAP_ANYWHERE,
		ACT_MOVE_UP, ACT_MOVE_DOWN, ACT_PICK_SLOT, ACT_SAVE_SLOT, ACT_GO_TO_SLOT, ACT_SAVE_TELEPORTS, ACT_LOAD_TELEPORTS,
		ACT_UNLOCK_ALL, ACT_BUY_ALL, ACT_MAX_STATS, ACT_KEEP_MAXED, ACT_ADD_GOLD, ACT_ADD_COLLECTIBLE,
		ACT_LOCK_LIST_PAGE, ACT_OPEN_LOCK, ACT_SET_LOCK, ACT_TOGGLE_CHEAT,
		ACT_GAME_SPEED, ACT_SHOW_HUD, ACT_SET_SKY,
		ACT_ENTITY_LIST_PAGE, ACT_OPEN_ENTITY, ACT_GO_TO_ENTITY, ACT_GO_ABOVE_ENTITY, ACT_BRING_ENTITY,
		ACT_CONTROL_ENTITY, ACT_DELETE_ENTITY
	};

	struct Row
	{
		char text[RowLength];
		int action;
		int value;
		int leftRightAction;
		int step;
	};

	struct Slot
	{
		bool used;
		Vec3 position;
	};

	struct Collectible { const char* label; const char* stat; int amount; };
	const Collectible collectibles[] = {
		{ "Gold",           "Stat_GOLD_COLLECTED",               10000 },
		{ "Gold Available", "Stat_GOLD_AVAILABLE",               10000 },
		{ "Imagination",    "Stat_IMAGINATION_POINTS_COLLECTED", 10000 },
		{ "Gold Stars",     "Stat_GOLDSTAR_COLLECTED",              10 },
		{ "Al's Tokens",    "Stat_ALS_TOY_TOKEN_COUNT",             25 },
		{ "Cowbells",       "Stat_COWBELLS_FOUND",                  10 },
		{ "Cards",          "Stat_CARDS_FOUND",                     10 },
		{ "Zurg Pieces",    "Stat_ZURG_PIECES_FOUND",               10 },
		{ "Muffins",        "Stat_MUFFINS_FOUND",                   10 },
		{ "Patterns",       "Stat_PATTERNS_FOUND",                  10 },
		{ "Dino Bones",     "Stat_DINOSAUR_BONES_FOUND",            10 },
		{ "Gems",           "Stat_PRECIOUS_STONES_FOUND",           10 },
		{ "Skeleton Keys",  "Stat_SKELETON_KEY_COUNT",              10 },
		{ "Coins In Hamm",  "Stat_COINS_IN_HAMM",                 1000 },
		{ "Hats",           "Stat_LP_HAT_CUSTOMIZE_TOTAL",          25 },
		{ "Hair",           "Stat_LP_HAIR_CUSTOMIZE_TOTAL",         25 },
		{ "Bodies",         "Stat_LP_BODY_CUSTOMIZE_TOTAL",         25 },
		{ "Accessories",    "Stat_LP_ACCESSORY_CUSTOMIZE_TOTAL",    25 },
	};

	struct GameSpeed { int value; const char* name; };
	const GameSpeed gameSpeeds[] = {
		{ 0, "Stopped" }, { 2, "1/8x" }, { 3, "1/4x" }, { 4, "1/2x" }, { 9, "Normal" }, { 5, "Double" },
	};

	Row rows[MaxRows];
	int rowCount = 0;
	char pageTitle[32];
	int pageHistory[MaxDepth] = { PAGE_MAIN };
	int depth = 0;

	bool wantOpen = false;
	bool isOpen = false;
	void* menuScreen = nullptr;
	CloseFn closeHandler = nullptr;
	ULONGLONG closeAskedAt = 0;
	ULONGLONG lastCloseTry = 0;
	ULONGLONG lastSelect = 0;

	void* selectedEntity = nullptr;
	int selectedLock = 0;
	int entityListStart = 0;
	int lockListStart = 0;
	int selectedSlot = 0;

	bool godMode = false;
	bool hover = false;
	bool flying = false;
	bool keepMaxed = false;
	bool swapAnywhere = false;
	int runSpeed = 100;
	int flySpeed = 30;
	int flyHeldFrames = 0;
	float flyHeight = 0.0f;
	void* flyingPlayer = nullptr;
	int playerSize = 100;
	ULONGLONG nextSwap = 0;

	Slot slots[SlotCount];
	bool teleportsLoaded = false;
	char fileText[2048];

	ULONGLONG arrowHeldSince = 0;
	ULONGLONG arrowLastStep = 0;
	int arrowDirection = 0;

	int KeepBetween(int value, int low, int high)
	{
		return value < low ? low : (value > high ? high : value);
	}

	int LoopAround(int value, int count)
	{
		return value < 0 ? count - 1 : (value >= count ? 0 : value);
	}

	int CurrentSpeedChoice()
	{
		const int current = game::GetGameSpeed();
		for (int i = 0; i < LengthOf(gameSpeeds); i++)
		{
			if (gameSpeeds[i].value == current)
				return i;
		}
		return NormalSpeed;
	}

	void SetPageTitle(const char* text)
	{
		snprintf(pageTitle, sizeof(pageTitle), "%s", text);
	}

	Row* AddEmptyRow(int action, int value)
	{
		if (rowCount >= MaxRows)
			return nullptr;

		Row& row = rows[rowCount++];
		row.text[0] = '\0';
		row.action = action;
		row.value = value;
		row.leftRightAction = ACT_NONE;
		row.step = 0;
		return &row;
	}

	void AddRow(const char* label, int action = ACT_NONE, int value = 0)
	{
		if (Row* row = AddEmptyRow(action, value))
			snprintf(row->text, sizeof(row->text), "%s", label);
	}

	void AddCustomRow(int action, int value, _Printf_format_string_ const char* format, ...)
	{
		Row* row = AddEmptyRow(action, value);
		if (!row)
			return;

		va_list args;
		va_start(args, format);
		vsnprintf(row->text, sizeof(row->text), format, args);
		va_end(args);
	}

	void AddOnOffRow(const char* label, bool on, int action, int value = 0)
	{
		AddCustomRow(action, value, "%s   [%s]", label, on ? "ON" : "OFF");
	}

	void AddNumberRow(const char* label, int number, int action = ACT_NONE, int value = 0)
	{
		AddCustomRow(action, value, "%s  %d", label, number);
	}

	void AddLeftRightRow(int action, int step, _Printf_format_string_ const char* format, ...)
	{
		Row* row = AddEmptyRow(ACT_NONE, 0);
		if (!row)
			return;

		row->leftRightAction = action;
		row->step = step;

		va_list args;
		va_start(args, format);
		vsnprintf(row->text, sizeof(row->text), format, args);
		va_end(args);
	}

	void AddPositionRows(void* actor)
	{
		const Vec3 p = game::GetPosition(actor);
		AddNumberRow("X", static_cast<int>(p.x));
		AddNumberRow("Y", static_cast<int>(p.y));
		AddNumberRow("Z", static_cast<int>(p.z));
	}

	void GetFilePath(char* out, size_t size, const char* name)
	{
		snprintf(out, size, "%sWonderland\\%s", GameFolder(), name);
	}

	void SaveTeleports()
	{
		int length = 0;
		for (int i = 0; i < SlotCount; i++)
		{
			if (!slots[i].used)
				continue;
			const Vec3& p = slots[i].position;
			length += snprintf(fileText + length, sizeof(fileText) - length, "%d %d %d %d\r\n",
				i, static_cast<int>(p.x), static_cast<int>(p.y), static_cast<int>(p.z));
		}

		char path[MAX_PATH];
		GetFilePath(path, sizeof(path), "");
		CreateDirectoryA(path, nullptr);
		GetFilePath(path, sizeof(path), "TS3.cfg");

		HANDLE file = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (file == INVALID_HANDLE_VALUE)
			return;

		DWORD written = 0;
		WriteFile(file, fileText, static_cast<DWORD>(length), &written, nullptr);
		CloseHandle(file);
	}

	void LoadTeleports()
	{
		char path[MAX_PATH];
		GetFilePath(path, sizeof(path), "TS3.cfg");

		HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (file == INVALID_HANDLE_VALUE)
			return;

		DWORD length = 0;
		const BOOL ok = ReadFile(file, fileText, sizeof(fileText) - 1, &length, nullptr);
		CloseHandle(file);
		if (!ok)
			return;
		fileText[length] = '\0';

		for (Slot& slot : slots)
			slot.used = false;

		char* line = fileText;
		while (line < fileText + length)
		{
			char* lineEnd = strchr(line, '\n');
			if (lineEnd)
				*lineEnd = '\0';

			int index, x, y, z;
			if (sscanf(line, "%d %d %d %d", &index, &x, &y, &z) == 4 && index >= 0 && index < SlotCount)
				slots[index] = { true, { static_cast<float>(x), static_cast<float>(y), static_cast<float>(z) } };

			if (!lineEnd)
				break;
			line = lineEnd + 1;
		}
	}

	void* EntityIfExists(void* entity)
	{
		return entity && game::StillExists(entity) ? entity : nullptr;
	}

	void BuildPage(int page)
	{
		rowCount = 0;

		switch (page)
		{
		case PAGE_PLAYER:
		{
			SetPageTitle("Player");
			void* player = game::GetPlayer();
			if (!player)
			{
				AddRow("Load into a level first");
				break;
			}

			AddOnOffRow("God Mode", godMode, ACT_GOD_MODE);
			if (game::GetHealthObject(player))
				AddCustomRow(ACT_HEAL, 0, "Heal  %d / %d", static_cast<int>(game::GetCurrentHealth()), static_cast<int>(game::GetMaxHealth()));
			AddOnOffRow("Fly  (Space / Ctrl / Shift)", flying, ACT_FLY);
			AddLeftRightRow(ACT_FLY_SPEED, 10, "< Fly Speed  %d.%02d >", flySpeed / 100, flySpeed % 100);
			AddOnOffRow("Hover", hover, ACT_HOVER);
			AddLeftRightRow(ACT_RUN_SPEED, 25, "< Run Speed  %d%% >", runSpeed);
			AddLeftRightRow(ACT_PLAYER_SIZE, 25, "< Player Size  %d%% >", playerSize);
			AddRow("Super Jump", ACT_SUPER_JUMP, 1200);
			AddOnOffRow("Swap Character Anywhere", swapAnywhere, ACT_SWAP_ANYWHERE);
			break;
		}

		case PAGE_TELEPORT:
		{
			SetPageTitle("Teleport");
			void* player = game::GetPlayer();
			if (!player)
			{
				AddRow("No player found");
				break;
			}

			AddPositionRows(player);
			AddRow("Up 200 (unstick)", ACT_MOVE_UP, 200);
			AddRow("Up 1000", ACT_MOVE_UP, 1000);
			AddRow("Down 200", ACT_MOVE_DOWN, 200);

			const Slot& slot = slots[selectedSlot];
			AddLeftRightRow(ACT_PICK_SLOT, 1, "< Slot %d  %s >", selectedSlot + 1, slot.used ? "saved" : "empty");
			if (slot.used)
			{
				AddNumberRow("Go To X", static_cast<int>(slot.position.x), ACT_GO_TO_SLOT);
				AddRow("Overwrite This Slot", ACT_SAVE_SLOT);
			}
			else
			{
				AddRow("Save To This Slot", ACT_SAVE_SLOT);
			}
			AddRow("Save Teleports", ACT_SAVE_TELEPORTS);
			AddRow("Load Teleports", ACT_LOAD_TELEPORTS);
			break;
		}

		case PAGE_PROGRESS:
		{
			SetPageTitle("Progress");
			AddCustomRow(ACT_UNLOCK_ALL, 0, "Unlock Everything  (%d left)", game::CountLockedItems());
			AddRow("Buy All Shop Items (free)", ACT_BUY_ALL);
			AddRow("Max All Stats", ACT_MAX_STATS);
			AddOnOffRow("Keep Stats Maxed", keepMaxed, ACT_KEEP_MAXED);

			const int gold = game::StatsLoaded() ? game::FindStat("Stat_GOLD_COLLECTED") : -1;
			if (gold >= 0)
				AddNumberRow("Gold +10,000   have", game::GetStat(gold), ACT_ADD_GOLD, 10000);

			AddRow("Collectibles", ACT_OPEN_PAGE, PAGE_COLLECTIBLES);
			AddRow("Unlockables", ACT_OPEN_PAGE, PAGE_UNLOCKABLES);
			AddRow("Game Cheats", ACT_OPEN_PAGE, PAGE_CHEATS);
			break;
		}

		case PAGE_COLLECTIBLES:
			SetPageTitle("Collectibles");
			if (!game::StatsLoaded())
			{
				AddRow("Stats not loaded");
				break;
			}
			for (int i = 0; i < LengthOf(collectibles); i++)
			{
				const int id = game::FindStat(collectibles[i].stat);
				if (id < 0)
					AddRow(collectibles[i].label);
				else
					AddNumberRow(collectibles[i].label, game::GetStat(id), ACT_ADD_COLLECTIBLE, i);
			}
			break;

		case PAGE_UNLOCKABLES:
		{
			SetPageTitle("Unlockables");
			const int total = game::CountLocks();
			if (total == 0)
			{
				AddRow("Nothing to unlock here");
				break;
			}

			if (lockListStart >= total)
				lockListStart = 0;

			AddNumberRow("Unlocked", total - game::CountLockedItems());
			AddNumberRow("Total", total);
			AddRow("UNLOCK ALL", ACT_UNLOCK_ALL);
			if (lockListStart > 0)
				AddRow("< Previous", ACT_LOCK_LIST_PAGE, lockListStart - LocksPerPage);

			for (int i = lockListStart; i < total && i < lockListStart + LocksPerPage; i++)
			{
				void* lock = game::GetLock(i);
				AddOnOffRow(game::GetLockName(lock), game::IsUnlocked(lock), ACT_OPEN_LOCK, i);
			}

			if (lockListStart + LocksPerPage < total)
				AddRow("Next >", ACT_LOCK_LIST_PAGE, lockListStart + LocksPerPage);
			break;
		}

		case PAGE_UNLOCKABLE:
		{
			SetPageTitle("Unlockable");
			void* lock = game::GetLock(selectedLock);
			if (!lock)
			{
				AddRow("This item is gone");
				break;
			}
			AddRow(game::GetLockName(lock));
			AddOnOffRow("State", game::IsUnlocked(lock), ACT_NONE);
			AddRow("Unlock", ACT_SET_LOCK, 1);
			AddRow("Lock", ACT_SET_LOCK, 0);
			break;
		}

		case PAGE_CHEATS:
		{
			SetPageTitle("Cheats");
			const int count = game::CountCheats();
			if (count == 0)
			{
				AddRow("No cheats found");
				break;
			}
			for (int i = 0; i < count; i++)
			{
				if (void* cheat = game::GetCheat(i))
					AddOnOffRow(game::GetCheatName(cheat), game::IsCheatOn(cheat), ACT_TOGGLE_CHEAT, i);
			}
			break;
		}

		case PAGE_WORLD:
			SetPageTitle("World");
			AddLeftRightRow(ACT_GAME_SPEED, 1, "< Game Speed  %s >", gameSpeeds[CurrentSpeedChoice()].name);
			AddOnOffRow("Show HUD", game::IsHudShown(), ACT_SHOW_HUD);
			AddRow("Sky", ACT_OPEN_PAGE, PAGE_SKY);
			AddRow("Entities", ACT_OPEN_PAGE, PAGE_ENTITIES);
			AddRow("Mission Givers", ACT_OPEN_PAGE, PAGE_MISSIONS);
			break;

		case PAGE_SKY:
		{
			SetPageTitle("Sky");
			void* skies = game::GetLevelSkies();
			const int count = game::CountSkies(skies);
			if (count == 0)
			{
				AddRow("No skies for this level");
				break;
			}

			const int current = game::GetCurrentSky(skies);
			for (int i = 0; i < count; i++)
			{
				if (const char* name = game::GetSkyName(skies, i))
					AddOnOffRow(name, i == current, ACT_SET_SKY, i);
			}
			break;
		}

		case PAGE_ENTITIES:
		{
			SetPageTitle("Entities");
			int total = 0;
			game::GetEntity(-1, &total);
			if (total == 0)
			{
				AddRow("No entities found");
				break;
			}

			if (entityListStart >= total)
				entityListStart = 0;

			AddNumberRow("Total", total);
			if (entityListStart > 0)
				AddRow("< Previous", ACT_ENTITY_LIST_PAGE, entityListStart - EntitiesPerPage);

			int index = 0;
			game::ForEachEntity([&](void* entity)
			{
				if (index >= entityListStart + EntitiesPerPage)
					return true;
				if (index >= entityListStart)
					AddRow(game::GetEntityName(entity), ACT_OPEN_ENTITY, index);
				index++;
				return false;
			});

			if (entityListStart + EntitiesPerPage < total)
				AddRow("Next >", ACT_ENTITY_LIST_PAGE, entityListStart + EntitiesPerPage);
			break;
		}

		case PAGE_ENTITY:
		{
			SetPageTitle("Entity");
			void* entity = EntityIfExists(selectedEntity);
			if (!entity)
			{
				AddRow("This entity is gone");
				break;
			}

			const bool isPlayer = entity == game::GetPlayer();
			AddRow(game::GetEntityName(entity));
			AddPositionRows(entity);
			AddRow("Teleport To It", ACT_GO_TO_ENTITY);
			AddRow("Teleport Above It", ACT_GO_ABOVE_ENTITY);

			if (isPlayer)
				AddRow("Become: this is you");
			else if (!game::IsCharacter(entity))
				AddRow("Become: not a character");
			else if (!game::IsUnusedCharacter(entity))
				AddRow("Become: already in use");
			else
				AddRow("Become This Character", ACT_CONTROL_ENTITY);

			AddRow("Bring To Me", ACT_BRING_ENTITY);
			if (isPlayer)
				AddRow("Delete: can't delete yourself");
			else
				AddRow("Delete", ACT_DELETE_ENTITY);
			break;
		}

		case PAGE_MISSIONS:
		{
			SetPageTitle("Mission Givers");
			int index = 0;
			int found = 0;
			game::ForEachEntity([&](void* entity)
			{
				if (rowCount >= MaxRows - 1)
					return true;
				const char* name = game::GetEntityName(entity);
				if (strstr(name, "Giver"))
				{
					AddRow(name, ACT_OPEN_ENTITY, index);
					found++;
				}
				index++;
				return false;
			});
			if (found == 0)
				AddRow("No mission givers here");
			break;
		}

		case PAGE_MAIN:
		default:
			SetPageTitle("Wonderland");
			AddRow("Player", ACT_OPEN_PAGE, PAGE_PLAYER);
			AddRow("Teleport", ACT_OPEN_PAGE, PAGE_TELEPORT);
			AddRow("Progress", ACT_OPEN_PAGE, PAGE_PROGRESS);
			AddRow("World", ACT_OPEN_PAGE, PAGE_WORLD);
			AddRow("Close Menu", ACT_CLOSE_MENU);
			break;
		}
	}

	void FillList(void* list, int selected, int scrollPosition)
	{
		BuildPage(pageHistory[depth]);

		const int existing = game::CountItems(list);
		const int reused = existing < rowCount ? existing : rowCount;

		for (int i = 0; i < reused; i++)
			game::SetItemText(game::GetItem(list, i), rows[i].text, i);

		for (int i = reused; i < rowCount; i++)
		{
			game::AddItem(list, rows[i].text, i);
			game::SetItemText(game::GetItem(list, game::CountItems(list) - 1), rows[i].text, i);
		}

		for (int i = rowCount; i < existing; i++)
			game::HideItem(game::GetItem(list, i));

		game::SetTitle(list, pageTitle);
		game::RefreshList(list, rowCount, selected, scrollPosition);
	}

	void RedrawPage(void* screen, bool keepPosition = true)
	{
		void* list = game::GetMenuList(screen);
		if (!list)
			return;

		const int selected = keepPosition ? game::GetSelectedRow(list) : 0;
		const int scrollPosition = keepPosition ? game::GetScrollPosition(list) : 0;

		game::ShowHighlight(list, false);
		FillList(list, selected, scrollPosition);
		game::ShowHighlight(list, true);
	}

	void OpenPage(void* screen, int page)
	{
		if (depth + 1 >= MaxDepth)
			return;
		pageHistory[++depth] = page;
		RedrawPage(screen, false);
	}

	Vec3 PointAbove(void* actor, float height)
	{
		Vec3 p = game::GetPosition(actor);
		p.y += height;
		return p;
	}

	void SetFlying(bool on)
	{
		flying = on;
		flyHeldFrames = 0;
		flyingPlayer = nullptr;
		if (void* player = game::GetPlayer())
			flyHeight = game::GetPosition(player).y;
		if (!on)
		{
			game::SetCollision(true);
			game::StopPlayer();
		}
	}

	void DoAction(void* screen, int action, int value)
	{
		switch (action)
		{
		case ACT_OPEN_PAGE:
			OpenPage(screen, value);
			return;

		case ACT_CLOSE_MENU:
			wantOpen = false;
			return;

		case ACT_GOD_MODE:
			godMode = !godMode;
			game::SetInvincible(godMode);
			if (!godMode)
				game::SetPlayerFlag(offsets::CombatJuggernaut, false);
			break;

		case ACT_HEAL:
			game::Heal();
			break;

		case ACT_FLY:
			SetFlying(!flying);
			break;

		case ACT_FLY_SPEED:
			flySpeed = KeepBetween(flySpeed + value, 1, 2000);
			break;

		case ACT_HOVER:
			hover = !hover;
			break;

		case ACT_RUN_SPEED:
			runSpeed = KeepBetween(runSpeed + value, 25, 1000);
			game::SetRunSpeed(runSpeed);
			break;

		case ACT_PLAYER_SIZE:
			playerSize = KeepBetween(playerSize + value, 25, 500);
			game::SetPlayerSize(static_cast<float>(playerSize) / 100.0f);
			break;

		case ACT_SUPER_JUMP:
			game::SetUpwardSpeed(static_cast<float>(value));
			return;

		case ACT_SWAP_ANYWHERE:
			swapAnywhere = !swapAnywhere;
			if (swapAnywhere)
				game::UnlockCharacterSwap();
			break;

		case ACT_MOVE_UP:
			game::MovePlayerUp(static_cast<float>(value));
			break;

		case ACT_MOVE_DOWN:
			game::MovePlayerUp(-static_cast<float>(value));
			break;

		case ACT_PICK_SLOT:
			selectedSlot = LoopAround(selectedSlot + (value >= 0 ? 1 : -1), SlotCount);
			break;

		case ACT_SAVE_SLOT:
		{
			void* player = game::GetPlayer();
			if (!player)
				return;
			slots[selectedSlot] = { true, game::GetPosition(player) };
			break;
		}

		case ACT_GO_TO_SLOT:
			if (slots[selectedSlot].used)
				game::TeleportPlayer(slots[selectedSlot].position);
			return;

		case ACT_SAVE_TELEPORTS:
			SaveTeleports();
			break;

		case ACT_LOAD_TELEPORTS:
			LoadTeleports();
			RedrawPage(screen, false);
			return;

		case ACT_UNLOCK_ALL:
			game::UnlockAll();
			break;

		case ACT_BUY_ALL:
			game::BuyAllShopItems();
			break;

		case ACT_MAX_STATS:
			game::MaxAllStats();
			break;

		case ACT_KEEP_MAXED:
			keepMaxed = !keepMaxed;
			break;

		case ACT_ADD_GOLD:
		{
			const int id = game::FindStat("Stat_GOLD_COLLECTED");
			if (id >= 0)
				game::AddStat(id, value);
			break;
		}

		case ACT_ADD_COLLECTIBLE:
		{
			if (value < 0 || value >= LengthOf(collectibles))
				return;
			const int id = game::FindStat(collectibles[value].stat);
			if (id >= 0)
				game::AddStat(id, collectibles[value].amount);
			break;
		}

		case ACT_LOCK_LIST_PAGE:
			lockListStart = value < 0 ? 0 : value;
			RedrawPage(screen, false);
			return;

		case ACT_OPEN_LOCK:
			selectedLock = value;
			OpenPage(screen, PAGE_UNLOCKABLE);
			return;

		case ACT_SET_LOCK:
		{
			void* lock = game::GetLock(selectedLock);
			if (!lock)
				return;
			if (value)
				game::Unlock(lock);
			else
				game::LockAgain(lock);
			break;
		}

		case ACT_TOGGLE_CHEAT:
			game::ToggleCheat(game::GetCheat(value));
			break;

		case ACT_GAME_SPEED:
		{
			const int choice = KeepBetween(CurrentSpeedChoice() + (value >= 0 ? 1 : -1), 0, LengthOf(gameSpeeds) - 1);
			game::SetGameSpeed(gameSpeeds[choice].value);
			break;
		}

		case ACT_SHOW_HUD:
			game::ShowHud(!game::IsHudShown());
			break;

		case ACT_SET_SKY:
			game::SetSky(value);
			break;

		case ACT_ENTITY_LIST_PAGE:
			entityListStart = value < 0 ? 0 : value;
			RedrawPage(screen, false);
			return;

		case ACT_OPEN_ENTITY:
			selectedEntity = game::GetEntity(value, nullptr);
			if (selectedEntity)
				OpenPage(screen, PAGE_ENTITY);
			return;

		case ACT_GO_TO_ENTITY:
		case ACT_GO_ABOVE_ENTITY:
			if (void* entity = EntityIfExists(selectedEntity))
				game::TeleportPlayer(PointAbove(entity, action == ACT_GO_ABOVE_ENTITY ? 100.0f : 0.0f));
			return;

		case ACT_BRING_ENTITY:
		{
			void* entity = EntityIfExists(selectedEntity);
			void* player = game::GetPlayer();
			if (!entity || !player || entity == player)
				return;
			game::SetPosition(entity, PointAbove(player, 100.0f));
			break;
		}

		case ACT_CONTROL_ENTITY:
		{
			void* entity = EntityIfExists(selectedEntity);
			if (!entity)
				return;
			game::TakeControl(entity);
			break;
		}

		case ACT_DELETE_ENTITY:
		{
			void* entity = EntityIfExists(selectedEntity);
			if (!entity)
				return;
			game::RemoveEntity(entity);
			selectedEntity = nullptr;
			if (depth > 0)
				depth--;
			RedrawPage(screen, false);
			return;
		}

		default:
			return;
		}

		RedrawPage(screen);
	}

	void OpenMenu()
	{
		if (!teleportsLoaded)
		{
			teleportsLoaded = true;
			LoadTeleports();
		}

		depth = 0;
		pageHistory[0] = PAGE_MAIN;
		isOpen = true;

		game::OpenScreen("HelpScreen");
		if (!game::FindScreen("HelpScreen"))
		{
			isOpen = false;
			wantOpen = false;
		}
	}

	void ResetMenu()
	{
		isOpen = false;
		wantOpen = false;
		menuScreen = nullptr;
		depth = 0;
		selectedEntity = nullptr;
		closeAskedAt = 0;
	}

	void CloseMenu()
	{
		if (menuScreen && closeHandler)
		{
			if (closeAskedAt == 0)
				closeAskedAt = GetTickCount64();
			closeHandler(menuScreen);
		}
		else
		{
			ResetMenu();
		}
	}

	void UpdateFly(const Input& input)
	{
		void* player = game::GetPlayer();
		if (!player)
			return;

		if (player != flyingPlayer)
		{
			flyingPlayer = player;
			game::SetCollision(false);
		}

		const bool moving = input.forward || input.back || input.up || input.down;
		if (!moving)
			flyHeldFrames = 0;
		else if (flyHeldFrames < 75)
			flyHeldFrames++;

		const float boost = static_cast<float>(flyHeldFrames) / 75.0f;
		const float speed = (static_cast<float>(flySpeed) / 100.0f) * (1.0f + 11.0f * boost * boost);

		if (input.up)
			flyHeight += speed;
		if (input.down)
			flyHeight -= speed;

		Vec3 position = game::GetPosition(player);
		Vec3 facing;
		if ((input.forward || input.back) && game::GetFacingDirection(player, facing))
		{
			const float flatLength = facing.x * facing.x + facing.z * facing.z;
			if (flatLength > 1e-6f)
			{
				const float distance = (input.forward ? speed : -speed) / sqrtf(flatLength);
				position.x += facing.x * distance;
				position.z += facing.z * distance;
			}
		}

		position.y = flyHeight;
		game::TeleportPlayer(position);
		game::StopMoving(player);
	}

	void HandleLeftRight(int direction)
	{
		if (!isOpen || !menuScreen)
		{
			arrowDirection = 0;
			return;
		}

		const ULONGLONG now = GetTickCount64();
		int step = 0;

		if (direction == 0)
		{
			arrowDirection = 0;
		}
		else if (direction != arrowDirection)
		{
			arrowDirection = direction;
			arrowHeldSince = arrowLastStep = now;
			step = direction;
		}
		else if (now - arrowHeldSince > 350 && now - arrowLastStep > 90)
		{
			arrowLastStep = now;
			step = direction;
		}

		if (step == 0)
			return;

		void* list = game::GetMenuList(menuScreen);
		if (!list)
			return;

		const int selected = game::GetSelectedRow(list);
		if (selected >= 0 && selected < rowCount && rows[selected].leftRightAction != ACT_NONE)
			DoAction(menuScreen, rows[selected].leftRightAction, rows[selected].step * step);
	}
}

	void SetCloseHandler(CloseFn handler)
	{
		closeHandler = handler;
	}

	bool IsOpen()
	{
		return isOpen;
	}

	bool ShouldHideEscape()
	{
		return isOpen || wantOpen;
	}

	void OnScreenOpened(void* screen)
	{
		if (!isOpen)
			return;

		menuScreen = screen;
		Write<uint8_t>(screen, offsets::ScreenFromGameplay, 1);

		if (void* list = game::GetMenuList(screen))
			FillList(list, 0, 0);
	}

	bool OnSelect(void* screen)
	{
		if (!isOpen)
			return false;

		void* list = game::GetMenuList(screen);
		if (!list)
			return true;

		const ULONGLONG now = GetTickCount64();
		if (now - lastSelect < 250)
			return true;
		lastSelect = now;

		const int selected = game::GetSelectedRow(list);
		if (selected < 0 || selected >= rowCount)
			return true;

		const Row& row = rows[selected];
		if (row.action == ACT_NONE && row.leftRightAction != ACT_NONE)
			DoAction(screen, row.leftRightAction, row.step);
		else
			DoAction(screen, row.action, row.value);
		return true;
	}

	bool OnBack(void* screen)
	{
		if (!isOpen)
			return false;

		if (depth > 0)
		{
			depth--;
			RedrawPage(screen, false);
		}
		return true;
	}

	void OnScreenClosing(void* screen)
	{
		if (!isOpen)
			return;

		Write<uint8_t>(screen, offsets::HelpDismissed, 1);
		ResetMenu();
	}

	void Shutdown()
	{
		if (isOpen)
			CloseMenu();
	}

	void OnFrame(const Input& input)
	{
		if (input.toggle)
			wantOpen = !(isOpen || wantOpen);

		const ULONGLONG now = GetTickCount64();
		if (wantOpen && !isOpen)
		{
			OpenMenu();
		}
		else if (!wantOpen && isOpen && (closeAskedAt == 0 || now - lastCloseTry > 400))
		{
			lastCloseTry = now;
			CloseMenu();
		}

		if (isOpen && closeAskedAt != 0 && GetTickCount64() - closeAskedAt > 2000)
			ResetMenu();

		HandleLeftRight(input.leftRight);

		if (hover)
			game::SetUpwardSpeed(0.0f);
		if (keepMaxed)
			game::MaxAllStats();
		if (flying)
			UpdateFly(input);
		if (godMode)
		{
			game::SetInvincible(true);
			game::SetPlayerFlag(offsets::CombatJuggernaut, true);
			game::Heal();
		}

		if (playerSize != 100)
		{
			const float wanted = static_cast<float>(playerSize) / 100.0f;
			const float current = game::GetPlayerSize();
			if (current > wanted + 0.001f || current < wanted - 0.001f)
				game::SetPlayerSize(wanted);
		}

		if (swapAnywhere && now >= nextSwap)
		{
			game::UnlockCharacterSwap();
			nextSwap = now + 1000;
		}

		if (runSpeed != 100)
			game::SetRunSpeed(runSpeed);
	}
}
