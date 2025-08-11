#pragma once

#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/variant/dictionary.hpp>

namespace godot 
{
	class SBHot : public RefCounted
	{
		GDCLASS(SBHot, RefCounted)

	protected:
		static void _bind_methods();

	public:
		PackedVector2Array add_packed_vector2_arrays(
			PackedVector2Array source,
			int source_position,
			PackedVector2Array const& dest,
			int dest_position,
			int element_count,
			float scale_source);

		Dictionary sampler_tick(
			PackedVector2Array out_buffer,
			int out_buffer_cursor,
			PackedVector2Array const& buffer_data,
			float buffer_cursor,
			Vector2i buffer_loop_region,
			int loop_cross_fade_frames,
			float volume,
			float playback_step,
			float mod_depth,
			float mod_speed,
			float time,
			float target_pitch_step,
			float glide_speed,
			float fade_in_time,
			float fade_out_time,
			float buffer_length);
	};
}
