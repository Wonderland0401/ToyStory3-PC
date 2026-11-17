#pragma once
#include "Common.h"

struct Vec3
{
	float x, y, z;
};

namespace game
{
	void* GetScreenManager();
	void OpenScreen(const char* name);
	void* FindScreen(const char* name);

	void* GetMenuList(void* helpScreen);
	int CountItems(void* list);
	void* GetItem(void* list, int index);
	void SetItemText(void* item, const char* text, int userData);
	void HideItem(void* item);
	void AddItem(void* list, const char* text, int userData);
	void SetTitle(void* list, const char* text);
	void RefreshList(void* list, int rows, int selected, int scrollPosition);
	void ShowHighlight(void* list, bool on);
	int GetSelectedRow(void* list);
	int GetScrollPosition(void* list);
	bool HasSelectedRow(void* helpScreen);

	int CountCheats();
	void* GetCheat(int index);
	const char* GetCheatName(void* cheat);
	bool IsCheatOn(void* cheat);
	void ToggleCheat(void* cheat);
	int GetGameSpeed();
	void SetGameSpeed(int value);

	bool StatsLoaded();
	int CountStats();
	int FindStat(const char* name);
	int GetStat(int id);
	const char* GetStatName(int id);
	void AddStat(int id, int amount);
	int MaxAllStats();

	void* GetPlayer();
	Vec3 GetPosition(void* actor);
	void SetPosition(void* actor, const Vec3& position);
	bool TeleportPlayer(const Vec3& position);
	bool MovePlayerUp(float amount);
	void SetUpwardSpeed(float speed);
	void StopMoving(void* actor);
	void StopPlayer();
	bool GetFacingDirection(void* actor, Vec3& out);

	void SetPlayerFlag(int flag, bool on);
	void* GetHealthObject(void* actor);
	float GetCurrentHealth();
	float GetMaxHealth();
	void Heal();
	void SetInvincible(bool on);
	float GetPlayerSize();
	void SetPlayerSize(float size);
	bool IsHudShown();
	void ShowHud(bool show);
	void UnlockCharacterSwap();
	bool SetCollision(bool enabled);
	void* FindComponent(void* actor, const char* name);
	void SetRunSpeed(int percent);

	constexpr int MaxEntitiesToCheck = 8192;

	template <class Callback>
	void ForEachEntity(Callback&& callback)
	{
		using namespace mem;
		void* registry = ReadGlobalPointer(addr::ActorRegistry);
		if (!IsGoodPointer(registry))
			return;

		int checked = 0;
		for (size_t listStart : offsets::RegistryLists)
		{
			for (void* node = ReadPointer(registry, listStart); IsGoodPointer(node) && checked < MaxEntitiesToCheck; node = ReadPointer(node, offsets::NodeNext), checked++)
			{
				void* entity = ReadPointer(node, offsets::NodeActor);
				if (IsGoodPointer(entity) && callback(entity))
					return;
			}
		}
	}

	void* GetEntity(int index, int* total);
	bool StillExists(void* entity);
	const char* GetEntityName(void* entity);
	bool IsCharacter(void* entity);
	bool IsUnusedCharacter(void* entity);
	bool TakeControl(void* entity);
	void RemoveEntity(void* entity);

	int CountLocks();
	void* GetLock(int index);
	const char* GetLockName(void* lock);
	bool IsUnlocked(void* lock);
	bool Unlock(void* lock);
	void LockAgain(void* lock);
	int UnlockAll();
	int BuyAllShopItems();
	int CountLockedItems();

	void* GetLevelSkies();
	int CountSkies(void* skies);
	const char* GetSkyName(void* skies, int index);
	int GetCurrentSky(void* skies);
	bool SetSky(int index);
}
