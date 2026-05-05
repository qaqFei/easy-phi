#include <memory>
#include <functional>
#include <vector>
#include <iostream>
#include <variant>
#include <algorithm>
#include <windows.h>
#include <commctrl.h>

struct Win32Utils {
    static HFONT getFontFromHWnd(HWND hWnd) {
        HFONT hFont = (HFONT)SendMessage(hWnd, WM_GETFONT, 0, 0);
        if (!hFont) {
            hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        }
        return hFont;
    }

    static SIZE getTextSizeFromHdc(HDC hdc, const wchar_t* text) {
        SIZE size;
        GetTextExtentPoint32W(hdc, text, wcslen(text), &size);
        return size;
    }

    static SIZE getTextSizeFromHWnd(HWND hWnd, const wchar_t* text) {
        HFONT hFont = getFontFromHWnd(hWnd);
        HDC hdc = GetDC(hWnd);
        SelectObject(hdc, hFont);
        SIZE size = getTextSizeFromHdc(hdc, text);
        ReleaseDC(hWnd, hdc);
        return size;
    }

    static SIZE getWindowSize(HWND hWnd) {
        RECT rect;
        GetClientRect(hWnd, &rect);
        return { rect.right - rect.left, rect.bottom - rect.top };
    }

    static void resizeWindow(HWND hWnd, SIZE clientSize) {
        RECT rc = {0, 0, clientSize.cx, clientSize.cy};
        AdjustWindowRect(&rc, GetWindowLongW(hWnd, GWL_STYLE), FALSE);
        SetWindowPos(hWnd, NULL, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOMOVE | SWP_NOZORDER);
    }

    static std::string wstringToString(const std::wstring& ws) {
        if (ws.empty()) return {};
        
        int size_needed = WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            ws.data(), 
            static_cast<int>(ws.size()), 
            nullptr, 0, 
            nullptr, nullptr
        );
        
        if (size_needed == 0) return {};
        
        std::string result(size_needed, '\0');
        WideCharToMultiByte(
            CP_UTF8, WC_ERR_INVALID_CHARS,
            ws.data(), static_cast<int>(ws.size()),
            result.data(), size_needed,
            nullptr, nullptr
        );
        return result;
    }

    static std::wstring stringToWstring(const std::string& s) {
        if (s.empty()) return {};
        
        int size_needed = MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            s.data(), 
            static_cast<int>(s.size()),
            nullptr, 0
        );
        
        if (size_needed == 0) return {};
        
        std::wstring result(size_needed, L'\0');
        MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS,
            s.data(), static_cast<int>(s.size()),
            result.data(), size_needed
        );
        return result;
    }

    static std::wstring getWindowText(HWND hWnd) {
        int len = GetWindowTextLengthW(hWnd);
        std::wstring str;
        str.resize(len);
        GetWindowTextW(hWnd, str.data(), len + 1);
        return str;
    }
};

struct Win32WindowCreateConfig {
    std::wstring className = L"EASY_PHI_WINDOW_CLASS";
    std::wstring title = L"TITLE";
    int width = 800, height = 600;
};

struct Win32Window;

struct Widget {
    struct {
        Win32Window* parent;
        HWND hWnd;
        UINT id;
        bool isDeaf = false;

        struct {
            std::wstring text;
            std::function<void()> onClick;
            SIZE padding;
        } button;

        struct {
            std::wstring text;
        } label;

        struct {
            std::wstring text;
            bool checked;
            std::function<void(bool)> onChange;
        } checkBox;

        struct {
            std::wstring text;
            std::function<void(std::wstring)> onChange;
            std::function<void()> onUnfocus;
        } textInput;
    } store;

    struct CreationConfig {
        Win32Window* parent;
    };

    std::function<void(Widget*, CreationConfig)> creater;

    struct OnCommandConfig {
        WPARAM wParam;
        LPARAM lParam;
    };

    std::function<void(Widget*, OnCommandConfig)> onCommand;
    std::function<void(Widget*)> autoSizer;

    struct Deafer {
        Widget* widget;

        Deafer(Widget* widget) : widget(widget) {
            widget->store.isDeaf = true;
        }

        ~Deafer() {
            widget->store.isDeaf = false;
        }
    };
};

struct Win32Window {
    WNDCLASSW wc;
    HWND hWnd;
    UINT baseWidgetId = 1000;

    std::vector<Widget> widgets;

    struct {
        int padding = 5;
    } gridConfig;

    static std::shared_ptr<Win32Window> Make(const Win32WindowCreateConfig& config) {
        auto win = std::make_shared<Win32Window>();

        INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES };
        InitCommonControlsEx(&icc);

        win->wc.lpfnWndProc = [](HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT CALLBACK {
            static Win32Window* win = nullptr;

            switch (msg) {
                case WM_CREATE: {
                    auto cs = (CREATESTRUCT*)lParam;
                    win = (Win32Window*)cs->lpCreateParams;

                    return 0;
                }

                case WM_DESTROY: {
                    DestroyWindow(hWnd);
                    return 0;
                }

                case WM_COMMAND: {
                    for (auto& widget : win->widgets) {
                        if (!widget.store.isDeaf && widget.onCommand) {
                            widget.onCommand(&widget, {
                                .wParam = wParam,
                                .lParam = lParam
                            });
                        }
                    }
                    
                    return 0;
                }
            }

            return DefWindowProcW(hWnd, msg, wParam, lParam);
        };

        win->wc.hInstance = GetModuleHandle(NULL);
        win->wc.lpszClassName = config.className.c_str();
        win->wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        win->wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        RegisterClassW(&win->wc);

        win->hWnd = CreateWindowExW(
            0, config.className.c_str(), config.title.c_str(),
            WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX,
            CW_USEDEFAULT, CW_USEDEFAULT, config.width, config.height,
            NULL, NULL, win->wc.hInstance, win.get()
        );

        ShowWindow(win->hWnd, SW_SHOWDEFAULT);
        UpdateWindow(win->hWnd);

        return win;
    }

    UINT requestNewWidgetId() {
        return ++baseWidgetId;
    }

    int registerWidget(Widget widget) {
        widgets.emplace_back(std::move(widget));
        addWidgetToGrid(widgets.size() - 1);
        return widgets.size() - 1;
    }

    Widget& refWidget(int index) {
        return widgets[index];
    }

    void nextRow() {
        addNextRowToGrid();
    }

    void createWidgets() {
        for (auto& widget : widgets) {
            widget.creater(&widget, {
                .parent = this
            });
        }
    }

    void doGrid() {
        for (auto& widget : widgets) {
            if (widget.autoSizer) {
                widget.autoSizer(&widget);
            }
        }

        gridBounds = {0, 0};
        int x = gridConfig.padding, y = gridConfig.padding;
        int rowMaxHeight = 0;

        for (auto& item : gridInfo) {
            if (std::holds_alternative<GridInfoItem::GridWidget>(item.data)) {
                auto& widget = widgets[std::get<GridInfoItem::GridWidget>(item.data).index];
                SIZE size = Win32Utils::getWindowSize(widget.store.hWnd);
                MoveWindow(widget.store.hWnd, x, y, size.cx, size.cy, TRUE);
                rowMaxHeight = std::max<int>(rowMaxHeight, size.cy);
                gridBounds.cx = std::max<int>(gridBounds.cx, x + size.cx);
                gridBounds.cy = std::max<int>(gridBounds.cy, y + rowMaxHeight);
                x += size.cx + gridConfig.padding;
            } else if (std::holds_alternative<GridInfoItem::NextRow>(item.data)) {
                x = gridConfig.padding;
                y += rowMaxHeight + gridConfig.padding;
                rowMaxHeight = 0;
            }
        }

        gridBounds.cx += gridConfig.padding;
        gridBounds.cy += gridConfig.padding;
    }

    void resizeToGridBounds() {
        Win32Utils::resizeWindow(hWnd, gridBounds);
    }

    void setTitle(const std::wstring& title) {
        SetWindowTextW(hWnd, title.c_str());
    }

    void setHidden(bool value) {
        if (value) ShowWindow(hWnd, SW_HIDE);
        else ShowWindow(hWnd, SW_SHOW);
    }

    void quit() {
        PostQuitMessage(0);
    }

    void mainloop() {
        MSG msg;

        while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (!IsWindow(hWnd)) break;
        }
    }

    private:
    struct GridInfoItem {
        struct GridWidget { int index; };
        struct NextRow {};

        std::variant<GridWidget, NextRow> data;
    };

    std::vector<GridInfoItem> gridInfo;
    SIZE gridBounds;

    void addWidgetToGrid(int widgetIndex) {
        gridInfo.emplace_back(GridInfoItem {
            .data = GridInfoItem::GridWidget {
                .index = widgetIndex
            }
        });
    }

    void addNextRowToGrid() {
        gridInfo.emplace_back(GridInfoItem {
            .data = GridInfoItem::NextRow {}
        });
    }
};

struct WidgetStatics {
    static void sendWmCommand(Widget& widget, WORD loWord, LPARAM lParam = 0) {
        SendMessage(widget.store.parent->hWnd, WM_COMMAND, MAKEWPARAM(widget.store.id, loWord), lParam);
    }

    struct Button {
        static void click(Widget& widget) {
            sendWmCommand(widget, BN_CLICKED);
        }
    };

    struct CheckBox {
        static void toggle(Widget& widget, bool value) {
            SendMessage(widget.store.hWnd, BM_SETCHECK, value ? BST_CHECKED : BST_UNCHECKED, 0);
            sendWmCommand(widget, BN_CLICKED);
        }

        static void click(Widget& widget) {
            bool value = SendMessage(widget.store.hWnd, BM_GETCHECK, 0, 0) == BST_CHECKED;
            toggle(widget, !value);
        }
    };

    struct TextInput {
        static void setText(Widget& widget, const std::wstring& text) {
            DWORD selStart = 0, selEnd = 0;
            SendMessageW(widget.store.hWnd, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);
            int oldLen = GetWindowTextLengthW(widget.store.hWnd);
            SendMessageW(widget.store.hWnd, WM_SETTEXT, 0, (LPARAM)text.c_str());
            int newLen = text.length();

            if (selStart >= oldLen && selEnd >= oldLen) {
                selStart = selEnd = newLen;
            } else {
                selStart = std::min<DWORD>(selStart, newLen);
                selEnd = std::min<DWORD>(selEnd, newLen);
            }

            SendMessageW(widget.store.hWnd, EM_SETSEL, selStart, selEnd);
        }
    };
};

struct Widgets {
    struct ButtonConfig {
        std::wstring text = L"TEXT";
        std::function<void()> onClick;
        SIZE padding = {8, 2};
    };

    static Widget Button(const ButtonConfig& config) {
        return Widget {
            .store = {
                .button = {
                    .text = config.text,
                    .onClick = config.onClick,
                    .padding = config.padding
                }
            },

            .creater = [](Widget* self, Widget::CreationConfig createrConfig) {
                self->store.parent = createrConfig.parent;
                self->store.id = createrConfig.parent->requestNewWidgetId();
                self->store.hWnd = CreateWindowW(
                    L"BUTTON", self->store.button.text.c_str(),
                    BS_PUSHBUTTON | WS_VISIBLE | WS_CHILD | WS_TABSTOP,
                    0, 0, 0, 0,
                    createrConfig.parent->hWnd,
                    (HMENU)(UINT_PTR)self->store.id,
                    createrConfig.parent->wc.hInstance,
                    nullptr
                );
            },

            .onCommand = [](Widget* self, Widget::OnCommandConfig onCommandConfig) {
                if (LOWORD(onCommandConfig.wParam) == self->store.id) {
                    if (HIWORD(onCommandConfig.wParam) == BN_CLICKED) {
                        if (self->store.button.onClick) {
                            self->store.button.onClick();
                        }
                    }
                }
            },

            .autoSizer = [](Widget* self) {
                SIZE idealSize = {0, 0};

                if (!Button_GetIdealSize(self->store.hWnd, &idealSize)) {
                    SIZE textSize = Win32Utils::getTextSizeFromHWnd(self->store.hWnd, self->store.button.text.c_str());
                    idealSize = {textSize.cx + 16, textSize.cy + 12};
                }

                idealSize.cx += self->store.button.padding.cx * 2;
                idealSize.cy += self->store.button.padding.cy * 2;

                SetWindowPos(self->store.hWnd, NULL, 0, 0, idealSize.cx, idealSize.cy, SWP_NOMOVE | SWP_NOZORDER);
            }
        };
    }

    struct LabelConfig {
        std::wstring text = L"TEXT";
    };

    static Widget Label(const LabelConfig& config) {
        return Widget {
            .store = {
                .label = {
                    .text = config.text,
                }
            },

            .creater = [](Widget* self, Widget::CreationConfig createrConfig) {
                self->store.parent = createrConfig.parent;
                self->store.id = createrConfig.parent->requestNewWidgetId();
                self->store.hWnd = CreateWindowW(
                    L"STATIC", self->store.label.text.c_str(),
                    SS_CENTER | SS_CENTERIMAGE | WS_VISIBLE | WS_CHILD,
                    0, 0, 0, 0,
                    createrConfig.parent->hWnd,
                    (HMENU)(UINT_PTR)self->store.id,
                    createrConfig.parent->wc.hInstance,
                    nullptr
                );
            },

            .autoSizer = [](Widget* self) {
                SIZE idealSize = Win32Utils::getTextSizeFromHWnd(self->store.hWnd, self->store.label.text.c_str());
                idealSize.cx += 16;
                idealSize.cy += 12;
                SetWindowPos(self->store.hWnd, NULL, 0, 0, idealSize.cx, idealSize.cy, SWP_NOMOVE | SWP_NOZORDER);
            }
        };
    }

    struct CheckBoxConfig {
        std::wstring text = L"TEXT";
        bool checked = false;
        std::function<void(bool)> onChange;
    };

    static Widget CheckBox(const CheckBoxConfig& config) {
        bool initialChecked = config.checked;

        return Widget {
            .store = {
                .checkBox = {
                    .text = config.text,
                    .checked = config.checked,
                    .onChange = config.onChange
                }
            },

            .creater = [initialChecked](Widget* self, Widget::CreationConfig createrConfig) {
                self->store.parent = createrConfig.parent;
                self->store.id = createrConfig.parent->requestNewWidgetId();
                self->store.hWnd = CreateWindowW(
                    L"BUTTON", self->store.checkBox.text.c_str(),
                    BS_AUTOCHECKBOX | WS_VISIBLE | WS_CHILD | WS_TABSTOP,
                    0, 0, 0, 0,
                    createrConfig.parent->hWnd,
                    (HMENU)(UINT_PTR)self->store.id,
                    createrConfig.parent->wc.hInstance,
                    nullptr
                );

                WidgetStatics::CheckBox::toggle(*self, initialChecked);
            },

            .onCommand = [](Widget* self, Widget::OnCommandConfig onCommandConfig) {
                if (LOWORD(onCommandConfig.wParam) == self->store.id) {
                    if (HIWORD(onCommandConfig.wParam) == BN_CLICKED) {
                        self->store.checkBox.checked = SendMessageW(self->store.hWnd, BM_GETCHECK, 0, 0) == BST_CHECKED;

                        if (self->store.checkBox.onChange) {
                            self->store.checkBox.onChange(self->store.checkBox.checked);
                        }
                    }
                }
            },

            .autoSizer = [](Widget* self) {
                SIZE textSize = Win32Utils::getTextSizeFromHWnd(self->store.hWnd, self->store.checkBox.text.c_str());
                SIZE idealSize = {textSize.cx + 48, textSize.cy + 12};
                SetWindowPos(self->store.hWnd, NULL, 0, 0, idealSize.cx, idealSize.cy, SWP_NOMOVE | SWP_NOZORDER);
            }
        };
    }

    struct TextInputConfig {
        std::wstring text = L"TEXT";
        std::function<void(std::wstring)> onChange;
        int size = 24;
        std::function<void()> onUnfocus;
    };

    static Widget TextInput(const TextInputConfig& config) {
        int size = config.size;

        return Widget {
            .store = {
                .textInput = {
                    .text = config.text,
                    .onChange = config.onChange,
                    .onUnfocus = config.onUnfocus
                }
            },

            .creater = [](Widget* self, Widget::CreationConfig createrConfig) {
                self->store.parent = createrConfig.parent;
                self->store.id = createrConfig.parent->requestNewWidgetId();

                {
                    Widget::Deafer deafer(self);
                    self->store.hWnd = CreateWindowW(
                        L"EDIT", nullptr,
                        ES_LEFT | WS_BORDER | WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                        0, 0, 0, 0,
                        createrConfig.parent->hWnd,
                        (HMENU)(UINT_PTR)self->store.id,
                        createrConfig.parent->wc.hInstance,
                        nullptr
                    );
                }

                WidgetStatics::TextInput::setText(*self, self->store.textInput.text);
            },

            .onCommand = [](Widget* self, Widget::OnCommandConfig onCommandConfig) {
                if (LOWORD(onCommandConfig.wParam) == self->store.id) {
                    if (HIWORD(onCommandConfig.wParam) == EN_CHANGE) {
                        std::wstring text = Win32Utils::getWindowText(self->store.hWnd);
                        self->store.textInput.text = text;

                        if (self->store.textInput.onChange) {
                            self->store.textInput.onChange(text);
                        }
                    } else if (HIWORD(onCommandConfig.wParam) == EN_KILLFOCUS) {
                        if (self->store.textInput.onUnfocus) {
                            self->store.textInput.onUnfocus();
                        }
                    }
                }
            },

            .autoSizer = [size](Widget* self) {
                std::wstring text(size, L'X');
                SIZE idealSize = Win32Utils::getTextSizeFromHWnd(self->store.hWnd, text.c_str());
                idealSize.cx += 16;
                idealSize.cy += 12;
                SetWindowPos(self->store.hWnd, NULL, 0, 0, idealSize.cx, idealSize.cy, SWP_NOMOVE | SWP_NOZORDER);
            }
        };
    }
};

#ifdef WIN32UI_MAIN
int main() {
    auto win = Win32Window::Make({});

    win->registerWidget(Widgets::Button({
        .text = L"BUTTON 1!",
        .onClick = []() {
            std::cout << "1 CLICKED!" << std::endl;
        }
    }));

    int button2 = win->registerWidget(Widgets::Button({
        .text = L"BUTTON 2!",
        .onClick = []() {
            std::cout << "2 CLICKED!" << std::endl;
        }
    }));

    win->nextRow();

    win->registerWidget(Widgets::Button({
        .text = L"BUTTON 3!",
        .onClick = [&]() {
            std::cout << "3 CLICKED!, i will click button 2" << std::endl;
            WidgetStatics::Button::click(win->refWidget(button2));
        }
    }));

    win->registerWidget(Widgets::Label({
        .text = L"LABEL 1!"
    }));

    int checkbox1 = win->registerWidget(Widgets::CheckBox({
        .text = L"CHECKBOX 1!",
        .checked = true,
        .onChange = [](bool checked) {
            std::cout << "CHECKBOX 1: " << (checked ? "checked" : "unchecked") << std::endl;
        }
    }));

    win->nextRow();

    win->registerWidget(Widgets::Button({
        .text = L"BUTTON 4!",
        .onClick = [&](){
            std::cout << "4 CLICKED!, i will click checkbox 1" << std::endl;
            WidgetStatics::CheckBox::click(win->refWidget(checkbox1));
        }
    }));

    win->registerWidget(Widgets::TextInput({
        .text = L"TEXT INPUT 1!",
        .onChange = [&](const std::wstring& text) {
            std::wcout << "TEXT INPUT (W): " << text << std::endl;
            std::cout << "TEXT INPUT: " << Win32Utils::wstringToString(text) << std::endl;
        }
    }));

    win->createWidgets();
    win->doGrid();
    win->resizeToGridBounds();
    win->mainloop();
    return 0;
}
#endif
