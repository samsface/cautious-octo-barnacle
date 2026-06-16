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
namespace {
constexpr float kSampleRate = 44100.0f;
double wrap_cursor_to_loop(double cursor, int loop_start, int loop_end) {
	const int loop_length = loop_end - loop_start;
	if (loop_length <= 0) {
		return cursor;
	}
	const double loop_start_f = static_cast<double>(loop_start);
	const double loop_end_f = static_cast<double>(loop_end);
	const double loop_length_f = static_cast<double>(loop_length);
	if (cursor < loop_start_f) {
		return cursor;
	}
	while (cursor >= loop_end_f) {
		cursor -= loop_length_f;
	}
	return cursor;
}
Vector2 sample_buffer_at(const PackedVector2Array &buffer_data, double cursor, Vector2i buffer_loop_region) {
	if (buffer_data.is_empty()) {
		return Vector2();
	}
	const int loop_start = buffer_loop_region.x;
	const int loop_end = buffer_loop_region.y;
	if (loop_end > loop_start) {
		cursor = wrap_cursor_to_loop(cursor, loop_start, loop_end);
	}
	const int max_index = static_cast<int>(buffer_data.size()) - 1;
	if (cursor <= 0.0f) {
		return buffer_data[0];
	}
	if (cursor >= static_cast<double>(max_index)) {
		return buffer_data[max_index];
	}
	const int index = static_cast<int>(Math::floor(cursor));
	const double fraction = Math::clamp(cursor - static_cast<double>(index), 0.0, 1.0);
	const int next_index = index + 1;
	return buffer_data[index].lerp(buffer_data[next_index], fraction);
}
float ease_squared(double value) {
	return value * value;
}
} // namespace
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
	double buffer_cursor,
	Vector2i buffer_loop_region,
	int loop_cross_fade_frames,
	double volume,
	double playback_step,
	double mod_depth,
	double mod_speed,
	double time,
	double target_pitch_step,
	double glide_speed,
	double fade_in_time,
	double fade_out_time,
	double buffer_length)
{
	bool done{};
	const bool has_loop = buffer_loop_region.y > buffer_loop_region.x;
	const double loop_start = static_cast<double>(buffer_loop_region.x);
	const double loop_end = static_cast<double>(buffer_loop_region.y);
	const double crossfade_frames = static_cast<double>(Math::max(loop_cross_fade_frames, 0));
	for (int i = out_buffer_cursor; i < out_buffer.size(); i++)
	{
		const double time_to_end_of_buffer = static_cast<double>(buffer_length) - time;
		if (buffer_length > 0.0f && time_to_end_of_buffer <= 0.0f) {
			done = true;
			break;
		}
		if (buffer_data.is_empty()) {
			done = true;
			break;
		}
		if (has_loop) {
			buffer_cursor = wrap_cursor_to_loop(buffer_cursor, buffer_loop_region.x, buffer_loop_region.y);
		}
		if (!has_loop) {
			if (buffer_cursor < 0.0f || buffer_cursor >= static_cast<double>(buffer_data.size())) {
				done = true;
				break;
			}
		}
		Vector2 interpolated_sample = sample_buffer_at(buffer_data, buffer_cursor, buffer_loop_region);
		if (has_loop && crossfade_frames > 0.0f && buffer_cursor >= loop_end - crossfade_frames) {
			const double fade_progress = Math::clamp((buffer_cursor - (loop_end - crossfade_frames)) / crossfade_frames, 0.0, 1.0);
			const double looped_cursor = loop_start + (buffer_cursor - (loop_end - crossfade_frames));
			const Vector2 looped_sample = sample_buffer_at(buffer_data, looped_cursor, buffer_loop_region);
			interpolated_sample = interpolated_sample.lerp(looped_sample, fade_progress);
		}
		double fade_in_factor = 1.0f;
		if (fade_in_time > 0.0f) {
			fade_in_factor = ease_squared(Math::clamp(static_cast<double>(time / fade_in_time), 0.0, 1.0));
		}
		double fade_out_factor = 1.0f;
		if (fade_out_time > 0.0f && buffer_length > 0.0f) {
			fade_out_factor = ease_squared(Math::clamp(static_cast<double>(time_to_end_of_buffer / fade_out_time), 0.0, 1.0));
		}
		interpolated_sample *= volume * fade_in_factor * fade_out_factor;
		out_buffer[i] += interpolated_sample;
		const double glide_amount = Math::clamp(glide_speed, 0.0, 1.0);
		playback_step = Math::lerp(playback_step, target_pitch_step, glide_amount);
		const double pitch_modulation = Math::sin(static_cast<double>(time * mod_speed)) * mod_depth;
		const double modulated_step = playback_step * Math::pow(2.0, pitch_modulation / 12.0);
		buffer_cursor += modulated_step;
		if (has_loop) {
			buffer_cursor = wrap_cursor_to_loop(buffer_cursor, buffer_loop_region.x, buffer_loop_region.y);
		}
		time += (1.0 / kSampleRate);
	}
	Dictionary result;
	result["out_buffer"] = out_buffer;
	result["buffer_cursor"] = buffer_cursor;
	result["playback_step"] = playback_step;
	result["time"] = static_cast<double>(time);
	result["done"] = done;
	return result;
}

