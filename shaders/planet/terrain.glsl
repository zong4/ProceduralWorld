#define TERR_STEPS 120
#define TERR_EPS .005
#define rnoise (1. - abs(noise(p) * 2. - 1.))

DECL_FBM_FUNC(fbm_terr, 3, noise(p))
DECL_FBM_FUNC(fbm_terr_r, 3, rnoise)

DECL_FBM_FUNC(fbm_terr_normals, 7, noise(p))
DECL_FBM_FUNC(fbm_terr_r_normals, 7, rnoise)

vec2 sdf_terrain_map(_in(vec3) pos)
{
	float h0 = fbm_terr(pos * 2.0987, 2.0244, .454, .454);
	float n0 = smoothstep(.35, 1., h0);

	float h1 = fbm_terr_r(pos * 1.50987 + vec3(1.9489, 2.435, .5483), 2.0244, .454, .454);
	float n1 = smoothstep(.6, 1., h1);
	
	float n = n0 + n1;
	
	return vec2(length(pos) - planet.radius - n * max_height, n / max_height);
}

vec2 sdf_terrain_map_detail(_in(vec3) pos)
{
	float h0 = fbm_terr_normals(pos * 2.0987, 2.0244, .454, .454);
	float n0 = smoothstep(.35, 1., h0);

	float h1 = fbm_terr_r_normals(pos * 1.50987 + vec3(1.9489, 2.435, .5483), 2.0244, .454, .454);
	float n1 = smoothstep(.6, 1., h1);

	float n = n0 + n1;

	return vec2(length(pos) - planet.radius - n * max_height, n / max_height);
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
