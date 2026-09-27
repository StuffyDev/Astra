// 3D-материал для движка: достаточно одного .frag — вершинную часть даёт движок
// (EngineMeshVert). Хелперы и униформы описаны в documentation/*/SHADER_API.md.
void main() {
    vec3 n = normalize(v_Normal);
    vec3 base = EngineBaseColor().rgb;

    // Lambert + солнце + тень + ambient — то же, что считает системный шейдер
    vec3 col = EngineLit(base, n, v_World);

    // френелевская кромка и блик по направлению к солнцу
    col += vec3(0.35, 0.60, 1.00) * EngineFresnel(n, 2.5) * u_Params.x;
    col += vec3(1.00, 0.85, 0.60) * EngineSpecular(n, 48.0) * u_Params.y;

    // лёгкий градиент по высоте + подмешивание цвета материала u_PColor
    col *= mix(1.0, 0.75 + v_World.y * 0.0009, u_Params.z);
    col = mix(col, u_PColor.rgb * (0.4 + EngineLambert(n)), u_PColor.a * 0.6);

    fragColor = vec4(col, 1.0);
}
