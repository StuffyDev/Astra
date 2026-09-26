// «Чёрная дыра» — демо шейдерного API Astra.
// Нужен только .frag: геометрию и varying v_UV (0..1 по кваду) даёт движок.
// Из преамбулы движка здесь используются: u_Time, u_Color, fragColor,
// EngineFbm (фрактальный шум) и EnginePalette (палитра ИК Квентина).
void main() {
    vec2 p = v_UV * 2.0 - 1.0;          // центр квада — (0,0)
    float r = length(p);
    float a = atan(p.y, p.x);

    // Диск аккреци: угловая скорость растёт при приближении к горизонту
    // (дифференциальное вращение) + шумовые полосяные структуры.
    float twist = 1.0 / (r + 0.18);
    float diskAngle = a + u_Time * twist * 0.9;
    float n = EngineFbm(vec2(diskAngle * 2.5, r * 7.0 - u_Time * 0.35), 4);

    float inDisk = smoothstep(0.30, 0.40, r) * (1.0 - smoothstep(0.80, 1.10, r));
    vec3 hot = EnginePalette(n * 0.8 + 0.2,
                             vec3(1.0, 0.78, 0.55),
                             vec3(0.60, 0.42, 0.30),
                             vec3(1.0, 1.0, 0.9),
                             vec3(0.0, 0.12, 0.32));

    // Чем ближе к горизонту — тем ярче (доплеровский блик грубо поправлен)
    float brightness = inDisk * (0.30 + 1.3 * n) * (0.45 + 1.1 / (r + 0.4));

    // Фотонное кольцо — тонкая светящаяся окантовка горизонта
    float ring = exp(-pow((r - 0.30) * 30.0, 2.0)) * 1.5;

    // Горизонт событий — абсолютная тьма
    float hole = 1.0 - smoothstep(0.27, 0.31, r);

    vec3 col = hot * brightness + vec3(1.0, 0.88, 0.62) * ring;
    col = mix(col, vec3(0.0), hole);
    float alpha = clamp(brightness + ring + hole, 0.0, 1.0);

    fragColor = vec4(col * (u_Color * 0.5 + 0.5), alpha);
}
