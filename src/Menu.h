#pragma once
#include "Common.h"

namespace menu
{
	struct Input
	{
		bool toggle = false;
		int leftRight = 0;
		bool forward = false;
		bool back = false;
		bool up = false;
		bool down = false;
	};

	typedef void (*CloseFn)(void* screen);

	void SetCloseHandler(CloseFn handler);
	void OnFrame(const Input& input);
	bool IsOpen();
	bool ShouldHideEscape();
	void OnScreenOpened(void* screen);
	bool OnSelect(void* screen);
	bool OnBack(void* screen);
	void OnScreenClosing(void* screen);
	void Shutdown();
}
