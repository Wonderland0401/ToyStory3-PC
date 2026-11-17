#include "Menu.h"
#include "Game.h"

#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif
#include <dinput.h>

#pragma comment(lib, "dxguid.lib")

using namespace mem;

namespace
{
	constexpr uintptr_t OriginalBase = 0x140000000ull;

	struct FunctionPattern { addr::Id id; const char* bytes; };
	struct ValueReference { int offset; int length; const char* bytes; };
	struct ValuePattern { addr::Id id; ValueReference references[3]; };
	struct TableSlot { addr::Id id; uintptr_t usualAddress; addr::Id function; };

	const FunctionPattern functionPatterns[] = {
		{ addr::GamepadUpdate,       "40 53 48 81 EC 90 00 00 00 48 8B D9 E8 ?? ?? ?? ?? 8B 8B D0 02 00 00" },
		{ addr::HelpOnAccept,        "48 89 5C 24 10 56 48 83 EC 20 80 B9 69 02 00 00 00 8B F2 48 8B D9" },
		{ addr::HelpOnCancel,        "40 53 48 83 EC 20 C7 81 6C 02 00 00 03 00 00 00 48 8B D9 48 8B 0D ?? ?? ?? ??" },
		{ addr::HelpOnOpen,          "48 89 5C 24 10 57 48 83 EC 40 48 8B F9 E8 ?? ?? ?? ?? 48 8B 0D ?? ?? ?? ??" },
		{ addr::HelpOnClose,         "40 53 48 83 EC 20 48 8B D9 48 8D 15 ?? ?? ?? ?? 48 8B 0D ?? ?? ?? ?? 45 33 C0 E8 ?? ?? ?? ?? 80 3D ?? ?? ?? ?? 00" },
		{ addr::FindScreenByName,    "40 53 48 81 EC 20 01 00 00 48 8B D9 48 8D 4C 24 20 41 83 F8 FF 74 14" },
		{ addr::MenuListAddItem,     "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48 89 7C 24 20 41 56 48 83 EC 30 48 8B F9 41 0F B6 E9" },
		{ addr::MenuListPopulate,    "48 89 5C 24 08 57 48 83 EC 20 48 8B F9 E8 ?? ?? ?? ?? 44 8B 47 28" },
		{ addr::MenuListHighlight,   "88 54 24 10 53 56 48 83 EC 68 48 83 79 50 00 0F B6 F2 48 8B D9" },
		{ addr::ItemAssignText,      "40 55 56 57 48 83 EC 20 49 63 F8 48 8B EA 48 8B F1 45 85 C0" },
		{ addr::StatsAdd,            "48 83 EC 08 4C 8B D1 85 D2 0F 88 ?? ?? ?? ?? 3B 51 10 0F 8D ?? ?? ?? ??" },
		{ addr::SetPlayerAvatar,     "40 53 56 57 48 83 EC 20 48 63 DA 49 8B F0 48 8B F9 83 FB 01" },
		{ addr::SyncPhysics,         "48 89 5C 24 08 57 48 83 EC 30 48 8B B9 C0 00 00 00 48 8B D9" },
		{ addr::SetCombatFlags,      "48 89 5C 24 10 48 89 74 24 18 57 48 83 EC 20 48 8B 99 68 03 00 00 8B F2" },
		{ addr::CollisionSetEnabled, "48 89 5C 24 10 57 48 83 EC 20 48 8B 1D ?? ?? ?? ?? 8B FA 48 85 DB" },
		{ addr::GetComponentByName,  "48 8B 89 70 03 00 00 48 85 C9 74 09 48 83 C1 18 E9 ?? ?? ?? ??" },
		{ addr::QuatToBasis,         "48 8B C4 48 83 EC 68 F3 0F 10 4A 04 F3 0F 10 02 0F 28 D9 0F 29 70 E8" },
		{ addr::LockUnlock,          "48 83 EC 28 83 B9 A0 00 00 00 00 75 2C 8B 81 9C 00 00 00 48 8D 15 ?? ?? ?? ??" },
		{ addr::SkyApply,            "48 89 5C 24 10 48 89 74 24 18 57 48 83 EC 20 48 8B FA 48 8B F1 45 84 C0" },
		{ addr::GetLevelName,        "48 8B 05 ?? ?? ?? ?? C3 CC CC CC CC CC CC CC CC 48 8B 81 98 00 00 00" },
		{ addr::LockReward,          "48 89 6C 24 10 48 89 74 24 18 57 48 83 EC 20 41 8B E8 48 8B F1" },
		{ addr::MakeAllSwappable,    "40 53 48 83 EC 20 BB 10 00 00 00 0F 1F 44 00 00 48 8B 0D ?? ?? ?? ??" },
	};

	const ValuePattern valuePatterns[] = {
		{ addr::ScreenManager, {
			{ 3, 7, "48 8B 05 ?? ?? ?? ?? 48 8B 8B C8 02 00 00" },
			{ 3, 7, "48 8B 0D ?? ?? ?? ?? 80 B8 A4 00 00 00 00" },
			{ 3, 7, "48 8B 0D ?? ?? ?? ?? 44 8B C6 48 8B 01 FF 50 20" } } },
		{ addr::CheatTable, {
			{ 3, 7, "4C 8D 0D ?? ?? ?? ?? 49 8B D1 66 90 39 02" },
			{ 3, 7, "4C 8D 15 ?? ?? ?? ?? 49 8B D2 39 02 74 0D" },
			{ 3, 7, "4C 8D 1D ?? ?? ?? ?? 49 8B CB 39 01 74 0E" } } },
		{ addr::CheatCount, {
			{ 3, 7, "44 8B 05 ?? ?? ?? ?? 33 C9 45 85 C0 7E 33" },
			{ 3, 7, "44 8B 05 ?? ?? ?? ?? 33 C9 45 85 C0 7E 37" },
			{ 3, 7, "48 63 0D ?? ?? ?? ?? 41 8B D1 85 C9 7E 20" } } },
		{ addr::TimeSpeedIndex, {
			{ 2, 6,  "89 05 ?? ?? ?? ?? 0F 95 C0 48 83 C4 20 5B" },
			{ 2, 6,  "8B 0D ?? ?? ?? ?? 3B C1 74 5C 83 F9 09 74 57" },
			{ 2, 10, "C7 05 ?? ?? ?? ?? 09 00 00 00 C7 05 ?? ?? ?? ?? 00 00 00 00" } } },
		{ addr::TimeSpeedDirty, {
			{ 2, 6, "88 1D ?? ?? ?? ?? 48 8B AC 24 A0 00 00 00" },
			{ 2, 7, "C6 05 ?? ?? ?? ?? 01 41 0F 44 C0 41 3B C0" },
			{ 2, 7, "C6 05 ?? ?? ?? ?? 00 C7 05 ?? ?? ?? ?? 09 00 00 00" } } },
		{ addr::PlayerStats, {
			{ 3, 7, "48 8B 0D ?? ?? ?? ?? 44 8B C2 BA 57 00 00 00" },
			{ 3, 7, "48 8B 0D ?? ?? ?? ?? 44 8B C2 BA 58 00 00 00" },
			{ 3, 7, "48 8B 0D ?? ?? ?? ?? 45 33 C0 BA 5B 00 00 00" } } },
		{ addr::PlayerManager, {
			{ 3, 7, "48 8B 05 ?? ?? ?? ?? 33 DB 39 58 04 7E 50" },
			{ 3, 7, "48 8B 05 ?? ?? ?? ?? 4C 8B 80 A0 01 00 00" },
			{ 3, 7, "48 8B 0D ?? ?? ?? ?? 83 79 04 02 48 8B CF" } } },
		{ addr::AvatarClassId, {
			{ 2, 6, "3B 05 ?? ?? ?? ?? 74 06 C7 03 00 00 00 00" },
			{ 2, 6, "3B 05 ?? ?? ?? ?? 75 09 48 8B 07 48 8B CF" },
			{ 2, 6, "3B 05 ?? ?? ?? ?? 75 0E 4C 39 42 38 75 08" } } },
		{ addr::AvatarClassMask, {
			{ 2, 6, "23 05 ?? ?? ?? ?? 3B 05 ?? ?? ?? ?? 40 0F B6 ED" },
			{ 2, 6, "23 05 ?? ?? ?? ?? 3B 05 ?? ?? ?? ?? 41 0F 94 C6" },
			{ 2, 6, "23 05 ?? ?? ?? ?? 3B 05 ?? ?? ?? ?? 74 04 33 C0" } } },
		{ addr::DefaultOrientation, {
			{ 3, 7, "48 8D 1D ?? ?? ?? ?? 0F 10 0B 48 8B 4D 48" },
			{ 3, 7, "48 8D 1D ?? ?? ?? ?? 0F 10 0B 48 8D 55 A0" },
			{ 3, 7, "48 8D 3D ?? ?? ?? ?? 48 8B 98 00 02 00 00" } } },
		{ addr::ActorRegistry, {
			{ 3, 7, "48 8B 05 ?? ?? ?? ?? 33 F6 45 0F 29 4B B8" },
			{ 3, 7, "48 8B 05 ?? ?? ?? ?? C6 80 01 02 00 00 00" },
			{ 3, 7, "48 8B 0D ?? ?? ?? ?? 48 81 C1 50 02 00 00" } } },
		{ addr::LockManager, {
			{ 3, 7, "48 8B 05 ?? ?? ?? ?? 4C 63 84 24 B8 00 00 00" },
			{ 3, 7, "48 8B 05 ?? ?? ?? ?? 8B 88 80 01 00 00 85 C9" },
			{ 3, 7, "48 8B 0D ?? ?? ?? ?? 48 89 84 24 88 00 00 00" } } },
		{ addr::LockCount, {
			{ 3, 7, "44 8B 05 ?? ?? ?? ?? 33 C0 45 85 C0 7E 1D" },
			{ 2, 6, "8B 05 ?? ?? ?? ?? 03 FB BB 04 00 00 00 85 C0" },
			{ 2, 6, "3B 2D ?? ?? ?? ?? 0F 8C ?? ?? ?? ?? 4C 8B 7C 24 50" } } },
		{ addr::SkyManager, {
			{ 3, 8, "48 83 3D ?? ?? ?? ?? 00 75 27 B9 90 00 00 00" },
			{ 3, 7, "48 8B 0D ?? ?? ?? ?? 48 8B 01 FF 50 18 48 8B 0D ?? ?? ?? ?? 48 85 C9" },
			{ 3, 7, "48 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ??" } } },
		{ addr::HealthInterfaceId, {
			{ 3, 7, "48 8B 15 ?? ?? ?? ?? 48 8B B6 F8 01 00 00" },
			{ 2, 6, "89 15 ?? ?? ?? ?? 48 8B 01 EB 29 4D 85 C0" },
			{ 2, 6, "89 15 ?? ?? ?? ?? 48 8B 00 FF 50 48 48 8B CB" } } },
		{ addr::InterfaceIdCounter, {
			{ 2, 6, "89 15 ?? ?? ?? ?? 89 15 ?? ?? ?? ?? 48 8B 0F 4C 8B 41 48" },
			{ 2, 6, "89 15 ?? ?? ?? ?? 89 15 ?? ?? ?? ?? 48 8B 00 FF 50 48 4C 8B F0" },
			{ 2, 6, "89 15 ?? ?? ?? ?? 89 15 ?? ?? ?? ?? 48 8B 01 FF 50 48 48 85 FF" } } },
		{ addr::Hud, {
			{ 3, 7, "48 8B 0D ?? ?? ?? ?? 48 8B 01 48 83 C4 28" },
			{ 3, 7, "48 8B 0D ?? ?? ?? ?? 48 8B 01 48 8B 53 38" },
			{ 3, 7, "48 8B 0D ?? ?? ?? ?? 48 8B 01 FF 50 18 90" } } },
	};

	const TableSlot tableSlots[] = {
		{ addr::GamepadUpdateSlot, 0x1407BCDD0, addr::GamepadUpdate },
		{ addr::HelpOnAcceptSlot,  0x140780158, addr::HelpOnAccept },
		{ addr::HelpOnCancelSlot,  0x140780160, addr::HelpOnCancel },
		{ addr::HelpOnOpenSlot,    0x1407801A0, addr::HelpOnOpen },
		{ addr::HelpOnCloseSlot,   0x1407801A8, addr::HelpOnClose },
	};

	uintptr_t foundAddresses[addr::Count];

	struct Section
	{
		const uint8_t* start;
		size_t size;

		bool Contains(uintptr_t address) const
		{
			const uintptr_t begin = reinterpret_cast<uintptr_t>(start);
			return address >= begin && address < begin + size;
		}
	};

	Section codeSection;
	Section dataSections[8];
	int dataSectionCount = 0;

	struct Pattern
	{
		uint8_t bytes[96];
		uint8_t mask[96];
		int length;
	};

	void FindSections()
	{
		codeSection = {};
		dataSectionCount = 0;

		const uint8_t* base = reinterpret_cast<const uint8_t*>(GameBase());
		const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
		if (dos->e_magic != IMAGE_DOS_SIGNATURE)
			return;
		const auto* headers = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
		if (headers->Signature != IMAGE_NT_SIGNATURE)
			return;

		const IMAGE_SECTION_HEADER* sections = IMAGE_FIRST_SECTION(headers);
		for (int i = 0; i < headers->FileHeader.NumberOfSections; i++)
		{
			const Section section{ base + sections[i].VirtualAddress, sections[i].Misc.VirtualSize };
			if ((sections[i].Characteristics & IMAGE_SCN_CNT_CODE) && !codeSection.start)
				codeSection = section;
			else if (dataSectionCount < LengthOf(dataSections))
				dataSections[dataSectionCount++] = section;
		}
	}

	bool IsInDataSection(uintptr_t address)
	{
		for (int i = 0; i < dataSectionCount; i++)
		{
			if (dataSections[i].Contains(address))
				return true;
		}
		return false;
	}

	int HexValue(char c)
	{
		if (c >= '0' && c <= '9') return c - '0';
		if (c >= 'A' && c <= 'F') return c - 'A' + 10;
		if (c >= 'a' && c <= 'f') return c - 'a' + 10;
		return -1;
	}

	bool ReadPattern(const char* text, Pattern& out)
	{
		out.length = 0;
		while (*text)
		{
			if (*text == ' ')
			{
				text++;
				continue;
			}
			if (out.length >= LengthOf(out.bytes))
				return false;

			if (text[0] == '?' && text[1] == '?')
			{
				out.bytes[out.length] = 0;
				out.mask[out.length] = 0;
			}
			else
			{
				const int high = HexValue(text[0]);
				const int low = high < 0 ? -1 : HexValue(text[1]);
				if (low < 0)
					return false;
				out.bytes[out.length] = static_cast<uint8_t>(high * 16 + low);
				out.mask[out.length] = 0xFF;
			}
			out.length++;
			text += 2;
		}
		return out.length > 0 && out.mask[0] != 0;
	}

	bool MatchesHere(const uint8_t* here, const Pattern& pattern)
	{
		for (int i = 0; i < pattern.length; i++)
		{
			if ((here[i] & pattern.mask[i]) != pattern.bytes[i])
				return false;
		}
		return true;
	}

	uintptr_t FindOnce(const char* text)
	{
		Pattern pattern;
		if (!codeSection.start || !ReadPattern(text, pattern))
			return 0;

		const uint8_t* match = nullptr;
		int matches = 0;
		for (size_t i = 0; i + pattern.length <= codeSection.size && matches < 2; i++)
		{
			if (codeSection.start[i] == pattern.bytes[0] && MatchesHere(codeSection.start + i, pattern))
			{
				if (matches++ == 0)
					match = codeSection.start + i;
			}
		}
		return matches == 1 ? reinterpret_cast<uintptr_t>(match) : 0;
	}

	uintptr_t FindValue(const ValuePattern& pattern)
	{
		uintptr_t result = 0;
		for (const ValueReference& reference : pattern.references)
		{
			const uintptr_t code = FindOnce(reference.bytes);
			if (!code)
				continue;

			const int32_t distance = *reinterpret_cast<const int32_t*>(code + reference.offset);
			const uintptr_t target = code + reference.length + static_cast<intptr_t>(distance);
			if (!IsInDataSection(target))
				continue;

			if (!result)
				result = target;
			else if (result != target)
				return 0;
		}
		return result;
	}

	uintptr_t FindSlot(const TableSlot& slot)
	{
		const uintptr_t function = foundAddresses[slot.function];
		if (!function)
			return 0;

		const uintptr_t usual = GameBase() + (slot.usualAddress - OriginalBase);
		if (IsInDataSection(usual) && (usual & 7) == 0 && *reinterpret_cast<const uintptr_t*>(usual) == function)
			return usual;

		uintptr_t match = 0;
		int matches = 0;
		for (int s = 0; s < dataSectionCount && matches < 2; s++)
		{
			const uintptr_t begin = (reinterpret_cast<uintptr_t>(dataSections[s].start) + 7) & ~uintptr_t(7);
			const uintptr_t end = reinterpret_cast<uintptr_t>(dataSections[s].start) + dataSections[s].size;
			for (uintptr_t a = begin; a + 8 <= end && matches < 2; a += 8)
			{
				if (*reinterpret_cast<const uintptr_t*>(a) == function && matches++ == 0)
					match = a;
			}
		}
		return matches == 1 ? match : 0;
	}

	template <class Entry, class Finder>
	int FindAll(const Entry* entries, int count, Finder&& find)
	{
		int missing = 0;
		for (int i = 0; i < count; i++)
		{
			foundAddresses[entries[i].id] = find(entries[i]);
			if (!foundAddresses[entries[i].id])
				missing++;
		}
		return missing;
	}
}

uint64_t mem::emptyValue[2] = {};

uintptr_t mem::GameBase()
{
	static const uintptr_t base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
	return base;
}

uintptr_t mem::AddressOf(addr::Id id)
{
	return id >= 0 && id < addr::Count ? foundAddresses[id] : 0;
}

int mem::FindAddresses()
{
	memset(foundAddresses, 0, sizeof(foundAddresses));
	FindSections();

	int missing = 0;
	missing += FindAll(functionPatterns, LengthOf(functionPatterns), [](const FunctionPattern& p) { return FindOnce(p.bytes); });
	missing += FindAll(valuePatterns, LengthOf(valuePatterns), [](const ValuePattern& p) { return FindValue(p); });
	missing += FindAll(tableSlots, LengthOf(tableSlots), [](const TableSlot& s) { return FindSlot(s); });
	return missing;
}

namespace
{
	struct SlotHook
	{
		void** slot;
		void* original;
		void* replacement;
		bool active;
	};

	SlotHook controllerHook, selectHook, backHook, openHook, closeHook;

	bool HookSlot(SlotHook& hook, addr::Id slotId, addr::Id functionId, void* replacement)
	{
		void** slot = reinterpret_cast<void**>(AddressOf(slotId));
		void* expected = reinterpret_cast<void*>(AddressOf(functionId));
		if (!slot || !expected || *slot != expected)
			return false;

		DWORD protection = 0;
		if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &protection))
			return false;

		hook.slot = slot;
		hook.original = *slot;
		hook.replacement = replacement;
		*slot = replacement;
		VirtualProtect(slot, sizeof(void*), protection, &protection);
		FlushInstructionCache(GetCurrentProcess(), slot, sizeof(void*));

		hook.active = true;
		return true;
	}

	void UnhookSlot(SlotHook& hook)
	{
		if (!hook.active)
			return;

		DWORD protection = 0;
		if (VirtualProtect(hook.slot, sizeof(void*), PAGE_READWRITE, &protection))
		{
			if (*hook.slot == hook.replacement)
				*hook.slot = hook.original;
			VirtualProtect(hook.slot, sizeof(void*), protection, &protection);
		}
		hook.active = false;
	}

	template <class F>
	F OriginalOf(const SlotHook& hook)
	{
		return reinterpret_cast<F>(hook.original);
	}

	constexpr int ReadKeyboardSlot = 9;
	constexpr int ReadKeyEventsSlot = 10;

	typedef HRESULT (STDMETHODCALLTYPE* ReadKeyboardFn)(void* device, DWORD size, void* data);
	typedef HRESULT (STDMETHODCALLTYPE* ReadKeyEventsFn)(void* device, DWORD eventSize, DIDEVICEOBJECTDATA* events, DWORD* count, DWORD flags);

	struct KeyboardHook
	{
		void** methods;
		ReadKeyboardFn readKeyboard;
		ReadKeyEventsFn readKeyEvents;
	};

	KeyboardHook keyboards[2];
	int keyboardCount = 0;
	bool escapeBlockStarted = false;

	const KeyboardHook* FindKeyboard(void* device)
	{
		void** methods = GetMethods(device);
		for (int i = 0; i < keyboardCount; i++)
		{
			if (keyboards[i].methods == methods)
				return &keyboards[i];
		}
		return nullptr;
	}

	HRESULT STDMETHODCALLTYPE OnReadKeyboard(void* device, DWORD size, void* data)
	{
		const KeyboardHook* keyboard = FindKeyboard(device);
		if (!keyboard)
			return DIERR_GENERIC;

		const HRESULT result = keyboard->readKeyboard(device, size, data);
		if (SUCCEEDED(result) && size == 256 && data && menu::ShouldHideEscape())
			static_cast<BYTE*>(data)[DIK_ESCAPE] = 0;
		return result;
	}

	HRESULT STDMETHODCALLTYPE OnReadKeyEvents(void* device, DWORD eventSize, DIDEVICEOBJECTDATA* events, DWORD* count, DWORD flags)
	{
		const KeyboardHook* keyboard = FindKeyboard(device);
		if (!keyboard)
			return DIERR_GENERIC;

		const HRESULT result = keyboard->readKeyEvents(device, eventSize, events, count, flags);
		if (FAILED(result) || !events || !count || !menu::ShouldHideEscape() || eventSize < sizeof(DIDEVICEOBJECTDATA_DX3))
			return result;

		BYTE* first = reinterpret_cast<BYTE*>(events);
		DWORD kept = 0;
		for (DWORD i = 0; i < *count; i++)
		{
			BYTE* event = first + i * eventSize;
			if (reinterpret_cast<const DIDEVICEOBJECTDATA_DX3*>(event)->dwOfs == DIK_ESCAPE)
				continue;
			if (kept != i)
				memmove(first + kept * eventSize, event, eventSize);
			kept++;
		}
		*count = kept;
		return result;
	}

	void WritePointer(void** where, void* value)
	{
		DWORD protection = 0;
		if (!VirtualProtect(where, sizeof(void*), PAGE_READWRITE, &protection))
			return;
		*where = value;
		VirtualProtect(where, sizeof(void*), protection, &protection);
	}

	void HookKeyboardMethods(void** methods)
	{
		if (!methods || keyboardCount >= LengthOf(keyboards))
			return;
		for (int i = 0; i < keyboardCount; i++)
		{
			if (keyboards[i].methods == methods)
				return;
		}

		KeyboardHook& keyboard = keyboards[keyboardCount];
		keyboard.methods = methods;
		keyboard.readKeyboard = reinterpret_cast<ReadKeyboardFn>(methods[ReadKeyboardSlot]);
		keyboard.readKeyEvents = reinterpret_cast<ReadKeyEventsFn>(methods[ReadKeyEventsSlot]);
		MemoryBarrier();
		keyboardCount++;

		WritePointer(&methods[ReadKeyboardSlot], reinterpret_cast<void*>(&OnReadKeyboard));
		WritePointer(&methods[ReadKeyEventsSlot], reinterpret_cast<void*>(&OnReadKeyEvents));
	}

	template <class Input, class Device>
	void HookKeyboard(REFIID iid)
	{
		Input* input = nullptr;
		if (FAILED(CreateRealDirectInput(GetModuleHandleW(nullptr), DIRECTINPUT_VERSION, &iid, reinterpret_cast<void**>(&input), nullptr)) || !input)
			return;

		Device* keyboard = nullptr;
		if (FAILED(input->CreateDevice(GUID_SysKeyboard, &keyboard, nullptr)) || !keyboard)
			return;

		HookKeyboardMethods(GetMethods(keyboard));
	}

	void StartEscapeBlock()
	{
		if (escapeBlockStarted)
			return;
		escapeBlockStarted = true;

		HookKeyboard<IDirectInput8A, IDirectInputDevice8A>(IID_IDirectInput8A);
		HookKeyboard<IDirectInput8W, IDirectInputDevice8W>(IID_IDirectInput8W);
	}

	void StopEscapeBlock()
	{
		for (int i = 0; i < keyboardCount; i++)
		{
			WritePointer(&keyboards[i].methods[ReadKeyboardSlot], reinterpret_cast<void*>(keyboards[i].readKeyboard));
			WritePointer(&keyboards[i].methods[ReadKeyEventsSlot], reinterpret_cast<void*>(keyboards[i].readKeyEvents));
		}
	}

	bool IsKeyHeld(int key)
	{
		return (GetAsyncKeyState(key) & 0x8000) != 0;
	}

	bool IsGameFocused()
	{
		HWND window = GetForegroundWindow();
		if (!window)
			return false;
		DWORD processId = 0;
		GetWindowThreadProcessId(window, &processId);
		return processId == GetCurrentProcessId();
	}

	bool IsButtonHeld(void* controller, size_t button)
	{
		return Read<uint8_t>(controller, button) != 0;
	}

	bool toggleReady = true;
	ULONGLONG lastToggle = 0;

	void ReadInput(void* controller)
	{
		menu::Input input;
		const bool focused = IsGameFocused();
		const bool leftTrigger = IsButtonHeld(controller, offsets::PadLT);

		const bool alt = IsKeyHeld(VK_LMENU);
		const bool w = IsKeyHeld('W');
		if (!alt && !w)
			toggleReady = true;

		const ULONGLONG now = GetTickCount64();
		if (toggleReady && focused && alt && w && now - lastToggle > 300)
		{
			toggleReady = false;
			lastToggle = now;
			input.toggle = true;
		}

		const bool right = !leftTrigger && (IsButtonHeld(controller, offsets::PadRight) || (focused && IsKeyHeld(VK_RIGHT)));
		const bool left = !leftTrigger && (IsButtonHeld(controller, offsets::PadLeft) || (focused && IsKeyHeld(VK_LEFT)));
		input.leftRight = right ? 1 : (left ? -1 : 0);

		input.forward = IsButtonHeld(controller, offsets::PadRT) || (focused && (IsKeyHeld(VK_LSHIFT) || IsKeyHeld(VK_NUMPAD8)));
		input.back = leftTrigger || (focused && IsKeyHeld(VK_NUMPAD2));
		input.up = IsButtonHeld(controller, offsets::PadL3) || (focused && (IsKeyHeld(VK_SPACE) || IsKeyHeld(VK_NUMPAD9)));
		input.down = IsButtonHeld(controller, offsets::PadR3) || (focused && (IsKeyHeld(VK_LCONTROL) || IsKeyHeld(VK_NUMPAD3)));

		menu::OnFrame(input);
	}

	typedef int (*ControllerUpdateFn)(void* controller);
	typedef void* (*ScreenFn)(void* screen);
	typedef void* (*ScreenButtonFn)(void* screen, unsigned button, int64_t extra);

	void TryReadInput(void* controller)
	{
		__try { ReadInput(controller); }
		__except (EXCEPTION_EXECUTE_HANDLER) {}
	}

	void TryScreenOpened(void* screen)
	{
		__try { menu::OnScreenOpened(screen); }
		__except (EXCEPTION_EXECUTE_HANDLER) {}
	}

	bool TrySelect(void* screen)
	{
		bool handled = true;
		__try { handled = menu::OnSelect(screen); }
		__except (EXCEPTION_EXECUTE_HANDLER) { handled = true; }
		return handled;
	}

	bool TryBack(void* screen)
	{
		bool handled = false;
		__try { handled = menu::OnBack(screen); }
		__except (EXCEPTION_EXECUTE_HANDLER) { handled = false; }
		return handled;
	}

	void TryScreenClosing(void* screen)
	{
		__try { menu::OnScreenClosing(screen); }
		__except (EXCEPTION_EXECUTE_HANDLER) {}
	}

	int OnControllerUpdate(void* controller)
	{
		const int result = OriginalOf<ControllerUpdateFn>(controllerHook)(controller);

		if (IsGoodPointer(controller))
		{
			StartEscapeBlock();
			if (Read<uint32_t>(controller, offsets::PadUserIndex) == 0)
				TryReadInput(controller);
		}
		return result;
	}

	void* OnHelpOpen(void* screen)
	{
		void* result = OriginalOf<ScreenFn>(openHook)(screen);
		TryScreenOpened(screen);
		return result;
	}

	void* OnHelpSelect(void* screen, unsigned button, int64_t extra)
	{
		if (menu::IsOpen())
		{
			if (TrySelect(screen))
				return screen;
		}
		else if (Read<uint8_t>(screen, offsets::HelpDismissed) || !game::HasSelectedRow(screen))
		{
			return screen;
		}
		return OriginalOf<ScreenButtonFn>(selectHook)(screen, button, extra);
	}

	void* OnHelpBack(void* screen, unsigned button, int64_t extra)
	{
		if (menu::IsOpen() && TryBack(screen))
			return screen;
		return OriginalOf<ScreenButtonFn>(backHook)(screen, button, extra);
	}

	void* OnHelpClose(void* screen)
	{
		TryScreenClosing(screen);
		return OriginalOf<ScreenFn>(closeHook)(screen);
	}

	void CloseMenuScreen(void* screen)
	{
		if (backHook.active)
			OriginalOf<ScreenButtonFn>(backHook)(screen, 0, 0);
	}
}

namespace hooks
{
	bool Start()
	{
		FindAddresses();
		menu::SetCloseHandler(&CloseMenuScreen);

		const bool menuHooked =
			HookSlot(openHook, addr::HelpOnOpenSlot, addr::HelpOnOpen, reinterpret_cast<void*>(&OnHelpOpen)) &&
			HookSlot(selectHook, addr::HelpOnAcceptSlot, addr::HelpOnAccept, reinterpret_cast<void*>(&OnHelpSelect)) &&
			HookSlot(backHook, addr::HelpOnCancelSlot, addr::HelpOnCancel, reinterpret_cast<void*>(&OnHelpBack)) &&
			HookSlot(closeHook, addr::HelpOnCloseSlot, addr::HelpOnClose, reinterpret_cast<void*>(&OnHelpClose));

		if (!menuHooked || !HookSlot(controllerHook, addr::GamepadUpdateSlot, addr::GamepadUpdate, reinterpret_cast<void*>(&OnControllerUpdate)))
		{
			Stop();
			return false;
		}
		return true;
	}

	void Stop()
	{
		UnhookSlot(controllerHook);
		StopEscapeBlock();
		menu::Shutdown();
		UnhookSlot(closeHook);
		UnhookSlot(backHook);
		UnhookSlot(selectHook);
		UnhookSlot(openHook);
	}
}
