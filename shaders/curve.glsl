struct Curve
{
    vec3 start;
    vec3 controlPoint1;
    vec3 controlPoint2;
    vec3 end;
};

vec3 SampleCurvePoint(Curve curve, float t)
{
    float u = 1.0f - t;
    float tt = t * t;
    float uu = u * u;
    float uuu = uu * u;
    float ttt = tt * t;

    return uuu * curve.start
    + 3.0f * uu * t * curve.controlPoint1
    + 3.0f * u * tt * curve.controlPoint2
    + ttt * curve.end;
}

vec3 SampleCurveAxis(Curve curve, float t)
{
    float u = 1.0f - t;

    return -3.0f * u * u * curve.start
    + 3.0f * (3.0f * t * t - 4.0f * t + 1.0f) * curve.controlPoint1
    + 3.0f * (2.0f - 3.0f * t) * t * curve.controlPoint2
    + 3.0f * t * t * curve.end;
}

Curve TransformCurve(Curve curve, mat4 transform)
{
    Curve transformedCurve;
    transformedCurve.start = (transform * vec4(curve.start, 1.0)).xyz;
    transformedCurve.controlPoint1 = (transform * vec4(curve.controlPoint1, 1.0)).xyz;
    transformedCurve.controlPoint2 = (transform * vec4(curve.controlPoint2, 1.0)).xyz;
    transformedCurve.end = (transform * vec4(curve.end, 1.0)).xyz;

    return transformedCurve;
}

float CurveDistanceToCylinder(Curve curve, vec3 point)
{
    return length(cross(point - curve.start, point - curve.end)) / length(curve.end - curve.start);
}
