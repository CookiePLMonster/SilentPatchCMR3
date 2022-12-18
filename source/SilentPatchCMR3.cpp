#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <shellapi.h>
#include <timeapi.h>

#include "Utils/MemoryMgr.h"
#include "Utils/Patterns.h"

#include "Destruct.h"
#include "Globals.h"
#include "Graphics.h"
#include "Language.h"
#include "Menus.h"
#include "RenderState.h"
#include "Registry.h"

#include <d3d9.h>
#include <wil/com.h>
#include <wil/win32_helpers.h>

#include <algorithm>
#include <array>
#define _USE_MATH_DEFINES
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <DirectXMath.h>

#pragma comment(lib, "winmm.lib")

static int DesktopWidth, DesktopHeight;
static HICON SmallIcon;

template<typename AT>
HMODULE GetModuleHandleFromAddress(AT address)
{
	HMODULE result = nullptr;
	GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, reinterpret_cast<LPCTSTR>(address), &result);
	return result;
}

namespace Localization
{
	uint32_t (*orgGetLanguageIDByCode)(char code);
	uint32_t GetCurrentLanguageID_Patched()
	{
		return orgGetLanguageIDByCode(Registry::GetRegistryChar(Registry::REGISTRY_SECTION_NAME, L"LANGUAGE").value_or('\0'));
	}

	uint32_t (*orgGetCurrentLanguageID)();

	uint8_t* gCoDriverLanguage;
	uint32_t GetCoDriverLanguage()
	{
		return *gCoDriverLanguage;
	}

	void SetCoDriverLanguage(uint32_t language)
	{
		*gCoDriverLanguage = static_cast<int8_t>(language);
	}

	static void (*orgSetUnknown)(int a1);
	static void setUnknown_AndCoDriverLanguage(int a1)
	{
		SetCoDriverLanguage(orgGetCurrentLanguageID());

		orgSetUnknown(a1);
	}

	void (*orgSetMeasurementSystem)(bool bImperial);
	void SetMeasurementSystem_Defaults()
	{
		orgSetMeasurementSystem(orgGetCurrentLanguageID() == 0);
	}

	void SetMeasurementSystem_FromLanguage(bool)
	{
		SetMeasurementSystem_Defaults();
	}
}

namespace Cubes
{
	void** gBlankCubeTexture;
	void** gGearCubeTextures;
	void** gStageCubeTextures;

	void** gGearCubeLayouts;
	void** gStageCubeLayouts;

	static void (*orgLoadCubeTextures)();
	void LoadCubeTextures_AndSetUpLayouts()
	{
		orgLoadCubeTextures();

		void* BLANK = *gBlankCubeTexture;

		// Gear cubes
		{
			const auto& automatic = gGearCubeLayouts;
			const auto& manual = automatic+3;

			automatic[0] = gGearCubeTextures[0]; // A
			automatic[1] = gGearCubeTextures[2]; // T
			automatic[2] = BLANK;

			manual[0] = BLANK;
			manual[1] = gGearCubeTextures[1]; // M
			manual[2] = gGearCubeTextures[2]; // T
		}

		// Stage cubes
		{
			const auto& stage1 = gStageCubeLayouts;
			const auto& stage2 = stage1+3;
			const auto& stage3 = stage2+3;
			const auto& stage4 = stage3+3;
			const auto& stage5 = stage4+3;
			const auto& stage6 = stage5+3;
			const auto& specialStage = stage6+3;

			void* S = gStageCubeTextures[6];

			// Keep stage numbers as-is
			stage1[0] = stage2[0] = stage3[0] = stage4[0] = stage5[0] = stage6[0] = S;
			stage1[1] = stage2[1] = stage3[1] = stage4[1] = stage5[1] = stage6[1] = S;
			specialStage[0] = specialStage[1] = specialStage[2] = S;
		}
	}
}

namespace OcclusionQueries
{
	// Divide the occlusion query results by the sample count of the backbuffer
	struct OcclusionQuery
	{
		// Stock fields, must not move
		int field_0;
		IDirect3DQuery9* m_pD3DQuery;
		DWORD m_queryData;
		uint32_t m_queryState;

		// Added in SilentPatch
		uint32_t m_sampleCount;
	};

	OcclusionQuery* (*orgOcclusionQuery_Create)();
	OcclusionQuery* OcclusionQuery_Create_SilentPatch()
	{
		OcclusionQuery* result = orgOcclusionQuery_Create();
		if (result != nullptr)
		{
			result->m_sampleCount = 0;
		}
		return result;
	}

	static bool OcclusionQuery_UpdateInternal(OcclusionQuery* query)
	{
		if (query->m_queryState == 2)
		{
			DWORD data;
			if (query->m_pD3DQuery->GetData(&data, sizeof(data), 0) == S_OK)
			{
				query->m_queryData = data;
				query->m_queryState = 3;
			}
		}
		return query->m_queryState == 3;
	}

	void OcclusionQuery_IssueBegin(OcclusionQuery* query)
	{
		query->m_pD3DQuery->Issue(D3DISSUE_BEGIN);
		query->m_queryState = 1;

		// Get the sample count
		uint32_t sampleCount = 1;

		wil::com_ptr_nothrow<IDirect3DDevice9> device;
		wil::com_ptr_nothrow<IDirect3DSurface9> depthStencil;
		if (SUCCEEDED(query->m_pD3DQuery->GetDevice(device.put())) && SUCCEEDED(device->GetDepthStencilSurface(depthStencil.put())))
		{
			D3DSURFACE_DESC desc;
			if (SUCCEEDED(depthStencil->GetDesc(&desc)))
			{
				if (desc.MultiSampleType >= D3DMULTISAMPLE_2_SAMPLES)
				{
					sampleCount = static_cast<uint32_t>(desc.MultiSampleType);
				}
			}
		}
		query->m_sampleCount = sampleCount;
	}

	void OcclusionQuery_IssueEnd(OcclusionQuery* query)
	{
		query->m_pD3DQuery->Issue(D3DISSUE_END);
		query->m_queryState = 2;
	}

	DWORD OcclusionQuery_GetDataScaled(OcclusionQuery* query)
	{	
		if (query->m_sampleCount > 1)
		{
			return query->m_queryData / query->m_sampleCount;
		}
		return query->m_queryData;
	}

	// Checks if query is completed, returns a fake state and a cached result if it's not
	uint32_t OcclusionQuery_UpdateStateIfFinished(OcclusionQuery* query)
	{
		if (!OcclusionQuery_UpdateInternal(query))
		{
			// Fake result, don't store it. OcclusionQuery_GetDataScaled will return old data
			return 3;
		}
		return query->m_queryState;
	}

	// Returns 3 when idle, 0 otherwise - to reduce the amount of assembly patches needed
	uint32_t OcclusionQuery_IsIdle(OcclusionQuery* query)
	{
		OcclusionQuery_UpdateInternal(query);
		return query->m_queryState == 0 || query->m_queryState == 3 ? 3 : 0;
	}

	void (*orgCalculateSunColorFromOcclusion)(float val);
	void CalculateSunColorFromOcclusion_Clamped(float val)
	{
		orgCalculateSunColorFromOcclusion(std::clamp(val, 0.0f, 1.0f));
	}

	static OcclusionQuery** gpSunOcclusion_Original;
	static float* gfVisibleSun_Original;

	std::array<OcclusionQuery*, 4> gapSunOcclusion;
	std::array<float, 4> gafVisibleSun;

	static OcclusionQuery* (*orgOcclusion_Create)();
	static void (*orgOcclusion_Destroy)(OcclusionQuery* occlusion);

	void* Occlusion_Create_Hooked()
	{
		for (auto& occlusion : gapSunOcclusion)
		{
			occlusion = orgOcclusion_Create();
		}

		// Return a fake result so null checks pass
		return reinterpret_cast<void*>(1);
	}

	void Occlusion_Destroy_Hooked(OcclusionQuery* /*occlusion*/)
	{
		for (auto& occlusion : gapSunOcclusion)
		{
			orgOcclusion_Destroy(occlusion);
			occlusion = nullptr;
		}
	}

	static void* Render_RenderGameCommon1_JumpBack;
	__declspec(naked) void Render_RenderGameCommon1_Original(uint8_t)
	{
		__asm
		{
			sub		esp, 20h
			push	ebx
			push	ebp
			jmp		[Render_RenderGameCommon1_JumpBack]
		}
	}

	void Render_RenderGameCommon1_SwitchOcclusion(uint8_t viewportNo)
	{
		*gpSunOcclusion_Original = gapSunOcclusion[viewportNo];
		*gfVisibleSun_Original = gafVisibleSun[viewportNo];

		Render_RenderGameCommon1_Original(viewportNo);

		gafVisibleSun[viewportNo] = *gfVisibleSun_Original;
	}
}

namespace QuitMessageFix
{
	static uint32_t* gboExitProgram;

	void WINAPI PostQuitMessage_AndRequestExit(int nExitCode)
	{
		*gboExitProgram = 1;
		::PostQuitMessage(nExitCode);
	}
	auto* const pPostQuitMessage_AndRequestExit = &PostQuitMessage_AndRequestExit;
}

namespace TelemetryFadingLegend
{
	static int8_t originalWidth, originalHeight;

	void (*orgHandyFunction_Draw2DBox)(int posX, int posY, int width, int height, int color);
	void Draw2DBox_HackedAlpha(int posX, int posY, uint32_t alpha, uint32_t color)
	{
		// Parameters width and height got replaced by alpha
		orgHandyFunction_Draw2DBox(posX, posY, originalWidth, originalHeight, HandyFunction_AlphaCombineFlat(color, alpha));
	}
}

namespace CappedResolutionCountdown
{
	int __cdecl sprintf_clamp(char* Buffer, const char* Format, const char* str1, int int1, const char* str2)
	{
		return sprintf_s(Buffer, 256, Format, str1, std::max(0, int1), str2);
	}
}

namespace ConsistentControlsScreen
{
	template<std::size_t Index>
	const char* (*orgLanguage_GetString)(uint32_t ID);

	template<std::size_t Index>
	static const char* Language_GetString_Formatted(uint32_t ID)
	{
		sprintf_s(gszTempString, 512, " %s", orgLanguage_GetString<Index>(ID));
		return gszTempString;
	}

	template<std::size_t Ctr, typename Tuple, std::size_t... I, typename Func>
	void HookEachImpl(Tuple&& tuple, std::index_sequence<I...>, Func&& f)
	{
		(f(std::get<I>(tuple), orgLanguage_GetString<Ctr << 16 | I>, Language_GetString_Formatted<Ctr << 16 | I>), ...);
	}

	template<std::size_t Ctr = 0, typename Vars, typename Func>
	void HookEach(Vars&& vars, Func&& f)
	{
		auto tuple = std::tuple_cat(std::forward<Vars>(vars));
		HookEachImpl<Ctr>(std::move(tuple), std::make_index_sequence<std::tuple_size_v<decltype(tuple)>>{}, std::forward<Func>(f));
	}

	static int (*orgGetControllerName)(void* a1, const char** outText);
	static int GetControllerName_Uppercase(void* a1, const char** outText)
	{
		const char* text = nullptr;
		const int result = orgGetControllerName(a1, &text);

		static char stringBuffer[MAX_PATH]; // Corresponds to DirectInput's cap
		auto it =std::transform(text, text + strlen(text), stringBuffer, ::toupper);
		*it = '\0';

		*outText = stringBuffer;
		return result;
	}
}

namespace UnrandomizeUnknownCodepoints
{
	int Random_RandSequence_Fixed(uint32_t)
	{
		return 0;
	}

	void CMR3Font_SetFontForBlitChar_NOP(uint8_t, uint32_t)
	{
	}
}

namespace SPText
{
	static std::string BuildTextInternal()
	{
		std::string text("SilentPatch for CMR3 Build ");
		text.append(std::to_string(rsc_RevisionID));
#if rsc_BuildID > 0
		text.append(".");
		text.append(std::to_string(rsc_BuildID));
#endif
		text.append(" (" __DATE__ ")\nDetected executable: ");
		text.append("UNKNOWN");

		std::string result;
		std::transform(text.begin(), text.end(), std::back_inserter(result), ::toupper);
		return result;
	}

	void DrawSPText(uint32_t color)
	{
		const uint32_t alpha = color & 0xFF000000;

		const uint32_t ScreenEdge = static_cast<uint32_t>(GetScaledResolutionWidth());
		const static std::string DISCLAIMER_TEXT(BuildTextInternal());

		const uint32_t DROP_SHADOW_WIDTH = 1;
		CMR3Font_BlitText(0, DISCLAIMER_TEXT.c_str(), ScreenEdge - 10 + DROP_SHADOW_WIDTH, 10, alpha, 4);
		CMR3Font_BlitText(0, DISCLAIMER_TEXT.c_str(), ScreenEdge - 10 - DROP_SHADOW_WIDTH, 10, alpha, 4);
		CMR3Font_BlitText(0, DISCLAIMER_TEXT.c_str(), ScreenEdge - 10, 10 + DROP_SHADOW_WIDTH, alpha, 4);
		CMR3Font_BlitText(0, DISCLAIMER_TEXT.c_str(), ScreenEdge - 10, 10 - DROP_SHADOW_WIDTH, alpha, 4);
		CMR3Font_BlitText(0, DISCLAIMER_TEXT.c_str(), ScreenEdge - 10, 10, color, 4);
	}

	template<std::size_t Index>
	void (*orgHandyFunction_BlitTexture)(void* texture, int u1, int v1, float u2, int v2, int posX, int posY, float width, int height, uint32_t color);

	template<std::size_t Index>
	void HandyFunction_BlitTexture_SPText(void* texture, int u1, int v1, float u2, int v2, int posX, int posY, float width, int height, uint32_t color)
	{
		DrawSPText(color);
		orgHandyFunction_BlitTexture<Index>(texture, u1, v1, u2, v2, posX, posY, width, height, color);
	}

	template<std::size_t Ctr, typename Tuple, std::size_t... I, typename Func>
	void HookEachImpl(Tuple&& tuple, std::index_sequence<I...>, Func&& f)
	{
		(f(std::get<I>(tuple), orgHandyFunction_BlitTexture<Ctr << 16 | I>, HandyFunction_BlitTexture_SPText<Ctr << 16 | I>), ...);
	}

	template<std::size_t Ctr = 0, typename Vars, typename Func>
	void HookEach(Vars&& vars, Func&& f)
	{
		auto tuple = std::tuple_cat(std::forward<Vars>(vars));
		HookEachImpl<Ctr>(std::move(tuple), std::make_index_sequence<std::tuple_size_v<decltype(tuple)>>{}, std::forward<Func>(f));
	}
}

namespace EnvMapWithSky
{
	static bool alwaysDrawSky = true;
	static void* (*orgAfterSetupTextureStages)();
	static void* Graphics_CarMultitexture_AfterSetupTextureStages()
	{
		alwaysDrawSky = Registry::GetRegistryDword(Registry::ADVANCED_SECTION_NAME, Registry::ENVMAP_SKY_KEY_NAME).value_or(1) != 0;	
		return orgAfterSetupTextureStages();
	}

	static int (*orgSfxEnable)(void* a1);
	int SFX_Enable_Force(void* a1)
	{
#ifndef NDEBUG
		if (GetAsyncKeyState(VK_F3) & 0x8000)
		{
			return 0;
		}
#endif
		if (alwaysDrawSky)
		{
			return 1;
		}
		return orgSfxEnable(a1);
	}
}

namespace Timers
{
	static int64_t GetQPC()
	{
		LARGE_INTEGER time;
		QueryPerformanceCounter(&time);
		return time.QuadPart;
	}

	static LARGE_INTEGER Frequency;
	uint64_t GetTimeInMS()
	{
		// Calculate time in MS but with overflow avoidance
		const int64_t curTime = GetQPC();
		const int64_t freq = Frequency.QuadPart;

		const int64_t whole = (curTime / freq) * 1000;
		const int64_t part = (curTime % freq) * 1000 / freq;
		return whole + part;
	}

	static LARGE_INTEGER StartQPC;
	void Reset()
	{
		QueryPerformanceCounter(&StartQPC);

		// To fix issues occuring with the first physics tick having a zero delta,
		// knock the starting time off by a second. Hacky, but affects only the first physics tick/frame.
		StartQPC.QuadPart -= Frequency.QuadPart;
	}

	void Setup()
	{
		QueryPerformanceFrequency(&Frequency);
		Reset();
	}

	static DWORD WINAPI timeGetTime_Reset()
	{
		Reset();
		return 0;
	}
	static const auto pTimeGetTime_Reset = &timeGetTime_Reset;

	static DWORD WINAPI timeGetTime_Update()
	{
		const int64_t curTime = GetQPC() - StartQPC.QuadPart;
		const int64_t freq = Frequency.QuadPart;

		const int64_t whole = (curTime / freq) * 1000;
		const int64_t part = (curTime % freq) * 1000 / freq;
		return static_cast<DWORD>(whole + part);
	}
	static const auto pTimeGetTime_Update = &timeGetTime_Update;

	static DWORD WINAPI timeGetTime_Precise()
	{
		return static_cast<DWORD>(GetTimeInMS());
	}

	static void ReplaceFunction(void** funcPtr)
	{
		DWORD dwProtect;

		VirtualProtect(funcPtr, sizeof(*funcPtr), PAGE_READWRITE, &dwProtect);
		*funcPtr = &timeGetTime_Precise;
		VirtualProtect(funcPtr, sizeof(*funcPtr), dwProtect, &dwProtect);
	}

	static bool RedirectImports()
	{
		const DWORD_PTR instance = reinterpret_cast<DWORD_PTR>(GetModuleHandle(nullptr));
		const PIMAGE_NT_HEADERS ntHeader = reinterpret_cast<PIMAGE_NT_HEADERS>(instance + reinterpret_cast<PIMAGE_DOS_HEADER>(instance)->e_lfanew);

		// Find IAT
		PIMAGE_IMPORT_DESCRIPTOR pImports = reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>(instance + ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

		for ( ; pImports->Name != 0; pImports++ )
		{
			if ( _stricmp(reinterpret_cast<const char*>(instance + pImports->Name), "winmm.dll") == 0 )
			{
				if ( pImports->OriginalFirstThunk != 0 )
				{
					const PIMAGE_THUNK_DATA pThunk = reinterpret_cast<PIMAGE_THUNK_DATA>(instance + pImports->OriginalFirstThunk);

					for ( ptrdiff_t j = 0; pThunk[j].u1.AddressOfData != 0; j++ )
					{
						if ( strcmp(reinterpret_cast<PIMAGE_IMPORT_BY_NAME>(instance + pThunk[j].u1.AddressOfData)->Name, "timeGetTime") == 0 )
						{
							void** pAddress = reinterpret_cast<void**>(instance + pImports->FirstThunk) + j;
							ReplaceFunction(pAddress);
							return true;
						}
					}
				}
				else
				{
					void** pFunctions = reinterpret_cast<void**>(instance + pImports->FirstThunk);

					for ( ptrdiff_t j = 0; pFunctions[j] != nullptr; j++ )
					{
						if ( pFunctions[j] == &::timeGetTime )
						{
							ReplaceFunction(&pFunctions[j]);
							return true;
						}
					}
				}
			}
		}
		return false;
	}
}

namespace ResolutionsList
{
	// Easiest to do this via runtime patching...
	std::vector<std::pair<void*, size_t>> placesToPatch;

	std::vector<MenuResolutionEntry> displayModeStorage;
	static constexpr uint32_t RELOCATION_INITIAL_THRESHOLD = 128;

	uint32_t (*orgGetDisplayModeCount)(uint32_t adapter);
	uint32_t GetDisplayModeCount_RelocateArray(uint32_t adapter)
	{
		const uint32_t modeCount = orgGetDisplayModeCount(adapter);
		const uint32_t modeCapacity = std::max(RELOCATION_INITIAL_THRESHOLD, displayModeStorage.size());
		if (modeCount > modeCapacity)
		{
			if (placesToPatch.empty())
			{
				// Failsafe, limit to the max number of resolutions stock allocations can handle
				return RELOCATION_INITIAL_THRESHOLD;
			}

			displayModeStorage.resize(modeCount);
			std::byte* buf = reinterpret_cast<std::byte*>(displayModeStorage.data());

			// Patch pointers, ugly but saves a lot of effort
			auto Protect = ScopedUnprotect::UnprotectSectionOrFullModule( GetModuleHandle( nullptr ), ".text" );
			
			using namespace Memory;
			for (const auto& addr : placesToPatch)
			{
				Patch(addr.first, buf+addr.second);
			}
		}
		return modeCount;
	}
}

namespace HalfPixel
{
	using namespace DirectX;

	static float OffsetTexel(float val)
	{
		return std::ceil(val) - 0.5f;
	}

	static XMVECTOR ComputePoint(float x, float y, const XMMATRIX& rot, const XMVECTOR& trans)
	{
		return XMVectorAdd(trans, XMVector2TransformNormal(XMVectorSet(x, y, 0.0f, 0.0f), rot));
	}

	static float ComputeConstantScale(const XMVECTOR& pos, const XMMATRIX& view, const XMFLOAT4X4& proj)
	{
		const XMVECTOR ppcam0 = XMVector4Transform(pos, view);
		const XMVECTOR ppcam1 = XMVectorAdd(ppcam0, XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f));

		const XMVECTOR column0 = XMVectorSet(proj.m[0][0], proj.m[1][0], proj.m[2][0], 0.0f);
		const XMVECTOR column3 = XMVectorSet(proj.m[0][3], proj.m[1][3], proj.m[2][3], 0.0f);

		const float l1 = 1.0f / (XMVectorGetX(XMVector3Dot(ppcam0, column3)) + proj.m[3][3]);
		const float c1 = (XMVectorGetX(XMVector3Dot(ppcam0, column0)) + proj.m[3][0]) * l1;
		const float l2 = 1.0f / (XMVectorGetX(XMVector3Dot(ppcam1, column3)) + proj.m[3][3]);
		const float c2 = (XMVectorGetX(XMVector3Dot(ppcam1, column0)) + proj.m[3][0]) * l2;
		return 1.0f / ((c2 - c1) * Graphics_GetScreenWidth());
	}

	// This and the above functions have been adapted for the game from "Textured Lines In D3D" by Pierre Terdiman
	// https://www.flipcode.com/archives/Textured_Lines_In_D3D.shtml
	void ComputeScreenQuad(const XMMATRIX& inverseview, const XMMATRIX& view, const XMFLOAT4X4* proj, XMVECTOR* verts, const XMVECTOR& p0, const XMVECTOR& p1, float size)
	{
		// Compute delta in camera space
		const XMVECTOR Delta = XMVector3TransformNormal(p1-p0, view);

		// Compute size factors
		float SizeP0 = size;
		float SizeP1 = size;

		if(proj != nullptr)
		{
			// Compute scales so that screen-size is constant
			SizeP0 *= ComputeConstantScale(p0, view, *proj);
			SizeP1 *= ComputeConstantScale(p1, view, *proj);
		}
		else
		{
			SizeP0 /= 2.0f;
			SizeP1 /= 2.0f;
		}

		// Compute quad vertices
		const float Theta0 = atan2f(-XMVectorGetX(Delta), -XMVectorGetY(Delta));
		const float c0 = SizeP0 * std::cos(Theta0);
		const float s0 = SizeP0 * std::sin(Theta0);
		*verts++ = ComputePoint(c0, -s0, inverseview, p0);
		*verts++ = ComputePoint(-c0, s0, inverseview, p0);

		const float Theta1 = atan2f(XMVectorGetX(Delta), XMVectorGetY(Delta));
		const float c1 = SizeP1 * std::cos(Theta1);
		const float s1 = SizeP1 * std::sin(Theta1);
		*verts++ = ComputePoint(-c1, s1, inverseview, p1);
		*verts++ = ComputePoint(c1, -s1, inverseview, p1);
	}

	void (*Core_Blitter2D_Tri2D_G)(BlitTri2D_G* tris, uint32_t numTris);
	void (*Core_Blitter3D_Tri3D_G)(BlitTri3D_G* tris, uint32_t numTris);

	static void** dword_936C0C;

	static void* Core_Blitter2D_Rect2D_G_JumpBack;
	__declspec(naked) void Core_Blitter2D_Rect2D_G_Original(BlitRect2D_G*, uint32_t)
	{
		__asm
		{
			sub		esp, 028h
			mov		eax, dword ptr [dword_936C0C]
			mov		eax, dword ptr [eax]
			jmp		[Core_Blitter2D_Rect2D_G_JumpBack]
		}
	}

	void Core_Blitter2D_Rect2D_G_HalfPixel(BlitRect2D_G* verts, uint32_t numVerts)
	{
		for (uint32_t i = 0; i < numVerts; i++)
		{
			for (float& val : verts[i].X)
			{
				val = OffsetTexel(val);
			}
			for (float& val : verts[i].Y)
			{
				val = OffsetTexel(val);
			}
		}
		Core_Blitter2D_Rect2D_G_Original(verts, numVerts);
	}

	static void* Core_Blitter2D_Rect2D_GT_JumpBack;
	__declspec(naked) void Core_Blitter2D_Rect2D_GT_Original(BlitRect2D_GT*, uint32_t)
	{
		__asm
		{
			sub		esp, 028h
			mov		eax, dword ptr [dword_936C0C]
			mov		eax, dword ptr [eax]
			jmp		[Core_Blitter2D_Rect2D_GT_JumpBack]
		}
	}

	void Core_Blitter2D_Rect2D_GT_HalfPixel(BlitRect2D_GT* verts, uint32_t numRectangles)
	{
		for (uint32_t i = 0; i < numRectangles; i++)
		{
			for (float& val : verts[i].X)
			{
				val = OffsetTexel(val);
			}
			for (float& val : verts[i].Y)
			{
				val = OffsetTexel(val);
			}
		}
		Core_Blitter2D_Rect2D_GT_Original(verts, numRectangles);
	}

	static void* Core_Blitter2D_Quad2D_G_JumpBack;
	__declspec(naked) void Core_Blitter2D_Quad2D_G_Original(BlitQuad2D_G*, uint32_t)
	{
		__asm
		{
			sub		esp, 028h
			mov		eax, dword ptr [dword_936C0C]
			mov		eax, dword ptr [eax]
			jmp		[Core_Blitter2D_Quad2D_G_JumpBack]
		}
	}

	void Core_Blitter2D_Quad2D_G_HalfPixel(BlitQuad2D_G* quads, uint32_t numQuads)
	{
		for (uint32_t i = 0; i < numQuads; i++)
		{
			for (float& val : quads[i].X)
			{
				val = OffsetTexel(val);
			}
			for (float& val : quads[i].Y)
			{
				val = OffsetTexel(val);
			}
		}
		Core_Blitter2D_Quad2D_G_Original(quads, numQuads);
	}

	static void* Core_Blitter2D_Quad2D_GT_JumpBack;
	__declspec(naked) void Core_Blitter2D_Quad2D_GT_Original(BlitQuad2D_GT*, uint32_t)
	{
		__asm
		{
			sub		esp, 028h
			mov		eax, dword ptr [dword_936C0C]
			mov		eax, dword ptr [eax]
			jmp		[Core_Blitter2D_Quad2D_GT_JumpBack]
		}
	}

	void Core_Blitter2D_Quad2D_GT_HalfPixel(BlitQuad2D_GT* quads, uint32_t numQuads)
	{
		for (uint32_t i = 0; i < numQuads; i++)
		{
			for (float& val : quads[i].X)
			{
				val = OffsetTexel(val);
			}
			for (float& val : quads[i].Y)
			{
				val = OffsetTexel(val);
			}
		}
		Core_Blitter2D_Quad2D_GT_Original(quads, numQuads);
	}

	static D3DMATRIX* pViewMatrix;
	static D3DMATRIX* pProjectionMatrix;

	static void* Core_Blitter2D_Line2D_G_JumpBack;
	__declspec(naked) void Core_Blitter2D_Line2D_G_Original(BlitLine2D_G*, uint32_t)
	{
		__asm
		{
			sub		esp, 010h
			mov		ecx, dword ptr [dword_936C0C]
			mov		ecx, dword ptr [ecx]
			jmp		[Core_Blitter2D_Line2D_G_JumpBack]
		}	
	}

	void Core_Blitter2D_Line2D_G_HalfPixelAndThickness(BlitLine2D_G* lines, uint32_t numLines)
	{
#ifndef NDEBUG
		if (GetAsyncKeyState(VK_F1) & 0x8000)
		{
			Core_Blitter2D_Line2D_G_Original(lines, numLines);
			return;
		}
#endif

		auto buf = _malloca(sizeof(BlitTri2D_G) * 2 * numLines);
		if (buf != nullptr)
		{
			using namespace DirectX;

			const XMMATRIX identityMatrix = XMMatrixIdentity();
			const float targetThickness = Graphics_GetScreenHeight() / 480.0f;

			BlitTri2D_G* tris = reinterpret_cast<BlitTri2D_G*>(buf);
			BlitTri2D_G* currentTri = tris;
			for (uint32_t i = 0; i < numLines; i++)
			{
				XMVECTOR quad[4];
				ComputeScreenQuad(identityMatrix, identityMatrix, nullptr, quad, XMVectorSet(std::floor(lines[i].X[0]), std::floor(lines[i].Y[0]), 0.0f, 1.0f),
													XMVectorSet(std::floor(lines[i].X[1]), std::floor(lines[i].Y[1]), 0.0f, 1.0f), targetThickness);

				// Make triangles from quad
				currentTri->X[0] = OffsetTexel(XMVectorGetX(quad[1]));
				currentTri->Y[0] = OffsetTexel(XMVectorGetY(quad[1]));
				currentTri->X[1] = OffsetTexel(XMVectorGetX(quad[0]));
				currentTri->Y[1] = OffsetTexel(XMVectorGetY(quad[0]));
				currentTri->X[2] = OffsetTexel(XMVectorGetX(quad[2]));
				currentTri->Y[2] = OffsetTexel(XMVectorGetY(quad[2]));
				currentTri->Z = lines[i].Z[0];
				std::fill(std::begin(currentTri->color), std::end(currentTri->color), lines[i].color[0]);
				currentTri++;

				currentTri->X[0] = OffsetTexel(XMVectorGetX(quad[2]));
				currentTri->Y[0] = OffsetTexel(XMVectorGetY(quad[2]));
				currentTri->X[1] = OffsetTexel(XMVectorGetX(quad[3]));
				currentTri->Y[1] = OffsetTexel(XMVectorGetY(quad[3]));
				currentTri->X[2] = OffsetTexel(XMVectorGetX(quad[1]));
				currentTri->Y[2] = OffsetTexel(XMVectorGetY(quad[1]));
				currentTri->Z = lines[i].Z[0];
				std::fill(std::begin(currentTri->color), std::end(currentTri->color), lines[i].color[1]);
				currentTri++;
			}

			Core_Blitter2D_Tri2D_G(tris, 2 * numLines);
		}
		_freea(buf);
	}

	static void* Core_Blitter3D_Line3D_G_JumpBack;
	__declspec(naked) void Core_Blitter3D_Line3D_G_Original(BlitLine3D_G*, uint32_t)
	{
		__asm
		{
			push	ebp
			mov		ebp, esp
			sub		esp, 8
			jmp		[Core_Blitter3D_Line3D_G_JumpBack]
		}
	}

	void Core_Blitter3D_Line3D_G_LineThickness(BlitLine3D_G* lines, uint32_t numLines)
	{
#ifndef NDEBUG
		if (GetAsyncKeyState(VK_F1) & 0x8000)
		{
			Core_Blitter3D_Line3D_G_Original(lines, numLines);
			return;
		}
#endif

		auto buf = _malloca(sizeof(BlitTri3D_G) * 2 * numLines);
		if (buf != nullptr)
		{
			const XMMATRIX viewMatrix = XMLoadFloat4x4(reinterpret_cast<XMFLOAT4X4*>(pViewMatrix));
			const XMMATRIX invViewMatrix = XMMatrixInverse(nullptr, viewMatrix);

			const float targetThickness = Graphics_GetScreenHeight() / 480.0f;

			BlitTri3D_G* tris = reinterpret_cast<BlitTri3D_G*>(buf);
			BlitTri3D_G* currentTri = tris;
			for (uint32_t i = 0; i < numLines; i++)
			{
				XMVECTOR quad[4];
				ComputeScreenQuad(invViewMatrix, viewMatrix, reinterpret_cast<const XMFLOAT4X4*>(pProjectionMatrix), quad,
					XMVectorSet(lines[i].X1, lines[i].Y1, lines[i].Z1, 1.0f), XMVectorSet(lines[i].X2, lines[i].Y2, lines[i].Z2, 1.0f), targetThickness);

				// Make triangles from quad
				XMStoreFloat4(reinterpret_cast<XMFLOAT4*>(&currentTri->X1), quad[1]);
				XMStoreFloat4(reinterpret_cast<XMFLOAT4*>(&currentTri->X2), quad[0]);
				XMStoreFloat4(reinterpret_cast<XMFLOAT4*>(&currentTri->X3), quad[2]);
				std::fill(std::begin(currentTri->color), std::end(currentTri->color), lines[i].color[0]);
				currentTri++;

				XMStoreFloat4(reinterpret_cast<XMFLOAT4*>(&currentTri->X1), quad[2]);
				XMStoreFloat4(reinterpret_cast<XMFLOAT4*>(&currentTri->X2), quad[3]);
				XMStoreFloat4(reinterpret_cast<XMFLOAT4*>(&currentTri->X3), quad[1]);
				std::fill(std::begin(currentTri->color), std::end(currentTri->color), lines[i].color[1]);
				currentTri++;
			}

			Core_Blitter3D_Tri3D_G(tris, 2 * numLines);
		}
		_freea(buf);
	}
}

namespace ScissorTacho
{
	__declspec(naked) int __cdecl ftol_fake()
	{
		__asm
		{
			sub		esp, 4
			fstp	dword ptr [esp]
			pop		eax
			retn
		}
	}

	static void (*orgHandyFunction_BlitTexture)(void* texture, int u1, int v1, int u2, int v2, int posX, int posY, int width, int height, uint32_t color);
	void HandyFunction_BlitTexture_Scissor(void* texture, int u1, int v1, float u2, int v2, int posX, int posY, float width, int height, uint32_t color)
	{
		if (width <= 0.0f)
		{
			return;
		}

		IDirect3DDevice9* device = *gpd3dDevice;

		if ((gD3DCaps->RasterCaps & D3DPRASTERCAPS_SCISSORTEST) != 0)
		{
			const float ScaleX = Graphics_GetScreenWidth() / GetScaledResolutionWidth();

			device->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);

			const int viewportOffset = Viewport_GetCurrent()->m_left;
			const RECT scissorRect { static_cast<LONG>(posX * ScaleX) + viewportOffset, 0,
				static_cast<LONG>((posX+width) * ScaleX) + viewportOffset, static_cast<LONG>(Graphics_GetScreenHeight()) };
			device->SetScissorRect(&scissorRect);

			orgHandyFunction_BlitTexture(texture, u1, v1, 128, v2, posX, posY, (*gpPositionInfoMulti)->m_TachoWidth, height, color);

			device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
		}
		else
		{
			orgHandyFunction_BlitTexture(texture, u1, v1, static_cast<int>(u2), v2, posX, posY, static_cast<int>(width), height, color);
		}
	}
}

namespace BetterBoxDrawing
{
	void DisplaySelectionBox(int posX, int posY, int width, int height, unsigned int alpha, int fill)
	{
		static constexpr uint32_t BOX_COLOR = 0xDCDCDC;

		const float Scale = GetScaledResolutionWidth();
		const float ScaleX = Graphics_GetScreenWidth() / Scale;
		const float ScaleY = Graphics_GetScreenHeight() / 480.0f;

		// The original game used lines, but we use rects for nice and tidy line thickness
		const float LineThickness = std::max(1.0f, std::floor(ScaleX));
		const float HalfLineThickness = LineThickness / 2.0f;

		BlitRect2D_G rects[5];
		{
			const uint32_t color = (alpha << 24) | BOX_COLOR;
			for (auto it = rects; it != rects+4; ++it)
			{
				std::fill(std::begin(it->color), std::end(it->color), color);
				it->Z = 0.0f;
			}
		}

		uint32_t rectsToDraw = 0;
		{
			BlitRect2D_G& top = rects[rectsToDraw++];
			top.X[0] = (posX * ScaleX) - HalfLineThickness;
			top.X[1] = ((posX + width) * ScaleX) + HalfLineThickness;
			top.Y[0] = (posY * ScaleY) - HalfLineThickness;
			top.Y[1] = (posY * ScaleY) + HalfLineThickness;
		}
		{
			BlitRect2D_G& right = rects[rectsToDraw++];
			right.X[0] = ((posX + width) * ScaleX) - HalfLineThickness;
			right.X[1] = ((posX + width) * ScaleX) + HalfLineThickness;
			right.Y[0] = (posY * ScaleY) + HalfLineThickness;
			right.Y[1] = ((posY + height) * ScaleY) - HalfLineThickness;
		}
		{
			BlitRect2D_G& bottom = rects[rectsToDraw++];
			bottom.X[0] = (posX * ScaleX) - HalfLineThickness;
			bottom.X[1] = ((posX + width) * ScaleX) + HalfLineThickness;
			bottom.Y[0] = ((posY + height) * ScaleY) - HalfLineThickness;
			bottom.Y[1] = ((posY + height) * ScaleY) + HalfLineThickness;
		}
		{
			BlitRect2D_G& left = rects[rectsToDraw++];
			left.X[0] = (posX * ScaleX) - HalfLineThickness;
			left.X[1] = (posX * ScaleX) + HalfLineThickness;
			left.Y[0] = (posY * ScaleY) + HalfLineThickness;
			left.Y[1] = ((posY + height) * ScaleY) - HalfLineThickness;
		}

		if (fill != 0)
		{
			const uint32_t color = ((alpha / 2) << 24) | BOX_COLOR;

			BlitRect2D_G& rect = rects[rectsToDraw++];
			rect.Z = 0.0f;
			std::fill(std::begin(rect.color), std::end(rect.color), color);

			rect.X[0] = (posX * ScaleX) + HalfLineThickness;
			rect.X[1] = ((posX + width) * ScaleX) - HalfLineThickness;

			rect.Y[0] = (posY * ScaleY) + HalfLineThickness;
			rect.Y[1] = ((posY + height) * ScaleY) - HalfLineThickness;
		}

		Core_Blitter2D_Rect2D_G(rects, rectsToDraw);
	}

	void Keyboard_DrawTextEntryBox(int posX, int posY, int width, int height, unsigned int color, int fill)
	{
		const float Scale = GetScaledResolutionWidth();
		const float ScaleX = Graphics_GetScreenWidth() / Scale;
		const float ScaleY = Graphics_GetScreenHeight() / 480.0f;

		// The original game used lines, but we use rects for nice and tidy line thickness
		const float LineThickness = std::max(1.0f, std::floor(ScaleX));
		const float HalfLineThickness = LineThickness / 2.0f;

		if (fill != 0)
		{
			BlitRect2D_G rect;
			rect.Z = 0.0f;
			std::fill(std::begin(rect.color), std::end(rect.color), color);

			rect.X[0] = (posX * ScaleX) - HalfLineThickness;
			rect.X[1] = ((posX + width) * ScaleX) + HalfLineThickness;
			
			rect.Y[0] = (posY * ScaleY) - HalfLineThickness;
			rect.Y[1] = ((posY + height) * ScaleY) + HalfLineThickness;
			Core_Blitter2D_Rect2D_G(&rect, 1);
			return;
		}

		BlitRect2D_G rects[4];
		for (BlitRect2D_G& rect : rects)
		{
			std::fill(std::begin(rect.color), std::end(rect.color), color);
			rect.Z = 0.0f;
		}

		{
			BlitRect2D_G& top = rects[0];
			top.X[0] = (posX * ScaleX) - HalfLineThickness;
			top.X[1] = ((posX + width) * ScaleX) + HalfLineThickness;
			top.Y[0] = (posY * ScaleY) - HalfLineThickness;
			top.Y[1] = (posY * ScaleY) + HalfLineThickness;
		}
		{
			BlitRect2D_G& right = rects[1];
			right.X[0] = ((posX + width) * ScaleX) - HalfLineThickness;
			right.X[1] = ((posX + width) * ScaleX) + HalfLineThickness;
			right.Y[0] = (posY * ScaleY) + HalfLineThickness;
			right.Y[1] = ((posY + height) * ScaleY) - HalfLineThickness;
		}
		{
			BlitRect2D_G& bottom = rects[2];
			bottom.X[0] = (posX * ScaleX) - HalfLineThickness;
			bottom.X[1] = ((posX + width) * ScaleX) + HalfLineThickness;
			bottom.Y[0] = ((posY + height) * ScaleY) - HalfLineThickness;
			bottom.Y[1] = ((posY + height) * ScaleY) + HalfLineThickness;
		}
		{
			BlitRect2D_G& left = rects[3];
			left.X[0] = (posX * ScaleX) - HalfLineThickness;
			left.X[1] = (posX * ScaleX) + HalfLineThickness;
			left.Y[0] = (posY * ScaleY) + HalfLineThickness;
			left.Y[1] = ((posY + height) * ScaleY) - HalfLineThickness;
		}

		Core_Blitter2D_Rect2D_G(rects, std::size(rects));
	}

	void HandyFunction_DrawClipped2DBox(int posX, int posY, int width, int height, unsigned int color, int clipX, int clipY, int clipWidth, int clipHeight, int fill)
	{
		const float Scale = GetScaledResolutionWidth();
		const float ScaleX = Graphics_GetScreenWidth() / Scale;
		const float ScaleY = Graphics_GetScreenHeight() / 480.0f;

		// Cut height/width down by 1px to make clipped boxes consistent with clipped fonts (original bug most likely)
		const float ClipX1 = clipX * ScaleX, ClipX2 = (clipX + clipWidth) * ScaleX - 1.0f;
		const float ClipY1 = clipY * ScaleY, ClipY2 = (clipY + clipHeight) * ScaleY - 1.0f;

		// The original game used lines, but we use rects for nice and tidy line thickness
		const float LineThickness = std::max(1.0f, std::floor(ScaleX));
		const float HalfLineThickness = LineThickness / 2.0f;

		if (fill != 0)
		{
			BlitRect2D_G rect;
			rect.Z = 0.0f;
			std::fill(std::begin(rect.color), std::end(rect.color), color);

			rect.X[0] = (posX * ScaleX) - HalfLineThickness;
			rect.X[1] = ((posX + width) * ScaleX) + HalfLineThickness;

			rect.Y[0] = (posY * ScaleY) - HalfLineThickness;
			rect.Y[1] = ((posY + height) * ScaleY) + HalfLineThickness;
			if (HandyFunction_Clip2DRect(&rect, ClipX1, ClipY1, ClipX2, ClipY2) != 0)
			{
				Core_Blitter2D_Rect2D_G(&rect, 1);
			}
			return;
		}

		BlitRect2D_G rects[4];
		for (BlitRect2D_G& rect : rects)
		{
			std::fill(std::begin(rect.color), std::end(rect.color), color);
			rect.Z = 0.0f;
		}

		uint32_t numSides = 0;
		{
			BlitRect2D_G& top = rects[numSides];
			top.X[0] = (posX * ScaleX) - HalfLineThickness;
			top.X[1] = ((posX + width) * ScaleX) + HalfLineThickness;
			top.Y[0] = (posY * ScaleY) - HalfLineThickness;
			top.Y[1] = (posY * ScaleY) + HalfLineThickness;
			if (HandyFunction_Clip2DRect(&top, ClipX1, ClipY1, ClipX2, ClipY2) != 0)
			{
				numSides++;
			}
		}
		{
			BlitRect2D_G& right = rects[numSides];
			right.X[0] = ((posX + width) * ScaleX) - HalfLineThickness;
			right.X[1] = ((posX + width) * ScaleX) + HalfLineThickness;
			right.Y[0] = (posY * ScaleY) + HalfLineThickness;
			right.Y[1] = ((posY + height) * ScaleY) - HalfLineThickness;
			if (HandyFunction_Clip2DRect(&right, ClipX1, ClipY1, ClipX2, ClipY2) != 0)
			{
				numSides++;
			}
		}
		{
			BlitRect2D_G& bottom = rects[numSides];
			bottom.X[0] = (posX * ScaleX) - HalfLineThickness;
			bottom.X[1] = ((posX + width) * ScaleX) + HalfLineThickness;
			bottom.Y[0] = ((posY + height) * ScaleY) - HalfLineThickness;
			bottom.Y[1] = ((posY + height) * ScaleY) + HalfLineThickness;
			if (HandyFunction_Clip2DRect(&bottom, ClipX1, ClipY1, ClipX2, ClipY2) != 0)
			{
				numSides++;
			}
		}
		{
			BlitRect2D_G& left = rects[numSides];
			left.X[0] = (posX * ScaleX) - HalfLineThickness;
			left.X[1] = (posX * ScaleX) + HalfLineThickness;
			left.Y[0] = (posY * ScaleY) + HalfLineThickness;
			left.Y[1] = ((posY + height) * ScaleY) - HalfLineThickness;
			if (HandyFunction_Clip2DRect(&left, ClipX1, ClipY1, ClipX2, ClipY2) != 0)
			{
				numSides++;
			}
		}

		Core_Blitter2D_Rect2D_G(rects, numSides);
	}

	void DrawCountdownOutline(float X1, float Y1, float X2, float Y2, unsigned int color)
	{
		const float Scale = GetScaledResolutionWidth();
		const float ScaleX = Graphics_GetScreenWidth() / Scale;

		// The original game used lines, but we use rects for nice and tidy line thickness
		const float LineThickness = std::max(1.0f, std::floor(ScaleX));
		const float HalfLineThickness = LineThickness / 2.0f;

		BlitRect2D_G rects[4];
		for (BlitRect2D_G& rect : rects)
		{
			std::fill(std::begin(rect.color), std::end(rect.color), color);
			rect.Z = 0.0f;
		}

		{
			BlitRect2D_G& top = rects[0];
			top.X[0] = X1 - HalfLineThickness;
			top.X[1] = X2 + HalfLineThickness;
			top.Y[0] = Y1 - HalfLineThickness;
			top.Y[1] = Y1 + HalfLineThickness;
		}
		{
			BlitRect2D_G& right = rects[1];
			right.X[0] = X2 - HalfLineThickness;
			right.X[1] = X2 + HalfLineThickness;
			right.Y[0] = Y1 + HalfLineThickness;
			right.Y[1] = Y2 - HalfLineThickness;
		}
		{
			BlitRect2D_G& bottom = rects[2];
			bottom.X[0] = X1 - HalfLineThickness;
			bottom.X[1] = X2 + HalfLineThickness;
			bottom.Y[0] = Y2 - HalfLineThickness;
			bottom.Y[1] = Y2 + HalfLineThickness;
		}
		{
			BlitRect2D_G& left = rects[3];
			left.X[0] = X1 - HalfLineThickness;
			left.X[1] = X1 + HalfLineThickness;
			left.Y[0] = Y1 + HalfLineThickness;
			left.Y[1] = Y2 - HalfLineThickness;
		}

		Core_Blitter2D_Rect2D_G(rects, std::size(rects));
	}

	static thread_local float SavedPosY;
	void Blitter2D_Line2D_G_FirstLine(BlitLine2D_G* lines, uint32_t /*numLines*/)
	{
		SavedPosY = lines->Y[0];
	}

	void Blitter2D_Line2D_G_SecondLine(BlitLine2D_G* lines, uint32_t /*numLines*/)
	{
		DrawCountdownOutline(lines->X[0], SavedPosY, lines->X[1], lines->Y[1], lines->color[0]);
	}

	void Blitter2D_Line2D_G_NOP(BlitLine2D_G* /*lines*/, uint32_t /*numLines*/)
	{
	
	}
}

// Hack to scretch full-width solid backgrounds to the entire screen, saves 30+ patches
namespace SolidRectangleWidthHack
{
	static void* HandyFunction_Draw2DBox_JumpBack;
	__declspec(naked) void HandyFunction_Draw2DBox_Original(int /*posX*/, int /*posY*/, int /*width*/, int /*height*/, int /*color*/)
	{
		__asm
		{
			sub		esp, 50h
			push	esi
			push	edi
			jmp		[HandyFunction_Draw2DBox_JumpBack]
		}
	}

	void HandyFunction_Draw2DBox_Hack(int posX, int posY, int width, int height, int color)
	{
		// If it's a draw spanning the entire width, stretch it automatically.
		if (posX == 0 && width == 640)
		{
			width = static_cast<int>(std::ceil(GetScaledResolutionWidth()));
		}
		HandyFunction_Draw2DBox_Original(posX, posY, width, height, color);
	}

	static void* CMR3Font_SetViewport_JumpBack;
	__declspec(naked) void CMR3Font_SetViewport_Original(int /*a1*/, int /*x1*/, int /*y1*/, int /*x2*/, int /*y2*/)
	{
		__asm
		{
			mov		eax, [esp+4]
			push	ebx
			jmp		[CMR3Font_SetViewport_JumpBack]
		}
	}

	void CMR3Font_SetViewport_Hack(int a1, int x1, int y1, int x2, int y2)
	{
		// If it's a viewport spanning the entire width, stretch it automatically.
		if (x1 == 0 && x2 == 640)
		{
			x2 = static_cast<int>(std::ceil(GetScaledResolutionWidth()));
		}
		CMR3Font_SetViewport_Original(a1, x1, y1, x2, y2);
	}
}

namespace ConstantViewports
{
	template<std::size_t Index>
	void (*orgViewport_SetDimension)(D3DViewport* viewport, int left, int top, int right, int bottom);

	template<std::size_t Index>
	static void Viewport_SetDimension(D3DViewport* viewport, int left, int top, int right, int bottom)
	{
		orgViewport_SetDimension<Index>(viewport, left, top, right, bottom);
		Viewport_SetAspectRatio(viewport, 1.0f, 1.0f);
	}

	template<std::size_t Ctr, typename Tuple, std::size_t... I, typename Func>
	void HookEachImpl(Tuple&& tuple, std::index_sequence<I...>, Func&& f)
	{
		(f(std::get<I>(tuple), orgViewport_SetDimension<Ctr << 16 | I>, Viewport_SetDimension<Ctr << 16 | I>), ...);
	}

	template<std::size_t Ctr = 0, typename Vars, typename Func>
	void HookEach(Vars&& vars, Func&& f)
	{
		auto tuple = std::tuple_cat(std::forward<Vars>(vars));
		HookEachImpl<Ctr>(std::move(tuple), std::make_index_sequence<std::tuple_size_v<decltype(tuple)>>{}, std::forward<Func>(f));
	}
}

void CMR3Font_BlitText_CalibrateAxisName(uint8_t a1, const char* text, int /*posX*/, int posY, int a5, char /*a6*/)
{
	CMR3Font_BlitText_Center(a1, text, 140, posY + 10, a5, 0x14);
}

namespace ScaledTexturesSupport
{
	D3DTexture* CreateTexture_Misc_Scaled_Internal(D3DTexture* result, const char* name)
	{
		if (result != nullptr && name != nullptr)
		{
			struct TexData
			{
				uint32_t width, height;
				bool useNearest = false;
			};

			static const std::map<std::string_view, TexData, std::less<>> textureDimensions = {
				{ "Arrow1Player", { 32, 16 } },
				{ "ArrowMultiPlayer", { 32, 16 } },
				{ "ArrowSmall", { 16, 16 } },
				{ "Base", { 128, 128, true } },
				{ "certina", { 64, 8 } },
				{ "Colour", { 128, 128, true } },
				{ "Ck_base", { 128, 64 } },
				{ "Ck_00", { 32, 32 } },
				{ "Ck_01", { 32, 32 } },
				{ "Ck_02", { 32, 32 } },
				{ "Ck_03", { 32, 32 } },
				{ "Ck_04", { 32, 32 } },
				{ "Ck_05", { 32, 32 } },
				{ "colin3_2", { 256, 64 } },
				{ "ct_3", { 64, 64, true } },
				{ "dialcntr", { 32, 32 } },
				{ "infobox", { 128, 128 } },
				{ "MiniStageBanner", { 128, 32 } },
				{ "osd_glow", { 32, 32 } },
				{ "rescert", { 128, 32 } },
				{ "swiss", { 128, 8 } },
				{ "AUS", { 32, 16 } },
				{ "FIN", { 32, 16 } },
				{ "GRE", { 32, 16 } },
				{ "JAP", { 32, 16 } },
				{ "SPA", { 32, 16 } },
				{ "SWE", { 32, 16 } },
				{ "UK", { 32, 16 } },
				{ "USA", { 32, 16 } },
			};
			auto it = textureDimensions.find(name);
			if (it != textureDimensions.end())
			{
				result->m_width = it->second.width;
				result->m_height = it->second.height;

				if (it->second.useNearest)
				{
					Core_Texture_SetFilteringMethod(result, D3DTEXF_POINT, D3DTEXF_POINT, D3DTEXF_POINT);
				}
			}
		}
		return result;
	}

	template<std::size_t Index>
	D3DTexture* (*orgCreateTexture_Misc_Scaled)(void* a1, const char* name, int a3, int a4, int a5);

	template<std::size_t Index>
	D3DTexture* CreateTexture_Misc_Scaled(void* a1, const char* name, int a3, int a4, int a5)
	{
		D3DTexture* result = orgCreateTexture_Misc_Scaled<Index>(a1, name, a3, a4, a5);
		return CreateTexture_Misc_Scaled_Internal(result, name);
	}

	template<std::size_t Ctr, typename Tuple, std::size_t... I, typename Func>
	void HookEachImpl_Misc_Scaled(Tuple&& tuple, std::index_sequence<I...>, Func&& f)
	{
		(f(std::get<I>(tuple), orgCreateTexture_Misc_Scaled<Ctr << 16 | I>, CreateTexture_Misc_Scaled<Ctr << 16 | I>), ...);
	}

	template<std::size_t Ctr = 0, typename Vars, typename Func>
	void HookEach_Misc_Scaled(Vars&& vars, Func&& f)
	{
		auto tuple = std::tuple_cat(std::forward<Vars>(vars));
		HookEachImpl_Misc_Scaled<Ctr>(std::move(tuple), std::make_index_sequence<std::tuple_size_v<decltype(tuple)>>{}, std::forward<Func>(f));
	}

	static D3DTexture* (*orgCreateTexture_Font)(void* a1, const char* name, int a3, int a4, int a5);
	D3DTexture* CreateTexture_Font_Scaled(void* a1, const char* name, int a3, int a4, int a5)
	{
		D3DTexture* result = orgCreateTexture_Font(a1, name, a3, a4, a5);
		if (result != nullptr)
		{
			bool useNearest = false;
			if (name != nullptr)
			{
				static const std::map<std::string_view, std::pair<uint32_t, bool>, std::less<>> textureDimensions = {
					{ "kro_11", { 128, false } },
					{ "kro_20", { 256, true } },
					{ "O_10pt", { 64, false } },
					{ "O_12pt", { 64, false } },
					{ "O_30pt", { 128, false } },
					{ "Out_30pt", { 128, false } },
					{ "Dig_18pt", { 128, false } },
					{ "3d_lcd", { 128, false } },
					{ "gears", { 128, true } },
					{ "time", { 64, true } },
					{ "mph", { 128, false } },
					{ "speed", { 64, true } },
					{ "hel_18pt", { 256, false } },
				};
				auto it = textureDimensions.find(name);
				if (it != textureDimensions.end())
				{
					// Fonts need to take ratios into consideration, unlike normal textures where we force a fixed size
					const float heightRatio = result->m_height / static_cast<float>(it->second.first);

					result->m_width = static_cast<uint32_t>(result->m_width / heightRatio);
					result->m_height = it->second.first;
					useNearest = it->second.second;
				}
			}

			if (useNearest)
			{
				Core_Texture_SetFilteringMethod(result, D3DTEXF_POINT, D3DTEXF_POINT, D3DTEXF_POINT);
			}
			else
			{
				Core_Texture_SetFilteringMethod(result, D3DTEXF_LINEAR, D3DTEXF_LINEAR, D3DTEXF_LINEAR);
			}
		}
		return result;
	}

	void Texture_SetFilteringMethod_NOP(D3DTexture*, void*, void*, void*)
	{
	}
}

namespace NewAdvancedGraphicsOptions
{
	BOOL WINAPI AdjustWindowRectEx_NOP(LPRECT /*lpRect*/, DWORD /*dwStyle*/, BOOL /*bMenu*/, DWORD /*dwExStyle*/)
	{
		return TRUE;
	}
	static const auto pAdjustWindowRectEx_NOP = &AdjustWindowRectEx_NOP;

	HWND WINAPI FindWindowExA_IgnoreWindowName(HWND hWndParent, HWND hWndChildAfter, LPCSTR lpszClass, LPCSTR /*lpszWindow*/)
	{
		return FindWindowExA(hWndParent, hWndChildAfter, lpszClass, nullptr);
	}
	static const auto pFindWindowExA_IgnoreWindowName = &FindWindowExA_IgnoreWindowName;

	HINSTANCE* ghInstance;
	HWND* ghWindow;
	void Main_WindowDestructor()
	{
		DestroyWindow(*ghWindow);
		*ghWindow = nullptr;
	}

	bool ResizingViaSetWindowPos = false;

	static LRESULT (CALLBACK *orgWndProc)(HWND, UINT, WPARAM, LPARAM);
	LRESULT CALLBACK CustomWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		switch (uMsg)
		{
		case WM_SIZING: // Block the message from reaching the game
		case WM_SETCURSOR:
		{
			return DefWindowProcA(hwnd, uMsg, wParam, lParam);
		}
		case WM_WINDOWPOSCHANGED: // Prevent SetWindowPos from recursing into our WM_SIZE
		{
			if (ResizingViaSetWindowPos)
			{
				return 0;
			}
			break;
		}
		case WM_GETMINMAXINFO:
		{
			auto lpMMI = reinterpret_cast<LPMINMAXINFO>(lParam);
			lpMMI->ptMinTrackSize.x = 640;
			lpMMI->ptMinTrackSize.y = 480;
			return 0;
		}
		default:
			break;
		}
		return orgWndProc(hwnd, uMsg, wParam, lParam);
	}

	static void GetDesiredWindowStyle(uint32_t displayMode, DWORD& dwStyle, DWORD& dwExStyle)
	{
		dwStyle = WS_VISIBLE;
		dwExStyle = WS_EX_APPWINDOW;
		if (displayMode == 1) // Non-borderless windowed only
		{
			dwStyle |= WS_OVERLAPPEDWINDOW;
		}
		else
		{
			dwStyle |= WS_POPUP;
		}
	}

	void (*orgGraphics_Initialise)(Graphics_Config* config);
	void Graphics_Initialise_NewOptions(Graphics_Config* config)
	{
		using namespace Registry;

		const uint32_t displayMode = GetRegistryDword(REGISTRY_SECTION_NAME, DISPLAY_MODE_KEY_NAME).value_or(0);

		config->m_windowed = displayMode != 0;
		config->m_borderless = displayMode == 2;

		config->m_presentationInterval = GetRegistryDword(REGISTRY_SECTION_NAME, VSYNC_KEY_NAME).value_or(1) != 0
					? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;

		CMR_FE_SetAnisotropicLevel(GetRegistryDword(REGISTRY_SECTION_NAME, ANISOTROPIC_KEY_NAME).value_or(0));

		RenderState_InitialiseFallbackFilters(config->m_adapter);

		orgGraphics_Initialise(config);
	}

	BOOL CreateClassAndWindow(HINSTANCE hInstance, HWND* outWindow, LPCSTR lpClassName, LRESULT (CALLBACK *wndProc)(HWND, UINT, WPARAM, LPARAM), RECT rect)
	{
		ghWindow = outWindow;
		*ghInstance = hInstance;
		orgWndProc = wndProc;

		WNDCLASSEXA wndClass { sizeof(wndClass) };
		wndClass.lpfnWndProc = CustomWndProc;
		wndClass.hInstance = hInstance;
		wndClass.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
		wndClass.lpszClassName = lpClassName;
		wndClass.style = CS_VREDRAW|CS_HREDRAW;
		wndClass.hIcon = SmallIcon = LoadIcon(hInstance, MAKEINTRESOURCE(104));
		if (wndClass.hIcon == nullptr)
		{
			// Extract the icon from Rally_3PC.ico instead
			wil::unique_cotaskmem_string pathToExe;
			if (SUCCEEDED(wil::GetModuleFileNameW(hInstance, pathToExe)))
			{
				wndClass.hIcon = SmallIcon = ExtractIconW(hInstance, std::filesystem::path(pathToExe.get()).replace_filename(L"Rally_3PC.ico").c_str(), 0);
			}
		}
		if (RegisterClassExA(&wndClass) != 0)
		{
			using namespace Registry;

			const uint32_t displayMode = GetRegistryDword(REGISTRY_SECTION_NAME, DISPLAY_MODE_KEY_NAME).value_or(0);
			const char* windowName = "Colin McRae Rally 3";

			DWORD dwStyle = 0;
			DWORD dwExStyle = 0;
			GetDesiredWindowStyle(displayMode, dwStyle, dwExStyle);
			AdjustWindowRectEx(&rect, dwStyle, FALSE, dwExStyle);

			HWND window;
			if (displayMode == 1) // Non-borderless windowed only
			{
				window = CreateWindowExA(dwExStyle, lpClassName, windowName, dwStyle, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, hInstance, nullptr);
			}
			else
			{
				const int cxScreen = DesktopWidth;
				const int cyScreen = DesktopHeight;
				window = CreateWindowExA(dwExStyle, lpClassName, windowName, dwStyle, 0, 0, cxScreen, cyScreen, nullptr, nullptr, hInstance, nullptr);
			}
			*outWindow = window;
			if (window != nullptr)
			{
				SetFocus(window);
				Destruct_AddDestructor(Destruct_GetCoreDestructorGroup(), Main_WindowDestructor);
				return TRUE;
			}
		}

		return FALSE;
	}

	LONG WINAPI SetWindowLongA_NOP(HWND /*hWnd*/, int /*nIndex*/, LONG /*dwNewLong*/)
	{
		return 0;
	}
	static const auto pSetWindowLongA_NOP = &SetWindowLongA_NOP;

	BOOL WINAPI SetWindowPos_Adjust(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags)
	{
		ResizingViaSetWindowPos = true;

		const Graphics_Config& config = Graphics_GetCurrentConfig();

		uint32_t displayMode = 0;
		if (config.m_windowed != 0 && config.m_borderless == 0) displayMode = 1;
		else if (config.m_windowed != 0 && config.m_borderless != 0) displayMode = 2;

		RECT rect { X, Y, X+cx, Y+cy };
		DWORD dwStyle = 0;
		DWORD dwExStyle = 0;
		GetDesiredWindowStyle(displayMode, dwStyle, dwExStyle);
		AdjustWindowRectEx(&rect, dwStyle, FALSE, dwExStyle);

		SetWindowLongPtr(hWnd, GWL_STYLE, dwStyle);
		BOOL result = SetWindowPos(hWnd, hWndInsertAfter, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, 
			uFlags|SWP_FRAMECHANGED);

		SendMessage(hWnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(SmallIcon));

		ResizingViaSetWindowPos = false;
		return result;
	}
	static const auto pSetWindowPos_Adjust = &SetWindowPos_Adjust;

	void ResizeWindowAndUpdateConfig(Graphics_Config* config)
	{
		if (config->m_windowed != 0)
		{
			RECT& rect = config->m_windowRect;
			if (config->m_borderless == 0)
			{		
				rect.left = (DesktopWidth - config->m_resWidth) / 2;
				rect.right = rect.left + config->m_resWidth;
				rect.top = (DesktopHeight - config->m_resHeight) / 2;
				rect.bottom = rect.top + config->m_resHeight;
			}
			else
			{
				// When resizing to borderless, span the entire desktop
				rect.left = 0;
				rect.right = DesktopWidth;
				rect.top = 0;
				rect.bottom = DesktopHeight;
			}

			// Force the game to resize the window
			gd3dPP->Windowed = static_cast<BOOL>(-1);
		}

		// Those are "missing" from the Change call
		gGraphicsConfig->m_borderless = config->m_borderless;
		gGraphicsConfig->m_presentationInterval = config->m_presentationInterval;

		RenderState_InitialiseFallbackFilters(config->m_adapter);
	}

	template<std::size_t Index>
	uint32_t (*orgGraphics_Change)(Graphics_Config*);

	template<std::size_t Index>
	uint32_t Graphics_Change_ResizeWindow(Graphics_Config* config)
	{
		ResizeWindowAndUpdateConfig(config);
		return orgGraphics_Change<Index>(config);
	}

	template<std::size_t Ctr, typename Tuple, std::size_t... I, typename Func>
	void HookEachImpl_Graphics_Change(Tuple&& tuple, std::index_sequence<I...>, Func&& f)
	{
		(f(std::get<I>(tuple), orgGraphics_Change<Ctr << 16 | I>, Graphics_Change_ResizeWindow<Ctr << 16 | I>), ...);
	}

	template<std::size_t Ctr = 0, typename Vars, typename Func>
	void HookEach_Graphics_Change(Vars&& vars, Func&& f)
	{
		auto tuple = std::tuple_cat(std::forward<Vars>(vars));
		HookEachImpl_Graphics_Change<Ctr>(std::move(tuple), std::make_index_sequence<std::tuple_size_v<decltype(tuple)>>{}, std::forward<Func>(f));
	}

	static void* PC_GraphicsAdvanced_Display_NewOptionsJumpBack;
	__declspec(naked) void PC_GraphicsAdvanced_Display_CaseNewOptions()
	{
		__asm
		{
			push	ebx // onColor
			push	edi // offColor
			push	eax // entryID
			push	ebp // posY
			push	dword ptr [esp+338h+8] // interp
			push	dword ptr [esp+33Ch+4] // menu
			call	PC_GraphicsAdvanced_Display_NewOptions
			jmp		[PC_GraphicsAdvanced_Display_NewOptionsJumpBack]
		}
	}

	static void* PC_GraphicsAdvanced_Handle_JumpBack;
	__declspec(naked) int PC_GraphicsAdvanced_Handle_Original(MenuDefinition* /*menu*/, uint32_t /*a2*/, int /*a3*/)
	{
		__asm
		{
			sub		esp, 50h
			lea		eax, [esp+50h-50h]
			jmp		[PC_GraphicsAdvanced_Handle_JumpBack]
		}
	}

	static int gnCurrentWindowMode;
	int PC_GraphicsAdvanced_Handle_NewOptions(MenuDefinition* menu, uint32_t a2, int a3)
	{
		const int adapter = menu->m_entries[EntryID::GRAPHICS_ADV_DRIVER].m_value;
		const Graphics_Config& config = Graphics_GetCurrentConfig();

		// Only care about switching to/from windowed mode, incl. borderless
		if (adapter == *gnCurrentAdapter && gnCurrentWindowMode != config.m_windowed)
		{
			PC_GraphicsAdvanced_PopulateFromCaps(menu, adapter, adapter);
			PC_GraphicsAdvanced_PopulateFromCaps_NewOptions(menu, adapter, adapter);
		}
		gnCurrentWindowMode = config.m_windowed;

		return PC_GraphicsAdvanced_Handle_Original(menu, a2, a3);
	}

	uint32_t (*orgGraphics_SetupRenderFromMenuOptions)(Graphics_Config*);
	uint32_t Graphics_Change_SetupRenderFromMenuOptions(Graphics_Config* config)
	{
		const uint32_t displayMode = gmoFrontEndMenus[MenuID::GRAPHICS_ADVANCED].m_entries[EntryID::GRAPHICS_ADV_DISPLAYMODE].m_value;
		config->m_windowed = displayMode != 0;
		config->m_borderless = displayMode == 2;

		config->m_presentationInterval = gmoFrontEndMenus[MenuID::GRAPHICS_ADVANCED].m_entries[EntryID::GRAPHICS_ADV_VSYNC].m_value != 0
			? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;

		CMR_FE_SetAnisotropicLevel(gmoFrontEndMenus[MenuID::GRAPHICS_ADVANCED].m_entries[EntryID::GRAPHICS_ADV_ANISOTROPIC].m_value);

		return orgGraphics_SetupRenderFromMenuOptions(config);
	}

	static D3DCAPS9* (*orgGraphics_GetAdapterCaps)(D3DCAPS9* hdc, int index);
	D3DCAPS9* Graphics_GetAdapterCaps_DisableGammaForWindow(D3DCAPS9* hdc, int index)
	{
		D3DCAPS9* result = orgGraphics_GetAdapterCaps(hdc, index);
		const Graphics_Config& config = Graphics_GetCurrentConfig();
		if (config.m_windowed != 0)
		{
			result->Caps2 &= ~D3DCAPS2_FULLSCREENGAMMA;
		}
		return result;
	}


	// Anisotropic Filtering
	static void* RenderState_SetSamplerState_JumpBack;
	static int* guSSChanges;
	__declspec(naked) void RenderState_SetSamplerState_Original(DWORD /*Sampler*/, D3DSAMPLERSTATETYPE /*Type*/, DWORD /*Value*/)
	{
		__asm
		{
			mov		edx, [guSSChanges]
			mov		edx, dword ptr [edx]
			jmp		[RenderState_SetSamplerState_JumpBack]
		}
	}

	void RenderState_SetSamplerState_Fallback(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value)
	{
		RenderState_SetSamplerState_Original(Sampler, Type, RenderState_GetFallbackSamplerValue(Type, Value));
	}

	static void (*orgMarkProfiler)(void*);
	void MarkProfiler_SetAF(void* a1)
	{
		orgMarkProfiler(a1);

		const DWORD anisotropicLevel = CMR_GetAnisotropicLevel();
		for (DWORD Sampler = 0; Sampler < 8; ++Sampler)
		{
			RenderState_SetTextureAnisotropicLevel(Sampler, anisotropicLevel);
		}
	}

	static void (*orgSetMipBias)(D3DTexture*, float);
	void Texture_SetMipBiasAndAnisotropic(D3DTexture* texture, float bias)
	{
		orgSetMipBias(texture, bias);
		Core_Texture_SetFilteringMethod(texture, D3DTEXF_ANISOTROPIC, D3DTEXF_ANISOTROPIC, D3DTEXF_LINEAR);
	}

	D3DTEXTUREFILTERTYPE GetAnisotropicFilter()
	{
		return D3DTEXF_ANISOTROPIC;
	}
}

namespace NewGraphicsOptions
{
	static void* PC_GraphicsOptions_Display_NewOptionsJumpBack;
	__declspec(naked) void PC_GraphicsOptions_Display_CaseNewOptions()
	{
		__asm
		{
			push	dword ptr [esp+228h-214h] // onColor
			push	dword ptr [esp+22Ch-218h] // offColor
			push	ebx // entryID
			push	dword ptr [esp+234h-208h] // posY
			push	dword ptr [esp+238h+8] // interp
			push	dword ptr [esp+23Ch+4] // menu
			call	PC_GraphicsOptions_Display_NewOptions
			jmp		[PC_GraphicsOptions_Display_NewOptionsJumpBack]
		}
	}

	static float exteriorFOV = static_cast<float>(75.0f * M_PI / 180.0f);
	static float interiorFOV = static_cast<float>(75.0f * M_PI / 180.0f);

	static void* (*orgCameras_AfterInitialise)();
	void* Cameras_AfterInitialise()
	{
		exteriorFOV = static_cast<float>(CMR_FE_GetExteriorFOV() * M_PI / 180.0f);
		interiorFOV = static_cast<float>(CMR_FE_GetInteriorFOV() * M_PI / 180.0f);

		return orgCameras_AfterInitialise();
	}

	void Camera_SetFOV_Exterior(void* camera, float /*FOV*/)
	{
		float* fov = reinterpret_cast<float*>(static_cast<char*>(camera) + 4);
		*fov = exteriorFOV;
	}

	void Camera_SetFOV_Interior(void* camera, float /*FOV*/)
	{
		float* fov = reinterpret_cast<float*>(static_cast<char*>(camera) + 4);
		*fov = interiorFOV;
	}

	static bool VerticalSplitscreen = false;
	uint32_t IsVerticalSplitscreen()
	{
		return VerticalSplitscreen;
	}

	uint32_t IsVerticalSplitscreen_ReadOption()
	{
		VerticalSplitscreen = CMR_FE_GetVerticalSplitscreen();
		return IsVerticalSplitscreen();
	}

	static bool DigitalTacho = false;

	template<std::size_t Index>
	uint8_t (*orgGameInfo_GetNumberOfPlayersInThisRace)();

	template<std::size_t Index>
	uint8_t GameInfo_GetNumberOfPlayersInThisRace_DigitalTachoHack()
	{
		const uint8_t numPlayers = orgGameInfo_GetNumberOfPlayersInThisRace<Index>();
		if (numPlayers == 1 && DigitalTacho)
		{
			return 2;
		}
		return numPlayers;
	}

	template<std::size_t Ctr, typename Tuple, std::size_t... I, typename Func>
	void HookEachImpl_GetNumberOfPlayersInThisRace(Tuple&& tuple, std::index_sequence<I...>, Func&& f)
	{
		(f(std::get<I>(tuple), orgGameInfo_GetNumberOfPlayersInThisRace<Ctr << 16 | I>, GameInfo_GetNumberOfPlayersInThisRace_DigitalTachoHack<Ctr << 16 | I>), ...);
	}

	template<std::size_t Ctr = 0, typename Vars, typename Func>
	void HookEach_GetNumberOfPlayersInThisRace(Vars&& vars, Func&& f)
	{
		auto tuple = std::tuple_cat(std::forward<Vars>(vars));
		HookEachImpl_GetNumberOfPlayersInThisRace<Ctr>(std::move(tuple), std::make_index_sequence<std::tuple_size_v<decltype(tuple)>>{}, std::forward<Func>(f));
	}

	template<std::size_t Index>
	bool (*orgGetWidescreen)();

	template<std::size_t Index>
	bool GetWidesreen_DigitalTachoHack()
	{
		// Initialize the option from the first patch
		if constexpr (Index == 0)
		{
			DigitalTacho = CMR_FE_GetDigitalTacho();
		}

		if (DigitalTacho && GameInfo_GetNumberOfPlayersInThisRace() == 1)
		{
			return false;
		}
		return orgGetWidescreen<Index>();
	}

	template<std::size_t Ctr, typename Tuple, std::size_t... I, typename Func>
	void HookEachImpl_GetWidescreen(Tuple&& tuple, std::index_sequence<I...>, Func&& f)
	{
		(f(std::get<I>(tuple), orgGetWidescreen<Ctr << 16 | I>, GetWidesreen_DigitalTachoHack<Ctr << 16 | I>), ...);
	}

	template<std::size_t Ctr = 0, typename Vars, typename Func>
	void HookEach_GetWidescreen(Vars&& vars, Func&& f)
	{
		auto tuple = std::tuple_cat(std::forward<Vars>(vars));
		HookEachImpl_GetWidescreen<Ctr>(std::move(tuple), std::make_index_sequence<std::tuple_size_v<decltype(tuple)>>{}, std::forward<Func>(f));
	}

	static void (*orgDrawTacho)(int playerNo, float);
	void DrawTacho_SetPosition(int playerNo, float a2)
	{
		if (DigitalTacho && GameInfo_GetNumberOfPlayersInThisRace() == 1)
		{
			OSD_Data2* PositionInfoMulti = *gpPositionInfoMulti;

			const int OriginalTachoX = std::exchange(PositionInfoMulti->m_TachoX, static_cast<int>(GetScaledResolutionWidth()) - 160);
			const int OriginalTachoY = std::exchange(PositionInfoMulti->m_TachoY, 310);

			orgDrawTacho(playerNo, a2);

			PositionInfoMulti->m_TachoX = OriginalTachoX;
			PositionInfoMulti->m_TachoY = OriginalTachoY;
		}
		else
		{
			orgDrawTacho(playerNo, a2);
		}
	}
}

// Those belong in their cpp files, but they're heavily templated so it's easier to drop them here...
namespace Menus::Patches
{
	template<std::size_t Index>
	void (*orgFrontEndMenuSystem_SetupMenus)(int);

	template<std::size_t Index>
	void FrontEndMenuSystem_SetupMenus(int languagesOnly)
	{
		orgFrontEndMenuSystem_SetupMenus<Index>(languagesOnly);
		FrontEndMenuSystem_SetupMenus_Custom(languagesOnly);
	}

	template<std::size_t Ctr, typename Tuple, std::size_t... I, typename Func>
	void HookEachImpl(Tuple&& tuple, std::index_sequence<I...>, Func&& f)
	{
		(f(std::get<I>(tuple), orgFrontEndMenuSystem_SetupMenus<Ctr << 16 | I>, FrontEndMenuSystem_SetupMenus<Ctr << 16 | I>), ...);
	}

	template<std::size_t Ctr = 0, typename Vars, typename Func>
	void HookEach(Vars&& vars, Func&& f)
	{
		auto tuple = std::tuple_cat(std::forward<Vars>(vars));
		HookEachImpl<Ctr>(std::move(tuple), std::make_index_sequence<std::tuple_size_v<decltype(tuple)>>{}, std::forward<Func>(f));
	}
}

namespace Graphics::Patches
{
	// Fake name, as different functions are being patched with the same wrapper
	template<std::size_t Index>
	void (*org_Graphics_Config_Func)(Graphics_Config*);

	template<std::size_t Index>
	void Graphics_Config_Func_RecalculateUI(Graphics_Config* config)
	{
		org_Graphics_Config_Func<Index>(config);
		RecalculateUI();
	}

	template<std::size_t Ctr, typename Tuple, std::size_t... I, typename Func>
	void HookEachImpl_RecalculateUI(Tuple&& tuple, std::index_sequence<I...>, Func&& f)
	{
		(f(std::get<I>(tuple), org_Graphics_Config_Func<Ctr << 16 | I>, Graphics_Config_Func_RecalculateUI<Ctr << 16 | I>), ...);
	}

	template<std::size_t Ctr = 0, typename Vars, typename Func>
	void HookEach_RecalculateUI(Vars&& vars, Func&& f)
	{
		auto tuple = std::tuple_cat(std::forward<Vars>(vars));
		HookEachImpl_RecalculateUI<Ctr>(std::move(tuple), std::make_index_sequence<std::tuple_size_v<decltype(tuple)>>{}, std::forward<Func>(f));
	}
}


void OnInitializeHook()
{
	static_assert(std::string_view(__FUNCSIG__).find("__stdcall") != std::string_view::npos, "This codebase must default to __stdcall, please change your compilation settings.");

	using namespace Memory;
	using namespace hook::txn;

	// TODO: Check this against HiDPI
	DesktopWidth = GetSystemMetrics(SM_CXSCREEN);
	DesktopHeight = GetSystemMetrics(SM_CYSCREEN);

	const HINSTANCE mainModuleInstance = GetModuleHandle(nullptr);

	auto Protect = ScopedUnprotect::UnprotectSectionOrFullModule(mainModuleInstance, ".text");

	auto InterceptCall = [](void* addr, auto&& func, auto&& hook)
	{
		ReadCall(addr, func);
		InjectHook(addr, hook);
	};

	// Globally replace timeGetTime with a QPC-based timer
	Timers::Setup();
	Timers::RedirectImports();


	// Locate globals later patches might rely on
	bool HasGlobals = false;
	try
	{
		gszTempString = *get_pattern<char*>("68 ? ? ? ? E8 ? ? ? ? 8B 46 4C", 1);

		HasGlobals = true;
	}
	TXN_CATCH();

	bool HasDestruct = false;
	try
	{
		auto get_destructor = pattern("5F 5E B8 ? ? ? ? 5B 83 C4 28").get_one();

		ReadCall(get_destructor.get<void>(-11), Destruct_GetCoreDestructorGroup);
		ReadCall(get_destructor.get<void>(-5), Destruct_AddDestructor);

		HasDestruct = true;
	}
	TXN_CATCH();

	bool HasCored3d = false;
	try
	{
		auto check_for_device_lost = pattern("68 ? ? ? ? 50 FF 52 40").get_one();
		auto caps = *get_pattern<D3DCAPS9*>("BF ? ? ? ? F3 A5 A1 ? ? ? ? 85 C0", 1);

		gd3dPP = *check_for_device_lost.get<D3DPRESENT_PARAMETERS*>(1);
		gpd3dDevice = *check_for_device_lost.get<IDirect3DDevice9**>(-7 + 1);
		gD3DCaps = caps;

		HasCored3d = true;
	}
	TXN_CATCH();

	bool HasCMR3FE = false;
	try
	{
		auto funcs_save = pattern("E8 ? ? ? ? 8A 15 ? ? ? ? 52 E8 ? ? ? ? A1 ? ? ? ? 50").get_one();
		auto funcs_load = pattern("E8 ? ? ? ? 25 ? ? ? ? A3 ? ? ? ? E8 ? ? ? ? A3 ? ? ? ? E8 ? ? ? ? DB 05 ? ? ? ? 51 A3").get_one();

		CMR_FE_GetTextureQuality = reinterpret_cast<decltype(CMR_FE_GetTextureQuality)>(ReadCallFrom(funcs_load.get<void>(0)));
		CMR_FE_GetEnvironmentMap = reinterpret_cast<decltype(CMR_FE_GetEnvironmentMap)>(ReadCallFrom(funcs_load.get<void>(15)));
		CMR_FE_GetDrawShadow = reinterpret_cast<decltype(CMR_FE_GetDrawShadow)>(ReadCallFrom(funcs_load.get<void>(25)));
		CMR_FE_GetGraphicsQuality = reinterpret_cast<decltype(CMR_FE_GetGraphicsQuality)>(ReadCallFrom(get_pattern("E8 ? ? ? ? 25 ? ? ? ? 33 C9")));
		CMR_FE_GetDrawDistance = reinterpret_cast<decltype(CMR_FE_GetDrawDistance)>(ReadCallFrom(get_pattern("E8 ? ? ? ? 89 44 24 24 DB 44 24 24")));

		CMR_FE_SetGraphicsQuality = reinterpret_cast<decltype(CMR_FE_SetTextureQuality)>(ReadCallFrom(funcs_save.get<void>(0)));
		CMR_FE_SetTextureQuality = reinterpret_cast<decltype(CMR_FE_SetTextureQuality)>(ReadCallFrom(funcs_save.get<void>(5+6+1)));
		CMR_FE_SetEnvironmentMap = reinterpret_cast<decltype(CMR_FE_SetEnvironmentMap)>(ReadCallFrom(funcs_save.get<void>(0x17)));
		CMR_FE_SetDrawShadow = reinterpret_cast<decltype(CMR_FE_SetDrawShadow)>(ReadCallFrom(funcs_save.get<void>(0x23)));
		CMR_FE_SetDrawDistance = reinterpret_cast<decltype(CMR_FE_SetDrawDistance)>(ReadCallFrom(funcs_save.get<void>(0x4C)));
		CMR_FE_SetFSAA = reinterpret_cast<decltype(CMR_FE_SetFSAA)>(ReadCallFrom(funcs_save.get<void>(0x2F)));

		CMR_FE_StoreRegistry = reinterpret_cast<decltype(CMR_FE_StoreRegistry)>(ReadCallFrom(funcs_save.get<void>(-0xB)));

		SetUseLowQualityTextures = reinterpret_cast<decltype(SetUseLowQualityTextures)>(ReadCallFrom(get_pattern("E8 ? ? ? ? 8D 94 24 ? ? ? ? C7 05")));

		DrawLeftRightArrows = reinterpret_cast<decltype(DrawLeftRightArrows)>(get_pattern("81 EC ? ? ? ? 0F BF 41 18", -8));

		HasCMR3FE = true;
	}
	TXN_CATCH();

	bool HasCMR3Font = false;
	try
	{
		CMR3Font_BlitText = reinterpret_cast<decltype(CMR3Font_BlitText)>(get_pattern("8B 74 24 30 8B 0D", -6));
		CMR3Font_GetTextWidth = reinterpret_cast<decltype(CMR3Font_GetTextWidth)>(get_pattern("83 EC 20 8B 44 24 24"));

		HasCMR3Font = true;
	}
	TXN_CATCH();

	bool HasGraphics = false;
	try
	{
		auto set_gamma_ramp = get_pattern("D9 44 24 08 81 EC");
		auto get_num_adapters = ReadCallFrom(get_pattern("E8 ? ? ? ? 8B F8 32 DB"));
		auto get_current_config = reinterpret_cast<uintptr_t>(ReadCallFrom(get_pattern("E8 ? ? ? ? 51 8B 7C 24 68")));
		auto check_for_vertex_shaders = get_pattern("81 EC ? ? ? ? 8D 8C 24", -4);
		auto setup_render = get_pattern("81 EC ? ? ? ? 56 8D 44 24 08");
		auto get_adapter_caps = get_pattern("8B 08 81 EC ? ? ? ? 56", -5);

		auto save_func = pattern("E8 ? ? ? ? 8B 0D ? ? ? ? 6A FF 51 8B F8 E8").get_one();
		auto get_modes = pattern("E8 ? ? ? ? 33 F6 85 C0 89 44 24 14").get_one();

		auto get_screen_width = ReadCallFrom(get_pattern("E8 ? ? ? ? 33 F6 89 44 24 28"));
		auto get_screen_height = ReadCallFrom(get_pattern("E8 ? ? ? ? 8D 4C 6D 00"));

		Graphics_SetGammaRamp = reinterpret_cast<decltype(Graphics_SetGammaRamp)>(set_gamma_ramp);
		Graphics_GetNumAdapters = reinterpret_cast<decltype(Graphics_GetNumAdapters)>(get_num_adapters);
		Graphics_CheckForVertexShaders = reinterpret_cast<decltype(Graphics_CheckForVertexShaders)>(check_for_vertex_shaders);
		Graphics_GetAdapterCaps = reinterpret_cast<decltype(Graphics_GetAdapterCaps)>(get_adapter_caps);

		Graphics_GetScreenWidth = reinterpret_cast<decltype(Graphics_GetScreenWidth)>(get_screen_width);
		Graphics_GetScreenHeight = reinterpret_cast<decltype(Graphics_GetScreenHeight)>(get_screen_height);

		CMR_GetAdapterProductID = reinterpret_cast<decltype(CMR_GetAdapterProductID)>(ReadCallFrom(save_func.get<void>(0)));
		CMR_GetAdapterVendorID = reinterpret_cast<decltype(CMR_GetAdapterVendorID)>(ReadCallFrom(save_func.get<void>(0x10)));
		GetMenuResolutionEntry = reinterpret_cast<decltype(GetMenuResolutionEntry)>(ReadCallFrom(save_func.get<void>(0x26)));

		CMR_GetValidModeIndex = reinterpret_cast<decltype(CMR_GetValidModeIndex)>(get_modes.get<void>(-9));
		CMR_ValidateModeFormats = reinterpret_cast<decltype(CMR_ValidateModeFormats)>(ReadCallFrom(get_modes.get<void>(0)));

		CMR_SetupRender = reinterpret_cast<decltype(CMR_SetupRender)>(setup_render);

		gGraphicsConfig = *reinterpret_cast<Graphics_Config**>(get_current_config + 0xC);

		HasGraphics = true;
	}
	TXN_CATCH();

	bool HasViewport = false;
	try
	{
		auto set_aspect_ratio = get_pattern("8B 44 24 04 85 C0 75 1C");
		auto viewports = *get_pattern<D3DViewport**>("8B 35 ? ? ? ? 8B C6 5F", 2);
		auto full_screen_viewport = *get_pattern<D3DViewport**>("A1 ? ? ? ? D9 44 24 08 D9 58 1C", 1);
		auto current_viewport = *get_pattern<D3DViewport**>("A3 ? ? ? ? 8B 48 54", 1);

		gpFullScreenViewport = full_screen_viewport;
		gpCurrentViewport = current_viewport;
		Viewport_SetAspectRatio = reinterpret_cast<decltype(Viewport_SetAspectRatio)>(set_aspect_ratio);
		gViewports = viewports;

		HasViewport = true;
	}
	TXN_CATCH();

	bool HasHandyFunction = false;
	try
	{
		auto draw_2d_box = get_pattern("6A 01 E8 ? ? ? ? 6A 05 E8 ? ? ? ? 6A 06 E8 ? ? ? ? DB 44 24 5C", -5);
		auto draw_2d_line_from_to = get_pattern("8B 54 24 54 89 44 24 10", -0xB);
		auto clip_2d_rect = get_pattern("DF E0 25 ? ? ? ? 75 0E D9 41 10 D8 5C 24 10", -0xB);

		HandyFunction_Draw2DBox = reinterpret_cast<decltype(HandyFunction_Draw2DBox)>(draw_2d_box);
		HandyFunction_Draw2DLineFromTo = reinterpret_cast<decltype(HandyFunction_Draw2DLineFromTo)>(draw_2d_line_from_to);
		HandyFunction_Clip2DRect = reinterpret_cast<decltype(HandyFunction_Clip2DRect)>(clip_2d_rect);

		HasHandyFunction = true;
	}
	TXN_CATCH();

	bool HasBlitter2D = false;
	try
	{
		auto blitter2d_rect2d_g = ReadCallFrom(get_pattern("E8 ? ? ? ? 8D 44 24 68"));
		auto blitter2d_rect2d_gt = ReadCallFrom(get_pattern("DD D8 E8 ? ? ? ? 8B 7C 24 30", 2));
		auto blitter2d_line2d_g = get_pattern("F7 D8 57", -0x14);

		Core_Blitter2D_Rect2D_G = reinterpret_cast<decltype(Core_Blitter2D_Rect2D_G)>(blitter2d_rect2d_g);
		Core_Blitter2D_Rect2D_GT = reinterpret_cast<decltype(Core_Blitter2D_Rect2D_GT)>(blitter2d_rect2d_gt);
		Core_Blitter2D_Line2D_G = reinterpret_cast<decltype(Core_Blitter2D_Line2D_G)>(blitter2d_line2d_g);

		HasBlitter2D = true;
	}
	TXN_CATCH();

	bool HasRenderState = false;
	try
	{
		RenderState_SetSamplerState = reinterpret_cast<decltype(RenderState_SetSamplerState)>(ReadCallFrom(get_pattern("E8 ? ? ? ? D9 44 24 0C D9 1C B5 ? ? ? ? D9 44 24 08")));
		CurrentRenderState = *get_pattern<RenderState*>("8B 0D ? ? ? ? A1 ? ? ? ? 8B 10 51 6A 07", 2);

		// Facade initialization
		const uintptr_t RenderStateStart = reinterpret_cast<uintptr_t>(CurrentRenderState);
		const uintptr_t MaxAnisotropy = *get_pattern<uintptr_t>("89 1C B5 ? ? ? ? 8B 0C B5", 7 + 3);
		RenderStateFacade::OFFS_maxAnisotropy = MaxAnisotropy - RenderStateStart;

		HasRenderState = true;
	}
	TXN_CATCH();

	bool HasKeyboard = false;
	try
	{
		auto draw_text_entry_box = get_pattern("56 3B C3 57 0F 84 ? ? ? ? DB 84 24", -0xD);

		Keyboard_DrawTextEntryBox = reinterpret_cast<decltype(Keyboard_DrawTextEntryBox)>(draw_text_entry_box);
		HasKeyboard = true;
	}
	TXN_CATCH();

	bool HasGameInfo = false;
	try
	{
		auto get_num_players = ReadCallFrom(get_pattern("E8 ? ? ? ? 88 44 24 23"));
		GameInfo_GetNumberOfPlayersInThisRace = reinterpret_cast<decltype(GameInfo_GetNumberOfPlayersInThisRace)>(get_num_players);

		HasGameInfo = true;
	}
	TXN_CATCH();

	bool HasUIPositions = false;
	try
	{
		auto position_into_multi = *get_pattern<OSD_Data2**>("A1 ? ? ? ? 8B 50 60 8B 48 64", 1);

		gpPositionInfoMulti = position_into_multi;

		HasUIPositions = true;
	}
	TXN_CATCH();

	// Texture replacements
	try
	{
		using namespace ScaledTexturesSupport;

		std::array<void*, 3> load_texture = {
			get_pattern("E8 ? ? ? ? 89 06 5E C2 0C 00"),
			get_pattern("E8 ? ? ? ? 56 89 07"),
			[]
			{
				try
				{
					// Polish/EFIGS
					return get_pattern("E8 ? ? ? ? 89 07 5F");
				}
				catch (const hook::txn_exception&)
				{
					// Czech
					return get_pattern("E8 ? ? ? ? 89 45 00 5D");
				}
			}()
		};
		auto load_font = pattern("57 E8 ? ? ? ? 8B 0D ? ? ? ? 6A 01").get_one();

		HookEach_Misc_Scaled(load_texture, InterceptCall);

		InterceptCall(load_font.get<void>(1), orgCreateTexture_Font, CreateTexture_Font_Scaled);

		// NOP Core::Texture_SetFilteringMethod for fonts as we handle it in the above function now
		InjectHook(load_font.get<void>(0x1F), Texture_SetFilteringMethod_NOP);
	}
	TXN_CATCH();


	// Added range check for localizations + new strings added in the Polish release
	bool HasLanguageHook = false;
	try
	{
		using namespace Localization;

		auto get_localized_string = ReadCallFrom(get_pattern("E8 ? ? ? ? 8D 56 E2"));

		gpCurrentLanguage = *get_pattern<LangFile**>("89 0D ? ? ? ? B8 ? ? ? ? 5E", 2);
		InjectHook(get_localized_string, Language_GetString, PATCH_JUMP);

		HasLanguageHook = true;
	}
	TXN_CATCH();


	try
	{
		using namespace Localization;

		auto get_current_language_id = pattern("6A 45 E8 ? ? ? ? C3").get_one();
		orgGetCurrentLanguageID = static_cast<decltype(orgGetCurrentLanguageID)>(get_current_language_id.get<void>());

		// Un-hardcoded English text language
		ReadCall(get_current_language_id.get<void>(2), orgGetLanguageIDByCode);
		InjectHook(get_current_language_id.get<void>(), GetCurrentLanguageID_Patched, PATCH_JUMP);

		// Un-hardcoded co-driver language
		// Patch will probably fail pattern matches on set_defaults when used on the English executable
		try
		{
			auto get_codriver_language = ReadCallFrom(get_pattern("E8 ? ? ? ? 83 F8 03 77 40"));
			auto set_codriver_language = ReadCallFrom(get_pattern("E8 ? ? ? ? 8B 4F 24"));
			auto set_defaults = pattern("A2 ? ? ? ? C6 05 ? ? ? ? 01 88 1D").get_one();
			auto unk_on_main_menu_start = get_pattern("75 ? 6A 02 E8 ? ? ? ? 81 C4", 4);

			gCoDriverLanguage = *set_defaults.get<uint8_t*>(5 + 2);
			InjectHook(get_codriver_language, GetCoDriverLanguage, PATCH_JUMP);
			InjectHook(set_codriver_language, SetCoDriverLanguage, PATCH_JUMP);

			//  mov bCoDriverLanguage, 1 -> mov bCoDriverLanguage, al
			Patch(set_defaults.get<void>(5), { 0x90, 0xA2 });
			Patch(set_defaults.get<void>(5 + 2), gCoDriverLanguage);
			Nop(set_defaults.get<void>(5 + 6), 1);

			// Unknown, called once as main menu starts
			ReadCall(unk_on_main_menu_start, orgSetUnknown);
			InjectHook(unk_on_main_menu_start, setUnknown_AndCoDriverLanguage);
		}
		TXN_CATCH();

		// Revert measurement systems defaulting to imperial for English, metric otherwise (Polish forced metric)
		try
		{
			auto set_measurement_system = get_pattern("E8 ? ? ? ? 8B 46 4C 33 C9");

			void* set_system_places_to_patch[] = {
				get_pattern("E8 ? ? ? ? E8 ? ? ? ? 50 E8 ? ? ? ? 68"),
				get_pattern("6A 00 E8 ? ? ? ? 6A 01 E8 ? ? ? ? 5F", 2),
				get_pattern("56 E8 ? ? ? ? 56 E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? 57", 1),
			};

			auto set_defaults = pattern("E8 ? ? ? ? B0 64").get_one();
			
			ReadCall(set_measurement_system, orgSetMeasurementSystem);
			for (void* addr : set_system_places_to_patch)
			{
				InjectHook(addr, SetMeasurementSystem_FromLanguage);
			}

			InjectHook(set_defaults.get<void>(), SetMeasurementSystem_Defaults);
			Nop(set_defaults.get<void>(5 + 2), 6);
		}
		TXN_CATCH();
	}
	TXN_CATCH();


	// Restored cube layouts
	try
	{
		using namespace Cubes;

		void* load_cube_textures[] = {
			get_pattern("E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? 8B 44 24 08 6A 0E"),
			get_pattern("E8 ? ? ? ? A1 ? ? ? ? 85 C0 75 14"),
		};

		gBlankCubeTexture = *get_pattern<void**>("56 68 ? ? ? ? E8 ? ? ? ? 68 ? ? ? ? E8 ? ? ? ? 68", 1 + 5 + 5 + 1);
		gGearCubeTextures = *get_pattern<void**>("BE ? ? ? ? BF 07 00 00 00 56", 1);
		gStageCubeTextures = *get_pattern<void**>("BE ? ? ? ? BF 09 00 00 00 56", 1);

		gGearCubeLayouts = *get_pattern<void**>("8B 04 95 ? ? ? ? 50 6A 00 E8 ? ? ? ? 83 E0 03 83 C0 02 50 6A 00", 3);
		gStageCubeLayouts = *get_pattern<void**>("8B 15 ? ? ? ? A3 ? ? ? ? 89 15", 6 + 1);

		ReadCall(load_cube_textures[0], orgLoadCubeTextures);
		for (void* addr : load_cube_textures)
		{
			InjectHook(addr, LoadCubeTextures_AndSetUpLayouts);
		}
	}
	TXN_CATCH();


	// Menu changes
	bool HasMenuHook = false;
	try
	{
		using namespace Menus::Patches;

		auto menus = *get_pattern<MenuDefinition*>("C7 05 ? ? ? ? ? ? ? ? 89 3D ? ? ? ? 89 35", 2+4);
		std::array<void*, 3> update_menu_entries = {
			get_pattern("6A 01 E8 ? ? ? ? 5F 5E C2 08 00", 2),
			get_pattern("E8 ? ? ? ? 6A 00 E8 ? ? ? ? E8 ? ? ? ? C2 08 00", 5 + 2),
			get_pattern("E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? 50 E8 ? ? ? ? 50"),
		};

		gnCurrentAdapter = *get_pattern<int*>("A3 ? ? ? ? 56 50", 1);
		PC_GraphicsAdvanced_PopulateFromCaps = reinterpret_cast<decltype(PC_GraphicsAdvanced_PopulateFromCaps)>(get_pattern("8D 84 24 ? ? ? ? 53 55", -6));

		gmoFrontEndMenus = menus;
		HookEach(update_menu_entries, InterceptCall);
		HasMenuHook = true;

		// This only matches in the Polish executable
		try
		{
			// Don't override focus every time menus are updated
			auto set_focus_on_lang_screen = get_pattern("89 0D ? ? ? ? 66 89 35", 6);
			Nop(set_focus_on_lang_screen, 14);
		}
		TXN_CATCH();
	}
	TXN_CATCH();


	// Fix sun flickering with multisampling enabled, and make it fully async to avoid a GPU flush and stutter every time sun shows onscreen
	try
	{
		using namespace OcclusionQueries;

		auto mul_struct_size = get_pattern("C1 E0 04 50 C7 05 ? ? ? ? ? ? ? ? C7 05");
		auto push_struct_size = get_pattern("C7 05 ? ? ? ? ? ? ? ? E8 ? ? ? ? 8B 15 ? ? ? ? 6A 00 50 6A 10", 25);
		auto issue_begin = get_pattern("56 8B 74 24 08 8B 46 04 6A 02");
		auto issue_end = get_pattern("56 8B 74 24 08 8B 46 04 8B 08");
		auto get_data = get_pattern("E8 ? ? ? ? 3B C7 89 44 24 18");
		auto update_state_if_finished = get_pattern("E8 ? ? ? ? 83 F8 03 75 EF");

		auto is_query_idle = pattern("E8 ? ? ? ? 83 F8 03 74 10").get_one();
		auto issue_query_return = get_pattern("E8 ? ? ? ? 8B C3 5F", 5);

		auto calculate_color_from_occlusion = get_pattern("E8 ? ? ? ? 53 56 57 6A 03");

		auto load_horizon = pattern("A3 ? ? ? ? E8 ? ? ? ? 50 E8 ? ? ? ? 5E 8B E5").get_one();
		auto destroy_horizon = get_pattern("50 E8 ? ? ? ? A1 ? ? ? ? 3B C6 74 0C", 1);
		auto visible_sun = *get_pattern<float*>("D9 05 ? ? ? ? D8 1D ? ? ? ? DF E0 F6 C4 44 7B 0B", 2);

		auto render_game_common1 = pattern("57 33 FF 57 E8 ? ? ? ? 57 E8 ? ? ? ? 57 E8 ? ? ? ? 6A 01").get_one();

		// shl eax, 4 -> imul eax, sizeof(OcclusionQuery)
		Patch(mul_struct_size, {0x6B, 0xC0, sizeof(OcclusionQuery)});
		Patch<uint8_t>(push_struct_size,  sizeof(OcclusionQuery));

		InjectHook(issue_begin, OcclusionQuery_IssueBegin, PATCH_JUMP);
		InjectHook(issue_end, OcclusionQuery_IssueEnd, PATCH_JUMP);
		InjectHook(get_data, OcclusionQuery_GetDataScaled);
		InjectHook(update_state_if_finished, OcclusionQuery_UpdateStateIfFinished);

		// while (D3DQuery_UpdateState(pOcclusionQuery) != 3 && D3DQuery_UpdateState(pOcclusionQuery))
		// ->
		// if (!OcclusionQuery_IsIdle(pOcclusionQuery)) return 1;
		InjectHook(is_query_idle.get<void>(0), OcclusionQuery_IsIdle);
		InjectHook(is_query_idle.get<void>(5 + 3 + 2), issue_query_return, PATCH_JUMP);

		InterceptCall(calculate_color_from_occlusion, orgCalculateSunColorFromOcclusion, CalculateSunColorFromOcclusion_Clamped);

		// One occlusion query per viewport
		gpSunOcclusion_Original = *load_horizon.get<OcclusionQuery**>(1);
		gfVisibleSun_Original = visible_sun;

		InterceptCall(load_horizon.get<void>(-10), orgOcclusion_Create, Occlusion_Create_Hooked);
		InterceptCall(destroy_horizon, orgOcclusion_Destroy, Occlusion_Destroy_Hooked);

		Render_RenderGameCommon1_JumpBack = render_game_common1.get<void>();
		InjectHook(render_game_common1.get<void>(-5), Render_RenderGameCommon1_SwitchOcclusion, PATCH_JUMP);
	}
	TXN_CATCH();


	// Slightly more precise timers, not dividing frequency
	try
	{
		using namespace Timers;

		auto get_time_in_ms = Memory::ReadCallFrom(get_pattern("E8 ? ? ? ? EB 3E"));
		InjectHook(get_time_in_ms, GetTimeInMS, PATCH_JUMP);
	}
	TXN_CATCH();


	// Backported fix from CMR04 for timeGetTime not having enough timer resolution for physics updates in splitscreen
	try
	{
		using namespace Timers;

		auto gettime_reset = get_pattern("FF 15 ? ? ? ? 8B F8 8B D0 8B C8", 2);
		auto gettime_update = get_pattern("FF 15 ? ? ? ? A3 ? ? ? ? E8 ? ? ? ? 83 F8 01 5D", 2);

		Patch(gettime_reset, &pTimeGetTime_Reset);
		Patch(gettime_update, &pTimeGetTime_Update);
	}
	TXN_CATCH();


	// Unlocked all resolutions and a 128 resolutions limit lifted
	try
	{
		using namespace ResolutionsList;

		auto check_aspect_ratio = get_pattern("8D 04 40 3B C2 74 05", 5);
		auto get_display_mode_count = get_pattern("E8 ? ? ? ? 33 C9 3B C1 89 44 24 1C");

		// Populate the list of addresses to patch with addresses and offsets
		// Those act as "optional", don't fail the entire change if any patterns fail
		// Instead, we'll limit the game to 128 resolutions in GetDisplayModeCount_RelocateArray 
		try
		{
			// To save effort, build patterns manually
			auto func_start = get_pattern_uintptr("8B 4C 24 18 51 55");
			auto func_end = get_pattern_uintptr("8B 4E E8 8B 56 E4");
			uint8_t* resolutions_list = *get_pattern<uint8_t*>("33 C9 85 C0 76 65 BF", 7) - offsetof(MenuResolutionEntry, m_format);

			auto patch_field = [resolutions_list, func_start, func_end](size_t offset)
			{
				uint8_t* ptr = resolutions_list+offset;

				const uint8_t mask[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
				uint8_t bytes[4];
				memcpy(bytes, &ptr, sizeof(ptr));

				pattern(func_start, func_end,
					std::basic_string_view<uint8_t>(bytes, sizeof(bytes)), std::basic_string_view<uint8_t>(mask, sizeof(mask)))
					.for_each_result([&offset](pattern_match match)
				{
					placesToPatch.emplace_back(match.get<void>(0), offset);
				});
			};

			patch_field(offsetof(MenuResolutionEntry, m_width));
			patch_field(offsetof(MenuResolutionEntry, m_height));
			patch_field(offsetof(MenuResolutionEntry, m_refreshRate));
			patch_field(offsetof(MenuResolutionEntry, m_format));
			patch_field(offsetof(MenuResolutionEntry, m_backBufferFormat));
			patch_field(offsetof(MenuResolutionEntry, field_54));
			patch_field(offsetof(MenuResolutionEntry, field_58));
			patch_field(offsetof(MenuResolutionEntry, field_5C));
			patch_field(offsetof(MenuResolutionEntry, field_60));
			patch_field(offsetof(MenuResolutionEntry, field_64));

			// GetMenuResolutionEntry
			placesToPatch.emplace_back(get_pattern("8D 04 91 8D 04 C5 ? ? ? ? C2 08 00", 3 + 3), 0);
		}
		catch (const hook::txn_exception&)
		{
			ResolutionsList::placesToPatch.clear();
		}
		ResolutionsList::placesToPatch.shrink_to_fit();
		
		// Allow all aspect ratios
		Patch<uint8_t>(check_aspect_ratio, 0xEB);

		// Set up for runtime patching of the hardcoded 128 resolutions list if there is a need
		// Not likely to be used anytime soon but it'll act as a failsafe just in case
		ReadCall(get_display_mode_count, orgGetDisplayModeCount);
		InjectHook(get_display_mode_count, GetDisplayModeCount_RelocateArray);
	}
	TXN_CATCH();


	// Fixed half pixel issues, and added line thickness
	// Requires patches: Graphics (for resolution)
	if (HasGraphics) try
	{
		using namespace HalfPixel;

		auto Blitter2D_Rect2D_G = pattern("76 39 8B 7C 24 3C").get_one();
		auto Blitter2D_Rect2D_GT = reinterpret_cast<intptr_t>(ReadCallFrom(get_pattern("E8 ? ? ? ? A1 ? ? ? ? 45 83 C3 40")));
		auto Blitter2D_Quad2D_G = reinterpret_cast<intptr_t>(ReadCallFrom(get_pattern("E8 ? ? ? ? 8B 4C 24 1C 8B 44 24 14")));
		auto Blitter2D_Quad2D_GT = pattern("2B EE 6B F6 54 55 03 F7 56 C7 05").get_one();
		auto Blitter2D_Line2D_G = pattern("8B 7C 24 28 1B C0").get_one();
		auto Blitter3D_Line3D_G = pattern("8B 75 0C 8B C2").get_one();
		auto matrices = pattern("BF ? ? ? ? F3 AB C7 05").get_one();

		Core_Blitter2D_Tri2D_G = reinterpret_cast<decltype(Core_Blitter2D_Tri2D_G)>(get_pattern("3B F2 57 76 3E", -0x30));
		Core_Blitter3D_Tri3D_G = reinterpret_cast<decltype(Core_Blitter3D_Tri3D_G)>(get_pattern("57 8B 7D 0C 8B C1", -0xD));

		pViewMatrix = *matrices.get<D3DMATRIX*>(-0xC + 1);
		pProjectionMatrix = *matrices.get<D3DMATRIX*>(1);

		dword_936C0C = *Blitter2D_Rect2D_G.get<void**>(-0x50 + 4);
		Core_Blitter2D_Rect2D_G_JumpBack = Blitter2D_Rect2D_G.get<void>(-0x50 + 8);
		InjectHook(Blitter2D_Rect2D_G.get<void>(-0x50), Core_Blitter2D_Rect2D_G_HalfPixel, PATCH_JUMP);

		Core_Blitter2D_Rect2D_GT_JumpBack = reinterpret_cast<void*>(Blitter2D_Rect2D_GT + 8);
		InjectHook(Blitter2D_Rect2D_GT, Core_Blitter2D_Rect2D_GT_HalfPixel, PATCH_JUMP);

		Core_Blitter2D_Quad2D_G_JumpBack = reinterpret_cast<void*>(Blitter2D_Quad2D_G + 8);
		InjectHook(Blitter2D_Quad2D_G, Core_Blitter2D_Quad2D_G_HalfPixel, PATCH_JUMP);

		Core_Blitter2D_Quad2D_GT_JumpBack = Blitter2D_Quad2D_GT.get<void>(-0x68 + 8);
		InjectHook(Blitter2D_Quad2D_GT.get<void>(-0x68), Core_Blitter2D_Quad2D_GT_HalfPixel, PATCH_JUMP);

		Core_Blitter2D_Line2D_G_JumpBack = Blitter2D_Line2D_G.get<void>(-0x17 + 9);
		InjectHook(Blitter2D_Line2D_G.get<void>(-0x17), Core_Blitter2D_Line2D_G_HalfPixelAndThickness, PATCH_JUMP);
		// Do not recurse into the custom function to avoid super-thick lines
		InjectHook(Blitter2D_Line2D_G.get<void>(0x2C), Core_Blitter2D_Line2D_G_Original);
		InjectHook(Blitter2D_Line2D_G.get<void>(0x44), Core_Blitter2D_Line2D_G_Original);

		Core_Blitter3D_Line3D_G_JumpBack = Blitter3D_Line3D_G.get<void>(-0xD + 6);
		InjectHook(Blitter3D_Line3D_G.get<void>(-0xD), Core_Blitter3D_Line3D_G_LineThickness, PATCH_JUMP);
		// Do not recurse into the custom function
		InjectHook(Blitter3D_Line3D_G.get<void>(0x34), Core_Blitter3D_Line3D_G_Original);
		InjectHook(Blitter3D_Line3D_G.get<void>(0x52), Core_Blitter3D_Line3D_G_Original);

	}
	TXN_CATCH();


	// Scissor-based digital tacho
	if (HasCored3d && HasGraphics && HasViewport && HasUIPositions) try
	{
		using namespace ScissorTacho;

		auto blit_texture = get_pattern("50 6A 00 6A 00 52 E8", 6);
		auto ftol = get_pattern("D9 44 24 10 E8 ? ? ? ? 8B 0D ? ? ? ? 6A FF 8B 51 0C", 4);

		InterceptCall(blit_texture, orgHandyFunction_BlitTexture, HandyFunction_BlitTexture_Scissor);
		InjectHook(ftol, ftol_fake);
	}
	TXN_CATCH();


	// Better line box drawing (without gaps and overlapping lines)
	if (HasBlitter2D && HasGraphics) try
	{
		using namespace BetterBoxDrawing;

		auto display_selection_box = get_pattern("81 E1 FF 00 00 00 99", -0xC);
		int32_t* car_setup_selection_box_posy = get_pattern<int32_t>("B9 ? ? ? ? 89 4C 24 1C", 1);
		int8_t* car_setup_selection_box_height = get_pattern<int8_t>("6A 08 6A 09", 1);

		auto osd_countdown_draw = pattern("D8 C9 D9 5C 24 3C DD D8 E8").count(4);
		auto osd_startlights_draw1 = pattern("D8 C9 D9 5C 24 38 DD D8 E8 ? ? ? ? E8 ? ? ? ? 89 44 24 10").count(2);
		void* osd_startlights_draw2[] = {
			get_pattern("6A 01 50 E8 ? ? ? ? E8 ? ? ? ? 89 44 24 10", 3),
			get_pattern("D9 5C 24 38 DD D8 E8 ? ? ? ? 5F 5E 5B", 6),
		};

		InjectHook(display_selection_box, DisplaySelectionBox, PATCH_JUMP);

		Patch<int32_t>(car_setup_selection_box_posy, 347 + 3);
		Patch<int8_t>(car_setup_selection_box_height, 9);

		InjectHook(osd_countdown_draw.get(0).get<void>(8), Blitter2D_Line2D_G_FirstLine);
		InjectHook(osd_startlights_draw1.get(0).get<void>(8), Blitter2D_Line2D_G_FirstLine);
		InjectHook(osd_countdown_draw.get(1).get<void>(8), Blitter2D_Line2D_G_SecondLine);
		InjectHook(osd_startlights_draw1.get(1).get<void>(8), Blitter2D_Line2D_G_SecondLine);
		InjectHook(osd_countdown_draw.get(2).get<void>(8), Blitter2D_Line2D_G_NOP);
		InjectHook(osd_countdown_draw.get(3).get<void>(8), Blitter2D_Line2D_G_NOP);
		for (void* addr : osd_startlights_draw2)
		{
			InjectHook(addr, Blitter2D_Line2D_G_NOP);
		}

		if (HasKeyboard)
		{
			InjectHook(::Keyboard_DrawTextEntryBox, BetterBoxDrawing::Keyboard_DrawTextEntryBox, PATCH_JUMP);
		}

		// Fixed HandyFunction_DrawClipped2DBox requires HandyFunction_Clip2DRect
		if (HasHandyFunction) try
		{
			auto draw_clipped_2d_box = get_pattern("53 55 56 33 F6 3B C6 57 0F 84", -0xA);

			InjectHook(draw_clipped_2d_box, BetterBoxDrawing::HandyFunction_DrawClipped2DBox, PATCH_JUMP);
		}
		TXN_CATCH();
	}
	TXN_CATCH();


	// Better widescreen support
	// Requires: Graphics
	if (HasGraphics && HasViewport) try
	{
		// Viewports
		try
		{
			auto set_viewport = ReadCallFrom(get_pattern("E8 ? ? ? ? 8B 4C 24 0C 8B 56 60"));
			auto set_aspect_ratios = get_pattern("83 EC 64 56 E8");

			auto recalc_fov = pattern("D8 0D ? ? ? ? DA 74 24 30 ").get_one();

			InjectHook(set_viewport, Viewport_SetDimensions, PATCH_JUMP);
			InjectHook(set_aspect_ratios, Graphics_Viewports_SetAspectRatios, PATCH_JUMP);

			// Change the horizontal FOV instead of vertical when refreshing viewports
			// fidiv -> fidivr and m_vertFov -> m_horFov
			static const float f4By3 = 3.0f/4.0f;
			Patch(recalc_fov.get<void>(2), &f4By3);
			Patch<uint8_t>(recalc_fov.get<void>(6+1), 0x7C);
			Patch<uint8_t>(recalc_fov.get<void>(10+2), 0x1C);

			// Pin viewports into constant aspect ratio
			try
			{
				using namespace ConstantViewports;

				std::array<void*, 3> viewports_constant_aspect_ratio =
				{
					get_pattern("E8 ? ? ? ? 8B 0D ? ? ? ? 8D 94 24"),
					get_pattern("E8 ? ? ? ? 6A 02 6A 01 6A 01"),
					get_pattern("E8 ? ? ? ? 6A 02 6A 01 6A 02"),
				};

				HookEach(viewports_constant_aspect_ratio, InterceptCall);
			}
			TXN_CATCH();
		}
		TXN_CATCH();

		// UI
		if (HasBlitter2D && HasCMR3Font && HasHandyFunction && HasKeyboard) try
		{
			using namespace Graphics::Patches;

			extern void (*orgSetMovieDirectory)(const char* path);

			std::array<void*, 2> graphics_change_recalculate_ui = {
				get_pattern("E8 ? ? ? ? 8B 54 24 24 89 5C 24 18"),
				get_pattern("E8 ? ? ? ? 8B 15 ? ? ? ? A1 ? ? ? ? 8B 0D"),
			};

			auto osd_data = pattern("03 C6 8D 0C 85 ? ? ? ? 8D 04 F5 00 00 00 00").get_one();

			void* osd_element_init_center[] = {
				get_pattern("52 50 E8 ? ? ? ? A1 ? ? ? ? C7 86", 2),
				get_pattern("E8 ? ? ? ? 68 ? ? ? ? C7 05 ? ? ? ? ? ? ? ? E8 ? ? ? ? 5E"),
			};
			
			void* osd_element_init_right[] = {
				get_pattern("E8 ? ? ? ? 33 C0 6A 01"),
				get_pattern("E8 ? ? ? ? 8B 0D ? ? ? ? 33 C0 81 E1"),
				get_pattern("68 ? ? ? ? E8 ? ? ? ? 5F 5E 5D C7 05", 5),
			};

			void* solid_background_full_width[] = {
				// Stage loading tiles
				get_pattern("E8 ? ? ? ? 46 83 C7 04 83 FE 05"),
			};

			// Constants to change
			auto patch_field = [](std::string_view str, ptrdiff_t offset)
			{
				pattern(str).for_each_result([offset](pattern_match match)
				{
					int32_t* const addr = match.get<int32_t>(offset);
					const int32_t val = *addr;
					UI_RightAlignElements.emplace_back(std::in_place_type<Int32Patch>, addr, val);
				});
			};

			auto patch_field_center = [](std::string_view str, ptrdiff_t offset)
			{
				pattern(str).for_each_result([offset](pattern_match match)
				{
					int32_t* const addr = match.get<int32_t>(offset);
					const int32_t val = *addr;
					UI_CenteredElements.emplace_back(std::in_place_type<Int32Patch>, addr, val);
				});
			};

			float* resolutionWidthMult = *get_pattern<float*>("DF 6C 24 18 D8 0D ? ? ? ? D9 5C 24 38", 4+2);
			if (mainModuleInstance == GetModuleHandleFromAddress(resolutionWidthMult))
			{
				UI_resolutionWidthMult = resolutionWidthMult;
			}
			UI_RightAlignElements.emplace_back(std::in_place_type<FloatPatch>, *get_pattern<float*>("D8 3D ? ? ? ? D9 5C 24 18", 2), 640.0f);

			UI_RightAlignElements.emplace_back(std::in_place_type<Int32Patch>, get_pattern<int32_t>("B9 ? ? ? ? D9 5C 24 0C", 1), 640);
			UI_RightAlignElements.emplace_back(std::in_place_type<Int32Patch>, get_pattern<int32_t>("B9 ? ? ? ? 8D 3C B6", 1), 640);
			UI_RightAlignElements.emplace_back(std::in_place_type<FloatPatch>, *get_pattern<float*>("D8 E2 D8 0D ? ? ? ? D8 0D", 2+2), 590.0f);

			UI_CenteredElements.emplace_back(std::in_place_type<Int32Patch>, get_pattern<int32_t>("B8 ? ? ? ? 6A 01 6A 00 68", 1), 292);
			UI_CoutdownPosXVertical[0] = get_pattern<int32_t>("B8 ? ? ? ? B9 ? ? ? ? EB 1F E8", 1);
			UI_CoutdownPosXVertical[1] = get_pattern<int32_t>("76 0C B8 ? ? ? ? B9", 2+1);

			UI_MenuBarTextDrawLimit = get_pattern<int32_t>("C7 44 24 2C 01 00 00 00 81 FD", 8+2);

			UI_TachoInitialised = *get_pattern<int32_t*>("89 74 24 24 A1", 4+1);

			patch_field("05 89 01 00 00", 1); // add eax, 393
			patch_field("81 C5 89 01 00 00", 2); // add ebp, 393
			patch_field("81 C3 89 01 00 00", 2); // add ebx, 393
			patch_field("81 C7 89 01 00 00", 2); // add edi, 393
			patch_field("81 C6 89 01 00 00", 2); // add esi, 393

			// push 393
			patch_field("68 89 01 00 00 68 ? ? ? ? 6A 00", 1);
			UI_RightAlignElements.emplace_back(std::in_place_type<Int32Patch>, get_pattern<int32_t>("6A 09 51 68 89 01 00 00", 3 + 1), 393);

			// Menu arrows
			UI_RightAlignElements.emplace_back(std::in_place_type<FloatPatch>, *get_pattern<float*>("D8 0D ? ? ? ? F3 A5 D9 5C 24 2C", 2), 376.0f);
			pattern("C7 44 24 ? 00 00 BC 43").count(2).for_each_result([](pattern_match match)
			{
				UI_RightAlignElements.emplace_back(std::in_place_type<FloatPatch>, match.get<float>(4), 376.0f);
			});
			UI_RightAlignElements.emplace_back(std::in_place_type<FloatPatch>, get_pattern<float>("C7 44 24 ? 00 80 BA 43", 4), 376.0f - 3.0f);
			UI_RightAlignElements.emplace_back(std::in_place_type<FloatPatch>, get_pattern<float>("C7 84 24 ? ? ? ? 00 80 BA 43", 7), 376.0f - 3.0f);
			UI_RightAlignElements.emplace_back(std::in_place_type<FloatPatch>, get_pattern<float>("C7 44 24 ? 00 00 BE 43", 4), 376.0f + 3.0f);
			UI_RightAlignElements.emplace_back(std::in_place_type<FloatPatch>, get_pattern<float>("C7 84 24 ? ? ? ? 00 00 BE 43", 7), 376.0f + 3.0f);

			// Controls screen menu
			UI_RightAlignElements.emplace_back(std::in_place_type<FloatPatch>, get_pattern<float>("68 00 00 7F 43 52 56", 1), 255.0f);
			patch_field("81 C6 FF 00 00 00", 2); // add esi, 255
			patch_field("81 C3 FF 00 00 00", 2); // add ebx, 255
			UI_RightAlignElements.emplace_back(std::in_place_type<Int32Patch>, get_pattern<int32_t>("56 68 FF 00 00 00 68", 2), 255);

			// Championship standings pre-race
			void* race_standings[] = {
				get_pattern("E8 ? ? ? ? 6A 00 6A 00 6A 00 6A 00 6A FF E8 ? ? ? ? 33 ED"),
				get_pattern("E8 ? ? ? ? 55 E8 ? ? ? ? 50 E8 ? ? ? ? 8D 8C 24"),
				get_pattern("E8 ? ? ? ? 6A 0C 8D 54 24 38"),
				get_pattern("52 6A 00 E8 ? ? ? ? 55", 3),
				get_pattern("68 ? ? ? ? 52 6A 00 E8 ? ? ? ? 6A 00", 8),
			};
			
			// Championship standings pre-championship + unknown + Special Stage versus
			void* champ_standings1[] = {
				get_pattern("E8 ? ? ? ? 55 E8 ? ? ? ? 50 E8 ? ? ? ? 8D 4C 24 64"),
				get_pattern("51 6A 00 E8 ? ? ? ? 55 E8 ? ? ? ? 50", 3),
				get_pattern("6A 00 E8 ? ? ? ? 55 E8 ? ? ? ? 50 E8 ? ? ? ? 8D 4C 24 60", 2),
				get_pattern("50 6A 00 E8 ? ? ? ? 55", 3),
				get_pattern("E8 ? ? ? ? 6A 00 6A 00 6A 00 6A 00 6A FF E8 ? ? ? ? 5E 83 C4 08"),

				// Special Stage versus
				get_pattern("E8 ? ? ? ? E8 ? ? ? ? 50 E8 ? ? ? ? 85 C0 A1"),
				get_pattern("E8 ? ? ? ? A1 ? ? ? ? 83 F8 04"),
				get_pattern("E8 ? ? ? ? E8 ? ? ? ? 50 E8 ? ? ? ? 85 C0 75 42")
			};
			auto champ_standings2 = pattern("68 EA 01 00 00 50 6A 00 E8").count(2);

			void* champ_standings_redbar[] = {
				get_pattern("E8 ? ? ? ? EB 05 BB"),
				get_pattern("68 ? ? ? ? E8 ? ? ? ? EB 08", 5),

				// Special Stage versus
				get_pattern("E8 ? ? ? ? A1 ? ? ? ? 83 F8 03 75 0A"),
				get_pattern("68 ? ? ? ? E8 ? ? ? ? 6A 00 6A 00 6A 00", 5),
			};

			pattern("BA 80 02 00 00 2B D0").count(2).for_each_result([](pattern_match match)
			{
				UI_RightAlignElements.emplace_back(std::in_place_type<Int32Patch>, match.get<int32_t>(1), 640);
			});

			// Championship loading screen
			void* new_championship_loading_screen_text[] = {
				get_pattern("6A 0C E8 ? ? ? ? EB 04", 2),
				get_pattern("E8 ? ? ? ? 8B 44 24 2C 8B 4C 24 18"),
			};

			void* new_championship_loading_screen_rectangles[] = {
				get_pattern("E8 ? ? ? ? EB 02 DD D8 D9 44 24 14 D8 1D"),
				get_pattern("E8 ? ? ? ? EB 02 DD D8 D9 44 24 14 D8 1C AD"),
			};

			void* new_championship_loading_screen_lines[] = {
				get_pattern("DD D8 E8 ? ? ? ? D9 44 24 14", 2),
				get_pattern("E8 ? ? ? ? B8 ? ? ? ? 33 ED"),
				get_pattern("DD D8 E8 ? ? ? ? D9 44 24 20", 2),
			};

			// Stage loading background tiles
			UI_RightAlignElements.emplace_back(std::in_place_type<FloatPatch>, get_pattern<float>("C7 44 24 ? ? ? ? ? 89 44 24 3C 89 44 24 38", 4), 640.0f);
			UI_RightAlignElements.emplace_back(std::in_place_type<FloatPatch>, get_pattern<float>("DF 6C 24 78 C7 44 24", 4+4), 640.0f);

			// CMR3 logo in menus
			UI_RightAlignElements.emplace_back(std::in_place_type<Int32Patch>, get_pattern<int32_t>("68 ? ? ? ? 6A 40 6A 40", 1), 519);

			// CMR3 logo in the engagement screen
			patch_field_center("68 C0 00 00 00 68", 1);
			
			auto post_race_certina_logos1 = pattern("E8 ? ? ? ? 6A 15 E8 ? ? ? ? 50").count(5);
			auto post_race_certina_logos2 = pattern("E8 ? ? ? ? 68 51 02 00 00").count(2);
			auto post_race_certina_logos3 = pattern("E8 ? ? ? ? 68 46 02 00 00 E8").count(2);
			auto post_race_certina_logos4 = pattern("E8 ? ? ? ? 68 DA 00 00 00 E8").count(1);
			auto post_race_flags = pattern("E8 ? ? ? ? 83 ? ? 8B 54 24 ? 42").count(2);
			patch_field("68 34 02 00 00", 1); // push 564
			
			auto post_race_centered_texts1 = pattern("68 40 01 00 00 68 ? ? ? ? E8 ? ? ? ? 50 6A 00 E8 ? ? ? ? 5F 5E").count(6);
			auto post_race_centered_texts2 = pattern("68 40 01 00 00 68 ? ? ? ? E8 ? ? ? ? 50 6A 00 E8 ? ? ? ? 5E 5D").count(1);
			auto post_race_right_texts1 = pattern("6A 00 E8 ? ? ? ? ? 83 ? 06").count(2);
			auto post_race_right_texts2 = pattern("68 ? ? ? ? 68 ? ? ? ? 6A 0C E8 ? ? ? ? 8B 74 24 ? 8B 7C 24").count(2);
			auto post_race_right_texts3 = pattern("68 17 02 00 00 68 ? ? ? ? 6A 0C E8").count(4);
			
			// Time trial texts need a range check
			auto post_race_right_texts4 = pattern(get_pattern_uintptr("53 55 56 E8 ? ? ? ? D9 84 24 ? ? ? ? D8 0D"),
												post_race_centered_texts2.get_one().get_uintptr(),
												"E8 ? ? ? ? 46 83 FE").count(5);

			// Shakedown
			auto post_race_right_texts5 = pattern("E8 ? ? ? ? 46 83 FE 03 7C 9A").count(1);
			auto post_race_right_texts6 = pattern("68 7B 01 00 00 68 ? ? ? ? 55 E8").count(3);
			auto post_race_right_texts7 = pattern("68 C9 01 00 00 68 ? ? ? ? 6A 0C E8").count(3);

			// Stages high scores
			auto stages_highscores_begin = get_pattern_uintptr("53 55 56 57 33 FF 89 7C 24 14");
			auto stages_highscores_end = get_pattern_uintptr("0F 82 ? ? ? ? 6A 00 6A 00 6A 00 6A 00");
			auto stages_highscores_centered_texts1 = pattern(stages_highscores_begin, stages_highscores_end, "68 ? ? ? ? 57 E8");
			auto stages_highscores_centered_texts2 = pattern(stages_highscores_begin, stages_highscores_end, "6A 00 E8");
			auto stages_highscores_centered_texts3 = pattern(stages_highscores_begin, stages_highscores_end, "6A 0C E8");

			// Championship high scores
			auto championship_highscores_begin = get_pattern_uintptr("53 55 56 57 E8 ? ? ? ? 25");
			auto championship_highscores_end = get_pattern_uintptr("81 FF ? ? ? ? 0F 8C ? ? ? ? 6A 00");
			auto championship_highscores_centered_texts1 = pattern(championship_highscores_begin, championship_highscores_end, "6A 00 E8");
			auto championship_highscores_centered_texts2 = pattern(championship_highscores_begin, championship_highscores_end, "6A 0C E8");

			// OSD keyboard
			auto osd_keyboard_draw_text_entry_box1 = pattern("E8 ? ? ? ? 50 6A 1F 68 ? ? ? ? 68 ? ? ? ? 68 ? ? ? ? E8").count(3);
			auto osd_keyboard_draw_text_entry_box2 = pattern("E8 ? ? ? ? 8B 46 20 45 81 C7 ? ? ? ? 3B E8").count(3);
			auto osd_keyboard_blit_text_centered1 = pattern("50 6A 00 E8 ? ? ? ? 8B 15 ? ? ? ? 56 53 52").count(2);
			auto osd_keyboard_blit_text_centered2 = pattern("50 6A 00 E8 ? ? ? ? A1 ? ? ? ? 56").count(1);
			auto osd_keyboard_blit_text_centered3 = pattern("E8 ? ? ? ? FF 44 24 ? E9").count(3);
			auto osd_keyboard_blit_text_centered4 = pattern("E8 ? ? ? ? A1 ? ? ? ? 8B 54 24 18 52 53 50").count(3);
			void* osd_keyboard_best_score_text = get_pattern("E8 ? ? ? ? 8B 7D 14 C1 EF 0A");
			void* osd_keyboard_access_code_text = get_pattern("6A 0C E8 ? ? ? ? 8B 6C 24 24", 2);

			// Engagement screen
			auto engagement_screen_press_return_text1 = pattern("68 ? ? ? ? 6A 0C E8 ? ? ? ? D9 44 24").count(10);

			// Telemetry screen
			void* telemetry_legend_boxes_centered = get_pattern("E8 ? ? ? ? 8B 15 ? ? ? ? 6A 22");
			void* telemetry_texts_centered[] = {
				get_pattern("E8 ? ? ? ? 81 C7 ? ? ? ? 83 C5 04"),
				get_pattern("83 C4 08 50 53 E8", 5),
				get_pattern("E8 ? ? ? ? 8B 44 24 1C 8B 54 24 28"),
			};
			auto telemetry_lines1 = pattern("D8 C9 D9 5C 24 ? DD D8 E8 ? ? ? ? 83 C5 20").count(3);
			auto telemetry_lines2 = pattern("D8 C9 D9 9C 24 ? ? ? ? DD D8 E8 ? ? ? ? 83 C5 20").count(3);

			// Secrets screen
			// The amount of texts to patch differs between executables, so a range check is needed
			auto secrets_begin = pattern("C7 05 ? ? ? ? 80 02 00 00 D9 44 24 20").get_one();
			auto secrets_end = get_pattern_uintptr("89 44 24 1C 3B FA");
			auto secrets_centered_texts1 = pattern(secrets_begin.get_uintptr(), secrets_end, "68 40 01 00 00 68 ? ? ? ? E8 ? ? ? ? 50 6A ? E8");
			auto secrets_centered_texts2 = pattern(secrets_begin.get_uintptr(), secrets_end, "68 40 01 00 00 68 ? ? ? ? 6A ? E8");

			UI_RightAlignElements.emplace_back(std::in_place_type<Int32Patch>, secrets_begin.get<int32_t>(6), 640);
			UI_RightAlignElements.emplace_back(std::in_place_type<Int32Patch>, get_pattern<int32_t>("C7 05 ? ? ? ? ? ? ? ? E8 ? ? ? ? 68 ? ? ? ? 55", 6), 640); 
			UI_RightAlignElements.emplace_back(std::in_place_type<Int32Patch>, get_pattern<int32_t>("BF ? ? ? ? 2B F9", 1), 640);
			UI_RightAlignElements.emplace_back(std::in_place_type<Int32Patch>, get_pattern<int32_t>("BF ? ? ? ? 2B FA 2B F9", 1), 640);

			// "Original settings will be restored in X seconds" dialogs
			// + slot delete
			void* settings_reset_dialog_backgrounds[] = {
				get_pattern("DD D8 E8 ? ? ? ? E8 ? ? ? ? 68", 2),
				get_pattern("DD D8 E8 ? ? ? ? E8 ? ? ? ? 8B C8", 2),
			};

			void* settings_reset_dialog_texts[] = {
				get_pattern("E8 ? ? ? ? 68 ? ? ? ? E8 ? ? ? ? 50 8D 4C 24 68"),
				get_pattern("E8 ? ? ? ? 68 ? ? ? ? E8 ? ? ? ? 50 56"),
				get_pattern("51 6A 00 E8 ? ? ? ? 5F 5E 5B", 3),

				get_pattern("E8 ? ? ? ? 68 ? ? ? ? E8 ? ? ? ? 50 8D 44 24 68"),
				get_pattern("E8 ? ? ? ? 68 ? ? ? ? E8 ? ? ? ? 50 8D 54 24 68"),
				get_pattern("68 ? ? ? ? 50 6A 00 E8 ? ? ? ? 5F", 8),

				get_pattern("E8 ? ? ? ? 5E 8B 54 24 0C"),
			};

			patch_field_center("BA ? ? ? ? D1 F8", 1);

			// Controller calibration screen
			void* controller_calibrate_centered_texts[] = {
				get_pattern("C6 01 00 E8 ? ? ? ? A1", 3),
			};
			auto controller_calibrate_right_align_text = get_pattern("6A 00 E8 ? ? ? ? 8B 84 24 ? ? ? ? 85 C0 74 12 85 FF", 2);
			auto controller_calibrate_centered_box = get_pattern("E8 ? ? ? ? 8D 4C 24 2C 8B 44 24 24");

			auto controller_calibrate_centered_line1 = pattern("56 50 57 50 E8").count(2);
			auto controller_calibrate_centered_line2 = pattern("8B 44 24 1C 50 57 50 E8").count(4);

			// Splitscreen
			// Technically these are not centered, but same math applies
			UI_CenteredElements.emplace_back(std::in_place_type<Int32Patch>, get_pattern<int32_t>("83 FF 01 75 1F A1 ? ? ? ? C7 00", 12), 140);
			UI_CenteredElements.emplace_back(std::in_place_type<Int32Patch>, get_pattern<int32_t>("83 FF 03 75 3A 8B 0D ? ? ? ? B8 ? ? ? ? C7 01", 0x12), 140);
			UI_CenteredElements.emplace_back(std::in_place_type<Int32Patch>, get_pattern<int32_t>("8B 0D ? ? ? ? C7 01 ? ? ? ? 8B 15 ? ? ? ? C7 42", 6+2), 160);

			auto splitscreen_4th_viewport_rect2d = get_pattern("DD D8 E8 ? ? ? ? 8B 7C 24 30", 2);

			// Movie rendering
			auto movie_rect = pattern("C7 05 ? ? ? ? 00 00 00 BF C7 05 ? ? ? ? 00 00 00 BF").get_one();
			auto movie_name_setdir = get_pattern("E8 ? ? ? ? E8 ? ? ? ? 85 C0 A1 ? ? ? ? 0F 95 C3");
			UI_MovieX1 = movie_rect.get<float>(6);
			UI_MovieY1 = movie_rect.get<float>(10 + 6);
			UI_MovieX2 = movie_rect.get<float>(20 + 6);
			UI_MovieY2 = movie_rect.get<float>(30 + 6);

			orgOSDPositions = *osd_data.get<OSD_Data*>(2+3);
			orgOSDPositionsMulti = *osd_data.get<OSD_Data2*>(27+3);

			orgStartLightData = *get_pattern<Object_StartLight*>("8D 34 8D ? ? ? ? 89 74 24 0C", 3);

			// Assembly hook into HandyFunction_Draw2DBox and CMR3Font_SetViewport to stretch fullscreen draws automatically
			// Opt out by drawing 1px offscreen
			{
				using namespace SolidRectangleWidthHack;

				auto draw_solid_background = pattern("DB 44 24 5C 8B 44 24 6C").get_one();
				auto set_string_extents = pattern("56 83 F8 FE 57").get_one();

				InjectHook(draw_solid_background.get<void>(-0x1A), HandyFunction_Draw2DBox_Hack, PATCH_JUMP);
				HandyFunction_Draw2DBox_JumpBack = draw_solid_background.get<void>(-0x1A + 5);

				InjectHook(set_string_extents.get<void>(-6), CMR3Font_SetViewport_Hack, PATCH_JUMP);
				CMR3Font_SetViewport_JumpBack = set_string_extents.get<void>(-6 + 5);
			}

			HookEach_RecalculateUI(graphics_change_recalculate_ui, InterceptCall);

			for (void* addr : osd_element_init_center)
			{
				InjectHook(addr, OSD_Element_Init_Center);
			}

			for (void* addr : osd_element_init_right)
			{
				InjectHook(addr, OSD_Element_Init_RightAlign);
			}

			for (void* addr : solid_background_full_width)
			{
				InjectHook(addr, HandyFunction_Draw2DBox_Stretch);
			}

			for (void* addr : race_standings)
			{
				InjectHook(addr, CMR3Font_BlitText_RightAlign);
			}
			for (void* addr : champ_standings1)
			{
				InjectHook(addr, CMR3Font_BlitText_RightAlign);
			}
			champ_standings2.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(8), CMR3Font_BlitText_RightAlign);
			});

			for (void* addr : champ_standings_redbar)
			{
				InjectHook(addr, HandyFunction_Draw2DBox_RightAlign);
			}

			for (void* addr : new_championship_loading_screen_text)
			{
				InjectHook(addr, CMR3Font_BlitText_Center);
			}
			for (void* addr : new_championship_loading_screen_rectangles)
			{
				InjectHook(addr, Core_Blitter2D_Rect2D_G_Center);
			}
			for (void* addr : new_championship_loading_screen_lines)
			{
				InjectHook(addr, Core_Blitter2D_Line2D_G_Center);
			}

			post_race_certina_logos1.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(), Core_Blitter2D_Rect2D_GT_RightAlign);
			});
			post_race_certina_logos2.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(), Core_Blitter2D_Rect2D_GT_RightAlign);
			});
			post_race_certina_logos3.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(), Core_Blitter2D_Rect2D_GT_RightAlign);
			});
			post_race_certina_logos4.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(), Core_Blitter2D_Rect2D_GT_RightAlign);
			});
			post_race_flags.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(), Core_Blitter2D_Rect2D_GT_RightAlign);
			});
			post_race_centered_texts1.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(18), CMR3Font_BlitText_Center);
			});
			post_race_centered_texts2.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(18), CMR3Font_BlitText_Center);
			});
			post_race_right_texts1.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(2), CMR3Font_BlitText_RightAlign);
			});
			post_race_right_texts2.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(12), CMR3Font_BlitText_RightAlign);
			});
			post_race_right_texts3.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(12), CMR3Font_BlitText_RightAlign);
			});
			post_race_right_texts4.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(), CMR3Font_BlitText_RightAlign);
			});
			post_race_right_texts5.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(), CMR3Font_BlitText_RightAlign);
			});
			post_race_right_texts6.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(11), CMR3Font_BlitText_RightAlign);
			});
			post_race_right_texts7.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(12), CMR3Font_BlitText_RightAlign);
			});

			stages_highscores_centered_texts1.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(6), CMR3Font_BlitText_Center);
			});
			stages_highscores_centered_texts2.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(2), CMR3Font_BlitText_Center);
			});
			stages_highscores_centered_texts3.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(2), CMR3Font_BlitText_Center);
			});
			championship_highscores_centered_texts1.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(2), CMR3Font_BlitText_Center);
			});
			championship_highscores_centered_texts2.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(2), CMR3Font_BlitText_Center);
			});

			osd_keyboard_draw_text_entry_box1.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(0x17), Keyboard_DrawTextEntryBox_Center);
			});
			osd_keyboard_draw_text_entry_box2.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(), Keyboard_DrawTextEntryBox_Center);
			});
			osd_keyboard_blit_text_centered1.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(3), CMR3Font_BlitText_Center);
			});
			osd_keyboard_blit_text_centered2.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(3), CMR3Font_BlitText_Center);
			});
			osd_keyboard_blit_text_centered3.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(), CMR3Font_BlitText_Center);
			});
			osd_keyboard_blit_text_centered4.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(), CMR3Font_BlitText_Center);
			});
			InjectHook(osd_keyboard_best_score_text, CMR3Font_BlitText_Center);
			InjectHook(osd_keyboard_access_code_text, CMR3Font_BlitText_RightAlign);

			engagement_screen_press_return_text1.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(5 + 2), CMR3Font_BlitText_Center);
			});

			InjectHook(telemetry_legend_boxes_centered, HandyFunction_Draw2DBox_Center);
			for (void* addr : telemetry_texts_centered)
			{
				InjectHook(addr, CMR3Font_BlitText_Center);
			}
			telemetry_lines1.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(8), Core_Blitter2D_Line2D_G_Center);
			});
			telemetry_lines2.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(11), Core_Blitter2D_Line2D_G_Center);
			});

			secrets_centered_texts1.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(18), CMR3Font_BlitText_Center);
			});
			secrets_centered_texts2.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(12), CMR3Font_BlitText_Center);
			});

			for (void* addr : settings_reset_dialog_backgrounds)
			{
				InjectHook(addr, Core_Blitter2D_Rect2D_G_Center);
			}
			for (void* addr : settings_reset_dialog_texts)
			{
				InjectHook(addr, CMR3Font_BlitText_Center);
			}
			for (void* addr : controller_calibrate_centered_texts)
			{
				InjectHook(addr, CMR3Font_BlitText_Center);
			}
			InjectHook(controller_calibrate_right_align_text, CMR3Font_BlitText_CalibrateAxisName);
			InjectHook(controller_calibrate_centered_box, HandyFunction_Draw2DBox_Center);
			controller_calibrate_centered_line1.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(4), HandyFunction_Draw2DLineFromTo_Center);
			});
			controller_calibrate_centered_line2.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(7), HandyFunction_Draw2DLineFromTo_Center);
			});

			InjectHook(splitscreen_4th_viewport_rect2d, Core_Blitter2D_Rect2D_GT_CenterHalf);
			
			ReadCall(movie_name_setdir, orgSetMovieDirectory);
			InjectHook(movie_name_setdir, SetMovieDirectory_SetDimensions);

			OSD_Main_SetUpStructsForWidescreen();
		}
		catch (const hook::txn_exception&)
		{
			Graphics::Patches::UI_CenteredElements.clear();
			Graphics::Patches::UI_RightAlignElements.clear();
		}
		Graphics::Patches::UI_CenteredElements.shrink_to_fit();
		Graphics::Patches::UI_RightAlignElements.shrink_to_fit();
	}
	TXN_CATCH();


	// Re-enabled Alt+F4
	try
	{
		using namespace QuitMessageFix;

		auto wndproc_messages_indirect_array = *get_pattern<uint8_t*>("8A 88 ? ? ? ? FF 24 8D ? ? ? ? 33 D2", 2);
		auto post_quit_message = get_pattern("6A 00 FF 15 ? ? ? ? E9", 2 + 2);
		gboExitProgram = *get_pattern<uint32_t*>("83 C4 04 39 2D", 3+2);

		const uint8_t def_proc = wndproc_messages_indirect_array[WM_CLOSE + 1 - 2];
		Patch<uint8_t>(&wndproc_messages_indirect_array[WM_CLOSE - 2], def_proc);

		Patch(post_quit_message, &pPostQuitMessage_AndRequestExit);
	}
	TXN_CATCH();


	// Fixed legend not fading on the telemetry screen
	try
	{
		using namespace TelemetryFadingLegend;

		auto draw_2d_box = pattern("6A ? 6A ? 8D 54 0A 19").get_one();

		originalHeight = *draw_2d_box.get<int8_t>(1);
		originalWidth = *draw_2d_box.get<int8_t>(3);

		// push dword ptr [esp+110h-D8h]
		Patch(draw_2d_box.get<void>(), { 0xFF, 0x74, 0x24, 0x38 });
		Patch<int8_t>(draw_2d_box.get<void>(10 + 3), 0x118 - 0xDC);

		ReadCall(draw_2d_box.get<void>(18), orgHandyFunction_Draw2DBox);
		InjectHook(draw_2d_box.get<void>(18), Draw2DBox_HackedAlpha);
	}
	TXN_CATCH();


	// Fixed the resolution change counter going into negatives
	try
	{
		using namespace CappedResolutionCountdown;

		auto countdown_sprintf = get_pattern("E8 ? ? ? ? 83 C4 14 8D 4C 24 64");
		InjectHook(countdown_sprintf, sprintf_clamp);
	}
	TXN_CATCH();


	// Fixed menu entries fading incorrectly
	try
	{
		auto on_submenu_enter = get_pattern("0F BF 46 18 39 44 24 20 75", 8);
		auto on_submenu_exit = get_pattern("39 54 24 20 74", 4);

		Patch<uint8_t>(on_submenu_enter, 0xEB);
		Nop(on_submenu_exit, 2);
	}
	TXN_CATCH();


	// Fixed an inconsistent Controls screen
	try
	{
		using namespace ConsistentControlsScreen;

		char* wrong_format_string = *get_pattern<char*>("68 ? ? ? ? 68 ? ? ? ? E8 ? ? ? ? 8B 4C 24 34", 1);
		auto uppercase_controller_name = get_pattern("E8 ? ? ? ? 8B 44 24 20 83 F8 04");

		auto get_language_sprintf_pattern = pattern("50 56 68 74 03 00 00 E8 ? ? ? ? 50").count(2);
		std::array<void*, 2> get_language_sprintf = {
			get_language_sprintf_pattern.get(0).get<void>(7),
			get_language_sprintf_pattern.get(1).get<void>(7),
		};

		// Patch the string directly, whatevz
		if (mainModuleInstance == GetModuleHandleFromAddress(wrong_format_string))
		{
			strcpy_s(wrong_format_string, 6, "%s: ");
		}

		InterceptCall(uppercase_controller_name, orgGetControllerName, GetControllerName_Uppercase);

		HookEach(get_language_sprintf, InterceptCall);
	}
	TXN_CATCH();


	// "Cut" character flicker for invalid codepoints - let it be ! now
	try
	{
		using namespace UnrandomizeUnknownCodepoints;

		auto rand_sequence1 = pattern("E8 ? ? ? ? 6A 00 88 44 24 20 E8 ? ? ? ? 6A 00 88 44 24 24 E8 ? ? ? ? 25").get_one();
		void* rand_sequence[] = {
			rand_sequence1.get<void>(),
			rand_sequence1.get<void>(0xB),
			rand_sequence1.get<void>(0x16),
			get_pattern("89 4C 24 18 E8 ? ? ? ? 8B 4D 00 33 D2", 4),
		};

		auto set_current_font = pattern("E8 ? ? ? ? 8B 45 04").get_one();

		for (void* addr : rand_sequence)
		{
			InjectHook(addr, Random_RandSequence_Fixed);
		}

		InjectHook(set_current_font.get<void>(), CMR3Font_SetFontForBlitChar_NOP);
		InjectHook(set_current_font.get<void>(0x2A), CMR3Font_SetFontForBlitChar_NOP);
	}
	TXN_CATCH();


	// Make the game portable
	// Removes settings from registry and reliance on INSTALL_PATH
	bool HasPatches_Registry = false;
	try
	{
		using namespace Registry;
		using namespace Patches;

		auto get_install_string_operator_new = get_pattern("E8 ? ? ? ? 8B 54 24 10 83 C4 04");
		auto get_install_string = get_pattern("E8 ? ? ? ? 50 E8 ? ? ? ? 68 ? ? ? ? 8B D8 68");
		auto get_registry_dword = get_pattern("83 EC 08 8B 4C 24 0C 8D 44 24 04");
		auto set_registry_dword = get_pattern("8D 54 24 0C 6A 04", -0x24);
		auto set_registry_char = get_pattern("8B 4C 24 04 8D 54 24 0C 6A 01", -0x20);

		ReadCall(get_install_string_operator_new, Patches::orgOperatorNew);
		InjectHook(get_install_string, GetInstallString_Portable);

		InjectHook(get_registry_dword, GetRegistryDword_Patch, PATCH_JUMP);
		InjectHook(set_registry_dword, SetRegistryDword_Patch, PATCH_JUMP);
		InjectHook(set_registry_char, SetRegistryChar_Patch, PATCH_JUMP);

		// This one is optional! Polish exe lacks it
		try
		{
			auto get_registry_char = get_pattern("75 5F 8B 4C 24 14", -0x21);
			InjectHook(get_registry_char, GetRegistryChar_Patch, PATCH_JUMP);
		}
		TXN_CATCH();

		Registry::Init();
		HasPatches_Registry = true;
	}
	TXN_CATCH();


	// Reflection map reflecting the sky
	// Registry is an optional dependency - without it, we can't restore "old" reflections via the INI file
	try
	{
		using namespace EnvMapWithSky;

		auto render_refmap_sfx_check = get_pattern("6A 02 E8 ? ? ? ? 85 C0 0F 85 ? ? ? ? 89 44 24 60", 2);

		InterceptCall(render_refmap_sfx_check, orgSfxEnable, SFX_Enable_Force);

		// Only patch the switch if we have registry
		if (HasPatches_Registry) try
		{
			auto after_setup_texture_stages = get_pattern("DD D8 E8 ? ? ? ? 50 E8", 2);

			InterceptCall(after_setup_texture_stages, orgAfterSetupTextureStages, Graphics_CarMultitexture_AfterSetupTextureStages);
		}
		TXN_CATCH();
	}
	TXN_CATCH();


	// Additional Advanced Graphics options
	// Windowed Mode, VSync, Anisotropic Filtering
	// Requires patches: Registry (for saving/loading), Core D3D (for resizing windows), Graphics, RenderState (for AF)
	if (HasPatches_Registry && HasCored3d && HasDestruct && HasGraphics && HasRenderState) try
	{
		using namespace NewAdvancedGraphicsOptions;

		// Both inner scopes patch this independently
		auto graphics_change_pattern = pattern("85 C0 74 0A 8D 4C 24 54").get_one();
		auto gamma_on_adapter_change = pattern("C7 83 ? ? ? ? ? ? ? ? EB 1B").get_one();

		// Try to decouple frontend options from the actual implementations
		bool HasPatches_AdvancedGraphics = false;
		try
		{
			auto nop_adjust_windowrect = get_pattern("FF 15 ? ? ? ? 6A 00", 2);
			auto find_window_ex = get_pattern("FF 15 ? ? ? ? 85 C0 74 0A", 2);
			auto create_class_and_window = get_pattern("83 EC 28 8B 44 24 38");
			auto graphics_initialise = get_pattern("E8 ? ? ? ? 8B 54 24 24 89 5C 24 18");

			auto set_sampler_state = reinterpret_cast<uintptr_t>(ReadCallFrom(get_pattern("E8 ? ? ? ? D9 44 24 0C D9 1C B5 ? ? ? ? D9 44 24 08")));
			auto render_game_common1_set_af = get_pattern("E8 ? ? ? ? 68 ? ? ? ? 6A 03 E8 ? ? ? ? E8 ? ? ? ? 8B 5C 24 30 85 C0 74 06 53 E8");
			auto get_filtering_method_for_3d = ReadCallFrom(get_pattern("E8 ? ? ? ? 8B 17 50"));
			auto set_mip_bias_and_anisotropic = get_pattern("E8 ? ? ? ? 8D 4C 24 20 51 57");

			std::array<void*, 2> graphics_change = {
				graphics_change_pattern.get<void>(-5),
				graphics_change_pattern.get<void>(9),
			};

			std::array<void*, 2> nop_presentinterval_changes = {
				get_pattern("74 12 8D 4C 24 04"),
				get_pattern("74 0E 8D 4C 24 0C"),
			};

			auto set_window_pos_adjust = pattern("50 FF 15 ? ? ? ? A1 ? ? ? ? 8B 0D ? ? ? ? 89 44 24 0C").get_one();
			auto disable_gamma_for_window = get_pattern("E8 ? ? ? ? B9 ? ? ? ? 8B F0 8D 7C 24 10 F3 A5 8D 8C 24");

			ghInstance = *get_pattern<HINSTANCE*>("89 3D ? ? ? ? 89 44 24 14", 2);

			Patch(nop_adjust_windowrect, &pAdjustWindowRectEx_NOP);
			Patch(find_window_ex, &pFindWindowExA_IgnoreWindowName);
			InjectHook(create_class_and_window, CreateClassAndWindow, PATCH_JUMP);
			InterceptCall(graphics_initialise, orgGraphics_Initialise, Graphics_Initialise_NewOptions);

			HookEach_Graphics_Change(graphics_change, InterceptCall);

			Patch(set_window_pos_adjust.get<void>(-0x22 + 2), &pSetWindowLongA_NOP);
			Patch(set_window_pos_adjust.get<void>(1 + 2), &pSetWindowPos_Adjust);

			InterceptCall(disable_gamma_for_window, orgGraphics_GetAdapterCaps, Graphics_GetAdapterCaps_DisableGammaForWindow);

			// NOP gamma changes in windowed mode so switching to windowed mode doesn't reset the option
			Nop(gamma_on_adapter_change.get<void>(0x12), 10);

			for (void* addr : nop_presentinterval_changes)
			{
				Patch<uint8_t>(addr, 0xEB);
			};

			guSSChanges = *reinterpret_cast<int**>(set_sampler_state + 2);
			RenderState_SetSamplerState_JumpBack = reinterpret_cast<void*>(set_sampler_state + 6);
			InjectHook(set_sampler_state, RenderState_SetSamplerState_Fallback, PATCH_JUMP);

			InterceptCall(render_game_common1_set_af, orgMarkProfiler, MarkProfiler_SetAF);
			InjectHook(get_filtering_method_for_3d, GetAnisotropicFilter, PATCH_JUMP);
			InterceptCall(set_mip_bias_and_anisotropic, orgSetMipBias, Texture_SetMipBiasAndAnisotropic);

			HasPatches_AdvancedGraphics = true;
		}
		TXN_CATCH();


		// Only make frontend changes if all functionality has been successfully patched in
		if (HasGlobals && HasGraphics && HasCMR3FE && HasCMR3Font && HasMenuHook && HasLanguageHook && HasPatches_AdvancedGraphics) try
		{
			auto graphics_advanced_enter = pattern("8B 46 24 A3 ? ? ? ? 8B 4E 4C").get_one();
			auto graphics_advanced_select = get_pattern("50 E8 ? ? ? ? 8B 0D ? ? ? ? 8B 74 24 58", -0xD);

			auto advanced_graphics_load_settings = get_pattern("83 EC 58 53 55 56 57 6A 00 68");
			auto advanced_graphics_save_settings = get_pattern("83 EC 2C A1 ? ? ? ? 53 56 57 6A FF 50");

			auto set_graphics_from_preset = get_pattern("25 ? ? ? ? 33 C9 2B C1", -6);

			auto advanced_graphics_display_jump_table = pattern("8B DF 83 F8 ? 0F 87").get_one();

			auto advanced_graphics_texture_quality_display_value = get_pattern("8B 86 ? ? ? ? 81 C7 ? ? ? ? 83 E8 00 74 0A", 2);
			auto advanced_graphics_envmap_display = pattern("8A 86 ? ? ? ? 8B 8C 24").get_one();
			auto advanced_graphics_shadows_display = pattern("8A 8E ? ? ? ? 8B 94 24").get_one();
			auto advanced_graphics_draw_distance_display = pattern("8B 8E ? ? ? ? 8B F8 81 C7").get_one();
			auto advanced_graphics_gamma_display = pattern("8B 86 ? ? ? ? 81 C7 ? ? ? ? 40").get_one();
			auto advanced_graphics_fsaa_display_value = get_pattern("8B 96 ? ? ? ? 8B 40 4C", 2);

			auto advanced_graphics_texture_quality_display_arrow = get_pattern("52 6A 03 56", 2);
			auto advanced_graphics_draw_distance_display_arrow = get_pattern("50 6A 06 56", 2);
			auto advanced_graphics_gamma_display_arrow = get_pattern("52 6A 07 56", 2);
			auto advanced_graphics_fsaa_display_arrow = get_pattern("52 6A 08 56", 2);

			auto advanced_graphics_handle_hook = pattern("83 EC 50 8D 44 24 00 53 55 56 57 50 E8").get_one();

			PC_GraphicsAdvanced_Display_NewOptionsJumpBack = get_pattern("8B 44 24 1C 8B 4C 24 18 25 ? ? ? ? 6A 09");

			void* fsaa_globals_value[] = {
				get_pattern("A1 ? ? ? ? 89 7C 24 20", 1),
			};

			void* gamma_locals_value[] = {
				get_pattern("51 8B 7C 24 68 DB 87", 1 + 4 + 2),
			};

			auto fsaa_on_adapter_change = pattern("8B 83 ? ? ? ? 7F 0A").get_one();
			auto envmap_on_adapter_change1 = get_pattern("89 15 ? ? ? ? 8D 84 24", -6 + 2);
			auto envmap_on_adapter_change2 = pattern("8B 83 ? ? ? ? 75 22").get_one();
			auto shadows_on_adapter_change = pattern("89 15 ? ? ? ? 85 84 24").get_one();

			InjectHook(advanced_graphics_load_settings, PC_GraphicsAdvanced_LoadSettings, PATCH_JUMP);
			InjectHook(advanced_graphics_save_settings, PC_GraphicsAdvanced_SaveSettings, PATCH_JUMP);

			InjectHook(set_graphics_from_preset, PC_GraphicsAdvanced_SetGraphicsFromPresetQuality, PATCH_JUMP);

			Patch<int32_t>(graphics_advanced_enter.get<void>(0x22 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_SHADOWS].m_value));

			InjectHook(graphics_advanced_enter.get<void>(0x3B), PC_GraphicsAdvanced_Enter_NewOptions, PATCH_JUMP);
			InjectHook(graphics_advanced_select, PC_GraphicsAdvanced_Select_NewOptions, PATCH_JUMP);

			for (void* addr : fsaa_globals_value)
			{
				Patch(addr, &gmoFrontEndMenus[MenuID::GRAPHICS_ADVANCED].m_entries[EntryID::GRAPHICS_ADV_FSAA].m_value);
			}

			for (void* addr : gamma_locals_value)
			{
				Patch<int32_t>(addr, offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_GAMMA].m_value));
			}

			Patch<int32_t>(fsaa_on_adapter_change.get<void>(-6 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_FSAA].m_entryDataInt));
			Patch<int32_t>(fsaa_on_adapter_change.get<void>(0 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_FSAA].m_visibilityAndName));
			Patch<int32_t>(fsaa_on_adapter_change.get<void>(8 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_FSAA].m_value));
			Patch<int32_t>(fsaa_on_adapter_change.get<void>(0x17 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_FSAA].m_visibilityAndName));
			
			Patch<int32_t>(gamma_on_adapter_change.get<void>(0 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_GAMMA].m_entryDataInt));
			Patch<int32_t>(gamma_on_adapter_change.get<void>(0xC + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_GAMMA].m_visibilityAndName));
			// Gamma m_value change was NOP'd above
			//Patch<int32_t>(gamma_on_adapter_change.get<void>(0x12 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_GAMMA].m_value));
			Patch<int32_t>(gamma_on_adapter_change.get<void>(0x21 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_GAMMA].m_visibilityAndName));

			Patch<int32_t>(envmap_on_adapter_change1, offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_ENVMAP].m_value));
			Patch<int32_t>(envmap_on_adapter_change2.get<void>(0 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_ENVMAP].m_visibilityAndName));
			Patch<int32_t>(envmap_on_adapter_change2.get<void>(0xF + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_ENVMAP].m_value));
			Patch<int32_t>(envmap_on_adapter_change2.get<void>(0x19 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_ENVMAP].m_visibilityAndName));
			Patch<int32_t>(envmap_on_adapter_change2.get<void>(0x37 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_ENVMAP].m_value));
			Patch<int32_t>(envmap_on_adapter_change2.get<void>(0x37 + 6 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_ENVMAP].m_visibilityAndName));

			Patch<int32_t>(shadows_on_adapter_change.get<void>(-6 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_SHADOWS].m_value));
			Patch<int32_t>(shadows_on_adapter_change.get<void>(0xF + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_SHADOWS].m_visibilityAndName));
			Patch<int32_t>(shadows_on_adapter_change.get<void>(0xF + 6 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_SHADOWS].m_value));
			Patch<int32_t>(shadows_on_adapter_change.get<void>(0x2B + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_SHADOWS].m_value));
			Patch<int32_t>(shadows_on_adapter_change.get<void>(0x31 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_SHADOWS].m_visibilityAndName));
			Patch<int32_t>(shadows_on_adapter_change.get<void>(0x41 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_SHADOWS].m_visibilityAndName));

			// Build and inject a new jump table
			Patch<uint8_t>(advanced_graphics_display_jump_table.get<void>(2 + 2), EntryID::GRAPHICS_ADV_NUM - 1);

			void** orgJumpTable = *advanced_graphics_display_jump_table.get<void**>(0xB + 3);
			static const void* advanced_graphics_display_new_jump_table[EntryID::GRAPHICS_ADV_NUM] = {
				orgJumpTable[0], orgJumpTable[1], orgJumpTable[2],
				&PC_GraphicsAdvanced_Display_CaseNewOptions, &PC_GraphicsAdvanced_Display_CaseNewOptions,
				orgJumpTable[3], orgJumpTable[4], orgJumpTable[5], orgJumpTable[6], &PC_GraphicsAdvanced_Display_CaseNewOptions, orgJumpTable[7], orgJumpTable[8],
				orgJumpTable[9], orgJumpTable[10]
			};
			Patch(advanced_graphics_display_jump_table.get<void**>(0xB + 3), &advanced_graphics_display_new_jump_table);

			Patch<int32_t>(advanced_graphics_texture_quality_display_value, offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_TEXTUREQUALITY].m_value));
			Patch<int8_t>(advanced_graphics_texture_quality_display_arrow, EntryID::GRAPHICS_ADV_TEXTUREQUALITY);

			Patch<int32_t>(advanced_graphics_envmap_display.get<void>(2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_ENVMAP].m_visibilityAndName) + 3);
			Patch<int32_t>(advanced_graphics_envmap_display.get<void>(6 + 7 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_ENVMAP].m_value));

			Patch<int32_t>(advanced_graphics_shadows_display.get<void>(2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_SHADOWS].m_visibilityAndName) + 3);
			Patch<int32_t>(advanced_graphics_shadows_display.get<void>(6 + 7 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_SHADOWS].m_value));

			Patch<int32_t>(advanced_graphics_draw_distance_display.get<void>(2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_DRAWDISTANCE].m_value));
			Patch<int32_t>(advanced_graphics_draw_distance_display.get<void>(0x24 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_DRAWDISTANCE].m_value));
			Patch<int8_t>(advanced_graphics_draw_distance_display_arrow, EntryID::GRAPHICS_ADV_DRAWDISTANCE);

			Patch<int32_t>(advanced_graphics_gamma_display.get<void>(2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_GAMMA].m_value));
			Patch<int32_t>(advanced_graphics_gamma_display.get<void>(0x22 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_GAMMA].m_value));
			Patch<int8_t>(advanced_graphics_gamma_display_arrow, EntryID::GRAPHICS_ADV_GAMMA);

			Patch<int32_t>(advanced_graphics_fsaa_display_value, offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_FSAA].m_value));
			Patch<int8_t>(advanced_graphics_fsaa_display_arrow, EntryID::GRAPHICS_ADV_FSAA);

			InterceptCall(graphics_change_pattern.get<void>(-5), orgGraphics_SetupRenderFromMenuOptions, Graphics_Change_SetupRenderFromMenuOptions);

			PC_GraphicsAdvanced_Handle_JumpBack = advanced_graphics_handle_hook.get<void>(7);
			InjectHook(advanced_graphics_handle_hook.get<void>(), PC_GraphicsAdvanced_Handle_NewOptions, PATCH_JUMP);

			Menus::Patches::ExtraAdvancedGraphicsOptionsPatched = true;
		}
		TXN_CATCH();
	}
	TXN_CATCH();

	// Additional Graphics options
	// FOV Control, Split Screen, Digital tacho
	// Requires patches: Registry (for saving/loading)
	if (HasPatches_Registry) try
	{
		using namespace NewGraphicsOptions;

		bool HasPatches_FOV = false;
		try
		{
			auto cameras_after_initialise = get_pattern("7C E5 68 ? ? ? ? E8", 7);

			void* exterior_fov[] = {
				get_pattern("E8 ? ? ? ? 57 E8 ? ? ? ? 8B 45 08"),
				get_pattern("E8 ? ? ? ? 85 FF 74 06 57 E8 ? ? ? ? 8B 45 08 85 C0 74 0B 6A 05"),
			};

			void* interior_fov[] = {
				get_pattern("E8 ? ? ? ? 85 FF 74 06 57 E8 ? ? ? ? 8B 45 08 85 C0 74 0B 6A 04"),
			};

			for (void* addr : exterior_fov)
			{
				InjectHook(addr, Camera_SetFOV_Exterior);
			}
			for (void* addr : interior_fov)
			{
				InjectHook(addr, Camera_SetFOV_Interior);
			}

			InterceptCall(cameras_after_initialise, orgCameras_AfterInitialise, Cameras_AfterInitialise);

			HasPatches_FOV = true;
		}
		TXN_CATCH();

		bool HasPatches_SplitScreenOption = false;
		try
		{
			auto get_widescreen_init = get_pattern("E8 ? ? ? ? E8 ? ? ? ? 25 ? ? ? ? 8B F0", 5);

			auto get_widescreen1 = pattern("E8 ? ? ? ? F6 D8 1B C0 40 8B F0").count(2);
			void* get_widescreen[] = {
				ReadCallFrom(get_pattern("E8 ? ? ? ? 8B 75 08 89 44 24 3C")),
				get_pattern("E8 ? ? ? ? F6 D8 1B C0 40 83 FE 02"),
				get_pattern("E8 ? ? ? ? F6 D8 1B C0 40 8B F8"),
				get_pattern("E8 ? ? ? ? F6 D8 1B C0 6A 01 40"),
				get_pattern("E8 ? ? ? ? 84 C0 75 08"),
				get_pattern("E8 ? ? ? ? F6 D8 1B C0 68"),
				get_pattern("E8 ? ? ? ? 8B 9C 24 ? ? ? ? F6 D8"), // Unsure
				get_pattern("75 1C E8 ? ? ? ? 85 C0 75 07", 2),
				get_widescreen1.get(0).get<void>(),
				get_widescreen1.get(1).get<void>(),
			};

			InjectHook(get_widescreen_init, IsVerticalSplitscreen_ReadOption);
			for (void* addr : get_widescreen)
			{
				InjectHook(addr, IsVerticalSplitscreen);
			}

			HasPatches_SplitScreenOption = true;
		}
		TXN_CATCH();

		bool HasPatches_DigitalTacho = false;
		if (HasGameInfo && HasUIPositions && HasGraphics) try
		{
			std::array<void*, 5> num_of_players_in_race = {
				get_pattern("E8 ? ? ? ? 3C 01 75 3D"),
				get_pattern("E8 ? ? ? ? 3C 02 75 15"),
				get_pattern("E8 ? ? ? ? 3C 01 75 29"),
				get_pattern("E8 ? ? ? ? 3C 01 0F 85 ? ? ? ? 6A 01"),
				get_pattern("E8 ? ? ? ? 3C 02 75 3B 8B 44 24 2C"),
			};

			std::array<void*, 2> get_widescreen = {
				get_pattern("E8 ? ? ? ? F6 D8 1B C0 68"),
				get_pattern("E8 ? ? ? ? F6 D8 1B C0 6A 01 40"),
			};

			auto draw_tacho = get_pattern("E8 ? ? ? ? 6A 1C");

			HookEach_GetNumberOfPlayersInThisRace(num_of_players_in_race, InterceptCall);
			HookEach_GetWidescreen(get_widescreen, InterceptCall);

			InterceptCall(draw_tacho, orgDrawTacho, DrawTacho_SetPosition);

			HasPatches_DigitalTacho = true;
		}
		TXN_CATCH();

		// Only make frontend changes if all functionality has been successfully patched in
		if (HasGlobals && HasCMR3FE && HasCMR3Font && HasMenuHook && HasLanguageHook && HasPatches_FOV && HasPatches_SplitScreenOption && HasPatches_DigitalTacho) try
		{	
			void* back_locals[] = {
				get_pattern("8D 85 ? ? ? ? 50 E8 ? ? ? ? BA", 2),
			};

			auto graphics_display_jump_table = pattern("83 FB 05 0F 87").get_one();
			auto graphics_enter_new_options = get_pattern("89 86 ? ? ? ? E8 ? ? ? ? 5E", 6 + 5 + 1);
			auto graphics_exit_new_options = get_pattern("8B 86 ? ? ? ? 50 E8 ? ? ? ? E8", 0x17);

			PC_GraphicsOptions_Display_NewOptionsJumpBack = get_pattern("0F BF 55 18 3B DA 75 07");

			for (void* addr : back_locals)
			{
				Patch<uint32_t>(addr, offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_BACK]));
			}

			// Build and inject a new jump table
			Patch<uint8_t>(graphics_display_jump_table.get<void>(2), EntryID::GRAPHICS_NUM - 1);

			void** orgJumpTable = *graphics_display_jump_table.get<void**>(9 + 3);
			static const void* graphics_display_new_jump_table[EntryID::GRAPHICS_NUM] = {
				orgJumpTable[0], orgJumpTable[1], orgJumpTable[2], orgJumpTable[3], &PC_GraphicsOptions_Display_CaseNewOptions,
				&PC_GraphicsOptions_Display_CaseNewOptions, &PC_GraphicsOptions_Display_CaseNewOptions, &PC_GraphicsOptions_Display_CaseNewOptions,
				orgJumpTable[4], orgJumpTable[5]
			};
			Patch(graphics_display_jump_table.get<void**>(9 + 3), &graphics_display_new_jump_table);

			InjectHook(graphics_enter_new_options, PC_GraphicsOptions_Enter_NewOptions, PATCH_JUMP);
			InjectHook(graphics_exit_new_options, PC_GraphicsOptions_Exit_NewOptions, PATCH_JUMP);

			Menus::Patches::ExtraGraphicsOptionsPatched = true;
		}
		TXN_CATCH();
	}
	TXN_CATCH();


	// SP text on the Start screen
	// Make sure this is always the last patch, just in case
	if (HasGraphics && HasCMR3Font) try
	{
		using namespace SPText;

		std::array<void*, 2> texts = {
			get_pattern("50 E8 ? ? ? ? EB 08", 1),
			get_pattern("51 E8 ? ? ? ? 5B 83 C4 08", 1)
		};

		HookEach(texts, InterceptCall);
	}
	TXN_CATCH();
}