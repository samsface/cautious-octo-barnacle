#include "sbhot.h"
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

// Hermite interpolation for Vector2 audio samples
Vector2 hermite4(const Vector2& y0, const Vector2& y1, const Vector2& y2, const Vector2& y3, float mu) {
	float mu2 = mu * mu;
	float mu3 = mu2 * mu;

	Vector2 m0 = (y2 - y0) * 0.5f;
	Vector2 m1 = (y3 - y1) * 0.5f;

	Vector2 a0 = 2.0f * (y1 - y2) + m0 + m1;
	Vector2 a1 = -3.0f * (y1 - y2) - m0 - m0 - m1;
	Vector2 a2 = m0;
	Vector2 a3 = y1;

	return a0 * mu3 + a1 * mu2 + a2 * mu + a3;
}

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
	float volume,
	float playback_step,
	float mod_depth,
	float mod_speed,
	float time,
	float target_pitch_step,
	float glide_speed,
	float fade_in_time,
	float fade_out_time,
	float buffer_length) 
{
	bool done{};

	for (int i = out_buffer_cursor; i < out_buffer.size(); i++)
	{
		float time_to_end_of_buffer = buffer_length - time;

		if (time_to_end_of_buffer <= 0.0)
		{
			done = true;
			break;
		}

		// We move the buffer cursor on a float so we have to read two frames and lerp them
		// together based on the cursors fraction
		int index = static_cast<int>(Math::floor(buffer_cursor));
		float fraction = buffer_cursor - static_cast<float>(index);
		//Vector2 interpolated_sample = buffer_data[index].lerp(buffer_data[index + 1], fraction);

		if (index >= buffer_data.size())
		{
			done = true;
			break;
		}

		// this sounds way better than lerping for bigger pitch shifts
		int i0 = Math::max(index - 1, 0);
		int i1 = index;
		int i2 = Math::min(index + 1, static_cast<int>(buffer_data.size()) - 1);
		int i3 = Math::min(index + 2, static_cast<int>(buffer_data.size()) - 1);

		Vector2 interpolated_sample = hermite4(
			buffer_data[i0],
			buffer_data[i1],
			buffer_data[i2],
			buffer_data[i3],
			fraction
		);

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


		float fade_out_factor = Math::clamp(time_to_end_of_buffer / fade_out_time, 0.0f, 1.0f);
		fade_out_factor = Math::pow(fade_out_factor, 2.0f);

		//float fade_in_factor = Math::pow(Math::min(buffer_cursor / fade_in_time, 1.0f), 2.0f);
		float fade_in_factor = 1.0;

		interpolated_sample = interpolated_sample * volume * fade_in_factor * fade_out_factor;

		out_buffer[i] += interpolated_sample;

		playback_step = Math::lerp(playback_step, target_pitch_step, glide_speed);

		float pitch_modulation = Math::sin(time * mod_speed) * mod_depth;
		float modulated_step = playback_step * Math::pow(2.0f, (pitch_modulation / 12.0f));

		buffer_cursor += modulated_step;
		time += (1.0f / 44100.0f);
			
		//if (i % 50 == 0)
		//	UtilityFunctions::print(interpolated_sample.x);
	}

	Dictionary result;
	result["out_buffer"] = out_buffer;
	result["buffer_cursor"] = buffer_cursor;
	result["playback_step"] = playback_step;
	result["time"] = time;
	result["done"] = done;

	return result;
}
