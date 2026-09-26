// Системный пример: растр с завихрением и радужной палитрой ИК.
void main() {
    vec2 uv = EngineSwirl(v_UV, vec2(0.5), 0.6, 0.7);
    float t = uv.x + uv.y * 0.5 + u_Time * 0.1;
    vec3 col = EngineRainbow(t);
    fragColor = vec4(col * u_Color, EngineCircleMask(v_UV));
}
