class_name LoopPointFinder
extends RefCounted


static func find_best_loop_points(
	pcm_data: PackedFloat32Array,
	channel_count: int = 1,
	sample_rate: int = 44100,
	min_loop_seconds: float = 0.25,
	max_loop_seconds: float = 4.0,
	search_start_ratio: float = 0.05,
	search_end_ratio: float = 0.95,
	window_size: int = 2048,
	step_size: int = 256
) -> Dictionary:
	if channel_count <= 0:
		return {"ok": false, "reason": "channel_count must be greater than 0"}

	if sample_rate <= 0:
		return {"ok": false, "reason": "sample_rate must be greater than 0"}

	if pcm_data.is_empty():
		return {"ok": false, "reason": "pcm_data is empty"}

	var frame_count := pcm_data.size() / channel_count
	if frame_count <= window_size * 2:
		return {"ok": false, "reason": "pcm_data is too short for loop analysis"}

	var mono := _to_mono_frames(pcm_data, channel_count)
	var min_loop_frames := max(int(min_loop_seconds * sample_rate), window_size)
	var max_loop_frames := min(int(max_loop_seconds * sample_rate), frame_count - window_size)

	if max_loop_frames <= min_loop_frames:
		return {"ok": false, "reason": "invalid loop duration constraints"}

	var search_start_frame := clampi(int(frame_count * search_start_ratio), 0, frame_count - window_size - 1)
	var search_end_frame := clampi(int(frame_count * search_end_ratio), window_size, frame_count - 1)

	var best_score := INF
	var best_start := -1
	var best_end := -1

	for loop_start in range(search_start_frame, search_end_frame - min_loop_frames, step_size):
		var end_min := loop_start + min_loop_frames
		var end_max := min(loop_start + max_loop_frames, search_end_frame)

		for loop_end in range(end_min, end_max, step_size):
			if loop_end + window_size >= frame_count:
				break

			var start_aligned := _find_nearest_zero_crossing(mono, loop_start, step_size)
			var end_aligned := _find_nearest_zero_crossing(mono, loop_end, step_size)
			if end_aligned - start_aligned < min_loop_frames:
				continue

			var score := _score_loop(mono, start_aligned, end_aligned, window_size)
			if score < best_score:
				best_score = score
				best_start = start_aligned
				best_end = end_aligned

	if best_start < 0 or best_end < 0:
		return {"ok": false, "reason": "no viable loop points found"}

	return {
		"ok": true,
		"loop_start_frame": best_start,
		"loop_end_frame": best_end,
		"loop_length_frames": best_end - best_start,
		"loop_start_seconds": float(best_start) / sample_rate,
		"loop_end_seconds": float(best_end) / sample_rate,
		"score": best_score
	}


static func _to_mono_frames(pcm_data: PackedFloat32Array, channel_count: int) -> PackedFloat32Array:
	var frame_count := pcm_data.size() / channel_count
	var mono := PackedFloat32Array()
	mono.resize(frame_count)

	for frame in range(frame_count):
		var sum := 0.0
		var base := frame * channel_count
		for channel in range(channel_count):
			sum += pcm_data[base + channel]
		mono[frame] = sum / channel_count

	return mono


static func _find_nearest_zero_crossing(mono: PackedFloat32Array, center: int, radius: int) -> int:
	var best_index := clampi(center, 1, mono.size() - 2)
	var best_distance := INF

	for offset in range(-radius, radius + 1):
		var index := center + offset
		if index <= 0 or index >= mono.size():
			continue

		var current := mono[index]
		var previous := mono[index - 1]
		var crosses_zero := (current >= 0.0 and previous < 0.0) or (current <= 0.0 and previous > 0.0)
		if not crosses_zero:
			continue

		var distance := absf(float(offset))
		if distance < best_distance:
			best_distance = distance
			best_index = index

	return best_index


static func _score_loop(mono: PackedFloat32Array, loop_start: int, loop_end: int, window_size: int) -> float:
	var diff_sum := 0.0
	var edge_penalty := absf(mono[loop_start] - mono[loop_end])
	var slope_penalty := absf(
		(mono[loop_start] - mono[max(loop_start - 1, 0)]) -
		(mono[loop_end] - mono[max(loop_end - 1, 0)])
	)

	for i in range(window_size):
		var start_value := mono[loop_start + i]
		var end_value := mono[loop_end + i]
		var diff := start_value - end_value
		diff_sum += diff * diff

	var rms_diff := sqrt(diff_sum / window_size)
	return rms_diff + (edge_penalty * 0.5) + (slope_penalty * 0.25)
