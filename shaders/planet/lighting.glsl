vec3 setup_lights(
	_in(vec3) L,
	_in(vec3) normal
){
	vec3 diffuse = vec3(0, 0, 0);

	vec3 c_L = mix(vec3(7.0, 2.6, .9), vec3(7, 5, 3), smoothstep(.0, .55, sun_direction().y)) * u_light_strength;
	diffuse += max(0., dot(L, normal)) * c_L;

	float hemi = clamp(.25 + .5 * normal.y, .0, 1.);
	diffuse += hemi * sky_color(normal) * .18;

	float amb = clamp(.12 + .8 * max(0., dot(-L, normal)), 0., 1.);
	diffuse += amb * vec3(.4, .5, .6);

	return diffuse;
}

vec3 triplanar_noise_color(
	_in(vec3) dir,
	_in(vec3) base,
	_in(float) scale,
	_in(float) amount
){
	float g = terrain_material_grain(dir, scale);
	return base * (1. + g * amount);
}

vec3 terrain_surface_color(
	_in(vec3) dir,
	_in(float) h,
	_in(float) radial_flatness
){
	float land_h = clamp((h - SEA_LEVEL) / .42, 0., 1.);
	float shore = smoothstep(.015, .12, h - SEA_LEVEL);
	float grass_band = smoothstep(.05, .20, land_h) * (1. - smoothstep(.48, .68, land_h));
	float alpine = smoothstep(.52, .78, land_h);
	float snow = smoothstep(.72, .92, land_h) * smoothstep(.15, .65, radial_flatness);
	float cliff = smoothstep(.18, .54, 1. - radial_flatness);
	float river = terrain_river_mask(dir) * smoothstep(.025, .22, h - SEA_LEVEL);
	float wear = terrain_erosion_wear(dir) * u_enable_erosion;
	float deposition = terrain_deposition_mask(dir, h) * u_enable_erosion;
	float veg = terrain_vegetation_mask(dir, h, radial_flatness);
	float trees = terrain_tree_canopy_mask(dir, h, radial_flatness);

	vec3 sand = triplanar_noise_color(dir, vec3(.58, .48, .30), 42., .75);
	vec3 soil = triplanar_noise_color(dir, vec3(.27, .19, .10), 34., .65);
	vec3 grass = triplanar_noise_color(dir, vec3(.060, .235, .045), 55., .85);
	vec3 forest = triplanar_noise_color(dir, vec3(.012, .135, .030), 84., .90);
	vec3 canopy = triplanar_noise_color(dir, vec3(.018, .175, .040), 115., .95);
	vec3 rock = triplanar_noise_color(dir, vec3(.18, .155, .13), 68., .65);
	vec3 bare = triplanar_noise_color(dir, vec3(.31, .29, .25), 120., .42);
	vec3 snow_col = triplanar_noise_color(dir, vec3(.82, .84, .79), 95., .25);
	vec3 wet = vec3(.045, .060, .050);
	vec3 river_col = vec3(.075, .25, .36);

	vec3 col = mix(sand, soil, shore);
	col = mix(col, grass, grass_band);
	col = mix(col, forest, saturate(veg * 1.25));
	col = mix(col, canopy, saturate(trees * 1.65));
	col = mix(col, rock, cliff);
	col = mix(col, bare, alpine * (1. - snow));
	col = mix(col, snow_col, snow);
	col = mix(col, wet, saturate(wear * .50 + deposition * .30));
	col = mix(col, sand * 1.20, deposition * .75);
	col = mix(col, river_col, river * .92);

	return max(col, vec3(.0));
}

vec3 illuminate(
	_in(vec3) pos,
	_in(vec3) eye,
	_in(mat3) local_xform,
	_in(vec2) df
){
	float h = df.y;

	vec3 w_normal = normalize(pos);
#define LIGHT
#ifdef LIGHT
	vec3 normal = sdf_terrain_normal(pos);
	float N = dot(normal, w_normal);
#else
	float N = w_normal.y;
#endif

	vec3 surface = terrain_surface_color(w_normal, h, clamp(N, 0., 1.));
#ifdef LIGHT
	vec3 L = mul(local_xform, sun_direction());
	float river = terrain_river_mask(w_normal) * smoothstep(SEA_LEVEL + .03, SEA_LEVEL + .25, h);
	float spec = pow(max(dot(reflect(-L, normal), -eye), 0.), 36.) * (.18 + river * 1.4);
	surface *= setup_lights(L, normal);
	surface += spec * mix(vec3(.8, .72, .55), vec3(.55, .80, .92), river) * u_light_strength;
#else
	surface = surface;
#endif

	return surface;
}
