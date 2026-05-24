volume_sampler_t begin_volume(
	_in(vec3) origin,
	_in(float) coeff_absorb
){
	volume_sampler_t v = _begin(volume_sampler_t)
		origin, origin, 0.,
		coeff_absorb, 1.,
		vec3(0., 0., 0.), 0.
	_end;
	return v;
}

float illuminate_volume(
	_inout(volume_sampler_t) vol,
	_in(vec3) V,
	_in(vec3) L
);

void integrate_volume(
	_inout(volume_sampler_t) vol,
	_in(vec3) V,
	_in(vec3) L,
	_in(float) density,
	_in(float) dt
){
	float T_i = exp(-vol.coeff_absorb * density * dt);
	vol.T *= T_i;
	vol.C += vol.T * illuminate_volume(vol, V, L) * density * dt;
	vol.alpha += (1. - T_i) * (1. - vol.alpha);
}
