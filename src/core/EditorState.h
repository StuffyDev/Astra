#pragma once

enum class EditorState {
    Edit,   // сцена статична, всё можно править
    Play,   // симуляция (физика, логика), правки запрещены
    Pause,  // мир заморожен на кадре Play
};
