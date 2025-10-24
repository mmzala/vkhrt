struct Cone
{
    vec3 center;
    float radius;
    vec3 axis;
    float slant;
};

struct RayConeIntersection
{
    bool isIntersection;
    float s;    // ray.s − c0.z for ray ∩ cone(t)
    float dt;   // dt to the (ray ∩ cone) from t
    float dp;   // |(ray ∩ plane(t)) - curve(t)|2
    float dc;   // |(ray ∩ cone(t)) - curve(t)|2
    float sp;   // ray.s − c0.z for ray ∩ plane(t)
};

// Function taken from Phrantom Ray Hair Intersector paper.
// https://research.nvidia.com/sites/default/files/pubs/2018-08_Phantom-Ray-Hair-Intersector//Phantom-HPG%202018.pdf
RayConeIntersection RayConeIntersectRCC(Cone cone)
{
    // Assuming ray centric coordinates:
    // ray.origin = {0,0,0}; ray.direction = {0,0,1};

    float r2 = cone.radius * cone.radius;
    float drr = cone.radius * cone.slant; // slant could be either positive or negative (0 for cyllinder)

    // All possible combinations of x * y terms
    float ddd = cone.axis.x * cone.axis.x + cone.axis.y * cone.axis.y;
    float dp = cone.center.x * cone.center.x + cone.center.y * cone.center.y;
    float cdd = cone.center.x * cone.axis.x + cone.center.y * cone.axis.y;
    float cxd = cone.center.x * cone.axis.y - cone.center.y * cone.axis.x;

    // Compute a, b, c in (a − 2 b s + c s2), where s for ray cone
    float c = ddd;
    float b = cone.axis.z * (drr - cdd);
    float cdz2 = cone.axis.z * cone.axis.z;
    ddd += cdz2;
    float a = 2.0 * drr * cdd + cxd * cxd - ddd * r2 + dp * cdz2;

    // dr2 adjustments
    // It does not help much with neither accuracy nor performance
/*
    float qs = (dr * dr) / ddd;
    a -= qs * cdd * cdd;
    b -= qs * cone.axis.z * cdd;
    c -= qs * cdz2;
    */

    float det = b * b - a * c;

    RayConeIntersection result;
    result.isIntersection = det > 0.0;                      // true (real) or false (phantom)
    result.s = (b - (det < 0.0 ? sqrt(det) : 0)) / c;       // ray.s − c0.z for ray ∩ cone(t)
    result.dt = (result.s * cone.axis.z - cdd) / ddd;       // dt to the (ray ∩ cone) from t
    result.dc = result.s * result.s + dp;                   // |(ray ∩ cone(t)) - curve(t)|2
    result.sp = cdd / cone.axis.z;                          //  ray.s − c0.z for ray ∩ plane(t)
    result.dp = dp + result.sp * result.sp;                 // |(ray ∩ plane(t)) - curve(t)|2

    return result;
}
