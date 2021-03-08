#pragma once

#ifndef _IMGUIMAN
#define _IMGUIMAN

#include <Windows.h>
#include <iostream>
#include <WinUser.h>
#include <string>
#include <thread>
#include "logger.hpp"

#include <d3d9.h>
//#include <d3dx9tex.h>

#include <imgui.h>
#include <imgui_impl_dx9.h>
#include <imgui_impl_win32.h>

//#include "PatchMan.hpp"
//#include "SokuAPI.hpp"

#ifndef DEBUG
#define DEBUG false
#endif

using namespace std;

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#define WINDOW_HEIGHT 509.0
#define WINDOW_WIDTH 646.0
#define SOKU_HWND ((HWND*)0x0089FF90)
#define SOKU_D3D_DEVICE ((IDirect3DDevice9**)0x08a0e30)

// struct Image {
// 	PDIRECT3DTEXTURE9 Texture;
// 	ImVec2 Size;

// 	Image(PDIRECT3DTEXTURE9 tex, float w, float h) : Texture(tex), Size(w, h) {}
// };

namespace ImGuiMan {
	inline DWORD TamperNearJmpOpr(DWORD addr, DWORD target) {
		DWORD old = *reinterpret_cast<PDWORD>(addr + 1) + (addr + 5);
		*reinterpret_cast<PDWORD>(addr + 1) = target - (addr + 5);
		return old;
	}

	typedef HRESULT(__stdcall* f_EndScene)(IDirect3DDevice9* pDevice); // Our function prototype 
	extern f_EndScene oldEndScene; // Original Endscene

	extern void (__fastcall* oldInput)(int);

	typedef LRESULT(CALLBACK* WNDPROC)(HWND, UINT, WPARAM, LPARAM);
	extern WNDPROC oldWndProc;

	typedef void (*PassedFun)(void);
	//For loading images/fonts.
	extern PassedFun LoadFunction;
	//For rendering with imgui/dx.
	extern PassedFun RenderFunction;

	extern HWND window;

	extern ImFont* fontDefault;

	//bool RemapNavFlag = true;

	/*
	Image* LoadImageFromTexture(PDIRECT3DTEXTURE9 texture) {
		// Retrieve description of the texture surface so we can access its size
		D3DSURFACE_DESC image_desc;
		texture->GetLevelDesc(0, &image_desc);
		return new Image(texture, image_desc.Width, image_desc.Height);
	}

	Image *LoadImageFromFile(wstring filename) {
		// Load texture from disk
		PDIRECT3DTEXTURE9 texture;
		HRESULT hr = D3DXCreateTextureFromFileW(*SOKU_D3D_DEVICE, filename.c_str(), &texture);
		if (hr != S_OK)
			return NULL;

		return LoadImageFromTexture(texture);
	}
	*/

	inline void SetupStyle() {
		ImGuiStyle& style = ImGui::GetStyle();
		style.Alpha = 1;
		
		style.WindowRounding = 0;
		style.ChildRounding = 0;
		style.FrameRounding = 0;
		style.GrabRounding = 0;
		style.PopupRounding = 0;
		style.ScrollbarRounding = 0;
		style.TabRounding = 0;

		style.ItemSpacing = ImVec2(4, 4);
		style.ItemInnerSpacing = ImVec2(4, 4);
		style.FramePadding = ImVec2(4, 4);

		style.WindowPadding = ImVec2(4, 4);
		style.ScrollbarSize = 10;

		style.Colors[ImGuiCol_WindowBg] = ImVec4(0.3, 0.3, 0.3, 0.9);
		style.Colors[ImGuiCol_PopupBg] = ImVec4(0.3, 0.3, 0.3, 0.9);
		style.Colors[ImGuiCol_Border] = ImVec4(1, 1, 1, 0.9);

		style.Colors[ImGuiCol_TitleBg] = ImVec4(0.3, 0.3, 0.3, 0.9);
		style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.3, 0.3, 0.3, 0.9);
		style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.4, 0.4, 0.4, 0.9);

		style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.8, 0.8, 0.8, 0.9);
		style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.9, 0.9, 0.9, 0.9);
		style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(1, 1, 1, 1);

		style.Colors[ImGuiCol_Button] = ImVec4(0.4, 0.4, 0.4, 1);
		style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.5, 0.5, 0.5, 1);
		style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.6, 0.6, 0.6, 1);

		style.Colors[ImGuiCol_CheckMark] = (ImVec4)ImColor(0, 200, 0, 200);

		style.Colors[ImGuiCol_Header] = (ImVec4)ImColor(0, 200, 0, 200);
		style.Colors[ImGuiCol_HeaderHovered] = (ImVec4)ImColor(0, 200, 0, 100);
		style.Colors[ImGuiCol_HeaderActive] = (ImVec4)ImColor(0, 100, 0, 100);

		style.Colors[ImGuiCol_FrameBg] = ImVec4(0.4, 0.4, 0.4, 1);
		style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.5, 0.5, 0.5, 1);
		style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.6, 0.6, 0.6, 1);

		style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.5, 0.5, 0.5, 1);

		style.Colors[ImGuiCol_Separator] = (ImVec4)ImColor(255, 255, 255, 150);

		style.Colors[ImGuiCol_NavHighlight] = (ImVec4)ImColor(0, 200, 0, 200);
	}

/*
	void RemapNav() {
		ImGuiIO& io = ImGui::GetIO();
		CInputManagerCluster* SokuInput = SokuAPI::GetInputManager();
		if (SokuInput->P1.Yaxis > 0) io.NavInputs[ImGuiNavInput_DpadDown] = 1.0f;
		if (SokuInput->P1.Yaxis < 0) io.NavInputs[ImGuiNavInput_DpadUp] = 1.0f;
		if (SokuInput->P1.Xaxis > 0) io.NavInputs[ImGuiNavInput_DpadRight] = 1.0f;
		if (SokuInput->P1.Xaxis < 0) io.NavInputs[ImGuiNavInput_DpadLeft] = 1.0f;
		if (SokuInput->P1.A) io.NavInputs[ImGuiNavInput_Activate] = 1.0f;
		if (SokuInput->P1.B) io.NavInputs[ImGuiNavInput_Cancel] = 1.0f;
		if (SokuInput->P1.C) io.NavInputs[ImGuiNavInput_Menu] = 1.0f;
		if (io.KeysDown[io.KeyMap[ImGuiKey_Enter]]) io.NavInputs[ImGuiNavInput_Input] = 1.0f;
		if (io.KeysDown[io.KeyMap[ImGuiKey_Tab]]) io.NavInputs[ImGuiNavInput_KeyTab_] = 1.0f;
	}
*/

	inline LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
		if (uMsg == WM_MOUSEMOVE) {
			RECT rect;
			GetWindowRect(window, &rect);
			float resized_width = rect.right - rect.left;
			float resized_height = rect.bottom - rect.top;
			long resized_x = (short)lParam;
			long resized_y = (short)(lParam >> 16);
			float correct_x = (resized_x * (WINDOW_WIDTH / resized_width));
			float correct_y = (resized_y * (WINDOW_HEIGHT / resized_height));

			ImGuiIO& io = ImGui::GetIO();
			io.MousePos.x = correct_x;
			io.MousePos.y = correct_y;
		}

		if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam)) {
			return true;
		}

		return CallWindowProc(oldWndProc, hWnd, uMsg, wParam, lParam);
	}

	inline void __fastcall Hooked_Input(int ecx) {
   		if (ImGui::IsAnyItemActive()) return;
    	oldInput(ecx);
	}

	inline HRESULT __stdcall Hooked_EndScene(IDirect3DDevice9* pDevice) {
		static bool init = true;
		if (init)
		{
			init = false;
			ImGui::CreateContext();
			ImGuiIO& io = ImGui::GetIO();

			io.WantCaptureKeyboard = true;
			io.WantCaptureMouse = true;
			io.WantTextInput = true;
			
			io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
			io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

			//io.RemapNavFlag = RemapNavFlag;
			//io.RemapNavFun = RemapNav;

			ImGui_ImplWin32_Init(window);
			ImGui_ImplDX9_Init(pDevice);

			oldWndProc = (WNDPROC)SetWindowLongPtr(window, GWL_WNDPROC, (LONG_PTR)WndProc);

			// input check
			DWORD dwOldProtect;
			::VirtualProtect(reinterpret_cast<LPVOID>(0x0043e512), 5, PAGE_EXECUTE_WRITECOPY, &dwOldProtect);
			oldInput = (void(__fastcall*)(int)) TamperNearJmpOpr(0x0043e512, reinterpret_cast<DWORD>(Hooked_Input));
			::VirtualProtect(reinterpret_cast<LPVOID>(0x0043e512), 5, dwOldProtect, &dwOldProtect);

			fontDefault = io.Fonts->AddFontDefault();

			if (LoadFunction != NULL)
				LoadFunction();

			SetupStyle();

			if(DEBUG) printf("Imgui init done.");
		}

		ImGui_ImplDX9_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		if(RenderFunction != nullptr)
			RenderFunction();

		ImGui::EndFrame();
		ImGui::Render();

		ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

		return oldEndScene(pDevice);
	}

	inline bool HookThread(PassedFun load, PassedFun render) {
		while (*SOKU_D3D_DEVICE == 0 || *SOKU_HWND == 0)
			this_thread::sleep_for(10ms);

		LoadFunction = load;
		RenderFunction = render;

		window = *SOKU_HWND;
		IDirect3DDevice9* Device = *SOKU_D3D_DEVICE;
		void** pVTable = *((void***)Device);

		//oldEndScene = (f_EndScene)PatchMan::HookVTable((DWORD*)&(pVTable[42]), (DWORD)Hooked_EndScene);
		oldEndScene = (f_EndScene)pVTable[42];
		pVTable[42] = Hooked_EndScene;

		return false;
	}
}

#ifdef IMGUIMAN_IMPL
ImGuiMan::f_EndScene ImGuiMan::oldEndScene; // Original Endscene
void (__fastcall* ImGuiMan::oldInput)(int);
WNDPROC ImGuiMan::oldWndProc;
ImGuiMan::PassedFun ImGuiMan::LoadFunction = NULL;
ImGuiMan::PassedFun ImGuiMan::RenderFunction = NULL;
HWND ImGuiMan::window;
ImFont* ImGuiMan::fontDefault = NULL;
#endif

#endif