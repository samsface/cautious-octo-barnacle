#include "effect_ott_compressor.h"

#include <godot_cpp/core/math.hpp>
#include <godot_cpp/core/property_info.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <cmath>
#include <vector>

using namespace godot;

namespace {

constexpr float kSampleRate = 44100.0f;
constexpr float kMinTime = 0.0001f;
constexpr float kMinRatio = 0.0001f;

float db_to_linear(float p_db) {
	return std::pow(10.0f, p_db * 0.05f);
}

Vector2 abs_vec(const Vector2 &p_value) {
	return Vector2(std::abs(p_value.x), std::abs(p_value.y));
}

Vector2 lerp_vec(const Vector2 &p_from, const Vector2 &p_to, float p_weight) {
	return p_from.lerp(p_to, p_weight);
}

void lowpass_frames(
		const EffectOTTCompressor::Settings &p_settings,
		EffectOTTCompressorState &p_state,
		const Vector2 *p_input,
		Vector2 *p_output,
		int32_t p_frame_count) {
	if (p_frame_count <= 0) {
		return;
	}

	const float rc = 1.0f / (2.0f * Math_PI * p_settings.low_crossover);
	const float dt = 1.0f / kSampleRate;
	const float alpha = dt / (rc + dt);

	p_output[0] = p_input[0] * alpha + p_state.last_low_pass_sample * (1.0f - alpha);
	for (int32_t i = 1; i < p_frame_count; ++i) {
		p_output[i] = p_input[i] * alpha + p_output[i - 1] * (1.0f - alpha);
	}

	p_state.last_low_pass_sample = p_output[p_frame_count - 1];
}

void highpass_frames(
		const EffectOTTCompressor::Settings &p_settings,
		EffectOTTCompressorState &p_state,
		const Vector2 *p_input,
		Vector2 *p_output,
		int32_t p_frame_count) {
	if (p_frame_count <= 0) {
		return;
	}

	const float rc = 1.0f / (2.0f * Math_PI * p_settings.high_crossover);
	const float dt = 1.0f / kSampleRate;
	const float alpha = rc / (rc + dt);

	Vector2 out = alpha * (p_input[0] + p_state.last_hi_pass_sample - p_state.previous_input);
	p_state.previous_input = p_input[0];
	p_output[0] = out;

	for (int32_t i = 1; i < p_frame_count; ++i) {
		out = alpha * (p_input[i] + p_output[i - 1] - p_state.previous_input);
		p_state.previous_input = p_input[i];
		p_output[i] = out;
	}

	p_state.last_hi_pass_sample = p_output[p_frame_count - 1];
}

void midpass_frames(
		const Vector2 *p_dry,
		const Vector2 *p_high,
		const Vector2 *p_low,
		Vector2 *p_mid,
		int32_t p_frame_count) {
	for (int32_t i = 0; i < p_frame_count; ++i) {
		p_mid[i] = p_dry[i] - (p_high[i] + p_low[i]);
	}
}

Vector2 batch_compress(
		Vector2 *p_samples,
		int32_t p_frame_count,
		Vector2 p_last_level,
		float p_time_scale,
		float p_attack,
		float p_release,
		float p_above,
		float p_below,
		float p_ratio_up,
		float p_ratio_down,
		float p_input_gain,
		float p_output_gain) {
	const float safe_attack = Math::max(p_attack, kMinTime);
	const float safe_release = Math::max(p_release, kMinTime);
	const float attack_coeff = std::exp(-1.0f / (Math::max(p_time_scale, kMinTime) * safe_attack * kSampleRate));
	const float release_coeff = std::exp(-1.0f / (Math::max(p_time_scale, kMinTime) * safe_release * kSampleRate));
	const float above_linear = db_to_linear(p_above);
	const float below_linear = db_to_linear(p_below);
	const float input_linear = db_to_linear(p_input_gain);
	const float output_linear = db_to_linear(p_output_gain);
	const float safe_ratio_up = Math::max(p_ratio_up, kMinRatio);
	const float safe_ratio_down = Math::max(p_ratio_down, kMinRatio);

	for (int32_t i = 0; i < p_frame_count; ++i) {
		const Vector2 level = abs_vec(p_samples[i] * input_linear);

		float target_gain_left = 1.0f;
		if (level.x > above_linear) {
			target_gain_left = 1.0f / safe_ratio_down;
		} else if (level.x < below_linear) {
			target_gain_left = 1.0f + (above_linear - level.x) * (safe_ratio_up - 1.0f) / above_linear;
		}

		float target_gain_right = 1.0f;
		if (level.y > above_linear) {
			target_gain_right = 1.0f / safe_ratio_down;
		} else if (level.y < below_linear) {
			target_gain_right = 1.0f + (above_linear - level.y) * (safe_ratio_up - 1.0f) / above_linear;
		}

		const float left_coeff = target_gain_left < p_last_level.x ? attack_coeff : release_coeff;
		const float right_coeff = target_gain_right < p_last_level.y ? attack_coeff : release_coeff;

		p_last_level.x = p_last_level.x * left_coeff + target_gain_left * (1.0f - left_coeff);
		p_last_level.y = p_last_level.y * right_coeff + target_gain_right * (1.0f - right_coeff);

		p_samples[i].x *= p_last_level.x * output_linear;
		p_samples[i].y *= p_last_level.y * output_linear;
	}

	return p_last_level;
}

void bind_float_property(const char *p_name, const char *p_setter, const char *p_getter, const String &p_hint = String()) {
	PropertyHint hint = p_hint.is_empty() ? PROPERTY_HINT_NONE : PROPERTY_HINT_RANGE;
	ClassDB::add_property(
		EffectOTTCompressor::get_class_static(),
		PropertyInfo(Variant::FLOAT, p_name, hint, p_hint),
		p_setter,
		p_getter);
}

} // namespace

void EffectOTTCompressorInstance::_bind_methods() {}

void EffectOTTCompressorInstance::set_owner(EffectOTTCompressor *p_owner) {
	owner = p_owner;
}

void EffectOTTCompressorInstance::_process(const void *p_src_buffer, AudioFrame *p_dst_buffer, int32_t p_frame_count) {
	if (owner == nullptr || p_src_buffer == nullptr || p_dst_buffer == nullptr || p_frame_count <= 0) {
		return;
	}

	const AudioFrame *source = static_cast<const AudioFrame *>(p_src_buffer);
	std::vector<Vector2> input(static_cast<size_t>(p_frame_count));
	std::vector<Vector2> output(static_cast<size_t>(p_frame_count));

	for (int32_t i = 0; i < p_frame_count; ++i) {
		input[static_cast<size_t>(i)] = Vector2(source[i].left, source[i].right);
	}

	EffectOTTCompressor::process_buffer(owner->get_settings(), state, input.data(), output.data(), p_frame_count);

	for (int32_t i = 0; i < p_frame_count; ++i) {
		p_dst_buffer[i].left = output[static_cast<size_t>(i)].x;
		p_dst_buffer[i].right = output[static_cast<size_t>(i)].y;
	}
}

bool EffectOTTCompressorInstance::_process_silence() const {
	return true;
}

void EffectOTTCompressor::_bind_methods() {
	ClassDB::bind_method(D_METHOD("apply", "frames"), &EffectOTTCompressor::apply);
	ClassDB::bind_method(D_METHOD("reset_state"), &EffectOTTCompressor::reset_state);

	ClassDB::bind_method(D_METHOD("set_low_crossover", "value"), &EffectOTTCompressor::set_low_crossover);
	ClassDB::bind_method(D_METHOD("get_low_crossover"), &EffectOTTCompressor::get_low_crossover);
	ClassDB::bind_method(D_METHOD("set_high_crossover", "value"), &EffectOTTCompressor::set_high_crossover);
	ClassDB::bind_method(D_METHOD("get_high_crossover"), &EffectOTTCompressor::get_high_crossover);
	ClassDB::bind_method(D_METHOD("set_mix", "value"), &EffectOTTCompressor::set_mix);
	ClassDB::bind_method(D_METHOD("get_mix"), &EffectOTTCompressor::get_mix);
	ClassDB::bind_method(D_METHOD("set_output", "value"), &EffectOTTCompressor::set_output);
	ClassDB::bind_method(D_METHOD("get_output"), &EffectOTTCompressor::get_output);
	ClassDB::bind_method(D_METHOD("set_time", "value"), &EffectOTTCompressor::set_time);
	ClassDB::bind_method(D_METHOD("get_time"), &EffectOTTCompressor::get_time);

	ClassDB::bind_method(D_METHOD("set_hi_input", "value"), &EffectOTTCompressor::set_hi_input);
	ClassDB::bind_method(D_METHOD("get_hi_input"), &EffectOTTCompressor::get_hi_input);
	ClassDB::bind_method(D_METHOD("set_hi_output", "value"), &EffectOTTCompressor::set_hi_output);
	ClassDB::bind_method(D_METHOD("get_hi_output"), &EffectOTTCompressor::get_hi_output);
	ClassDB::bind_method(D_METHOD("set_hi_attack", "value"), &EffectOTTCompressor::set_hi_attack);
	ClassDB::bind_method(D_METHOD("get_hi_attack"), &EffectOTTCompressor::get_hi_attack);
	ClassDB::bind_method(D_METHOD("set_hi_release", "value"), &EffectOTTCompressor::set_hi_release);
	ClassDB::bind_method(D_METHOD("get_hi_release"), &EffectOTTCompressor::get_hi_release);
	ClassDB::bind_method(D_METHOD("set_hi_above", "value"), &EffectOTTCompressor::set_hi_above);
	ClassDB::bind_method(D_METHOD("get_hi_above"), &EffectOTTCompressor::get_hi_above);
	ClassDB::bind_method(D_METHOD("set_hi_below", "value"), &EffectOTTCompressor::set_hi_below);
	ClassDB::bind_method(D_METHOD("get_hi_below"), &EffectOTTCompressor::get_hi_below);
	ClassDB::bind_method(D_METHOD("set_hi_ratio_up", "value"), &EffectOTTCompressor::set_hi_ratio_up);
	ClassDB::bind_method(D_METHOD("get_hi_ratio_up"), &EffectOTTCompressor::get_hi_ratio_up);
	ClassDB::bind_method(D_METHOD("set_hi_ratio_down", "value"), &EffectOTTCompressor::set_hi_ratio_down);
	ClassDB::bind_method(D_METHOD("get_hi_ratio_down"), &EffectOTTCompressor::get_hi_ratio_down);

	ClassDB::bind_method(D_METHOD("set_mid_input", "value"), &EffectOTTCompressor::set_mid_input);
	ClassDB::bind_method(D_METHOD("get_mid_input"), &EffectOTTCompressor::get_mid_input);
	ClassDB::bind_method(D_METHOD("set_mid_output", "value"), &EffectOTTCompressor::set_mid_output);
	ClassDB::bind_method(D_METHOD("get_mid_output"), &EffectOTTCompressor::get_mid_output);
	ClassDB::bind_method(D_METHOD("set_mid_attack", "value"), &EffectOTTCompressor::set_mid_attack);
	ClassDB::bind_method(D_METHOD("get_mid_attack"), &EffectOTTCompressor::get_mid_attack);
	ClassDB::bind_method(D_METHOD("set_mid_release", "value"), &EffectOTTCompressor::set_mid_release);
	ClassDB::bind_method(D_METHOD("get_mid_release"), &EffectOTTCompressor::get_mid_release);
	ClassDB::bind_method(D_METHOD("set_mid_above", "value"), &EffectOTTCompressor::set_mid_above);
	ClassDB::bind_method(D_METHOD("get_mid_above"), &EffectOTTCompressor::get_mid_above);
	ClassDB::bind_method(D_METHOD("set_mid_below", "value"), &EffectOTTCompressor::set_mid_below);
	ClassDB::bind_method(D_METHOD("get_mid_below"), &EffectOTTCompressor::get_mid_below);
	ClassDB::bind_method(D_METHOD("set_mid_ratio_up", "value"), &EffectOTTCompressor::set_mid_ratio_up);
	ClassDB::bind_method(D_METHOD("get_mid_ratio_up"), &EffectOTTCompressor::get_mid_ratio_up);
	ClassDB::bind_method(D_METHOD("set_mid_ratio_down", "value"), &EffectOTTCompressor::set_mid_ratio_down);
	ClassDB::bind_method(D_METHOD("get_mid_ratio_down"), &EffectOTTCompressor::get_mid_ratio_down);

	ClassDB::bind_method(D_METHOD("set_low_input", "value"), &EffectOTTCompressor::set_low_input);
	ClassDB::bind_method(D_METHOD("get_low_input"), &EffectOTTCompressor::get_low_input);
	ClassDB::bind_method(D_METHOD("set_low_output", "value"), &EffectOTTCompressor::set_low_output);
	ClassDB::bind_method(D_METHOD("get_low_output"), &EffectOTTCompressor::get_low_output);
	ClassDB::bind_method(D_METHOD("set_low_attack", "value"), &EffectOTTCompressor::set_low_attack);
	ClassDB::bind_method(D_METHOD("get_low_attack"), &EffectOTTCompressor::get_low_attack);
	ClassDB::bind_method(D_METHOD("set_low_release", "value"), &EffectOTTCompressor::set_low_release);
	ClassDB::bind_method(D_METHOD("get_low_release"), &EffectOTTCompressor::get_low_release);
	ClassDB::bind_method(D_METHOD("set_low_above", "value"), &EffectOTTCompressor::set_low_above);
	ClassDB::bind_method(D_METHOD("get_low_above"), &EffectOTTCompressor::get_low_above);
	ClassDB::bind_method(D_METHOD("set_low_below", "value"), &EffectOTTCompressor::set_low_below);
	ClassDB::bind_method(D_METHOD("get_low_below"), &EffectOTTCompressor::get_low_below);
	ClassDB::bind_method(D_METHOD("set_low_ratio_up", "value"), &EffectOTTCompressor::set_low_ratio_up);
	ClassDB::bind_method(D_METHOD("get_low_ratio_up"), &EffectOTTCompressor::get_low_ratio_up);
	ClassDB::bind_method(D_METHOD("set_low_ratio_down", "value"), &EffectOTTCompressor::set_low_ratio_down);
	ClassDB::bind_method(D_METHOD("get_low_ratio_down"), &EffectOTTCompressor::get_low_ratio_down);

	bind_float_property("low_crossover", "set_low_crossover", "get_low_crossover", "20,20000,0.1");
	bind_float_property("high_crossover", "set_high_crossover", "get_high_crossover", "20,20000,0.1");
	bind_float_property("mix", "set_mix", "get_mix", "0,1,0.01");
	bind_float_property("output", "set_output", "get_output", "-24,24,0.1");
	bind_float_property("time", "set_time", "get_time", "0.1,4,0.01");

	ADD_GROUP("hi", "hi_");
	bind_float_property("hi_input", "set_hi_input", "get_hi_input", "-24,24,0.1");
	bind_float_property("hi_output", "set_hi_output", "get_hi_output", "-24,24,0.1");
	bind_float_property("hi_attack", "set_hi_attack", "get_hi_attack", "0.001,1,0.0001");
	bind_float_property("hi_release", "set_hi_release", "get_hi_release", "0.001,2,0.0001");
	bind_float_property("hi_above", "set_hi_above", "get_hi_above", "-80,0,0.1");
	bind_float_property("hi_below", "set_hi_below", "get_hi_below", "-80,0,0.1");
	bind_float_property("hi_ratio_up", "set_hi_ratio_up", "get_hi_ratio_up", "1,1000,0.01");
	bind_float_property("hi_ratio_down", "set_hi_ratio_down", "get_hi_ratio_down", "1,1000000,0.1");

	ADD_GROUP("mid", "mid_");
	bind_float_property("mid_input", "set_mid_input", "get_mid_input", "-24,24,0.1");
	bind_float_property("mid_output", "set_mid_output", "get_mid_output", "-24,24,0.1");
	bind_float_property("mid_attack", "set_mid_attack", "get_mid_attack", "0.001,1,0.0001");
	bind_float_property("mid_release", "set_mid_release", "get_mid_release", "0.001,2,0.0001");
	bind_float_property("mid_above", "set_mid_above", "get_mid_above", "-80,0,0.1");
	bind_float_property("mid_below", "set_mid_below", "get_mid_below", "-80,0,0.1");
	bind_float_property("mid_ratio_up", "set_mid_ratio_up", "get_mid_ratio_up", "1,1000,0.01");
	bind_float_property("mid_ratio_down", "set_mid_ratio_down", "get_mid_ratio_down", "1,1000,0.01");

	ADD_GROUP("low", "low_");
	bind_float_property("low_input", "set_low_input", "get_low_input", "-24,24,0.1");
	bind_float_property("low_output", "set_low_output", "get_low_output", "-24,24,0.1");
	bind_float_property("low_attack", "set_low_attack", "get_low_attack", "0.001,1,0.0001");
	bind_float_property("low_release", "set_low_release", "get_low_release", "0.001,2,0.0001");
	bind_float_property("low_above", "set_low_above", "get_low_above", "-80,0,0.1");
	bind_float_property("low_below", "set_low_below", "get_low_below", "-80,0,0.1");
	bind_float_property("low_ratio_up", "set_low_ratio_up", "get_low_ratio_up", "1,1000,0.01");
	bind_float_property("low_ratio_down", "set_low_ratio_down", "get_low_ratio_down", "1,1000,0.01");
}

Ref<AudioEffectInstance> EffectOTTCompressor::_instantiate() {
	Ref<EffectOTTCompressorInstance> instance;
	instance.instantiate();
	instance->set_owner(this);
	return instance;
}

PackedVector2Array EffectOTTCompressor::apply(const PackedVector2Array &frames) {
	PackedVector2Array result;
	const int32_t frame_count = frames.size();
	result.resize(frame_count);
	if (frame_count <= 0) {
		return result;
	}

	std::vector<Vector2> input(static_cast<size_t>(frame_count));
	std::vector<Vector2> output(static_cast<size_t>(frame_count));
	for (int32_t i = 0; i < frame_count; ++i) {
		input[static_cast<size_t>(i)] = frames[i];
	}

	process_buffer(settings, preview_state, input.data(), output.data(), frame_count);

	for (int32_t i = 0; i < frame_count; ++i) {
		result.set(i, output[static_cast<size_t>(i)]);
	}

	return result;
}

void EffectOTTCompressor::reset_state() {
	preview_state = EffectOTTCompressorState();
}

EffectOTTCompressor::Settings EffectOTTCompressor::get_settings() const {
	return settings;
}

void EffectOTTCompressor::process_buffer(
		const Settings &p_settings,
		EffectOTTCompressorState &p_state,
		const Vector2 *p_input,
		Vector2 *p_output,
		int32_t p_frame_count) {
	if (p_frame_count <= 0) {
		return;
	}

	std::vector<Vector2> low_frames(static_cast<size_t>(p_frame_count));
	std::vector<Vector2> mid_frames(static_cast<size_t>(p_frame_count));
	std::vector<Vector2> high_frames(static_cast<size_t>(p_frame_count));

	lowpass_frames(p_settings, p_state, p_input, low_frames.data(), p_frame_count);
	highpass_frames(p_settings, p_state, p_input, high_frames.data(), p_frame_count);
	midpass_frames(p_input, high_frames.data(), low_frames.data(), mid_frames.data(), p_frame_count);

	p_state.low_band = batch_compress(
		low_frames.data(),
		p_frame_count,
		p_state.low_band,
		p_settings.time,
		p_settings.low_attack,
		p_settings.low_release,
		p_settings.low_above,
		p_settings.low_below,
		p_settings.low_ratio_up,
		p_settings.low_ratio_down,
		p_settings.low_input,
		p_settings.low_output);

	p_state.mid_band = batch_compress(
		mid_frames.data(),
		p_frame_count,
		p_state.mid_band,
		p_settings.time,
		p_settings.mid_attack,
		p_settings.mid_release,
		p_settings.mid_above,
		p_settings.mid_below,
		p_settings.mid_ratio_up,
		p_settings.mid_ratio_down,
		p_settings.mid_input,
		p_settings.mid_output);

	p_state.high_band = batch_compress(
		high_frames.data(),
		p_frame_count,
		p_state.high_band,
		p_settings.time,
		p_settings.hi_attack,
		p_settings.hi_release,
		p_settings.hi_above,
		p_settings.hi_below,
		p_settings.hi_ratio_up,
		p_settings.hi_ratio_down,
		p_settings.hi_input,
		p_settings.hi_output);

	const float wet = Math::clamp(p_settings.mix, 0.0f, 1.0f);
	const float output_linear = db_to_linear(p_settings.output);
	for (int32_t i = 0; i < p_frame_count; ++i) {
		const Vector2 compressed = low_frames[static_cast<size_t>(i)] + mid_frames[static_cast<size_t>(i)] + high_frames[static_cast<size_t>(i)];
		p_output[i] = lerp_vec(p_input[i], compressed, wet) * output_linear;
	}
}

float EffectOTTCompressor::get_low_crossover() const {
	return settings.low_crossover;
}

void EffectOTTCompressor::set_low_crossover(float p_value) {
	settings.low_crossover = p_value;
}

float EffectOTTCompressor::get_high_crossover() const {
	return settings.high_crossover;
}

void EffectOTTCompressor::set_high_crossover(float p_value) {
	settings.high_crossover = p_value;
}

float EffectOTTCompressor::get_mix() const {
	return settings.mix;
}

void EffectOTTCompressor::set_mix(float p_value) {
	settings.mix = p_value;
}

float EffectOTTCompressor::get_output() const {
	return settings.output;
}

void EffectOTTCompressor::set_output(float p_value) {
	settings.output = p_value;
}

float EffectOTTCompressor::get_time() const {
	return settings.time;
}

void EffectOTTCompressor::set_time(float p_value) {
	settings.time = p_value;
}

float EffectOTTCompressor::get_hi_input() const {
	return settings.hi_input;
}

void EffectOTTCompressor::set_hi_input(float p_value) {
	settings.hi_input = p_value;
}

float EffectOTTCompressor::get_hi_output() const {
	return settings.hi_output;
}

void EffectOTTCompressor::set_hi_output(float p_value) {
	settings.hi_output = p_value;
}

float EffectOTTCompressor::get_hi_attack() const {
	return settings.hi_attack;
}

void EffectOTTCompressor::set_hi_attack(float p_value) {
	settings.hi_attack = p_value;
}

float EffectOTTCompressor::get_hi_release() const {
	return settings.hi_release;
}

void EffectOTTCompressor::set_hi_release(float p_value) {
	settings.hi_release = p_value;
}

float EffectOTTCompressor::get_hi_above() const {
	return settings.hi_above;
}

void EffectOTTCompressor::set_hi_above(float p_value) {
	settings.hi_above = p_value;
}

float EffectOTTCompressor::get_hi_below() const {
	return settings.hi_below;
}

void EffectOTTCompressor::set_hi_below(float p_value) {
	settings.hi_below = p_value;
}

float EffectOTTCompressor::get_hi_ratio_up() const {
	return settings.hi_ratio_up;
}

void EffectOTTCompressor::set_hi_ratio_up(float p_value) {
	settings.hi_ratio_up = p_value;
}

float EffectOTTCompressor::get_hi_ratio_down() const {
	return settings.hi_ratio_down;
}

void EffectOTTCompressor::set_hi_ratio_down(float p_value) {
	settings.hi_ratio_down = p_value;
}

float EffectOTTCompressor::get_mid_input() const {
	return settings.mid_input;
}

void EffectOTTCompressor::set_mid_input(float p_value) {
	settings.mid_input = p_value;
}

float EffectOTTCompressor::get_mid_output() const {
	return settings.mid_output;
}

void EffectOTTCompressor::set_mid_output(float p_value) {
	settings.mid_output = p_value;
}

float EffectOTTCompressor::get_mid_attack() const {
	return settings.mid_attack;
}

void EffectOTTCompressor::set_mid_attack(float p_value) {
	settings.mid_attack = p_value;
}

float EffectOTTCompressor::get_mid_release() const {
	return settings.mid_release;
}

void EffectOTTCompressor::set_mid_release(float p_value) {
	settings.mid_release = p_value;
}

float EffectOTTCompressor::get_mid_above() const {
	return settings.mid_above;
}

void EffectOTTCompressor::set_mid_above(float p_value) {
	settings.mid_above = p_value;
}

float EffectOTTCompressor::get_mid_below() const {
	return settings.mid_below;
}

void EffectOTTCompressor::set_mid_below(float p_value) {
	settings.mid_below = p_value;
}

float EffectOTTCompressor::get_mid_ratio_up() const {
	return settings.mid_ratio_up;
}

void EffectOTTCompressor::set_mid_ratio_up(float p_value) {
	settings.mid_ratio_up = p_value;
}

float EffectOTTCompressor::get_mid_ratio_down() const {
	return settings.mid_ratio_down;
}

void EffectOTTCompressor::set_mid_ratio_down(float p_value) {
	settings.mid_ratio_down = p_value;
}

float EffectOTTCompressor::get_low_input() const {
	return settings.low_input;
}

void EffectOTTCompressor::set_low_input(float p_value) {
	settings.low_input = p_value;
}

float EffectOTTCompressor::get_low_output() const {
	return settings.low_output;
}

void EffectOTTCompressor::set_low_output(float p_value) {
	settings.low_output = p_value;
}

float EffectOTTCompressor::get_low_attack() const {
	return settings.low_attack;
}

void EffectOTTCompressor::set_low_attack(float p_value) {
	settings.low_attack = p_value;
}

float EffectOTTCompressor::get_low_release() const {
	return settings.low_release;
}

void EffectOTTCompressor::set_low_release(float p_value) {
	settings.low_release = p_value;
}

float EffectOTTCompressor::get_low_above() const {
	return settings.low_above;
}

void EffectOTTCompressor::set_low_above(float p_value) {
	settings.low_above = p_value;
}

float EffectOTTCompressor::get_low_below() const {
	return settings.low_below;
}

void EffectOTTCompressor::set_low_below(float p_value) {
	settings.low_below = p_value;
}

float EffectOTTCompressor::get_low_ratio_up() const {
	return settings.low_ratio_up;
}

void EffectOTTCompressor::set_low_ratio_up(float p_value) {
	settings.low_ratio_up = p_value;
}

float EffectOTTCompressor::get_low_ratio_down() const {
	return settings.low_ratio_down;
}

void EffectOTTCompressor::set_low_ratio_down(float p_value) {
	settings.low_ratio_down = p_value;
}
