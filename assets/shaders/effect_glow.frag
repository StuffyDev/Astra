// Системный пример: пульсирующее кольцо-свечение.
void main() {
    float ring = EngineRing(v_UV, 0.75 + 0.1 * EnginePulse(0.5), 0.08);
    float core = EngineCircleMask(v_UV) * 0.25;
    vec3 glow = u_Color * (ring + core) * (0.7 + 0.6 * EnginePulse(0.5));
    fragColor = vec4(glow, ring + core);
}
