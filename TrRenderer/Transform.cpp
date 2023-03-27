#include "Transform.h"

mat4 mat4_lookat(vec3 eye, vec3 target, vec3 up)
{
	// http://www.songho.ca/opengl/gl_camera.html
	vec3 z = eye - target;
	vec3 targetZAxis = uniform_vec<vec3>(z);
	vec3 x = cross(up, targetZAxis);
	vec3 targetXAxis = uniform_vec<vec3>(x);
	vec3 targetYAxis = cross(targetZAxis, targetXAxis);

	mat4 mat;
	mat.rows[0] = targetXAxis.add_dimension_1(-dot(targetXAxis, eye));
	mat.rows[1] = targetYAxis.add_dimension_1(-dot(targetYAxis, eye));
	mat.rows[2] = targetZAxis.add_dimension_1(-dot(targetZAxis, eye));
	mat.rows[3] = {0, 0, 0, 1};

	return mat;
}
