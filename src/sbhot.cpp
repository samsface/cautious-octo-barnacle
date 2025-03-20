#include "sbhot.h"
#include <godot_cpp/core/math.hpp>

using namespace godot;


void SBHot::_bind_methods() 
{
	ClassDB::bind_method(D_METHOD("add_packed_vector2_arrays"), &SBHot::add_packed_vector2_arrays);
	ClassDB::bind_method(D_METHOD("sampler_tick"), &SBHot::sampler_tick);
}

PackedVector2Array SBHot::add_packed_vector2_arrays(
	PackedVector2Array source, 
	int source_position, 
	PackedVector2Array const& dest, 
	int dest_position, 
	int element_count, 
	float scale_source)
{
    for (int i = 0; i < element_count; i++)
    {
        source.set(source_position + i, source[source_position + i] + dest[dest_position + i] * scale_source);
    }

    return source;
}

Dictionary SBHot::sampler_tick(
	PackedVector2Array out_buffer,
	int out_buffer_cursor,
	PackedVector2Array const& buffer_data,
	float buffer_cursor,
	Vector2i buffer_loop_region,
	int loop_cross_fade_frames,
	float v,
	float playback_step,
	float mod_depth,
	float mod_speed,
	float time,
	float target_pitch_step,
	float glide_speed,
	float fade_in_time,
	float fade_out_time,
	int count_of_frames_to_push) 
{
	for (int i = 0; i < count_of_frames_to_push; i++)
	{
		playback_step = Math::lerp(playback_step, target_pitch_step, glide_speed);

		float pitch_modulation = Math::sin(time * mod_speed) * mod_depth;
		float modulated_step = playback_step * Math::pow(2.0f, (pitch_modulation / 12.0f));

		// We move the buffer cursor on a float so we have to read two frames and lerp them
		// together based on the cursors fraction
		int index = Math::min(static_cast<int>(buffer_cursor), static_cast<int>(buffer_data.size() - 1));
		int next_index = Math::min(static_cast<int>(buffer_cursor + 1), static_cast<int>(buffer_data.size() - 1));
		float fraction = buffer_cursor - static_cast<float>(index);
		Vector2 interpolated_sample = buffer_data[index].lerp(buffer_data[next_index], fraction);

		// Handle looping with crossfade
		if (buffer_loop_region.y > 0)
		{
			if (buffer_cursor >= buffer_loop_region.y)
			{
				buffer_cursor = buffer_loop_region.x + loop_cross_fade_frames + fraction;
			}
			else if (buffer_cursor + loop_cross_fade_frames >= buffer_loop_region.y)
			{
				int distance_to_end_of_loop = loop_cross_fade_frames - (buffer_loop_region.y - buffer_cursor);
				int begin_loop_index = buffer_loop_region.x + distance_to_end_of_loop;

				Vector2 interpolated_sample_from_loop_begin = buffer_data[begin_loop_index].lerp(buffer_data[begin_loop_index + 1], fraction);

				float delta = distance_to_end_of_loop / static_cast<float>(loop_cross_fade_frames);
				interpolated_sample = interpolated_sample.lerp(interpolated_sample_from_loop_begin, delta);
			}
		}


		float distance_to_end_of_buffer = buffer_data.size() - buffer_cursor;
		float fade_out_factor = Math::pow(Math::min(distance_to_end_of_buffer / fade_out_time, 1.0f), 2.0f);
		float fade_in_factor = Math::pow(Math::min(buffer_cursor / fade_in_time, 1.0f), 2.0f);

		out_buffer[i + out_buffer_cursor] += interpolated_sample * v * fade_in_factor * fade_out_factor;

		buffer_cursor += modulated_step;
		time += (1.0f / 44100.0f);
	}

	Dictionary result;
	result["out_buffer"] = out_buffer;
	result["buffer_cursor"] = buffer_cursor;
	result["playback_step"] = playback_step;

	return result;
}
