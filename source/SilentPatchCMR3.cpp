#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <timeapi.h>

#include "Utils/MemoryMgr.h"
#include "Utils/Patterns.h"

#include "Graphics.h"
#include "Menus.h"

#include <d3d9.h>
#include <wil/com.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <vector>

#include <DirectXMath.h>

#pragma comment(lib, "winmm.lib")

char GetRegistryEntryChar(LPCWSTR subKey, LPCWSTR valueName)
{
	wchar_t buf[12];
	DWORD cbSize = sizeof(buf);
	RegGetValueW(HKEY_LOCAL_MACHINE, subKey, valueName, RRF_RT_REG_SZ|RRF_ZEROONFAILURE, nullptr, buf, &cbSize);
	return static_cast<char>(buf[0]);
}

namespace Localization
{
	struct LangFile
	{
		char m_magic[4]; // LANG
		uint32_t m_version;
		uint32_t m_numFiles;
		const char* m_texts[1]; // VLA
	};
	static LangFile** gActiveLanguage;
	const char* GetLocalizedString_BoundsCheck(uint32_t ID)
	{
		// Original code with bounds check added
		const LangFile* language = *gActiveLanguage;
		if (ID < language->m_numFiles)
		{
			return language->m_texts[ID];
		}

		// All strings that were hardcoded in the English release, but translated in the Polish release
		static const std::map<uint32_t, const char*> newLocalizedStrings = {
			{ 994, "NA" },
			{ 1004, "Return" }, { 1005, "Esc" }, { 1006, "BackSpc" }, { 1007, "TAB" },
			{ 1008, "F1" }, { 1009, "F2" }, { 1010, "F3" }, { 1011, "F4" },
			{ 1012, "F5" }, { 1013, "F6" }, { 1014, "F7" }, { 1015, "F8" },
			{ 1016, "F9" }, { 1017, "F10" }, { 1018, "F11" }, { 1019, "F12" },
			{ 1020, "PrintScrn" }, { 1021, "ScrollLock" }, { 1022, "Pause" }, { 1023, "Insert" },
			{ 1024, "Home" }, { 1025, "PgUp" }, { 1026, "Del" }, { 1027, "End" },
			{ 1028, "PgDn" }, { 1029, "Right" }, { 1030, "Left" }, { 1031, "Up" },
			{ 1032, "Down" }, { 1033, "[?]" },
		};
		auto it = newLocalizedStrings.find(ID);
		if (it != newLocalizedStrings.end())
		{
			return it->second;
		}

		return "";
	}

	uint32_t (*orgGetLanguageIDByCode)(char code);
	uint32_t GetCurrentLanguageID_Patched()
	{
		return orgGetLanguageIDByCode(GetRegistryEntryChar(L"SOFTWARE\\CODEMASTERS\\Colin McRae Rally 3", L"LANGUAGE"));
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
}

void (*orgCalculateSunColorFromOcclusion)(float val);
void CalculateSunColorFromOcclusion_Clamped(float val)
{
	orgCalculateSunColorFromOcclusion(std::clamp(val, 0.0f, 1.0f));
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

	static DWORD WINAPI timeGetTime_NOP()
	{
		return 0;
	}
	static const auto pTimeGetTime_NOP = &timeGetTime_NOP;

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
	struct MenuResolutionEntry
	{
		char m_displayText[64];
		int m_width;
		int m_height;
		int m_refreshRate;
		D3DFORMAT m_format;
		D3DFORMAT m_backBufferFormat;
		D3DFORMAT field_54;
		int field_58;
		int field_5C;
		int field_60;
		int field_64;
	};
	static_assert(sizeof(MenuResolutionEntry) == 104, "Wrong size: MenuResolutionEntry");

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

	struct BlitLine2D_G
	{
		uint32_t color[2];
		float X[2];
		float Y[2];
		float Z[2];
	};
	static_assert(sizeof(BlitLine2D_G) == 32, "Wrong size: BlitLine2D_G");

	struct BlitLine3D_G
	{
		float X1, Y1, Z1, W1;
		float X2, Y2, Z2, W2;
		uint32_t color[2];
		float _gap3[2];
	};
	static_assert(sizeof(BlitLine3D_G) == 48, "Wrong size: BlitLine3D_G");

	struct BlitTri2D_G
	{
		uint32_t color[3];
		float X[3];
		float Y[3];
		float Z;
	};
	static_assert(sizeof(BlitTri2D_G) == 40, "Wrong size: BlitTri2D_G");

	struct BlitTri3D_G
	{
		float X1, Y1, Z1, W1;
		float X2, Y2, Z2, W2;
		float X3, Y3, Z3, W3;
		uint32_t color[3];
		float _gap2;

	};
	static_assert(sizeof(BlitTri3D_G) == 64, "Wrong size: BlitTri3D_G");

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
		return 1.0f / ((c2 - c1) * GetResolutionWidth());
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
	__declspec(naked) void Core_Blitter2D_Rect2D_G_Original(float*, uint32_t)
	{
		__asm
		{
			sub		esp, 028h
			mov		eax, dword ptr [dword_936C0C]
			mov		eax, dword ptr [eax]
			jmp		[Core_Blitter2D_Rect2D_G_JumpBack]
		}
	}

	void Core_Blitter2D_Rect2D_G_HalfPixel(float* verts, uint32_t numVerts)
	{
		for (uint32_t i = 0; i < numVerts; i++)
		{
			float* vert = &verts[9 * i];

			vert[4] = OffsetTexel(vert[4]);
			vert[5] = OffsetTexel(vert[5]);
			vert[6] = OffsetTexel(vert[6]);
			vert[7] = OffsetTexel(vert[7]);
		}
		Core_Blitter2D_Rect2D_G_Original(verts, numVerts);
	}

	static void* Core_Blitter2D_Rect2D_GT_JumpBack;
	__declspec(naked) void Core_Blitter2D_Rect2D_GT_Original(float*, uint32_t)
	{
		__asm
		{
			sub		esp, 028h
			mov		eax, dword ptr [dword_936C0C]
			mov		eax, dword ptr [eax]
			jmp		[Core_Blitter2D_Rect2D_GT_JumpBack]
		}
	}

	void Core_Blitter2D_Rect2D_GT_HalfPixel(float* verts, uint32_t numRectangles)
	{
		for (uint32_t i = 0; i < numRectangles; i++)
		{
			float* vert = &verts[13 * i];

			vert[8] = OffsetTexel(vert[8]);
			vert[9] = OffsetTexel(vert[9]);
			vert[10] = OffsetTexel(vert[10]);
			vert[11] = OffsetTexel(vert[11]);
		}
		Core_Blitter2D_Rect2D_GT_Original(verts, numRectangles);
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
			const float targetThickness = GetResolutionHeight() / 480.0f;

			BlitTri2D_G* tris = reinterpret_cast<BlitTri2D_G*>(buf);
			BlitTri2D_G* currentTri = tris;
			for (uint32_t i = 0; i < numLines; i++)
			{
				XMVECTOR quad[4];
				ComputeScreenQuad(identityMatrix, identityMatrix, nullptr, quad, XMVectorSet(lines[i].X[0], lines[i].Y[0], 0.0f, 0.0f),
													XMVectorSet(lines[i].X[1], lines[i].Y[1], 0.0f, 0.0f), targetThickness);

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

			const float targetThickness = GetResolutionHeight() / 480.0f;

			BlitTri3D_G* tris = reinterpret_cast<BlitTri3D_G*>(buf);
			BlitTri3D_G* currentTri = tris;
			for (uint32_t i = 0; i < numLines; i++)
			{
				XMVECTOR quad[4];
				ComputeScreenQuad(invViewMatrix, viewMatrix, reinterpret_cast<const XMFLOAT4X4*>(pProjectionMatrix), quad,
					XMLoadFloat4(reinterpret_cast<XMFLOAT4*>(&lines[i].X1)), XMLoadFloat4(reinterpret_cast<XMFLOAT4*>(&lines[i].X2)), targetThickness);

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

void OnInitializeHook()
{
	static_assert(std::string_view(__FUNCSIG__).find("__stdcall") != std::string_view::npos, "This codebase must default to __stdcall, please change your compilation settings.");

	using namespace Memory;
	using namespace hook::txn;

	auto Protect = ScopedUnprotect::UnprotectSectionOrFullModule( GetModuleHandle( nullptr ), ".text" );

	// Globally replace timeGetTime with a QPC-based timer
	Timers::Setup();
	Timers::RedirectImports();
	// Added range check for localizations + new strings added in the Polish release
	try
	{
		using namespace Localization;

		auto get_localized_string = ReadCallFrom(get_pattern("E8 ? ? ? ? 8D 56 E2"));

		gActiveLanguage = *get_pattern<LangFile**>("89 0D ? ? ? ? B8 ? ? ? ? 5E", 2);
		InjectHook(get_localized_string, GetLocalizedString_BoundsCheck, PATCH_JUMP);

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
	try
	{
		extern void (*orgMenu_SetUpEntries)(int);

		auto menus = *get_pattern<MenuDefinition*>("C7 05 ? ? ? ? ? ? ? ? 89 3D ? ? ? ? 89 35", 2+4);
		void* update_menu_entries[] = {
			get_pattern("6A 01 E8 ? ? ? ? 5F 5E C2 08 00", 2),
			get_pattern("E8 ? ? ? ? 6A 00 E8 ? ? ? ? E8 ? ? ? ? C2 08 00", 5 + 2),
			get_pattern("E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? 50 E8 ? ? ? ? 50"),
		};

		auto set_focus_on_lang_screen = get_pattern("89 0D ? ? ? ? 66 89 35", 6);

		gMenus = menus;

		ReadCall(update_menu_entries[0], orgMenu_SetUpEntries);
		for (void* addr : update_menu_entries)
		{
			InjectHook(addr, Menu_SetUpEntries_Patched);
		}

		// Don't override focus every time menus are updated
		Nop(set_focus_on_lang_screen, 14);
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

		ReadCall(calculate_color_from_occlusion, orgCalculateSunColorFromOcclusion);
		InjectHook(calculate_color_from_occlusion, CalculateSunColorFromOcclusion_Clamped);
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

		void* noop_gettime[] = {
			get_pattern("74 1D FF 15", 4),
			get_pattern("FF 15 ? ? ? ? 8B 0D ? ? ? ? 8B 15 ? ? ? ? 2B C1", 2)
		};

		auto gettime_reset = get_pattern("FF 15 ? ? ? ? 8B F8 8B D0 8B C8", 2);
		auto gettime_update = get_pattern("FF 15 ? ? ? ? A3 ? ? ? ? E8 ? ? ? ? 83 F8 01 5D", 2);

		for (void* addr : noop_gettime)
		{
			Patch(addr, &pTimeGetTime_NOP);
		}

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
	try
	{
		using namespace HalfPixel;

		auto Blitter2D_Rect2D_G = pattern("76 39 8B 7C 24 3C").get_one();
		auto Blitter2D_Rect2D_GT = reinterpret_cast<intptr_t>(ReadCallFrom(get_pattern("E8 ? ? ? ? A1 ? ? ? ? 45 83 C3 40")));
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


	// Better widescreen support
	try
	{
		auto get_resolution_width = ReadCallFrom(get_pattern("E8 ? ? ? ? 33 F6 89 44 24 28"));
		auto get_resolution_height = ReadCallFrom(get_pattern("E8 ? ? ? ? 8D 4C 6D 00"));
		auto get_num_players = ReadCallFrom(get_pattern("E8 ? ? ? ? 88 44 24 23"));

		GetResolutionWidth = reinterpret_cast<decltype(GetResolutionWidth)>(get_resolution_width);
		GetResolutionHeight = reinterpret_cast<decltype(GetResolutionHeight)>(get_resolution_height);
		GetNumPlayers = reinterpret_cast<decltype(GetNumPlayers)>(get_num_players);
		gDefaultViewport = *get_pattern<D3DViewport**>("A1 ? ? ? ? D9 44 24 08 D9 58 1C", 1);

		// Viewports
		try
		{
			auto set_aspect_ratio = get_pattern("8B 44 24 04 85 C0 75 1C");
			auto viewports = *get_pattern<D3DViewport**>("8B 35 ? ? ? ? 8B C6 5F", 2);

			auto set_viewport = ReadCallFrom(get_pattern("E8 ? ? ? ? 8B 4C 24 0C 8B 56 60"));
			auto set_aspect_ratios = get_pattern("83 EC 64 56 E8");

			auto recalc_fov = pattern("D8 0D ? ? ? ? DA 74 24 30 ").get_one();

			D3DViewport_SetAspectRatio = reinterpret_cast<decltype(D3DViewport_SetAspectRatio)>(set_aspect_ratio);
			gViewports = viewports;

			InjectHook(set_viewport, D3DViewport_Set, PATCH_JUMP);
			InjectHook(set_aspect_ratios, Graphics_Viewports_SetAspectRatios, PATCH_JUMP);

			// Change the horizontal FOV instead of vertical when refreshing viewports
			// fidiv -> fidivr and m_vertFov -> m_horFov
			static const float f4By3 = 3.0f/4.0f;
			Patch(recalc_fov.get<void>(2), &f4By3);
			Patch<uint8_t>(recalc_fov.get<void>(6+1), 0x7C);
			Patch<uint8_t>(recalc_fov.get<void>(10+2), 0x1C);
		}
		TXN_CATCH();

		// UI
		try
		{
			using namespace Graphics::Patches;

			extern void (*orgD3D_Initialise)(void* param);
			extern void (*orgD3D_AfterReinitialise)(void* param);
			extern void (*orgSetMovieDirectory)(const char* path);

			HandyFunction_Draw2DBox = reinterpret_cast<decltype(HandyFunction_Draw2DBox)>(get_pattern("6A 01 E8 ? ? ? ? 6A 05 E8 ? ? ? ? 6A 06 E8 ? ? ? ? DB 44 24 5C", -5));
			CMR3Font_BlitText = reinterpret_cast<decltype(CMR3Font_BlitText)>(get_pattern("8B 74 24 30 8B 0D", -6));

			Keyboard_DrawTextEntryBox = reinterpret_cast<decltype(Keyboard_DrawTextEntryBox)>(get_pattern("56 3B C3 57 0F 84 ? ? ? ? DB 84 24", -0xD));

			Core_Blitter2D_Rect2D_G = reinterpret_cast<decltype(Core_Blitter2D_Rect2D_G)>(ReadCallFrom(get_pattern("E8 ? ? ? ? 8D 44 24 68")));
			Core_Blitter2D_Line2D_G = reinterpret_cast<decltype(Core_Blitter2D_Line2D_G)>(get_pattern("F7 D8 57", -0x14));
			Core_Blitter2D_Rect2D_GT = reinterpret_cast<decltype(Core_Blitter2D_Rect2D_GT)>(ReadCallFrom(get_pattern("DD D8 E8 ? ? ? ? 8B 7C 24 30", 2)));

			auto initialise = get_pattern("E8 ? ? ? ? 8B 54 24 24 89 5C 24 18");
			auto reinitialise = get_pattern("E8 ? ? ? ? 8B 15 ? ? ? ? A1 ? ? ? ? 8B 0D");

			auto osd_codriver_get_ar = get_pattern("E8 ? ? ? ? 8B 94 24 ? ? ? ? 8B 84 24 ? ? ? ? 8B 0D");

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

			UI_resolutionWidthMult = *get_pattern<float*>("DF 6C 24 18 D8 0D ? ? ? ? D9 5C 24 38", 4+2);
			UI_RightAlignElements.emplace_back(std::in_place_type<FloatPatch>, *get_pattern<float*>("D8 3D ? ? ? ? D9 5C 24 18", 2), 640.0f);

			UI_RightAlignElements.emplace_back(std::in_place_type<Int32Patch>, get_pattern<int32_t>("B9 ? ? ? ? D9 5C 24 0C", 1), 640);
			UI_RightAlignElements.emplace_back(std::in_place_type<Int32Patch>, get_pattern<int32_t>("B9 ? ? ? ? 8D 3C B6", 1), 640);
			UI_RightAlignElements.emplace_back(std::in_place_type<FloatPatch>, *get_pattern<float*>("D8 E2 D8 0D ? ? ? ? D8 0D", 2+2), 590.0f);

			UI_CenteredElements.emplace_back(std::in_place_type<Int32Patch>, get_pattern<int32_t>("B8 ? ? ? ? 6A 01 6A 00 68", 1), 292);
			UI_CoutdownPosXVertical[0] = get_pattern<int32_t>("B8 ? ? ? ? B9 ? ? ? ? EB 1F E8", 1);
			UI_CoutdownPosXVertical[1] = get_pattern<int32_t>("76 0C B8 ? ? ? ? B9", 2+1);

			UI_MenuBarTextDrawLimit = get_pattern<int32_t>("C7 44 24 2C 01 00 00 00 81 FD", 8+2);

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

			// Movie rendering
			auto movie_rect = pattern("C7 05 ? ? ? ? 00 00 00 BF C7 05 ? ? ? ? 00 00 00 BF").get_one();
			auto movie_name_setdir = get_pattern("E8 ? ? ? ? E8 ? ? ? ? 85 C0 A1 ? ? ? ? 0F 95 C3");
			UI_MovieX1 = movie_rect.get<float>(6);
			UI_MovieY1 = movie_rect.get<float>(10 + 6);
			UI_MovieX2 = movie_rect.get<float>(20 + 6);
			UI_MovieY2 = movie_rect.get<float>(30 + 6);

			orgOSDData = *osd_data.get<OSD_Data*>(2+3);
			orgOSDData2 = *osd_data.get<OSD_Data2*>(27+3);

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

			ReadCall(initialise, orgD3D_Initialise);
			InjectHook(initialise, D3D_Initialise_RecalculateUI);

			ReadCall(reinitialise, orgD3D_AfterReinitialise);
			InjectHook(reinitialise, D3D_AfterReinitialise_RecalculateUI);

			InjectHook(osd_codriver_get_ar, D3DViewport_GetAspectRatioForCoDriver);

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
}