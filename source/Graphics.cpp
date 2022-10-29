#include "Graphics.h"

void Graphics_Viewports_SetAspectRatios()
{
	const float Width = static_cast<float>(GetResolutionWidth());
	const float Height = static_cast<float>(GetResolutionHeight());
	const float InvAR = Height / Width;
	const float TargetAR = 4.0f / 3.0f;

	// TODO: Adjust FOV here
	auto setAspectRatio = [](D3DViewport* viewport, float hfov, float vfov)
	{
		const float FOV = 1.0f;
		D3DViewport_SetAspectRatio(viewport, hfov * FOV, vfov * FOV);
	};

	// Main viewport
	setAspectRatio(gViewports[0], TargetAR * InvAR, TargetAR);

	// Horizontal splitscreen - top
	setAspectRatio(gViewports[1], TargetAR * InvAR, TargetAR * 2.0f);

	// Horizontal splitscreen - bottom
	{
		const float HeightSplit = Height + 50.0f;
		const float InvARSplit = HeightSplit / Width;
		setAspectRatio(gViewports[2], TargetAR * InvARSplit, TargetAR * 2.0f);
	}

	// Vertical splitscreen - left
	{
		const float WidthSplit = Width - 50.0f;
		const float InvARSplit = Height / WidthSplit;
		setAspectRatio(gViewports[3], TargetAR * InvARSplit * 2.0f, TargetAR);
	}

	// Vertical splitscreen - right
	setAspectRatio(gViewports[4], TargetAR * InvAR * 2.0f, TargetAR);

	// 4p splitscreen - top left, top right
	setAspectRatio(gViewports[5], TargetAR * InvAR, TargetAR);
	setAspectRatio(gViewports[6], TargetAR * InvAR, TargetAR);

	// 4p splitscreen - bottom left, bottom right
	{
		const float HeightSplit = Height + 50.0f;
		const float InvARSplit = HeightSplit / Width;
		setAspectRatio(gViewports[7], TargetAR * InvARSplit, TargetAR);
		setAspectRatio(gViewports[8], TargetAR * InvARSplit, TargetAR);
	}
}
