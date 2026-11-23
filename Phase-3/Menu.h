#pragma once
// The logic of a menu: a list of items, a cursor that wraps around, and items that can be switched off
// (for example "Continue" when there is no saved game). Plain C++ with no Qt, so it is tested with the engine tests.
// The game window only draws it and passes it key presses.
#include <string>
#include <utility>
#include <vector>

struct MenuItem {
    std::string label;
    bool enabled = true;
};

class Menu {
public:
    Menu() = default;
    explicit Menu(std::vector<MenuItem> items) : items_(std::move(items)) { index_ = firstEnabled(); }

    const std::vector<MenuItem>& items() const { return items_; }
    size_t index() const { return index_; }
    bool empty() const { return items_.empty(); }
    const MenuItem& selected() const { return items_[index_]; }

    // Move the cursor, wrapping around and skipping items that are switched off. If every item is off, nothing moves.
    void next() { step(false); }
    void prev() { step(true); }

    void setLabel(size_t i, std::string label) { if (i < items_.size()) items_[i].label = std::move(label); }
    void setEnabled(size_t i, bool on) {
        if (i >= items_.size()) return;
        items_[i].enabled = on;
        if (!items_[index_].enabled) index_ = firstEnabled();     // the cursor never rests on a switched-off item
    }

private:
    size_t firstEnabled() const {
        for (size_t i = 0; i < items_.size(); ++i) if (items_[i].enabled) return i;
        return 0;
    }
    void step(bool backwards) {
        const size_t n = items_.size();
        if (n == 0) return;
        bool any = false;
        for (const auto& it : items_) any = any || it.enabled;
        if (!any) return;
        do index_ = (index_ + (backwards ? n - 1 : 1)) % n; while (!items_[index_].enabled);
    }

    std::vector<MenuItem> items_;
    size_t index_ = 0;
};
