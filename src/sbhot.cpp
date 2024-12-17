#include "sbhot.h"

using namespace godot;


void SBHot::_bind_methods() 
{
	ClassDB::bind_method(D_METHOD("add_packed_vector2_arrays"), &SBHot::add_packed_vector2_arrays);
}

PackedVector2Array SBHot::add_packed_vector2_arrays(PackedVector2Array source, int source_position, PackedVector2Array const& dest, int dest_position, int element_count, float scale_source)
{
    for(int i = 0; i < element_count; i++) 
	{
        source.set(source_position + i, source[source_position + i] + dest[dest_position + i] * scale_source);
    }

    return source;
}
