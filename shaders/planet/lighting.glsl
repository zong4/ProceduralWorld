vec3 setup_lights(
	_in(vec3) L,
	_in(vec3) normal
){
	vec3 diffuse = vec3(0, 0, 0);

	vec3 c_L = vec3(7, 5, 3) * u_light_strength;
	diffuse += max(0., dot(L, normal)) * c_L;

	float hemi = clamp(.25 + .5 * normal.y, .0, 1.);
	diffuse += hemi * vec3(.4, .6, .8) * .2;

	float amb = clamp(.12 + .8 * max(0., dot(-L, normal)), 0., 1.);
	diffuse += amb * vec3(.4, .5, .6);

	return diffuse;
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

	#define c_water vec3(.015, .110, .455)
	#define c_grass vec3(.086, .132, .018)
	#define c_beach vec3(.153, .172, .121)
	#define c_rock  vec3(.080, .050, .030)
	#define c_snow  vec3(.600, .600, .600)

	#define l_water .05
	#define l_shore .17
	#define l_grass .211
	#define l_rock .351

	float s = smoothstep(.4, 1., h);
	vec3 rock = mix(
		c_rock, c_snow,
		smoothstep(1. - .3*s, 1. - .2*s, N));

	vec3 grass = mix(
		c_grass, rock,
		smoothstep(l_grass, l_rock, h));
		
	vec3 shoreline = mix(
		c_beach, grass,
		smoothstep(l_shore, l_grass, h));

	vec3 water = mix(
		c_water / 2., c_water,
		smoothstep(0., l_water, h));

#ifdef LIGHT
	vec3 L = mul(local_xform, normalize(vec3(1, 1, 0)));
	shoreline *= setup_lights(L, normal);
	vec3 ocean = setup_lights(L, w_normal) * water;
#else
	vec3 ocean = water;
#endif
	
	return mix(
		ocean, shoreline,
		smoothstep(l_water, l_shore, h));
}
