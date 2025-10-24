struct Ray
{
    vec3 origin;
    vec3 direction;
};

struct HitPayload
{
    vec3 hitValue;
    Ray ray;
};

void MakeOrthonormalBasis(out vec3 u, out vec3 v, vec3 w)
{
    v = abs(w.x) > abs(w.y) ? normalize(vec3(-w.z, 0.0, w.x)) : normalize(vec3(0.0, w.z, -w.y));
    u = cross(v, w);
}

mat4 CreateRCCMatrix(Ray ray)
{
    vec3 e1;
    vec3 e2;
    vec3 e3 = normalize(ray.direction);
    MakeOrthonormalBasis(e1, e2, e3);

    mat4 rccInv = mat4(
    vec4(e1, 0.0),
    vec4(e2, 0.0),
    vec4(e3, 0.0),
    vec4(ray.origin, 1.0)
    );
    return inverse(rccInv);
}
