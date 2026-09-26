// Управление персонажем: стрелки/WASD через Input. Инклюды движок подключает сам.
class PlayerScript : public Script {
public:
    void Start() override {
        Input& in = Input::Get();
        in.BindKey("Left", GLFW_KEY_A);  in.BindKey("Left", GLFW_KEY_LEFT);
        in.BindKey("Right", GLFW_KEY_D); in.BindKey("Right", GLFW_KEY_RIGHT);
        in.BindKey("Down", GLFW_KEY_S);  in.BindKey("Down", GLFW_KEY_DOWN);
        in.BindKey("Up", GLFW_KEY_W);    in.BindKey("Up", GLFW_KEY_UP);
        in.BindAxis("MoveX", "Left", "Right");
        in.BindAxis("MoveY", "Down", "Up");
    }

    void Update(float dt) override {
        if (!Owner()) return;
        Input& in = Input::Get();
        glm::vec2 move(in.GetAxis("MoveX"), in.GetAxis("MoveY"));
        Translate(move * Speed * dt);
    }

    float Speed = 300.0f;
};

SCRIPT_ENTRY(PlayerScript)
