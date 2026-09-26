// Временный тест Scripting: компиляция+dlopen+Start/Update/OnDestroy на rotate.cpp
#include "core/Scripting.h"
#include "ecs/SceneManager.h"
#include <cstdio>

int main() {
    SceneManager sm;
    Entity e;
    e.name = "Rotator";
    e.scriptPath = "assets/scripts/rotate.cpp";
    sm.AddEntity(e);

    Scripting::SetScene(&sm);
    Scripting::LoadForScene(sm.GetEntities());
    printf("errors=%zu\n", Scripting::Errors().size());
    for (auto& err : Scripting::Errors()) printf("%s\n", err.c_str());

    uint32_t id = sm.GetEntities()[0].id; // AddEntity выдаёт реальный id копии
    Scripting::SyncInstances(sm.GetEntities());
    printf("instances=%d\n", Scripting::InstanceFor(id) ? 1 : 0);
    for (int i = 0; i < 10; i++) Scripting::Update(0.1f, sm.GetEntities());
    printf("rotation=%.1f (expect ~90)\n", sm.GetEntities()[0].transform.rotation);

    sm.RemoveEntity(0);
    Scripting::SyncInstances(sm.GetEntities());
    printf("after-destroy instances=%d (expect 0)\n", Scripting::InstanceFor(id) ? 1 : 0);
    Scripting::Unload();
    return 0;
}
