#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace addr
{
	enum Id : int
	{
		GamepadUpdate, HelpOnAccept, HelpOnCancel, HelpOnOpen, HelpOnClose,
		FindScreenByName, MenuListAddItem, MenuListPopulate, MenuListHighlight, ItemAssignText,
		StatsAdd, SetPlayerAvatar, SyncPhysics, SetCombatFlags, CollisionSetEnabled,
		GetComponentByName, QuatToBasis, LockUnlock, SkyApply, GetLevelName,
		LockReward, MakeAllSwappable,

		ScreenManager, CheatTable, CheatCount, TimeSpeedIndex, TimeSpeedDirty,
		PlayerStats, PlayerManager, AvatarClassId, AvatarClassMask, DefaultOrientation,
		ActorRegistry, LockManager, LockCount, SkyManager,
		HealthInterfaceId, InterfaceIdCounter, Hud,

		GamepadUpdateSlot, HelpOnAcceptSlot, HelpOnCancelSlot, HelpOnOpenSlot, HelpOnCloseSlot,

		Count
	};
}

namespace offsets
{
	constexpr int ScreenAnySlot = -1;
	constexpr size_t ScreenManagerOpen = 32;
	constexpr size_t ScreenFromGameplay = 598;
	constexpr size_t HelpBaseMenu = 608;
	constexpr size_t HelpDismissed = 617;
	constexpr size_t BaseMenuList = 608;

	constexpr size_t ListPending = 8;
	constexpr size_t ListCount = 40;
	constexpr size_t ListScrollTop = 44;
	constexpr size_t ListSelection = 48;
	constexpr size_t ListItems = 56;
	constexpr size_t ListTitle = 848;
	constexpr size_t TextSetLiteral = 1360;
	constexpr int ListVisibleRows = 7;

	constexpr size_t ItemLabel = 0;
	constexpr size_t ItemLiteral = 8;
	constexpr size_t ItemVisible = 24;
	constexpr size_t ItemUserData = 48;

	constexpr int CheatMax = 16;
	constexpr size_t CheatStride = 104;
	constexpr size_t CheatName = 8;
	constexpr size_t CheatEnabled = 20;
	constexpr size_t CheatCallback = 24;

	constexpr size_t StatsRecords = 8;
	constexpr size_t StatsCount = 16;
	constexpr size_t StatStride = 96;
	constexpr size_t StatName = 4;
	constexpr size_t StatKind = 60;
	constexpr size_t StatValueType = 64;
	constexpr size_t StatMax = 68;
	constexpr size_t StatValue = 76;

	constexpr size_t PlayerAvatar = 416;
	constexpr size_t ActorClass = 16;
	constexpr size_t ActorCollision = 40;
	constexpr size_t ActorMovement = 80;
	constexpr size_t ActorPosition = 128;
	constexpr size_t ActorScale = 160;
	constexpr size_t ActorVelocity = 192;
	constexpr size_t ActorName = 328;
	constexpr size_t ActorOrientation = 512;
	constexpr size_t ActorCombat = 872;
	constexpr size_t ActorShutdown = 1056;
	constexpr size_t ActorPlayerIndex = 1156;
	constexpr size_t ActorGetInterface = 72;
	constexpr size_t ActorInvalidate = 168;
	constexpr size_t ActorUpdate = 184;
	constexpr size_t CombatFlags = 24;
	constexpr int CombatJuggernaut = 0x2;

	constexpr size_t HealthAdd = 16;
	constexpr size_t HealthSetInvulnerable = 48;
	constexpr size_t HealthGet = 72;
	constexpr size_t HealthMax = 80;
	constexpr size_t GroundMoveScale = 64;

	constexpr size_t RegistryLists[3] = { 256, 448, 424 };
	constexpr size_t NodeActor = 8;
	constexpr size_t NodeNext = 48;

	constexpr size_t LockArray = 16;
	constexpr size_t LockName = 16;
	constexpr size_t LockRewardState = 156;
	constexpr size_t LockState = 160;
	constexpr int LockPurchasable = 2;
	constexpr int LockPurchased = 3;

	constexpr size_t SkyCurrent = 16;
	constexpr size_t SkyGroups = 56;
	constexpr size_t SkyGroupCount = 64;
	constexpr size_t GroupSkies = 8;
	constexpr size_t GroupSkyCount = 16;
	constexpr size_t GroupName = 40;
	constexpr size_t GroupCurrent = 56;

	constexpr size_t HudVisible = 176;

	constexpr size_t PadLeft = 19;
	constexpr size_t PadRight = 23;
	constexpr size_t PadLT = 39;
	constexpr size_t PadRT = 41;
	constexpr size_t PadL3 = 47;
	constexpr size_t PadR3 = 49;
	constexpr size_t PadUserIndex = 720;
}

namespace mem
{
	uintptr_t GameBase();
	uintptr_t AddressOf(addr::Id id);
	int FindAddresses();

	template <class T>
	inline T Read(const void* base, size_t offset)
	{
		return *reinterpret_cast<const T*>(static_cast<const uint8_t*>(base) + offset);
	}

	template <class T>
	inline void Write(void* base, size_t offset, T value)
	{
		*reinterpret_cast<T*>(static_cast<uint8_t*>(base) + offset) = value;
	}

	inline void* ReadPointer(const void* base, size_t offset)
	{
		return Read<void*>(base, offset);
	}

	inline void** GetMethods(void* object)
	{
		return *static_cast<void***>(object);
	}

	inline bool IsGoodPointer(const void* p)
	{
		const uintptr_t v = reinterpret_cast<uintptr_t>(p);
		return v >= 0x10000 && v < 0x00007FFFFFFFFFFFull && (v & 7) == 0;
	}

	inline bool IsGoodText(const void* p)
	{
		const uintptr_t v = reinterpret_cast<uintptr_t>(p);
		return v >= 0x10000 && v < 0x00007FFFFFFFFFFFull;
	}

	template <class F>
	inline F GameFunction(addr::Id id)
	{
		return reinterpret_cast<F>(AddressOf(id));
	}

	template <class F>
	inline F Method(void* object, size_t offset)
	{
		return reinterpret_cast<F>(GetMethods(object)[offset / sizeof(void*)]);
	}

	inline void* ReadGlobalPointer(addr::Id id)
	{
		const uintptr_t address = AddressOf(id);
		return address ? *reinterpret_cast<void**>(address) : nullptr;
	}

	extern uint64_t emptyValue[2];

	template <class T>
	inline T& GameValue(addr::Id id)
	{
		const uintptr_t address = AddressOf(id);
		if (!address)
		{
			emptyValue[0] = emptyValue[1] = 0;
			return *reinterpret_cast<T*>(emptyValue);
		}
		return *reinterpret_cast<T*>(address);
	}
}

template <class T, size_t N>
constexpr int LengthOf(const T (&)[N])
{
	return static_cast<int>(N);
}

const char* GameFolder();
HRESULT CreateRealDirectInput(HINSTANCE instance, DWORD version, const void* iid, void** out, void* outer);

namespace hooks
{
	bool Start();
	void Stop();
}
