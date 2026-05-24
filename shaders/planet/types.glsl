#define PI 3.14159265359

struct ray_t {
	vec3 origin;
	vec3 direction;
};
#define BIAS 1e-4

struct sphere_t {
	vec3 origin;
	float radius;
	int material;
};

struct plane_t {
	vec3 direction;
	float distance;
	int material;
};

struct hit_t {
	float t;
	int material_id;
	vec3 normal;
	vec3 origin;
};
#define max_dist 1e8
_constant(hit_t) no_hit = _begin(hit_t)
	float(max_dist + 1e1),
	-1,
	vec3(0., 0., 0.),
	vec3(0., 0., 0.)
_end;

struct volume_sampler_t {
	vec3 origin;
	vec3 pos;
	float height;

	float coeff_absorb;
	float T;

	vec3 C;
	float alpha;
};
