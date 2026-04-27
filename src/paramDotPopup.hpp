#ifndef PARAM_DOT_POPUP_HPP
#define PARAM_DOT_POPUP_HPP

#include <FL/Fl_Value_Input.H>
#include "modernButton.hpp"
#include "basePopup.hpp"
#include <functional>

class ParamDotPopup : public BasePopup {
public:
    ParamDotPopup();

    void open(int wx, int wy, int value, bool isAnchor,
              std::function<void(int)> onOk,
              std::function<void()>    onDelete);

    int  handle(int event) override;
    void hide() override;

private:
    void doOk();
    void doDelete();

    bool            committed = false;
    Fl_Value_Input* input     = nullptr;
    ModernButton*   deleteBtn = nullptr;

    std::function<void(int)> onOkCb;
    std::function<void()>    onDeleteCb;
};

#endif
