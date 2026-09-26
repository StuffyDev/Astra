// Пример фрагментного шейдера: fragColor, u_Color, u_Texture, EngineCircleMask уже от движка.
in vec2 vUV;

void main() {
    vec4 tex = texture(u_Texture, vUV);
    float wave = 0.5 + 0.5 * sin((vUV.y + u_Time * 0.3) * 20.0);
    fragColor = vec4(u_Color * tex.rgb, tex.a * (0.6 + 0.4 * wave));
}
