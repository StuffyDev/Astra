// Вечное вращение объекта. Инклюды движок подключает сам.
class RotateScript : public Script {
public:
    void Update(float dt) override {
        Entity* e = Owner();
        if (!e) return;
        e->transform.rotation += 90.0f * dt;
    }
};

SCRIPT_ENTRY(RotateScript)
