#ifndef PARAM_DOT_POPUP_HPP
#define PARAM_DOT_POPUP_HPP

#include <FL/Fl_Value_Input.H>
#include "modernButton.hpp"
#include "modern/inputEditorPopup.hpp"
#include <functional>

class ParamDotPopup : public InputEditorPopup {
public:
    ParamDotPopup();

    void open(int wx, int wy, int value, bool isAnchor, int maxVal,
              std::function<void(int)> onOk,
              std::function<void()>    onDelete);

private:
    void doOk() override;
    void doDelete();

    Fl_Value_Input* input     = nullptr;
    ModernButton*   deleteBtn = nullptr;

    std::function<void(int)> onOkCb;
    std::function<void()>    onDeleteCb;
};

#endif
