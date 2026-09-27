// Прыгающий куб для 3D-сцены: гравитация движка + импульс, когда под объектом есть пол.
// Демонстрирует 3D-хелперы Script: Position3D/Velocity3D/SetVelocity3D/Raycast3D.
class Bounce3D : public Script {
public:
    void Start() override {
        DefineVar("kick", 900.0f);      // начальная скорость прыжка, единиц/с
        DefineVar("probe", 70.0f);      // насколько далеко вниз искать опору
        SetGravityEnabled3D(true);
    }

    void Update(float dt) override {
        (void)dt;
        if (!Is3D()) return;

        RayHit3D hit;
        bool grounded = Raycast3D(Position3D(), glm::vec3(0.0f, -1.0f, 0.0f), GetVar("probe"), hit);
        if (grounded && Velocity3D().y <= 1.0f)
            SetVelocity3D(glm::vec3(Velocity3D().x, GetVar("kick"), Velocity3D().z));

        // чуть-чуть крутим, чтобы был виден направленный свет и тени
        glm::vec3 r = Rotation3D();
        r.y += dt * 20.0f;
        SetRotation3D(r);
    }

    void OnCollisionEnter(uint32_t otherId) override {
        Log("приземлился на " + std::to_string(otherId));
    }
};
SCRIPT_ENTRY(Bounce3D)
