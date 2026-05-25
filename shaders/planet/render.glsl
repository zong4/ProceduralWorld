vec3 water_normal(
	_in(vec3) dir,
	_in(mat3) local_xform
){
	vec3 p = mul(local_xform, dir);
	float w1 = noise(p * 42.0 + vec3(u_time * .14, 0., u_time * .07));
	float w2 = noise(p * 97.0 + vec3(0., u_time * .11, u_time * .19));
	float w3 = fbm(p * 19.0 + vec3(4.0, u_time * .08, 9.0), 2.03, .20, .52);
	vec3 f, r;
	fast_orthonormal_basis(dir, f, r);
	vec2 wave = (vec2(w1 - .5, w2 - .5) * .9 + vec2(w3 - .5, w1 - w2) * .35) * u_water_wave_strength * u_enable_water;
	return normalize(dir + f * wave.x * .075 + r * wave.y * .075);
}

vec3 shade_water(
	_in(vec3) pos,
	_in(ray_t) eye,
	_in(mat3) rot
){
	vec3 dir = normalize(pos);
	vec3 n = water_normal(dir, rot);
	vec3 view = normalize(eye.origin - pos);
	vec3 L = mul(rot, sun_direction());

	float terrain_h = terrain_height01_dir(dir);
	float depth = max(SEA_LEVEL - terrain_h, 0.);
	float shallow = exp(-depth * 12.0);
	float fresnel = pow(1. - max(dot(view, n), 0.), 5.);
	fresnel = mix(.035, 1., fresnel) * u_water_reflection_strength * u_enable_water;

	vec3 reflected_dir = reflect(-view, n);
	vec3 reflected_sky = sky_color(reflected_dir);
	float bottom_grain = terrain_material_grain(dir, 48.);
	vec3 refracted_bottom = mix(vec3(.46, .38, .23), vec3(.08, .18, .07), smoothstep(SEA_LEVEL + .03, .45, terrain_h));
	refracted_bottom *= 1. + bottom_grain * .45;
	vec3 shallow_water = vec3(.06, .34, .42);
	vec3 deep_water = vec3(.002, .026, .115);
	vec3 water_body = mix(deep_water, shallow_water, shallow);
	vec3 refraction = mix(water_body, refracted_bottom, shallow * .58 * u_water_refraction_strength * u_enable_water);

	float spec = pow(max(dot(reflect(-L, n), view), 0.), 90.) * 2.2 * u_light_strength;
	vec3 sun_glint = mix(vec3(1.0, .74, .42), vec3(.72, .88, 1.0), smoothstep(.18, .60, sun_direction().y)) * spec;

	vec3 foam = vec3(.72, .82, .76) * smoothstep(.005, .055, depth) * (1. - smoothstep(.055, .13, depth));
	float shore_noise = smoothstep(.38, .82, noise(dir * 150.0 + vec3(u_time * .20, 2.0, 7.0)));
	foam *= shore_noise * u_water_wave_strength * u_enable_water;

	return mix(refraction, reflected_sky, clamp(fresnel * .62, 0., 1.)) + sun_glint + foam;
}

vec3 render(
	_in(ray_t) eye,
	_in(vec3) point_cam
){
	mat3 rot_y = rotate_around_y(27.);
	mat3 mouse_rot = mul(rotate_around_y(-u_mouse.x), rotate_around_x(u_mouse.y));
	mat3 base_rot = mul(mouse_rot, rot_y);
	mat3 rot = mul(rotate_around_x(u_time * -12. * u_planet_speed), base_rot);
	mat3 rot_cloud = mul(rotate_around_x(u_time * 8. * u_cloud_speed), base_rot);

	sphere_t atmosphere = planet;
	atmosphere.radius += max_height;

	sphere_t water_sphere = planet;
	water_sphere.radius += SEA_LEVEL * max_height;

	hit_t hit = no_hit;
	intersect_sphere(eye, atmosphere, hit);
	if (hit.material_id < 0) {
		return background(eye);
	}

	hit_t water_hit = no_hit;
	intersect_sphere(eye, water_sphere, water_hit);
	bool water_candidate = water_hit.material_id >= 0;

	float t = 0.;
	float terrain_world_t = max_dist;
	vec2 df = vec2(1, max_height);
	vec3 pos;
	float max_cld_ray_dist = max_ray_dist;
	
	for (int i = 0; i < TERR_STEPS; i++) {
		if (t > max_ray_dist) break;
		
		vec3 o = hit.origin + t * eye.direction;
		pos = mul(rot, o - planet.origin);

		df = sdf_terrain_map(pos);

		if (df.x < TERR_EPS) {
			max_cld_ray_dist = t;
			terrain_world_t = hit.t + t;
			break;
		}

		t += df.x * .4567;
	}

#ifdef CLOUDS
	if (u_enable_clouds > .5) {
		cloud = begin_volume(hit.origin, vol_coeff_absorb);
		clouds_march(eye, cloud, max_cld_ray_dist, rot_cloud);
	} else {
		cloud = begin_volume(hit.origin, vol_coeff_absorb);
	}
#endif
	
	if (df.x < TERR_EPS) {
		vec3 c_terr = illuminate(pos, eye.direction, rot, df);
		vec3 c_cld = cloud.C;
		float alpha = cloud.alpha;
		float shadow = 1.;

#ifdef CLOUDS
		if (u_enable_clouds > .5) {
			pos = mul(transpose(rot), pos);
			cloud = begin_volume(pos, vol_coeff_absorb);
			vec3 local_up = normalize(pos);
			clouds_shadow_march(local_up, cloud, rot_cloud);
			shadow = mix(.7, 1., step(cloud.alpha, 0.33));
		}
#endif

		vec3 lit_terrain = c_terr * shadow;

		if (u_enable_water > .5 && water_candidate && water_hit.t < terrain_world_t && terrain_height01_fast(normalize(mul(rot, water_hit.origin - planet.origin))) < SEA_LEVEL) {
			vec3 water_pos = water_hit.origin - planet.origin;
			vec3 local_water = mul(rot, water_pos);
			vec3 c_water = shade_water(local_water, eye, rot);
			float depth_blend = smoothstep(.006, .12, SEA_LEVEL - terrain_height01_fast(normalize(local_water)));
			lit_terrain = mix(lit_terrain, c_water, depth_blend);
		}

		return mix(lit_terrain, c_cld, alpha);
	} else {
		vec3 bg = background(eye);
		if (u_enable_water > .5 && water_candidate && terrain_height01_fast(normalize(mul(rot, water_hit.origin - planet.origin))) < SEA_LEVEL) {
			vec3 local_water = mul(rot, water_hit.origin - planet.origin);
			bg = shade_water(local_water, eye, rot);
		}
		return mix(bg, cloud.C, cloud.alpha);
	}
}
