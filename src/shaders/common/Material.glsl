#ifndef MATERIAL_GLSL
#define MATERIAL_GLSL

#include "Random.glsl"
#include "Constants.glsl"

// TODO: REWRITE PBR CODE

layout(std430) readonly buffer samplers {
    vec4 materialInstance[];
};

// https://casual-effects.com/research/McGuire2013CubeMap/paper.pdf
vec3 BlinnPhongNormalized(in vec3 albedo, in float shiny, in vec3 specular, in vec3 n, in vec3 v, in vec3 l) {
    vec3 h = normalize(v + l);
    float distribution = pow(max(dot(n, h), 0.0f), shiny);
    float reflectance = distribution * (shiny + 8.0f) / 8.0f;
    return (albedo + specular * reflectance) / M_PI;
}

vec3 BlinnPhongNormalizedPBR(in vec3 albedo, in float roughness, in float metallic, in vec3 n, in vec3 v, in vec3 l) {
    // GGX != beckman but this is the only remapping I know
    float shiny = 2 / (roughness * roughness) - 2;
    vec3 f0 = mix(vec3(0.04f), albedo, metallic);
    return BlinnPhongNormalized(albedo * (1.0f - metallic), shiny, f0, n, v, l);
}

vec3 ReflectiveTest(in vec3 albedo, in float roughness, in float metallic, in vec3 n, in vec3 v, in vec3 l) {
    vec3 r = reflect(-l, n);
    float dist = pow(max(dot(r, v), 0.0f), 256.0f);
    return 1000000000.0 * albedo * dist;
}

//#define LOGL_PBR
#ifdef LOGL_PBR

float DistributionTrowbridgeReitz(vec3 N, vec3 H, float roughness)
{
    float a = roughness *roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = M_PI * denom * denom;

    return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 FresnelShlick(vec3 F0, float cosTheta)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 FresnelShlick(vec3 F0, vec3 n, vec3 d)
{
    return FresnelShlick(F0, max(dot(n, d), 0.0f));
}

#else

float DistributionTrowbridgeReitz(in vec3 n, in vec3 h, in float roughness) {
    float noh = max(dot(n, h), 0.0f);
    float a2 = roughness * roughness;
    float k = (noh * noh * (a2 - 1.0f) + 1.0f);
    float div = M_PI * k * k;
    return a2 / div;
}

float GeometryShlickGGX(vec3 n, vec3 v, float k) {
    float nov = max(dot(n, v), 0.0f);
    return nov / (nov * (1.0 - k) + k);
}

float GeometrySmith(in vec3 n, in vec3 v, in vec3 l, in float roughness) {
    float k = roughness + 1;
    k = k * k / 8;
    return GeometryShlickGGX(n, v, k) * GeometryShlickGGX(n, l, k);
}

float VisibilityGGX(in float nov, in float nol, in float a2) {
    return nol * sqrt(nov * nov * (1 - a2) + a2);
}

float VisibilitySmithGGXCorrelated(in vec3 n, in vec3 v, in vec3 l, in float roughness) {
    float a2 = roughness * roughness;
    float nov = max(dot(n, v), 0.0f);
    float nol = max(dot(n, l), 0.0f);
    float div = VisibilityGGX(nov, nol, a2) + VisibilityGGX(nol, nov, a2);
    return 0.5f / div;
}

vec3 FresnelShlick(in vec3 f0, in vec3 n, in vec3 v) {
    return f0 + (1.0 - f0) * pow(clamp(1.0 - max(dot(n, v), 0.0f), 0.0, 1.0), 5.0);
}

#endif

// https://docs.google.com/document/d/1ZLT1-fIek2JkErN9ZPByeac02nWipMbO89oCW2jxzXo/edit
vec3 SingleScatterCookTorrace(in vec3 albedo, in float roughness, in float metallic, in vec3 n, in vec3 v, in vec3 l) {
    // If any point is bellow the hemisphere then do not reflect; BRDFs only work when both points are above the surface
    if (dot(n, v) < 0.0f || dot(n, l) < 0.0f) {
        return vec3(0.0f);
    }
    // Cook torrance
    vec3 f0 = mix(vec3(0.04f), albedo, metallic);
    vec3 h = normalize(v + l);
    vec3 specular = DistributionTrowbridgeReitz(n, h, roughness) * VisibilitySmithGGXCorrelated(n, v, l, roughness) * FresnelShlick(f0, h, v) / max(4 * max(dot(n, v), 0.0f) * max(dot(n, l), 0.0f), 0.001f);
    // Energy conserving diffuse
    vec3 diffuse = (1.0 - FresnelShlick(f0, n, l)) * (1.0f - FresnelShlick(f0, n, v)) * albedo / M_PI;
    return specular + diffuse * (1.0 - metallic);
}
 
// https://schuttejoe.github.io/post/ggximportancesamplingpart1/
vec3 ImportanceSampleDistributionGGX(in float roughness, out float pdf) {
    float a2 = roughness * roughness;

    // Chose a direction https://agraphicsguy.wordpress.com/2015/11/01/sampling-microfacet-brdf/
    float rand0 = rand(), rand1 = rand();
    float theta = atan(roughness * sqrt(rand0 / (1.0 - rand0))); // The article presents two methods to convert from rand to theta, one that uses acos and one that uses atan. acos causes nans, while atan does not
    float phi = 2 * M_PI * rand1;

    // Compute the direction
    vec3 direction;
    direction.x = sin(theta) * sin(phi);
    direction.y = sin(theta) * cos(phi);
    direction.z = cos(theta);

    // Calculate pdf
    float div = (a2 - 1) * cos(theta) * cos(theta) + 1;
    pdf = a2 * cos(theta) * sin(theta) / (M_PI * div * div);

    return direction;
}

float SamplePdfDistributionGGX(in vec3 n, in vec3 h, in float a) {
    float costheta = max(dot(n, h), 0.0f);
    float sintheta = sqrt(1.0 - costheta * costheta);
    float a2 = a * a;
    float div = (a2 - 1) * costheta * costheta + 1;
    float pdf = a2 * costheta* sintheta / (M_PI * div * div);
    return pdf;
}

float SamplePdfCosine(in vec3 n, in vec3 l) {
    return max(dot(n, l), 0.0f);
}

vec3 ImportanceSampleCosine(out float pdf) {
    float r0 = rand(), r1 = rand();
    float r = sqrt(r0);
    float phi = 2 * M_PI * r1;
    pdf = sqrt(1.0 - r0);
    return vec3(r * vec2(sin(phi), cos(phi)), pdf);
}

#define BRDF(a, r, m, n, v, l) SingleScatterCookTorrace(a, r, m, n, v, l)

#endif