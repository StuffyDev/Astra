// Демо «Material»: живые параметры из инспектора.
// u_Params.x — горизонтальная позиция волны (0..1)
// u_Params.y — вертикаль волны (0..1)
// u_Params.z — частота (1..8)
// u_Params.w — мягкость края (0.01..1)
// u_PColor — цвет волны (alpha — яркость фона)
void main() {
    vec2 uv = v_UV;
    float wave = sin((uv.x - u_Params.x) * u_Params.z * 6.28318 + u_Time * 2.0) * 0.25;
    float d = abs(uv.y - (u_Params.y + wave));
    float edge = clamp(1.0 - d / max(u_Params.w, 0.01), 0.0, 1.0);

    vec3 bg = vec3(0.04, 0.05, 0.08) * u_PColor.a;
    vec3 col = mix(bg, u_PColor.rgb, edge);
    fragColor = vec4(col, 1.0);
}
