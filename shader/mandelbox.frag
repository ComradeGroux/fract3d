#version 460

layout(push_constant) uniform uPushConstant
{
	vec3	pos;
	vec4	rot;
	vec2	screenSize;
	int		theme;
} pc;

layout(location = 0) out vec4 fragColor;


const float	scale = 2.0;
const float	minRadius = 0.5;
const float	fixedRadius = 1.0;


const int	NB_ITER_SCALAR = 10;
// struct theme {
// 	const vec3	MAIN_COLOR;
// 	const vec3	SECOND_COLOR;
// 	const vec3	SHADOW_COLOR;
// };
const float	LOW_T = 0.0;
const float	HIGH_T = 1.0;
const float	TRAP_RADIUS = 1.5;
const float	CONSTRAST = 0.4;

const vec3	MAIN_COLOR = vec3(0.56, 0.38, 0.14);
const vec3	SECOND_COLOR = vec3(0.31, 0.10, 0.05);
const vec3	SHADOW_COLOR = vec3(0.224, 0.10, 0.05);


const int	MAX_STEPS = 512;
const float	MAX_DIST = 100.0;
const float	STEP_SAFETY = 0.7;
const bool	EXHAUSTED_IS_HIT = true;

const float	EPS_MIN = 1e-5;
const float	DETAIL_C = 4.0;
const int	MIN_ITER = 4;
const int	MAX_ITER = 30;
const float	ESCAPE_R2 = 1.0e6;

struct ray {
	vec3	ro;
	vec3	rd;
};

struct de {
	float	dist;
	int		iter;
};

vec3	rotate(vec3 v, vec4 q)
{
	vec3 t = 2.0 * cross(q.xyz, v);
	return v + q.w * t + cross(q.xyz, t);
}

vec3	center(vec3 p)
{
	return vec3(p.x - (pc.screenSize.x / 2.0), p.y - (pc.screenSize.y / 2.0), p.z);
}

ray	calculateRayDirection()
{
	vec3	normalizedCoord = center(vec3(gl_FragCoord.xy, 1.0));
	normalizedCoord = vec3(normalizedCoord.x / pc.screenSize.y, normalizedCoord.y / pc.screenSize.y, normalizedCoord.z);

	ray		test = { pc.pos, normalize(rotate(normalizedCoord, pc.rot)) };
	return test;
}

vec3	basicLambert(vec3 normal)
{
	vec3	lightDir = normalize(vec3(0.3, 0.8, -1.0));
	return vec3(max(dot(normal, lightDir), 0.0));
}

float	pixelEps(float t)
{
	return max(EPS_MIN, t / pc.screenSize.y);
}

float	iterationCount(float eps)
{
	float	n = log2(DETAIL_C / eps) / log2(abs(scale));
	return clamp(n, MIN_ITER, MAX_ITER);
}

de	distanceEstimator(vec3 p, float n)
{
	vec3	z = p;
	float	dr = 1.0;
	float	r2;
	float	factor;
	int		i = 0;
	int		k = int(floor(n));
	vec3	oldZ;
	float	oldDr;

	for (; i < MAX_ITER + 1; i++)
	{
		if (i == k + 1)
			break;
		oldZ = z;
		oldDr = dr;

		z = clamp(z, -1.0, 1.0) * 2.0 - z;

		r2 = dot(z, z);
		if (sqrt(r2) < minRadius)
			factor = (fixedRadius * fixedRadius) / (minRadius * minRadius);
		else if (sqrt(r2) < fixedRadius)
			factor = (fixedRadius * fixedRadius) / r2;
		else
			factor = 1.0;
		z *= factor;
		dr *= factor;

		z = scale * z + p;
		dr = dr * abs(scale) + 1.0;

		if (dot(z, z) > ESCAPE_R2)
			break;
	}

	float	f = n - k;
	return de((1 - f) * (length(oldZ) / abs(oldDr)) + f * (length(z) / abs(dr)), i);
}

vec3	calcNormal(vec3 p, float eps, float n)
{
	const vec2	k = vec2(1.0, -1.0);
	float		h = 0.1 * eps;

	return normalize(k.xyy * (distanceEstimator(p + k.xyy * h, n).dist)
				   + k.yyx * (distanceEstimator(p + k.yyx * h, n).dist)
				   + k.yxy * (distanceEstimator(p + k.yxy * h, n).dist)
				   + k.xxx * (distanceEstimator(p + k.xxx * h, n).dist));
}

float	calcScalarT(vec3 p)
{
	vec3	z = p;
	float	r2;
	float	factor;
	int		i = 0;

	float	trapMin = 1e4;
	for (; i < NB_ITER_SCALAR; i++)
	{
		z = clamp(z, -1.0, 1.0) * 2.0 - z;
		r2 = dot(z, z);

		trapMin = min(trapMin, abs(length(z) - TRAP_RADIUS));

		if (r2 < minRadius * minRadius)
			factor = (fixedRadius * fixedRadius) / (minRadius * minRadius);
		else if (r2 < fixedRadius * fixedRadius)
			factor = (fixedRadius * fixedRadius) / r2;
		else
			factor = 1.0;
		z *= factor;

		z = scale * z + p;

		if (dot(z, z) > ESCAPE_R2)
			break;
	}

	// return smoothstep(LOW_T, HIGH_T, trapMin);
	return 1 - exp(-trapMin / CONSTRAST);
}

float	hash12(vec2 p)
{
	vec3	p3 = fract(vec3(p.xyx) * 0.1031);
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.x + p3.y) * p3.z);
}

vec3	dither(vec3 linearColor)
{
	vec3	enc = pow(linearColor, vec3(1.0 / 2.2));
	float	n = hash12(gl_FragCoord.xy) + hash12(gl_FragCoord.xy + 17.0) - 1.0;
	enc += n / 255.0;
	return pow(max(enc, 0.0), vec3(2.2));
}

void	main()
{
	ray		r = calculateRayDirection();
	float	t = 0.0;
	float	oldT = 0.0;
	float	g = -1.0;
	de		d;
	float	eps = EPS_MIN;
	float	n = MIN_ITER;
	vec3	p = r.ro;
	bool	hit = false;
	bool	escaped = false;
	int		realIter = 0;

	for (int i = 0; i < MAX_STEPS; i++)
	{
		p = r.ro + r.rd * t;
		eps = pixelEps(t);
		n = iterationCount(eps);
		d = distanceEstimator(p, n);

		if (d.dist < eps)
		{
			hit = true;
			break;
		}

		oldT = t;
		g = d.dist - eps;

		t += d.dist * STEP_SAFETY;
		if (t > MAX_DIST)
		{
			escaped = true;
			break;
		}
	}

	if (!hit && !escaped && EXHAUSTED_IS_HIT)
		hit = true;

	if (!hit)
	{
		fragColor = vec4(0.01, 0.01, 0.01, 1.0);
		return;
	}
	else if (g >= 0.0 && t != 0.0 && d.dist < eps)
	{
		t = oldT + (g / (g - (d.dist - eps))) * (t - oldT);
	}

	p = r.ro + r.rd * t;
	eps = pixelEps(t);
	n = iterationCount(eps);
	d = distanceEstimator(p, n);

	vec3	normal = calcNormal(p, eps, n);

	float	scalarTColor = calcScalarT(p);
	vec3	color = mix(SECOND_COLOR, MAIN_COLOR, scalarTColor);
	vec3	light = basicLambert(normal);

	// fragColor = vec4(vec3(scalarTColor), 1.0);
	// fragColor = vec4(dither(vec3(scalarTColor)), 1.0);
	// fragColor = vec4(color, 1.0);
	fragColor = vec4(dither(vec3(color)), 1.0);
	// fragColor = vec4(color * light * 0.9 + 0.02, 1.0);
	// fragColor = vec4(dither(color * light) * 0.9 + 0.02, 1.0);
}
