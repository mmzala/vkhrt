struct Cylinder
{
    vec3 p0;
    vec3 p1;
    float radius;
};

bool RayCylinderIntersect(Ray ray, Cylinder cylinder)
{
    vec3 ba = cylinder.p1 - cylinder.p0;
    vec3 oc = ray.origin - cylinder.p0;

    float baba = dot(ba, ba);
    float bard = dot(ba, ray.direction);
    float baoc = dot(ba, oc);

    float k2 = baba - bard * bard;
    float k1 = baba * dot(oc, ray.direction) - baoc * bard;
    float k0 = baba * dot(oc, oc) - baoc * baoc - cylinder.radius * cylinder.radius * baba;

    float h = k1 * k1 - k2 * k0;

    if (h < 0.0)
    {
        return false;
    }

    h = sqrt(h);
    float t = (-k1 - h) / k2;

    // Body
    float y = baoc + t * bard;
    if (y > 0.0 && y < baba)
    {
        return true;
    }

    // Caps
    t = ((y < 0.0 ? 0.0 : baba) - baoc) / bard;
    if (abs(k1 + k2 * t) < h)
    {
        return true;
    }

    return false;
}
