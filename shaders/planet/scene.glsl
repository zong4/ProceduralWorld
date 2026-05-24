_constant(sphere_t) planet = _begin(sphere_t)
	vec3(0, 0, 0), 1., 0
_end;

#define max_height u_terrain_height
#define max_ray_dist (max_height * 4.)

vec3 background(
	_in(ray_t) eye
){
#if 0
	return vec3(.15, .3, .4);
#else
	_constant(vec3) sun_color = vec3(1., .9, .55);
	float sun_amount = dot(eye.direction, vec3(0, 0, 1));

	vec3 sky = mix(
		vec3(.0, .05, .2),
		vec3(.15, .3, .4),
		1.0 - eye.direction.y);
	sky += sun_color * min(pow(sun_amount, 30.0) * 5.0, 1.0);
	sky += sun_color * min(pow(sun_amount, 10.0) * .6, 1.0);

	return sky;
#endif
}

void setup_scene()
{
}

void setup_camera(
	_inout(vec3) eye,
	_inout(vec3) look_at
){
#if 0
	eye = vec3(.0, 0, -1.93);
	look_at = vec3(-.1, .9, 2);
#else
	eye = vec3(0, 0, -2.5);
	look_at = vec3(0, 0, 2);
#endif
}
