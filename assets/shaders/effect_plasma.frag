// Системный пример: плазма из фрактального шума (fbm).
void main() {
    vec2 p = (v_UV - 0.5) * 4.0;
    float n = EngineFbm(p + vec2(u_Time * 0.2, -u_Time * 0.13), 5);
    vec3 col = EnginePalette(n + u_Time * 0.05,
                             u_Color, u_Color * 0.9 + 0.1,
                             vec3(1.0, 0.7, 0.4), vec3(0.0, 0.15, 0.35));
    fragColor = vec4(col, EngineCircleMask(v_UV));
}
