vec3 colors[6] = vec3[](vec3(1.0, 0.0, 0.3), vec3(0.8, 0.2, 0.3), vec3(0.6, 0.4, 0.3), vec3(0.4, 0.6, 0.3), vec3(0.2, 0.8, 0.3), vec3(0.0, 1.0, 0.3));

vec3 GetDebugColor(int primitiveID)
{
    int index = primitiveID % 6;
    return colors[index];
}
