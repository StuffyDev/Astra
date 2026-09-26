// Пример шейдера для API движка: math уже объявлен (u_MVP, u_Model, u_Time, EngineUV()...),
// здесь — только main и свои униформы.
uniform float u_Amplitude = 6.0;
uniform float u_Frequency = 0.05;

out vec2 vUV;

void main() {
    vec2 p = a_Pos;
    p.y += sin(p.x * u_Frequency + u_Time * 3.0) * u_Amplitude * 0.01;
    vUV = EngineUV();
    gl_Position = u_MVP * vec4(p, 0.0, 1.0);
}
