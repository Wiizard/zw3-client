#pragma once

namespace Components::Remix
{
	struct MarkerStats
	{
		int markers = 0;
		int drawnMarkers = 0;
	};

	void DrawMapMarkers(IDirect3DDevice9* device);

	void ReleaseMarkers();

	MarkerStats GetMarkerStats();
}
