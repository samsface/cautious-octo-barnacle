#pragma once

#include <godot_cpp/classes/audio_effect.hpp>
#include <godot_cpp/classes/audio_effect_instance.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/vector2.hpp>

namespace godot {

class EffectOTTCompressor;

struct EffectOTTCompressorState {
	Vector2 low_band = Vector2();
	Vector2 mid_band = Vector2();
	Vector2 high_band = Vector2();
	Vector2 last_low_pass_sample = Vector2();
	Vector2 last_hi_pass_sample = Vector2();
	Vector2 previous_input = Vector2();
};

class EffectOTTCompressorInstance : public AudioEffectInstance {
	GDCLASS(EffectOTTCompressorInstance, AudioEffectInstance)

	EffectOTTCompressor *owner = nullptr;
	EffectOTTCompressorState state;

protected:
	static void _bind_methods();

public:
	void set_owner(EffectOTTCompressor *p_owner);

	void _process(const void *p_src_buffer, AudioFrame *p_dst_buffer, int32_t p_frame_count) override;
	bool _process_silence() const override;
};

class EffectOTTCompressor : public AudioEffect {
	GDCLASS(EffectOTTCompressor, AudioEffect)

public:
	struct Settings {
		float low_crossover = 88.3f;
		float high_crossover = 2500.0f;
		float mix = 0.0f;
		float output = 0.0f;
		float time = 1.0f;

		float hi_input = 5.2f;
		float hi_output = 10.3f;
		float hi_attack = 0.0135f;
		float hi_release = 0.132f;
		float hi_above = -35.0f;
		float hi_below = -40.8f;
		float hi_ratio_up = 4.17f;
		float hi_ratio_down = 999999.0f;

		float mid_input = 5.2f;
		float mid_output = 5.7f;
		float mid_attack = 0.0224f;
		float mid_release = 0.282f;
		float mid_above = -30.2f;
		float mid_below = -41.8f;
		float mid_ratio_up = 4.17f;
		float mid_ratio_down = 66.7f;

		float low_input = 5.2f;
		float low_output = 10.3f;
		float low_attack = 0.0478f;
		float low_release = 0.282f;
		float low_above = -33.8f;
		float low_below = -40.8f;
		float low_ratio_up = 4.17f;
		float low_ratio_down = 66.7f;
	};

private:
	Settings settings;
	EffectOTTCompressorState preview_state;

protected:
	static void _bind_methods();

public:
	Ref<AudioEffectInstance> _instantiate() override;
	PackedVector2Array apply(const PackedVector2Array &frames);
	void reset_state();

	Settings get_settings() const;

	float get_low_crossover() const;
	void set_low_crossover(float p_value);
	float get_high_crossover() const;
	void set_high_crossover(float p_value);
	float get_mix() const;
	void set_mix(float p_value);
	float get_output() const;
	void set_output(float p_value);
	float get_time() const;
	void set_time(float p_value);

	float get_hi_input() const;
	void set_hi_input(float p_value);
	float get_hi_output() const;
	void set_hi_output(float p_value);
	float get_hi_attack() const;
	void set_hi_attack(float p_value);
	float get_hi_release() const;
	void set_hi_release(float p_value);
	float get_hi_above() const;
	void set_hi_above(float p_value);
	float get_hi_below() const;
	void set_hi_below(float p_value);
	float get_hi_ratio_up() const;
	void set_hi_ratio_up(float p_value);
	float get_hi_ratio_down() const;
	void set_hi_ratio_down(float p_value);

	float get_mid_input() const;
	void set_mid_input(float p_value);
	float get_mid_output() const;
	void set_mid_output(float p_value);
	float get_mid_attack() const;
	void set_mid_attack(float p_value);
	float get_mid_release() const;
	void set_mid_release(float p_value);
	float get_mid_above() const;
	void set_mid_above(float p_value);
	float get_mid_below() const;
	void set_mid_below(float p_value);
	float get_mid_ratio_up() const;
	void set_mid_ratio_up(float p_value);
	float get_mid_ratio_down() const;
	void set_mid_ratio_down(float p_value);

	float get_low_input() const;
	void set_low_input(float p_value);
	float get_low_output() const;
	void set_low_output(float p_value);
	float get_low_attack() const;
	void set_low_attack(float p_value);
	float get_low_release() const;
	void set_low_release(float p_value);
	float get_low_above() const;
	void set_low_above(float p_value);
	float get_low_below() const;
	void set_low_below(float p_value);
	float get_low_ratio_up() const;
	void set_low_ratio_up(float p_value);
	float get_low_ratio_down() const;
	void set_low_ratio_down(float p_value);

	static void process_buffer(
		const Settings &p_settings,
		EffectOTTCompressorState &p_state,
		const Vector2 *p_input,
		Vector2 *p_output,
		int32_t p_frame_count);
};

} // namespace godot
