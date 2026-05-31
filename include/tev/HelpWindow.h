// This file was developed by Thomas Müller <thomas94@gmx.net>.
// It is published under the BSD 3-Clause License within the LICENSE file.

#pragma once

#include <tev/Common.h>

#include <nanogui/window.h>

#include <string>

namespace nanogui {
class TextArea;
}

namespace tev {

class HelpWindow : public nanogui::Window {
public:
    HelpWindow(nanogui::Widget* parent, bool supportsHdr, std::function<void()> closeCallback);

    void draw(NVGcontext* ctx) override;
    bool keyboard_event(int key, int scancode, int action, int modifiers) override;

    static std::string COMMAND;
    static std::string ALT;

private:
    void refreshLog();

    std::function<void()> mCloseCallback;
    nanogui::TextArea* mLogTextArea = nullptr;
    size_t mDisplayedLogLineCount = static_cast<size_t>(-1);
};

}
