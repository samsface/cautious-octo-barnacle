#pragma once

#include <godot_cpp/classes/audio_stream_player.hpp>

namespace godot {
class SBHot : public RefCounted {
	GDCLASS(SBHot, RefCounted)

protected:
	static void _bind_methods();

public:
	PackedVector2Array add_packed_vector2_arrays(PackedVector2Array source, int source_position, PackedVector2Array const& dest, int dest_position, int element_count, float scale_source);
};
};
