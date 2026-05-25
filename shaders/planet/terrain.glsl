#define TERR_STEPS 56
#define TERR_EPS .009
#define rnoise (1. - abs(noise(p) * 2. - 1.))
#define SEA_LEVEL u_sea_level

DECL_FBM_FUNC(fbm_terr, 3, noise(p))
DECL_FBM_FUNC(fbm_terr_r, 3, rnoise)

DECL_FBM_FUNC(fbm_terr_normals, 7, noise(p))
DECL_FBM_FUNC(fbm_terr_r_normals, 7, rnoise)

float terrain_deposition_mask(_in(vec3) dir, _in(float) h);
float terrain_vegetation_mask(_in(vec3) dir, _in(float) h, _in(float) radial_flatness);
float terrain_tree_canopy_mask(_in(vec3) dir, _in(float) h, _in(float) radial_flatness);
float terrain_height01_fast(_in(vec3) dir);

float saturate(
	_in(float) v
){
	return clamp(v, 0., 1.);
}

float ridged(
	_in(float) v
){
	return 1. - abs(v * 2. - 1.);
}

vec3 terrain_domain_warp(
	_in(vec3) dir
){
	float wx = fbm_terr(dir * 1.73 + vec3( 7.1, 11.3,  2.9), 2.03, .42, .48);
	float wy = fbm_terr(dir * 1.91 + vec3(13.5,  3.7, 17.8), 2.01, .42, .48);
	float wz = fbm_terr(dir * 1.57 + vec3( 5.2, 19.4, 23.1), 2.08, .42, .48);
	return normalize(dir + (vec3(wx, wy, wz) - .5) * .38);
}

float terrain_continent_mask(
	_in(vec3) dir
){
	vec3 w = normalize(dir + (vec3(
		noise(dir * 1.15 + vec3( 7.1, 11.3,  2.9)),
		noise(dir * 1.28 + vec3(13.5,  3.7, 17.8)),
		noise(dir * 1.06 + vec3( 5.2, 19.4, 23.1))) - .5) * .24);
	float plates = noise(w * 1.18 + vec3(2.3, 4.7, 9.1));
	float shelves = noise(w * 2.35 + vec3(8.0, 1.4, 5.7));
	return smoothstep(.28, .72, plates * .82 + shelves * .18);
}

float terrain_mountain_mask(
	_in(vec3) dir
){
	vec3 w = dir;
	float ranges = ridged(noise(w * 3.15 + vec3(1.9, 8.6, 4.1)));
	float broken = noise(w * 7.6 + vec3(6.2, 3.4, 9.9));
	return smoothstep(.44, .88, ranges + broken * .35);
}

float terrain_river_mask(
	_in(vec3) dir
){
	vec3 w = dir;
	float basin = noise(w * 2.25 + vec3(15.2, 1.7, 6.3));
	float trunk = ridged(noise(w * 8.5 + vec3(3.0, 41.0, 9.0)));
	float branch = ridged(noise(w * 18.0 + vec3(29.0, 7.0, 13.0)));
	float veins = ridged(noise(w * 34.0 + vec3(11.0, 31.0, 5.0)));

	float river = pow(smoothstep(.70, .98, trunk), 3.2);
	river += pow(smoothstep(.76, .99, branch), 4.4) * .60;
	river += pow(smoothstep(.82, 1.00, veins), 5.2) * .28;
	river *= smoothstep(.28, .72, basin);
	return saturate(river);
}

float terrain_erosion_wear(
	_in(vec3) dir
){
	vec3 w = dir;
	float striation = ridged(noise(w * 38.0 + vec3(4.0, 19.0, 27.0)));
	float fine = ridged(noise(w * 12.0 + vec3(2.7, 8.1, 1.6)));
	return saturate(pow(striation, 3.0) * .55 + smoothstep(.48, .92, fine) * .45);
}

float terrain_height01_base(
	_in(vec3) dir
){
	float h0 = fbm_terr(dir * 2.0987, 2.0244, .454, .454);
	float n0 = smoothstep(.35, 1., h0);
	float h1 = fbm_terr_r(dir * 1.50987 + vec3(1.9489, 2.435, .5483), 2.0244, .454, .454);
	float n1 = smoothstep(.6, 1., h1);
	float detail = noise(dir * 5.25 + vec3(19.0, 3.0, 7.0));
	float oceans = noise(dir * 1.05 + vec3(6.0, 13.0, 2.0));
	float basin = smoothstep(.35, .86, oceans);
	return clamp(.02 + basin * .26 + n0 * .34 + n1 * .37 + detail * .07, .0, 1.12);
}

float terrain_height01_fast(
	_in(vec3) dir
){
	return terrain_height01_base(dir);
}

float terrain_height01_dir(
	_in(vec3) dir
){
	float h = terrain_height01_fast(dir);
	float continents = terrain_continent_mask(dir);
	float mountains = terrain_mountain_mask(dir) * smoothstep(.25, .92, continents);
	float river = terrain_river_mask(dir);
	float wear = terrain_erosion_wear(dir);
	float erosion = u_enable_erosion * u_erosion_strength;

	float above_water = smoothstep(SEA_LEVEL + .04, SEA_LEVEL + .18, h);
	float highland = 1. - smoothstep(.76, 1.08, h);
	h -= river * above_water * highland * erosion * .06;
	h -= wear * mountains * erosion * .055;

	float deposition = terrain_deposition_mask(dir, h);
	h += deposition * .040 * erosion;

	float canopy = terrain_tree_canopy_mask(dir, h, .78);
	h += canopy * .030 * u_tree_height;

	return clamp(h, .0, 1.18);
}

float terrain_deposition_mask(
	_in(vec3) dir,
	_in(float) h
){
	float river = terrain_river_mask(dir);
	float lowland = band(SEA_LEVEL + .015, SEA_LEVEL + .105, SEA_LEVEL + .28, h);
	float fan = noise(dir * 18.0 + vec3(6.0, 4.0, 12.0));
	return saturate(river * lowland * smoothstep(.22, .72, fan));
}

float terrain_vegetation_mask(
	_in(vec3) dir,
	_in(float) h,
	_in(float) radial_flatness
){
	float land = smoothstep(SEA_LEVEL + .045, SEA_LEVEL + .12, h);
	float land_h = clamp((h - SEA_LEVEL) / .42, 0., 1.);
	float alpine = 1. - smoothstep(.56, .78, land_h);
	float slope = smoothstep(.36, .82, radial_flatness);
	float moisture = noise(dir * 4.0 + vec3(8.0, 22.0, 3.0));
	float river_moisture = terrain_river_mask(dir) * .45;
	float forest_clumps = smoothstep(.42, .80, moisture + river_moisture);
	float patch_breakup = smoothstep(.30, .92, noise(dir * 61.0 + vec3(3.0, 14.0, 9.0)));
	return saturate(land * alpine * slope * forest_clumps * patch_breakup * u_vegetation_density * u_enable_vegetation);
}

float terrain_tree_canopy_mask(
	_in(vec3) dir,
	_in(float) h,
	_in(float) radial_flatness
){
	float veg = terrain_vegetation_mask(dir, h, radial_flatness);
	vec3 f, r;
	fast_orthonormal_basis(dir, f, r);
	vec2 uv = vec2(dot(dir, f), dot(dir, r)) * u_tree_scale;
	vec2 cell = floor(uv);
	vec2 local = fract(uv) - .5;
	float cell_noise = noise(vec3(cell * .173, dot(cell, vec2(13.1, 7.7))));
	float crown_radius = mix(.18, .42, noise(vec3(cell * .37 + 4.1, 2.0)));
	float crown = smoothstep(crown_radius, crown_radius * .35, length(local));
	float keep = smoothstep(1. - u_tree_density * .55, 1., cell_noise);
	float clump = smoothstep(.35, .78, noise(dir * 18.0 + vec3(2.0, 9.0, 15.0)));
	return saturate(crown * keep * clump * veg);
}

float terrain_material_grain(
	_in(vec3) dir,
	_in(float) scale
){
	float a = noise(dir * scale + vec3(1.5, 7.1, 3.2));
	float b = ridged(noise(dir * scale * 2.7 + vec3(9.1, 4.3, 2.6)));
	return (a * .72 + b * .28 - .5) * u_material_detail * u_enable_materials;
}

vec2 sdf_terrain_map(_in(vec3) pos)
{
	vec3 dir = normalize(pos);
	float h = terrain_height01_fast(dir);
	return vec2(length(pos) - planet.radius - h * max_height, h);
}

vec2 sdf_terrain_map_detail(_in(vec3) pos)
{
	vec3 dir = normalize(pos);
	float h = terrain_height01_fast(dir);
	float detail = noise(dir * 34.0 + vec3(7.0, 2.0, 11.0)) * .018;
	detail += ridged(noise(dir * 71.0 + vec3(5.0, 13.0, 3.0))) * .010;
	h += detail * smoothstep(SEA_LEVEL + .05, .95, h) * u_material_detail;
	h += terrain_tree_canopy_mask(dir, h, .78) * .030 * u_tree_height;
	return vec2(length(pos) - planet.radius - h * max_height, h);
}

vec3 sdf_terrain_normal(_in(vec3) p)
{
#define F(t) sdf_terrain_map_detail(t).x
	vec3 dt = vec3(0.001, 0, 0);

	return normalize(vec3(
		F(p + dt.xzz) - F(p - dt.xzz),
		F(p + dt.zxz) - F(p - dt.zxz),
		F(p + dt.zzx) - F(p - dt.zzx)
	));
#undef F
}
