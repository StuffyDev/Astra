#pragma once
#include <glm/glm.hpp>
#include <string>

enum class SpriteType {
    None,
    Quad,
    Circle
};

enum class ColliderType {
    None,
    Box,
    Circle
};

struct Transform {
    glm::vec2 position = glm::vec2(0.0f);
    float rotation = 0.0f;
    glm::vec2 scale = glm::vec2(100.0f, 100.0f);
};

struct Sprite {
    SpriteType type = SpriteType::Quad;
    glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f);
    std::string texturePath;
    // Пользовательский шейдер: базовый путь без расширения (.vert/.frag рядом).
    // Пусто — системный шейдер движка.
    std::string shaderPath;
    // «Material»: 4 числа u_Params и цвет u_PColor — живые параметры шейдера из инспектора
    glm::vec4 materialParams = glm::vec4(0.0f, 1.0f, 0.5f, 1.0f);
    glm::vec4 materialColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    // Порядок отрисовки: меньшее — рисуется раньше (под остальными), как sortingOrder в Unity
    int sortingOrder = 0;
};

// покадровая анимация по сетке спрайтшита (row 0 = верхний ряд)
struct SpriteAnimation {
    bool active = false;       // крутится ли (в Edit — превью, в Play — по playOnAwake)
    std::string texturePath;   // спрайтшит; пусто — sprite.texturePath
    int cols = 1;
    int rows = 1;
    float fps = 8.0f;
    bool loop = true;
    bool playOnAwake = true;   // включать при входе в Play
};

struct Rigidbody {
    bool isKinematic = false;
    glm::vec2 velocity = glm::vec2(0.0f);
    float mass = 1.0f;
    float drag = 0.0f;
    bool useGravity = false;
};

struct Collider {
    ColliderType type = ColliderType::None;
    bool isTrigger = false;
    // Для Box: половина размера, для Circle: радиус
    glm::vec2 size = glm::vec2(50.0f, 50.0f);
    float radius = 50.0f;
};

struct CameraComponent {
    bool mainCamera = false;
    float zoom = 1.0f;
    glm::vec2 offset = glm::vec2(0.0f, 0.0f);
};

enum class UIKind {
    Button,
    Text,
    Slider,
    Checkbox,
    ProgressBar
};

// Экранная GUI-нода: рисуется оверлеем в Game-view.
// transform.position — мировые координаты (совпадают с gizmo в Scene),
// transform.scale — размер в мировых единицах.
struct UIComponent {
    UIKind kind = UIKind::Button;
    std::string label = "Button";
    float minValue = 0.0f;
    float maxValue = 1.0f;
    float value = 0.5f;
    bool interactable = true;
    // Стиль: цвета и шрифт. bgColor — фон кнопки/ползунка/полосы, textColor — текст.
    glm::vec4 textColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    glm::vec4 bgColor = glm::vec4(0.22f, 0.32f, 0.45f, 1.0f);
    // 0 = обычный шрифт, 1 = средний, 2 = крупный
    int fontScale = 0;
};

// Аналог Unity AudioSource: путь к клипу (wav/mp3/ogg/flac), playOnAwake запускает
// при входе в Play (и при создании инстанса со скриптом-носителем).
struct AudioSource {
    std::string path;
    float volume = 1.0f;   // 0..1, домножается на мастер-громкость
    float pitch = 1.0f;    // 0.1..3 — смена высоты/скорости
    bool loop = false;
    bool playOnAwake = true;
};
