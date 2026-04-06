#include "engine/world/Entity.h"

void EntityManager::Clear() {
    nextId_ = 1;
    entities_.clear();
}

Entity& EntityManager::CreateEntity(EntityType type, const Vec2& position) {
    Entity entity{};
    entity.id = nextId_++;
    entity.type = type;
    entity.position = position;
    entities_.push_back(entity);
    return entities_.back();
}

bool EntityManager::DestroyEntity(int id) {
    for (auto it = entities_.begin(); it != entities_.end(); ++it) {
        if (it->id == id) {
            entities_.erase(it);
            return true;
        }
    }
    return false;
}

Entity* EntityManager::GetEntityById(int id) {
    for (Entity& entity : entities_) {
        if (entity.id == id) {
            return &entity;
        }
    }
    return nullptr;
}

const Entity* EntityManager::GetEntityById(int id) const {
    for (const Entity& entity : entities_) {
        if (entity.id == id) {
            return &entity;
        }
    }
    return nullptr;
}

std::vector<Entity>& EntityManager::Entities() {
    return entities_;
}

const std::vector<Entity>& EntityManager::Entities() const {
    return entities_;
}
