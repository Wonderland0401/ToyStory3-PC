#include "Game.h"

using namespace mem;

namespace
{
	typedef void* (*FindScreenFn)(void* manager, const char* name, int slot);
	typedef void (*OpenScreenFn)(void* manager, const char* name, int slot);
	typedef void* (*AddItemFn)(void* list, const char* label, int64_t userData, char literal);
	typedef void (*ListFn)(void* list);
	typedef void (*HighlightFn)(void* list, int on);
	typedef void (*AssignTextFn)(void* item, const char* text, int length);
	typedef void (*SetTextFn)(void* node, const char* text);
	typedef bool (*CheatFn)(void* cheat);
	typedef void (*StatsAddFn)(void* stats, int id, int amount);
	typedef void (*UpdatePhysicsFn)(void* movement);
	typedef void (*ActorFn)(void* actor);
	typedef void (*ActorIntFn)(void* actor, int value);
	typedef void* (*GetPartFn)(void* actor, int id);
	typedef float (*HealthGetFn)(void* health);
	typedef void (*HealthAddFn)(void* health, float amount);
	typedef void (*SetInvincibleFn)(void* health, char on, float seconds, int effect, void* fx);
	typedef void (*CollisionFn)(void* collision, int enabled);
	typedef void* (*ComponentFn)(void* actor, const char* name);
	typedef void (*RotationToMatrixFn)(float* out, const float* rotation);
	typedef void (*SetAvatarFn)(void* manager, unsigned player, void* actor);
	typedef char (*LockUnlockFn)(void* lock);
	typedef char (*LockRewardFn)(void* manager, const char* name, unsigned state);
	typedef void (*SkyApplyFn)(void* manager, void* sky, char flag);
	typedef const char* (*LevelNameFn)();
	typedef void (*VoidFn)();

	void* GetStatsTable()
	{
		void* stats = ReadGlobalPointer(addr::PlayerStats);
		return IsGoodPointer(stats) ? stats : nullptr;
	}

	uint8_t* GetStatEntry(int id)
	{
		void* stats = GetStatsTable();
		if (!stats || id < 0 || id >= game::CountStats())
			return nullptr;
		uint8_t* entries = Read<uint8_t*>(stats, offsets::StatsRecords);
		return IsGoodPointer(entries) ? entries + static_cast<size_t>(id) * offsets::StatStride : nullptr;
	}

	int ReadStat(int id, size_t offset, int fallback)
	{
		uint8_t* entry = GetStatEntry(id);
		return entry ? Read<int>(entry, offset) : fallback;
	}

	bool HasLimit(int id)
	{
		return ReadStat(id, offsets::StatKind, -1) == 5;
	}

	bool IsWholeNumber(int id)
	{
		return ReadStat(id, offsets::StatValueType, -1) == 0;
	}

	int FindMatchingTotal(const char* name)
	{
		static const char* const endings[] = { "_FOUND", "_COLLECTED", "_COUNT" };
		static const char* const totals[] = { "_TOTAL", "_AVAILABLE", "_MAX" };

		const size_t length = strlen(name);
		for (const char* ending : endings)
		{
			const size_t endingLength = strlen(ending);
			if (length <= endingLength || length >= 56 || strcmp(name + length - endingLength, ending) != 0)
				continue;

			for (const char* total : totals)
			{
				char totalName[64];
				snprintf(totalName, sizeof(totalName), "%.*s%s", static_cast<int>(length - endingLength), name, total);
				const int id = game::FindStat(totalName);
				if (id >= 0 && game::GetStat(id) > 0)
					return game::GetStat(id);
			}
		}
		return -1;
	}

	void UpdatePhysics(void* actor)
	{
		if (auto update = GameFunction<UpdatePhysicsFn>(addr::SyncPhysics))
			update(static_cast<uint8_t*>(actor) + offsets::ActorMovement);
	}

	int GetPlayerFlags(void* actor)
	{
		if (!IsGoodPointer(actor))
			return 0;
		void* combat = ReadPointer(actor, offsets::ActorCombat);
		return IsGoodPointer(combat) ? Read<int>(combat, offsets::CombatFlags) : 0;
	}

	int GetHealthId()
	{
		if (!AddressOf(addr::HealthInterfaceId) || !AddressOf(addr::InterfaceIdCounter))
			return 0;
		int& id = GameValue<int>(addr::HealthInterfaceId);
		if (id == 0)
			id = ++GameValue<int>(addr::InterfaceIdCounter);
		return id;
	}

	void* GetPlayerHealth()
	{
		void* health = game::GetHealthObject(game::GetPlayer());
		return health && IsGoodPointer(GetMethods(health)) ? health : nullptr;
	}

	const char* NameOrDefault(const char* name)
	{
		return IsGoodText(name) && *name ? name : "(unnamed)";
	}

	int GetLockState(void* lock)
	{
		return IsGoodPointer(lock) ? Read<int>(lock, offsets::LockState) : -1;
	}

	bool BasicUnlock(void* lock)
	{
		if (!IsGoodPointer(lock) || game::IsUnlocked(lock))
			return false;
		auto unlock = GameFunction<LockUnlockFn>(addr::LockUnlock);
		return unlock && unlock(lock) != 0;
	}

	void* GetSky(void* skies, int index)
	{
		if (index < 0 || index >= game::CountSkies(skies))
			return nullptr;
		void** list = Read<void**>(skies, offsets::GroupSkies);
		if (!IsGoodPointer(list))
			return nullptr;
		return IsGoodPointer(list[index]) ? list[index] : nullptr;
	}

	void* GetItemList(void* list)
	{
		void* items = ReadPointer(list, offsets::ListItems);
		return IsGoodPointer(items) ? items : nullptr;
	}
}

namespace game
{
	void* GetScreenManager()
	{
		void* manager = ReadGlobalPointer(addr::ScreenManager);
		return IsGoodPointer(manager) ? manager : nullptr;
	}

	void OpenScreen(const char* name)
	{
		void* manager = GetScreenManager();
		if (!manager || !IsGoodPointer(GetMethods(manager)))
			return;
		Method<OpenScreenFn>(manager, offsets::ScreenManagerOpen)(manager, name, offsets::ScreenAnySlot);
	}

	void* FindScreen(const char* name)
	{
		void* manager = GetScreenManager();
		auto find = GameFunction<FindScreenFn>(addr::FindScreenByName);
		return manager && find ? find(manager, name, offsets::ScreenAnySlot) : nullptr;
	}

	void* GetMenuList(void* helpScreen)
	{
		if (!IsGoodPointer(helpScreen))
			return nullptr;
		void* baseMenu = ReadPointer(helpScreen, offsets::HelpBaseMenu);
		if (!IsGoodPointer(baseMenu))
			return nullptr;
		void* list = ReadPointer(baseMenu, offsets::BaseMenuList);
		return IsGoodPointer(list) ? list : nullptr;
	}

	int CountItems(void* list)
	{
		void* items = GetItemList(list);
		if (!items)
			return 0;
		const int count = Read<int>(items, 8);
		return count > 0 && count < 4096 ? count : 0;
	}

	void* GetItem(void* list, int index)
	{
		void* items = GetItemList(list);
		if (!items || index < 0 || index >= CountItems(list))
			return nullptr;
		void** data = Read<void**>(items, 0);
		if (!IsGoodPointer(data))
			return nullptr;
		return IsGoodPointer(data[index]) ? data[index] : nullptr;
	}

	void SetItemText(void* item, const char* text, int userData)
	{
		auto assign = GameFunction<AssignTextFn>(addr::ItemAssignText);
		if (!item || !assign)
			return;

		const char* current = Read<const char*>(item, offsets::ItemLabel);
		if (IsGoodText(current) && strcmp(current, text) != 0)
		{
			volatile LONG* users = reinterpret_cast<volatile LONG*>(const_cast<char*>(current) - 8);
			if (*users >= 1 && *users < 0x10000)
				InterlockedIncrement(users);
		}

		assign(item, text, static_cast<int>(strlen(text)));
		Write<uint8_t>(item, offsets::ItemLiteral, 1);
		Write<uint8_t>(item, offsets::ItemVisible, 1);
		Write<int64_t>(item, offsets::ItemUserData, userData);
	}

	void HideItem(void* item)
	{
		if (item)
			Write<uint8_t>(item, offsets::ItemVisible, 0);
	}

	void AddItem(void* list, const char* text, int userData)
	{
		if (auto add = GameFunction<AddItemFn>(addr::MenuListAddItem))
			add(list, text, userData, 1);
	}

	void SetTitle(void* list, const char* text)
	{
		void* title = ReadPointer(list, offsets::ListTitle);
		if (IsGoodPointer(title) && IsGoodPointer(GetMethods(title)))
			Method<SetTextFn>(title, offsets::TextSetLiteral)(title, text);
	}

	void RefreshList(void* list, int rows, int selected, int scrollPosition)
	{
		if (rows < 0)
			rows = 0;

		const int lastScroll = rows > offsets::ListVisibleRows ? rows - offsets::ListVisibleRows : 0;
		if (scrollPosition < 0)
			scrollPosition = 0;
		if (scrollPosition > lastScroll)
			scrollPosition = lastScroll;
		if (selected < 0)
			selected = 0;
		if (rows > 0 && selected >= rows)
			selected = rows - 1;

		Write<int>(list, offsets::ListCount, rows);
		Write<int>(list, offsets::ListScrollTop, scrollPosition);
		Write<int>(list, offsets::ListSelection, selected);
		Write<int>(list, offsets::ListPending, -1);

		if (auto redraw = GameFunction<ListFn>(addr::MenuListPopulate))
			redraw(list);
	}

	void ShowHighlight(void* list, bool on)
	{
		if (auto highlight = GameFunction<HighlightFn>(addr::MenuListHighlight))
			highlight(list, on ? 1 : 0);
	}

	int GetSelectedRow(void* list)
	{
		return Read<int>(list, offsets::ListSelection);
	}

	int GetScrollPosition(void* list)
	{
		return Read<int>(list, offsets::ListScrollTop);
	}

	bool HasSelectedRow(void* helpScreen)
	{
		void* list = GetMenuList(helpScreen);
		if (!list)
			return false;

		const int count = CountItems(list);
		const int selected = GetSelectedRow(list);
		if (count <= 0 || selected >= count)
			return false;

		return GetItem(list, selected < 0 ? count - 1 : selected) != nullptr;
	}

	int CountCheats()
	{
		const int count = GameValue<int>(addr::CheatCount);
		if (count < 0)
			return 0;
		return count > offsets::CheatMax ? offsets::CheatMax : count;
	}

	void* GetCheat(int index)
	{
		const uintptr_t table = AddressOf(addr::CheatTable);
		if (!table || index < 0 || index >= CountCheats())
			return nullptr;
		return reinterpret_cast<void*>(table + static_cast<size_t>(index) * offsets::CheatStride);
	}

	const char* GetCheatName(void* cheat)
	{
		return NameOrDefault(Read<const char*>(cheat, offsets::CheatName));
	}

	bool IsCheatOn(void* cheat)
	{
		return Read<uint8_t>(cheat, offsets::CheatEnabled) != 0;
	}

	void ToggleCheat(void* cheat)
	{
		if (!cheat)
			return;

		const bool turnOn = !IsCheatOn(cheat);
		void* onChange = ReadPointer(cheat, offsets::CheatCallback);

		if (turnOn && onChange)
		{
			for (int i = 0; i < CountCheats(); i++)
			{
				void* other = GetCheat(i);
				if (other != cheat && ReadPointer(other, offsets::CheatCallback) == onChange)
					Write<uint8_t>(other, offsets::CheatEnabled, 0);
			}
		}

		Write<uint8_t>(cheat, offsets::CheatEnabled, turnOn ? 1 : 0);

		if (onChange)
			reinterpret_cast<CheatFn>(onChange)(cheat);
	}

	int GetGameSpeed()
	{
		return GameValue<int>(addr::TimeSpeedIndex);
	}

	void SetGameSpeed(int value)
	{
		GameValue<int>(addr::TimeSpeedIndex) = value;
		GameValue<uint8_t>(addr::TimeSpeedDirty) = 1;
	}

	bool StatsLoaded()
	{
		return CountStats() > 0;
	}

	int CountStats()
	{
		void* stats = GetStatsTable();
		if (!stats)
			return 0;
		const int count = Read<int>(stats, offsets::StatsCount);
		return count > 0 && count < 100000 ? count : 0;
	}

	int FindStat(const char* name)
	{
		const int count = CountStats();
		for (int id = 0; id < count; id++)
		{
			uint8_t* entry = GetStatEntry(id);
			if (entry && strcmp(reinterpret_cast<const char*>(entry + offsets::StatName), name) == 0)
				return id;
		}
		return -1;
	}

	int GetStat(int id)
	{
		return ReadStat(id, offsets::StatValue, 0);
	}

	const char* GetStatName(int id)
	{
		uint8_t* entry = GetStatEntry(id);
		if (!entry)
			return "(none)";
		const char* name = reinterpret_cast<const char*>(entry + offsets::StatName);
		return *name ? name : "(unnamed)";
	}

	void AddStat(int id, int amount)
	{
		void* stats = GetStatsTable();
		if (!stats || !GetStatEntry(id) || amount == 0)
			return;
		if (auto add = GameFunction<StatsAddFn>(addr::StatsAdd))
			add(stats, id, amount);
	}

	int MaxAllStats()
	{
		const int count = CountStats();
		int changed = 0;

		for (int id = 0; id < count; id++)
		{
			if (!IsWholeNumber(id))
				continue;

			const int value = GetStat(id);
			if (HasLimit(id))
			{
				const int limit = ReadStat(id, offsets::StatMax, 0);
				if (value < limit)
				{
					AddStat(id, limit - value);
					changed++;
				}
				continue;
			}

			if (ReadStat(id, offsets::StatKind, -1) != 0)
				continue;

			const int total = FindMatchingTotal(GetStatName(id));
			if (total > value)
			{
				AddStat(id, total - value);
				changed++;
			}
		}
		return changed;
	}

	void* GetPlayer()
	{
		void* manager = ReadGlobalPointer(addr::PlayerManager);
		if (!IsGoodPointer(manager))
			return nullptr;
		void* player = ReadPointer(manager, offsets::PlayerAvatar);
		return IsGoodPointer(player) ? player : nullptr;
	}

	Vec3 GetPosition(void* actor)
	{
		return actor ? Read<Vec3>(actor, offsets::ActorPosition) : Vec3{};
	}

	void SetPosition(void* actor, const Vec3& position)
	{
		if (!IsGoodPointer(actor))
			return;

		Write<Vec3>(actor, offsets::ActorPosition, position);
		UpdatePhysics(actor);

		if (!IsGoodPointer(GetMethods(actor)))
			return;
		Method<ActorIntFn>(actor, offsets::ActorInvalidate)(actor, 0);
		Method<ActorFn>(actor, offsets::ActorUpdate)(actor);
	}

	bool TeleportPlayer(const Vec3& position)
	{
		void* player = GetPlayer();
		if (!player)
			return false;
		SetPosition(player, position);
		return true;
	}

	bool MovePlayerUp(float amount)
	{
		void* player = GetPlayer();
		if (!player)
			return false;
		Vec3 position = GetPosition(player);
		position.y += amount;
		SetPosition(player, position);
		return true;
	}

	void SetUpwardSpeed(float speed)
	{
		void* player = GetPlayer();
		if (!player)
			return;
		Write<float>(player, offsets::ActorVelocity + 4, speed);
		UpdatePhysics(player);
	}

	void StopMoving(void* actor)
	{
		Write<Vec3>(actor, offsets::ActorVelocity, Vec3{});
	}

	void StopPlayer()
	{
		void* player = GetPlayer();
		if (!player)
			return;
		StopMoving(player);
		UpdatePhysics(player);
	}

	bool GetFacingDirection(void* actor, Vec3& out)
	{
		if (!IsGoodPointer(actor))
			return false;

		auto toMatrix = GameFunction<RotationToMatrixFn>(addr::QuatToBasis);
		const uint8_t* data = reinterpret_cast<const uint8_t*>(AddressOf(addr::DefaultOrientation));
		void* rotation = ReadPointer(actor, offsets::ActorOrientation);

		if (IsGoodPointer(rotation))
		{
			void* owner = ReadPointer(rotation, 0x38);
			if (IsGoodPointer(owner) && Read<uint16_t>(rotation, 0x40) != Read<uint16_t>(owner, 0x40))
			{
				void* updater = ReadPointer(rotation, 0x30);
				if (IsGoodPointer(updater) && IsGoodPointer(GetMethods(updater)))
					Method<ActorFn>(updater, 0x78)(updater);
			}
			data = static_cast<const uint8_t*>(rotation) + 0x48;
		}

		if (!data || !toMatrix)
			return false;

		alignas(16) float angle[4];
		memcpy(angle, data + 0x10, sizeof(angle));

		alignas(16) float matrix[16] = {};
		toMatrix(matrix, angle);

		out = { matrix[6], matrix[7], matrix[8] };
		return true;
	}

	void SetPlayerFlag(int flag, bool on)
	{
		void* player = GetPlayer();
		if (!player)
			return;

		const int flags = GetPlayerFlags(player);
		const int newFlags = on ? flags | flag : flags & ~flag;
		if (newFlags == flags)
			return;

		if (auto set = GameFunction<ActorIntFn>(addr::SetCombatFlags))
			set(player, newFlags);
	}

	void* GetHealthObject(void* actor)
	{
		if (!IsGoodPointer(actor))
			return nullptr;

		const int id = GetHealthId();
		if (id == 0 || !IsGoodPointer(GetMethods(actor)))
			return nullptr;

		void* health = Method<GetPartFn>(actor, offsets::ActorGetInterface)(actor, id);
		return IsGoodPointer(health) ? health : nullptr;
	}

	float GetCurrentHealth()
	{
		void* health = GetPlayerHealth();
		return health ? Method<HealthGetFn>(health, offsets::HealthGet)(health) : 0.0f;
	}

	float GetMaxHealth()
	{
		void* health = GetPlayerHealth();
		return health ? Method<HealthGetFn>(health, offsets::HealthMax)(health) : 0.0f;
	}

	void Heal()
	{
		void* health = GetPlayerHealth();
		if (!health)
			return;

		const float current = Method<HealthGetFn>(health, offsets::HealthGet)(health);
		const float max = Method<HealthGetFn>(health, offsets::HealthMax)(health);
		if (max - current > 0.0f)
			Method<HealthAddFn>(health, offsets::HealthAdd)(health, max - current);
	}

	void SetInvincible(bool on)
	{
		if (void* health = GetPlayerHealth())
			Method<SetInvincibleFn>(health, offsets::HealthSetInvulnerable)(health, on ? 1 : 0, on ? 1.0e9f : 0.0f, 0, nullptr);
	}

	float GetPlayerSize()
	{
		void* player = GetPlayer();
		return player ? Read<float>(player, offsets::ActorScale + 4) : 1.0f;
	}

	void SetPlayerSize(float size)
	{
		void* player = GetPlayer();
		if (!player || size <= 0.0f)
			return;

		Write<float>(player, offsets::ActorScale + 0, size);
		Write<float>(player, offsets::ActorScale + 4, size);
		Write<float>(player, offsets::ActorScale + 8, size);
		Write<float>(player, offsets::ActorScale + 12, 0.0f);
		UpdatePhysics(player);
	}

	bool IsHudShown()
	{
		void* hud = ReadGlobalPointer(addr::Hud);
		return IsGoodPointer(hud) ? Read<uint8_t>(hud, offsets::HudVisible) != 0 : true;
	}

	void ShowHud(bool show)
	{
		void* hud = ReadGlobalPointer(addr::Hud);
		if (IsGoodPointer(hud))
			Write<uint8_t>(hud, offsets::HudVisible, show ? 1 : 0);
	}

	void UnlockCharacterSwap()
	{
		if (auto allowSwap = GameFunction<VoidFn>(addr::MakeAllSwappable))
			allowSwap();
	}

	bool SetCollision(bool enabled)
	{
		void* player = GetPlayer();
		if (!player)
			return false;

		void* collision = ReadPointer(player, offsets::ActorCollision);
		auto setEnabled = GameFunction<CollisionFn>(addr::CollisionSetEnabled);
		if (!IsGoodPointer(collision) || !setEnabled)
			return false;

		setEnabled(collision, enabled ? 1 : 0);
		return true;
	}

	void* FindComponent(void* actor, const char* name)
	{
		auto find = GameFunction<ComponentFn>(addr::GetComponentByName);
		if (!IsGoodPointer(actor) || !find)
			return nullptr;
		void* component = find(actor, name);
		return IsGoodPointer(component) ? component : nullptr;
	}

	void SetRunSpeed(int percent)
	{
		if (void* controller = FindComponent(GetPlayer(), "TS3GroundController"))
			Write<float>(controller, offsets::GroundMoveScale, static_cast<float>(percent) / 100.0f);
	}

	void* GetEntity(int index, int* total)
	{
		void* found = nullptr;
		int seen = 0;
		ForEachEntity([&](void* entity)
		{
			if (seen++ == index)
				found = entity;
			return false;
		});

		if (total)
			*total = seen;
		return found;
	}

	bool StillExists(void* entity)
	{
		if (!IsGoodPointer(entity))
			return false;

		bool exists = false;
		ForEachEntity([&](void* other)
		{
			exists = other == entity;
			return exists;
		});
		return exists;
	}

	const char* GetEntityName(void* entity)
	{
		return IsGoodPointer(entity) ? NameOrDefault(Read<const char*>(entity, offsets::ActorName)) : "(null)";
	}

	bool IsCharacter(void* entity)
	{
		if (!IsGoodPointer(entity))
			return false;
		const uint32_t mask = GameValue<uint32_t>(addr::AvatarClassMask);
		const uint32_t type = GameValue<uint32_t>(addr::AvatarClassId);
		return mask != 0 && (Read<uint32_t>(entity, offsets::ActorClass) & mask) == type;
	}

	bool IsUnusedCharacter(void* entity)
	{
		return IsCharacter(entity) && Read<int>(entity, offsets::ActorPlayerIndex) == -1;
	}

	bool TakeControl(void* entity)
	{
		void* manager = ReadGlobalPointer(addr::PlayerManager);
		auto setAvatar = GameFunction<SetAvatarFn>(addr::SetPlayerAvatar);
		if (!IsGoodPointer(manager) || !setAvatar || !IsUnusedCharacter(entity))
			return false;
		setAvatar(manager, 0, entity);
		return GetPlayer() == entity;
	}

	void RemoveEntity(void* entity)
	{
		if (IsGoodPointer(entity) && entity != GetPlayer())
			Write<int>(entity, offsets::ActorShutdown, 2);
	}

	int CountLocks()
	{
		if (!IsGoodPointer(ReadGlobalPointer(addr::LockManager)))
			return 0;
		const int count = GameValue<int>(addr::LockCount);
		return count > 0 && count < 100000 ? count : 0;
	}

	void* GetLock(int index)
	{
		void* manager = ReadGlobalPointer(addr::LockManager);
		if (!IsGoodPointer(manager) || index < 0 || index >= CountLocks())
			return nullptr;
		void** locks = Read<void**>(manager, offsets::LockArray);
		if (!IsGoodPointer(locks))
			return nullptr;
		return IsGoodPointer(locks[index]) ? locks[index] : nullptr;
	}

	const char* GetLockName(void* lock)
	{
		return IsGoodPointer(lock) ? NameOrDefault(Read<const char*>(lock, offsets::LockName)) : "(null)";
	}

	bool IsUnlocked(void* lock)
	{
		return IsGoodPointer(lock) && Read<int>(lock, offsets::LockState) != 0;
	}

	bool Unlock(void* lock)
	{
		if (!IsGoodPointer(lock) || IsUnlocked(lock))
			return false;

		void* manager = ReadGlobalPointer(addr::LockManager);
		const char* name = Read<const char*>(lock, offsets::LockName);
		auto reward = GameFunction<LockRewardFn>(addr::LockReward);
		if (!reward || !IsGoodPointer(manager) || !IsGoodText(name))
			return BasicUnlock(lock);

		unsigned state = static_cast<unsigned>(Read<int>(lock, offsets::LockRewardState));
		if (state == 0)
			state = 1;
		return reward(manager, name, state) != 0;
	}

	void LockAgain(void* lock)
	{
		if (IsGoodPointer(lock))
			Write<int>(lock, offsets::LockState, 0);
	}

	int UnlockAll()
	{
		int total = 0;
		for (int attempt = 0; attempt < 8; attempt++)
		{
			int unlocked = 0;
			const int count = CountLocks();
			for (int i = 0; i < count; i++)
			{
				void* lock = GetLock(i);
				if (lock && !IsUnlocked(lock) && Unlock(lock))
					unlocked++;
			}
			total += unlocked;
			if (unlocked == 0)
				break;
		}
		return total;
	}

	int BuyAllShopItems()
	{
		int bought = 0;
		const int count = CountLocks();
		for (int i = 0; i < count; i++)
		{
			void* lock = GetLock(i);
			if (lock && GetLockState(lock) == offsets::LockPurchasable)
			{
				Write<int>(lock, offsets::LockState, offsets::LockPurchased);
				bought++;
			}
		}
		return bought;
	}

	int CountLockedItems()
	{
		int locked = 0;
		const int count = CountLocks();
		for (int i = 0; i < count; i++)
		{
			if (!IsUnlocked(GetLock(i)))
				locked++;
		}
		return locked;
	}

	void* GetLevelSkies()
	{
		void* manager = ReadGlobalPointer(addr::SkyManager);
		auto getLevelName = GameFunction<LevelNameFn>(addr::GetLevelName);
		if (!IsGoodPointer(manager) || !getLevelName)
			return nullptr;

		const char* level = getLevelName();
		if (!IsGoodText(level))
			return nullptr;

		void** groups = Read<void**>(manager, offsets::SkyGroups);
		const uint64_t count = Read<uint64_t>(manager, offsets::SkyGroupCount);
		if (!IsGoodPointer(groups) || count == 0 || count > 4096)
			return nullptr;

		for (uint64_t i = 0; i < count; i++)
		{
			void* group = groups[i];
			if (!IsGoodPointer(group))
				continue;
			const char* name = Read<const char*>(group, offsets::GroupName);
			if (IsGoodText(name) && strcmp(name, level) == 0)
				return group;
		}
		return nullptr;
	}

	int CountSkies(void* skies)
	{
		if (!IsGoodPointer(skies))
			return 0;
		const uint64_t count = Read<uint64_t>(skies, offsets::GroupSkyCount);
		return count > 0 && count < 1024 ? static_cast<int>(count) : 0;
	}

	const char* GetSkyName(void* skies, int index)
	{
		void* sky = GetSky(skies, index);
		if (!sky)
			return nullptr;
		const char* name = Read<const char*>(sky, 0);
		return IsGoodText(name) ? name : nullptr;
	}

	int GetCurrentSky(void* skies)
	{
		return IsGoodPointer(skies) ? Read<int>(skies, offsets::GroupCurrent) : -1;
	}

	bool SetSky(int index)
	{
		void* manager = ReadGlobalPointer(addr::SkyManager);
		void* skies = GetLevelSkies();
		void* sky = GetSky(skies, index);
		auto apply = GameFunction<SkyApplyFn>(addr::SkyApply);
		if (!IsGoodPointer(manager) || !sky || !apply)
			return false;

		Write<int>(skies, offsets::GroupCurrent, index);
		Write<int>(manager, offsets::SkyCurrent, index);
		apply(manager, sky, 1);
		return true;
	}
}
