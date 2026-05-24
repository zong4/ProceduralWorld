#define CLOUDS

#define anoise (abs(noise(p) * 2. - 1.))
DECL_FBM_FUNC(fbm_clouds, 4, anoise)

#define vol_coeff_absorb u_cloud_absorb
_mutable(volume_sampler_t) cloud;

float illuminate_volume(
	_inout(volume_sampler_t) cloud,
	_in(vec3) V,
	_in(vec3) L
){
	return exp(cloud.height) / .055;
}

void clouds_map(
	_inout(volume_sampler_t) cloud,
	_in(float) t_step
){
	float dens = fbm_clouds(
		cloud.pos * 3.2343 + vec3(.35, 13.35, 2.67),
		2.0276, .5, .5);

	float cld_coverage = u_cloud_coverage;
	float cld_fuzzy = u_cloud_fuzzy;
	dens *= smoothstep(cld_coverage, cld_coverage + cld_fuzzy, dens);

	dens *= band(.2, .35, .65, cloud.height);

	integrate_volume(cloud,
		cloud.pos, cloud.pos,
		dens, t_step);
}

void clouds_march(
	_in(ray_t) eye,
	_inout(volume_sampler_t) cloud,
	_in(float) max_travel,
	_in(mat3) rot
){
	const int steps = 75;
	float t_step = max_ray_dist / float(steps);
	float t = 0.;

	for (int i = 0; i < steps; i++) {
		if (t > max_travel || cloud.alpha >= 1.) return;
			
		vec3 o = cloud.origin + t * eye.direction;
		cloud.pos = mul(rot, o - planet.origin);

		cloud.height = (length(cloud.pos) - planet.radius) / max_height;
		t += t_step;
		clouds_map(cloud, t_step);
	}
}

void clouds_shadow_march(
	_in(vec3) dir,
	_inout(volume_sampler_t) cloud,
	_in(mat3) rot
){
	const int steps = 5;
	float t_step = max_height / float(steps);
	float t = 0.;

	for (int i = 0; i < steps; i++) {
		vec3 o = cloud.origin + t * dir;
		cloud.pos = mul(rot, o - planet.origin);

		cloud.height = (length(cloud.pos) - planet.radius) / max_height;
		t += t_step;
		clouds_map(cloud, t_step);
	}
}
