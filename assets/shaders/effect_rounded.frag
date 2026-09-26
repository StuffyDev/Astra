// Системный пример: скруглённый квадрат с мягкой рамкой. Только .frag — вертекс даёт движок.
void main() {
    float body = EngineRoundedBox(v_UV, 0.18);
    float border = EngineRoundedBox(v_UV, 0.06) - EngineRoundedBox(v_UV, 0.16);
    vec3 col = u_Color * body + vec3(1.0) * border * 0.6;
    fragColor = vec4(col, body + border);
}
