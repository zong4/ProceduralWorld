#define FOV (tan(radians(30.)) * iZoom)

void mainImage(
	_out(vec4) fragColor,
#ifdef SHADERTOY
	vec2 fragCoord
#else
	_in(vec2) fragCoord
#endif
){
	vec2 aspect_ratio = vec2(u_res.x / u_res.y, 1);

	vec3 color = vec3(0, 0, 0);

	vec3 eye, look_at;
	setup_camera(eye, look_at);

	setup_scene();

	vec2 point_ndc = fragCoord.xy / u_res.xy;
#ifdef HLSL
		point_ndc.y = 1. - point_ndc.y;
#endif
	vec3 point_cam = vec3(
		(2.0 * point_ndc - 1.0) * aspect_ratio * FOV,
		-1.0);

	ray_t ray = get_primary_ray(point_cam, eye, look_at);

	color += render(ray, point_cam);

	fragColor = vec4(linear_to_srgb(color), 1);
}
