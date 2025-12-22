#include "terraria/game/InventorySystem.h"

#include <algorithm>

namespace terraria::game {

InventorySystem::InventorySystem(const core::AppConfig& config, entities::Player& player, input::IInputSystem& input)
    : config_{config},
      player_{player},
      input_{input} {}

bool InventorySystem::isOpen() const {
    return inventoryOpen_;
}

void InventorySystem::setOpen(bool open) {
    inventoryOpen_ = open;
    if (!inventoryOpen_) {
        hoveredInventorySlot_ = -1;
        hoveredArmorSlot_ = -1;
        hoveredAccessorySlot_ = -1;
    }
}

bool InventorySystem::handleInput() {
    const auto& state = input_.state();
    if (!inventoryOpen_) {
        hoveredInventorySlot_ = -1;
        hoveredArmorSlot_ = -1;
        hoveredAccessorySlot_ = -1;
        ammoSlotHovered_ = false;
        trashSlotHovered_ = false;
        return false;
    }

    updateAmmoLayout();
    updateTrashLayout();
    ammoSlotHovered_ = state.mouseX >= ammoSlotRect_.x && state.mouseX < ammoSlotRect_.x + ammoSlotRect_.w
        && state.mouseY >= ammoSlotRect_.y && state.mouseY < ammoSlotRect_.y + ammoSlotRect_.h;
    trashSlotHovered_ = state.mouseX >= trashSlotRect_.x && state.mouseX < trashSlotRect_.x + trashSlotRect_.w
        && state.mouseY >= trashSlotRect_.y && state.mouseY < trashSlotRect_.y + trashSlotRect_.h;
    if (trashSlotHovered_ && state.inventoryClick) {
        return handleTrashSlotClick();
    }
    if (ammoSlotHovered_ && state.inventoryClick) {
        return handleAmmoSlotClick();
    }

    const bool equipmentHandled = handleEquipmentInput();

    hoveredInventorySlot_ = inventorySlotIndexAt(state.mouseX, state.mouseY);
    if (!equipmentHandled && state.inventoryClick && hoveredInventorySlot_ >= 0) {
        handleInventoryClick(hoveredInventorySlot_);
    }
    return equipmentHandled;
}

void InventorySystem::handleInventoryClick(int slotIndex) {
    auto& slots = player_.inventory();
    if (slotIndex < 0 || slotIndex >= static_cast<int>(slots.size())) {
        return;
    }

    auto& target = slots[static_cast<std::size_t>(slotIndex)];
    if (!carryingItem()) {
        if (!target.empty()) {
            carriedSlot_ = target;
            target.clear();
        }
        return;
    }

    if (target.empty()) {
        target = carriedSlot_;
        if (target.isBlock()) {
            const int moved = std::min(carriedSlot_.count, entities::kMaxStackCount);
            target.count = moved;
            carriedSlot_.count -= moved;
            if (carriedSlot_.count <= 0) {
                carriedSlot_.clear();
            }
        } else {
            carriedSlot_.clear();
        }
        return;
    }

    if (target.canStackWith(carriedSlot_)) {
        const int space = std::max(0, entities::kMaxStackCount - target.count);
        if (space > 0) {
            const int moved = std::min(space, carriedSlot_.count);
            target.count += moved;
            carriedSlot_.count -= moved;
            if (carriedSlot_.count <= 0) {
                carriedSlot_.clear();
            }
        }
        return;
    }

    std::swap(target, carriedSlot_);
}

int InventorySystem::inventorySlotIndexAt(int mouseX, int mouseY) const {
    if (!inventoryOpen_) {
        return -1;
    }
    const int slotWidth = rendering::kInventorySlotWidth;
    const int slotHeight = rendering::kInventorySlotHeight;
    const int spacing = rendering::kInventorySlotSpacing;
    const int margin = spacing;
    const int columns = entities::kHotbarSlots;
    const int totalRows = entities::kInventoryRows;
    const int startX = margin;
    const int startY = config_.windowHeight - slotHeight - margin;

    for (int row = 0; row < totalRows; ++row) {
        const int rowY = startY - row * (slotHeight + spacing);
        if (mouseY < rowY || mouseY >= rowY + slotHeight) {
            continue;
        }
        for (int col = 0; col < columns; ++col) {
            const int colX = startX + col * (slotWidth + spacing);
            if (mouseX >= colX && mouseX < colX + slotWidth) {
                return row * columns + col;
            }
        }
    }
    return -1;
}

void InventorySystem::updateAmmoLayout() {
    const int slotWidth = rendering::kInventorySlotWidth;
    const int slotHeight = rendering::kInventorySlotHeight;
    const int spacing = rendering::kInventorySlotSpacing;
    const int margin = spacing;
    const int columns = entities::kHotbarSlots;
    const int totalRows = entities::kInventoryRows;
    const int startX = margin;
    const int startY = config_.windowHeight - slotHeight - margin;
    const int ammoColumn = std::max(0, columns - 2);
    const int ammoX = startX + ammoColumn * (slotWidth + spacing);
    const int topRowY = startY - (totalRows - 1) * (slotHeight + spacing);
    const int ammoY = topRowY - (slotHeight + spacing);
    ammoSlotRect_ = SlotRect{ammoX, ammoY, slotWidth, slotHeight};
}

void InventorySystem::updateTrashLayout() {
    const int slotWidth = rendering::kInventorySlotWidth;
    const int slotHeight = rendering::kInventorySlotHeight;
    const int spacing = rendering::kInventorySlotSpacing;
    const int margin = spacing;
    const int columns = entities::kHotbarSlots;
    const int totalRows = entities::kInventoryRows;
    const int startX = margin;
    const int startY = config_.windowHeight - slotHeight - margin;
    const int trashColumn = std::max(0, columns - 1);
    const int trashX = startX + trashColumn * (slotWidth + spacing);
    const int topRowY = startY - (totalRows - 1) * (slotHeight + spacing);
    const int trashY = topRowY - (slotHeight + spacing);
    trashSlotRect_ = SlotRect{trashX, trashY, slotWidth, slotHeight};
}

bool InventorySystem::handleAmmoSlotClick() {
    auto& ammoSlot = player_.ammoSlot();
    if (!carryingItem()) {
        if (ammoSlot.empty()) {
            return true;
        }
        carriedSlot_ = ammoSlot;
        ammoSlot.clear();
        return true;
    }
    if (!carriedSlot_.isBlock() || carriedSlot_.blockType != world::TileType::Arrow) {
        return false;
    }
    if (ammoSlot.empty()) {
        ammoSlot = carriedSlot_;
        const int moved = std::min(carriedSlot_.count, entities::kMaxStackCount);
        ammoSlot.count = moved;
        carriedSlot_.count -= moved;
        if (carriedSlot_.count <= 0) {
            carriedSlot_.clear();
        }
        return true;
    }
    if (!ammoSlot.canStackWith(carriedSlot_)) {
        return false;
    }
    const int space = std::max(0, entities::kMaxStackCount - ammoSlot.count);
    if (space > 0) {
        const int moved = std::min(space, carriedSlot_.count);
        ammoSlot.count += moved;
        carriedSlot_.count -= moved;
        if (carriedSlot_.count <= 0) {
            carriedSlot_.clear();
        }
    }
    return true;
}

bool InventorySystem::handleTrashSlotClick() {
    if (!carryingItem()) {
        return true;
    }
    carriedSlot_.clear();
    return true;
}

bool InventorySystem::placeCarriedStack() {
    if (!carryingItem()) {
        return true;
    }

    auto& slots = player_.inventory();
    auto& ammoSlot = player_.ammoSlot();
    if (carriedSlot_.isBlock()) {
        if (carriedSlot_.blockType == world::TileType::Arrow) {
            if (ammoSlot.empty() || ammoSlot.canStackWith(carriedSlot_)) {
                const int space = ammoSlot.empty() ? entities::kMaxStackCount : std::max(0, entities::kMaxStackCount - ammoSlot.count);
                if (space > 0) {
                    if (ammoSlot.empty()) {
                        ammoSlot = carriedSlot_;
                        ammoSlot.count = 0;
                    }
                    const int moved = std::min(space, carriedSlot_.count);
                    ammoSlot.count += moved;
                    carriedSlot_.count -= moved;
                    if (carriedSlot_.count <= 0) {
                        carriedSlot_.clear();
                        return true;
                    }
                }
            }
        }
        for (auto& slot : slots) {
            if (slot.canStackWith(carriedSlot_)) {
                const int space = std::max(0, entities::kMaxStackCount - slot.count);
                if (space <= 0) {
                    continue;
                }
                const int moved = std::min(space, carriedSlot_.count);
                slot.count += moved;
                carriedSlot_.count -= moved;
                if (carriedSlot_.count <= 0) {
                    carriedSlot_.clear();
                    return true;
                }
            }
        }
    }

    for (auto& slot : slots) {
        if (!slot.empty()) {
            continue;
        }
        slot = carriedSlot_;
        if (slot.isBlock()) {
            const int moved = std::min(carriedSlot_.count, entities::kMaxStackCount);
            slot.count = moved;
            carriedSlot_.count -= moved;
            if (carriedSlot_.count <= 0) {
                carriedSlot_.clear();
                return true;
            }
        } else {
            carriedSlot_.clear();
            return true;
        }
    }
    return false;
}

bool InventorySystem::carryingItem() const {
    return !carriedSlot_.empty();
}

const entities::InventorySlot& InventorySystem::carriedSlot() const {
    return carriedSlot_;
}

int InventorySystem::hoveredInventorySlot() const {
    return hoveredInventorySlot_;
}

int InventorySystem::hoveredArmorSlot() const {
    return hoveredArmorSlot_;
}

int InventorySystem::hoveredAccessorySlot() const {
    return hoveredAccessorySlot_;
}

bool InventorySystem::ammoSlotHovered() const {
    return ammoSlotHovered_;
}

bool InventorySystem::trashSlotHovered() const {
    return trashSlotHovered_;
}

void InventorySystem::updateEquipmentLayout() {
    const int slotWidth = rendering::kInventorySlotWidth;
    const int slotHeight = rendering::kInventorySlotHeight;
    const int spacing = rendering::kInventorySlotSpacing;
    const int margin = spacing;
    const int rows = entities::kInventoryRows;
    const int inventoryTop = config_.windowHeight - slotHeight - margin - (rows - 1) * (slotHeight + spacing);
    const int equipmentX = margin;
    int equipmentY = inventoryTop - spacing - slotHeight;

    std::size_t index = 0;
    for (int i = 0; i < rendering::kArmorSlotCount; ++i) {
        equipmentRects_[index++] = {true, i, equipmentX + i * (slotWidth + spacing), equipmentY, slotWidth, slotHeight};
    }
    equipmentY -= slotHeight + spacing;
    for (int i = 0; i < rendering::kAccessorySlotCount; ++i) {
        equipmentRects_[index++] = {false, i, equipmentX + i * (slotWidth + spacing), equipmentY, slotWidth, slotHeight};
    }
}

int InventorySystem::equipmentSlotAt(int mouseX, int mouseY, bool armor) const {
    for (const auto& rect : equipmentRects_) {
        if (rect.isArmor != armor) {
            continue;
        }
        if (mouseX >= rect.x && mouseX < rect.x + rect.w && mouseY >= rect.y && mouseY < rect.y + rect.h) {
            return rect.slotIndex;
        }
    }
    return -1;
}

bool InventorySystem::handleEquipmentInput() {
    updateEquipmentLayout();
    const auto& state = input_.state();
    hoveredArmorSlot_ = equipmentSlotAt(state.mouseX, state.mouseY, true);
    hoveredAccessorySlot_ = equipmentSlotAt(state.mouseX, state.mouseY, false);
    if (!state.inventoryClick) {
        return false;
    }
    if (hoveredArmorSlot_ >= 0) {
        return handleArmorSlotClick(hoveredArmorSlot_);
    }
    if (hoveredAccessorySlot_ >= 0) {
        return handleAccessorySlotClick(hoveredAccessorySlot_);
    }
    return false;
}

bool InventorySystem::handleArmorSlotClick(int slotIndex) {
    if (slotIndex < 0 || slotIndex >= rendering::kArmorSlotCount) {
        return false;
    }
    const auto slot = static_cast<entities::ArmorSlot>(slotIndex);
    const entities::ArmorId equipped = player_.armorAt(slot);
    if (!carryingItem()) {
        if (equipped == entities::ArmorId::None) {
            return true;
        }
        carriedSlot_.clear();
        carriedSlot_.category = entities::ItemCategory::Armor;
        carriedSlot_.armorId = equipped;
        carriedSlot_.count = 1;
        player_.equipArmor(slot, entities::ArmorId::None);
        return true;
    }
    if (!carriedSlot_.isArmor()) {
        return false;
    }
    const entities::ArmorSlot carriedSlot = entities::ArmorSlotFor(carriedSlot_.armorId);
    if (carriedSlot != slot) {
        return false;
    }
    const entities::ArmorId carriedId = carriedSlot_.armorId;
    carriedSlot_.clear();
    if (equipped != entities::ArmorId::None) {
        carriedSlot_.category = entities::ItemCategory::Armor;
        carriedSlot_.armorId = equipped;
        carriedSlot_.count = 1;
    }
    player_.equipArmor(slot, carriedId);
    return true;
}

bool InventorySystem::handleAccessorySlotClick(int slotIndex) {
    if (slotIndex < 0 || slotIndex >= rendering::kAccessorySlotCount) {
        return false;
    }
    const entities::AccessoryId equipped = player_.accessoryAt(slotIndex);
    if (!carryingItem()) {
        if (equipped == entities::AccessoryId::None) {
            return true;
        }
        carriedSlot_.clear();
        carriedSlot_.category = entities::ItemCategory::Accessory;
        carriedSlot_.accessoryId = equipped;
        carriedSlot_.count = 1;
        player_.equipAccessory(slotIndex, entities::AccessoryId::None);
        return true;
    }
    if (!carriedSlot_.isAccessory()) {
        return false;
    }
    const entities::AccessoryId carriedId = carriedSlot_.accessoryId;
    carriedSlot_.clear();
    if (equipped != entities::AccessoryId::None) {
        carriedSlot_.category = entities::ItemCategory::Accessory;
        carriedSlot_.accessoryId = equipped;
        carriedSlot_.count = 1;
    }
    player_.equipAccessory(slotIndex, carriedId);
    return true;
}

void InventorySystem::fillHud(rendering::HudState& hud) {
    hud.inventoryOpen = inventoryOpen_;
    hud.hoveredInventorySlot = inventoryOpen_ ? hoveredInventorySlot_ : -1;
    hud.carryingItem = carryingItem();
    if (hud.carryingItem) {
        hud.carriedItem = {};
        if (carriedSlot_.isTool()) {
            hud.carriedItem.isTool = true;
            hud.carriedItem.toolKind = carriedSlot_.toolKind;
            hud.carriedItem.toolTier = carriedSlot_.toolTier;
            if (carriedSlot_.toolKind == entities::ToolKind::Bow) {
                hud.carriedItem.count = player_.inventoryCount(world::TileType::Arrow);
            } else {
                hud.carriedItem.count = carriedSlot_.count;
            }
        } else if (carriedSlot_.isArmor()) {
            hud.carriedItem.isArmor = true;
            hud.carriedItem.armorId = carriedSlot_.armorId;
            hud.carriedItem.count = carriedSlot_.count;
        } else if (carriedSlot_.isAccessory()) {
            hud.carriedItem.isAccessory = true;
            hud.carriedItem.accessoryId = carriedSlot_.accessoryId;
            hud.carriedItem.count = carriedSlot_.count;
        } else if (carriedSlot_.isBlock()) {
            hud.carriedItem.isTool = false;
            hud.carriedItem.tileType = carriedSlot_.blockType;
            hud.carriedItem.count = carriedSlot_.count;
        }
    } else {
        hud.carriedItem = {};
    }

    updateAmmoLayout();
    updateTrashLayout();
    hud.ammoSlot = {};
    const auto& ammoSlot = player_.ammoSlot();
    if (ammoSlot.isBlock()) {
        hud.ammoSlot.tileType = ammoSlot.blockType;
        hud.ammoSlot.count = ammoSlot.count;
    }
    hud.ammoSlotX = ammoSlotRect_.x;
    hud.ammoSlotY = ammoSlotRect_.y;
    hud.ammoSlotW = ammoSlotRect_.w;
    hud.ammoSlotH = ammoSlotRect_.h;
    hud.ammoSlotHovered = ammoSlotHovered_;
    hud.ammoSlotVisible = inventoryOpen_;
    hud.trashSlotX = trashSlotRect_.x;
    hud.trashSlotY = trashSlotRect_.y;
    hud.trashSlotW = trashSlotRect_.w;
    hud.trashSlotH = trashSlotRect_.h;
    hud.trashSlotHovered = trashSlotHovered_;
    hud.trashSlotVisible = inventoryOpen_;

    if (inventoryOpen_) {
        updateEquipmentLayout();
        hud.equipmentSlotCount = rendering::kTotalEquipmentSlots;
        for (std::size_t i = 0; i < hud.equipmentSlots.size(); ++i) {
            auto& slotHud = hud.equipmentSlots[i];
            const auto& rect = equipmentRects_[i];
            slotHud = {};
            slotHud.isArmor = rect.isArmor;
            slotHud.slotIndex = rect.slotIndex;
            slotHud.x = rect.x;
            slotHud.y = rect.y;
            slotHud.w = rect.w;
            slotHud.h = rect.h;
            if (rect.isArmor) {
                slotHud.armorId = player_.armorAt(static_cast<entities::ArmorSlot>(rect.slotIndex));
                slotHud.occupied = slotHud.armorId != entities::ArmorId::None;
                slotHud.hovered = (hoveredArmorSlot_ == rect.slotIndex);
            } else {
                slotHud.accessoryId = player_.accessoryAt(rect.slotIndex);
                slotHud.occupied = slotHud.accessoryId != entities::AccessoryId::None;
                slotHud.hovered = (hoveredAccessorySlot_ == rect.slotIndex);
            }
        }
    } else {
        hud.equipmentSlotCount = 0;
        for (auto& slot : hud.equipmentSlots) {
            slot = {};
        }
    }
}

} // namespace terraria::game
