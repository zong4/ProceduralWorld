_constant(sphere_t) planet = _begin(sphere_t)
	vec3(0, 0, 0), 1., 0
_end;

#define max_height u_terrain_height
#define max_ray_dist (max_height * 4.)

vec3 sun_direction()
{
	float yaw = radians(u_sun_azimuth);
	float pitch = radians(u_sun_elevation);
	return normalize(vec3(cos(pitch) * sin(yaw), sin(pitch), cos(pitch) * cos(yaw)));
}

vec3 sky_color(
	_in(vec3) dir
){
	vec3 L = sun_direction();
	float sun_amount = max(dot(dir, L), 0.);
	float horizon = pow(1. - abs(dir.y), 2.3);
	float day = smoothstep(-.15, .25, L.y);

	vec3 night_zenith = vec3(.006, .010, .028);
	vec3 day_zenith = vec3(.09, .25, .58);
	vec3 horizon_day = vec3(.55, .70, .86);
	vec3 horizon_sunset = vec3(.95, .38, .16);

	vec3 zenith = mix(night_zenith, day_zenith, day);
	vec3 horizon_col = mix(horizon_sunset, horizon_day, smoothstep(.05, .55, L.y));
	vec3 sky = mix(zenith, horizon_col, horizon);

	float rayleigh = pow(max(dir.y * .5 + .5, 0.), .7);
	sky *= mix(vec3(.65, .74, .95), vec3(1.), rayleigh);

	vec3 sun_color = mix(vec3(1.0, .34, .12), vec3(1.0, .92, .66), smoothstep(.0, .55, L.y));
	sky += sun_color * min(pow(sun_amount, 420.0) * 6.0, 5.0);
	sky += sun_color * pow(sun_amount, 18.0) * .42;
	return sky * u_sky_exposure;
}

vec3 background(
	_in(ray_t) eye
){
	return sky_color(eye.direction);
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
