#pragma once

#include "terraria/core/Application.h"
#include "terraria/entities/Player.h"
#include "terraria/input/InputSystem.h"
#include "terraria/rendering/HudState.h"

#include <array>

namespace terraria::game {

class InventorySystem {
public:
    InventorySystem(const core::AppConfig& config, entities::Player& player, input::IInputSystem& input);

    bool isOpen() const;
    void setOpen(bool open);
    bool handleInput();
    bool placeCarriedStack();
    bool carryingItem() const;
    const entities::InventorySlot& carriedSlot() const;
    int hoveredInventorySlot() const;
    int hoveredArmorSlot() const;
    int hoveredAccessorySlot() const;
    bool ammoSlotHovered() const;
    bool trashSlotHovered() const;

    void fillHud(rendering::HudState& hud);

private:
    struct SlotRect {
        int x{0};
        int y{0};
        int w{0};
        int h{0};
    };
    struct EquipmentSlotRect {
        bool isArmor{false};
        int slotIndex{0};
        int x{0};
        int y{0};
        int w{0};
        int h{0};
    };

    void handleInventoryClick(int slotIndex);
    int inventorySlotIndexAt(int mouseX, int mouseY) const;
    void updateAmmoLayout();
    bool handleAmmoSlotClick();
    void updateTrashLayout();
    bool handleTrashSlotClick();
    void updateEquipmentLayout();
    bool handleEquipmentInput();
    bool handleArmorSlotClick(int slotIndex);
    bool handleAccessorySlotClick(int slotIndex);
    int equipmentSlotAt(int mouseX, int mouseY, bool armor) const;

    const core::AppConfig& config_;
    entities::Player& player_;
    input::IInputSystem& input_;
    bool inventoryOpen_{false};
    int hoveredInventorySlot_{-1};
    entities::InventorySlot carriedSlot_{};
    std::array<EquipmentSlotRect, rendering::kTotalEquipmentSlots> equipmentRects_{};
    int hoveredArmorSlot_{-1};
    int hoveredAccessorySlot_{-1};
    SlotRect ammoSlotRect_{};
    bool ammoSlotHovered_{false};
    SlotRect trashSlotRect_{};
    bool trashSlotHovered_{false};
};

} // namespace terraria::game
