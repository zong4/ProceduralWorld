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

	hit_t hit = no_hit;
	intersect_sphere(eye, atmosphere, hit);
	if (hit.material_id < 0) {
		return background(eye);
	}

	float t = 0.;
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
			break;
		}

		t += df.x * .4567;
	}

#ifdef CLOUDS
	cloud = begin_volume(hit.origin, vol_coeff_absorb);
	clouds_march(eye, cloud, max_cld_ray_dist, rot_cloud);
#endif
	
	if (df.x < TERR_EPS) {
		vec3 c_terr = illuminate(pos, eye.direction, rot, df);
		vec3 c_cld = cloud.C;
		float alpha = cloud.alpha;
		float shadow = 1.;

#ifdef CLOUDS
		pos = mul(transpose(rot), pos);
		cloud = begin_volume(pos, vol_coeff_absorb);
		vec3 local_up = normalize(pos);
		clouds_shadow_march(local_up, cloud, rot_cloud);
		shadow = mix(.7, 1., step(cloud.alpha, 0.33));
#endif

		return mix(c_terr * shadow, c_cld, alpha);
	} else {
		return mix(background(eye), cloud.C, cloud.alpha);
	}
}
