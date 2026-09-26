// Демо: планета на круговой орбите вокруг начала координат (там чёрная дыра).
// Скрипт не пишет инклюды — движок подставляет их сам при компиляции на Play.
// Радиус и стартовый угол берутся из позиции, выставленной в редакторе.
class OrbitScript : public Script {
public:
    float radius = 0.0f;
    float angle = 0.0f;      // градусы
    float speed = 45.0f;     // градусов в секунду
    bool spin = true;        // показать LookAt: планета «смотрит» верхом на дыру

    void Start() override {
        Entity* e = Owner();
        if (!e) return;
        glm::vec2 p = WorldPosition();
        radius = glm::length(p);
        angle = glm::degrees(std::atan2(p.y, p.x));
    }

    void Update(float dt) override {
        angle += speed * dt;
        float r = glm::radians(angle);
        SetWorldPosition(glm::vec2(std::cos(r), std::sin(r)) * radius);
        if (spin) LookAt(glm::vec2(0.0f));
    }
};

SCRIPT_ENTRY(OrbitScript)
