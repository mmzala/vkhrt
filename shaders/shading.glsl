vec3 Shade(vec3 normal)
{
    vec3 lightColor = vec3(0.4, 0.2, 0.1);
    vec3 lightDirection = vec3(0.0, -1.0, 0.0);
    vec3 ambientColor = vec3(0.3);

    vec3 color = abs(dot(normal, lightDirection)) * lightColor;
    color += ambientColor;

    return color;
}