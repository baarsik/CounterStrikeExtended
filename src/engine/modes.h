#pragma once

namespace cse_native_software {
constexpr int MaximumWidth = 5120;
constexpr int MaximumHeight = 1600; // The software rasterizer has one scan table entry per row.
}

struct vmode_s
{
	int width;
	int height;
	int bpp;
};
