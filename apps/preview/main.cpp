#include "../common/host.hpp"
#include <chrono>
#include <thread>
#include <voxelui/software.hpp>
#ifdef VOXELUI_SDL
#include <SDL3/SDL.h>
#endif
int main(int argc, char **argv) {
    using namespace voxelui;
    if (argc < 2) {
        std::cerr << "Usage: voxelui-preview document.xml [--headless output.png] [--width N --height N "
                     "--scale N --debug --frames N --root examples]\n";
        return 2;
    }
    try {
        std::filesystem::path file = argv[1], root = demo::rootFor(file);
        std::string output;
        FrameInfo frame;
        bool debug = false;
        int frames = 0;
        for (int i = 2; i < argc; ++i) {
            std::string arg = argv[i];
            auto value = [&]() {
                if (i + 1 >= argc)
                    throw std::runtime_error("Missing value for " + arg);
                return std::string(argv[++i]);
            };
            if (arg == "--headless")
                output = value();
            else if (arg == "--width")
                frame.viewport.x = std::stof(value());
            else if (arg == "--height")
                frame.viewport.y = std::stof(value());
            else if (arg == "--scale")
                frame.uiScale = std::stof(value());
            else if (arg == "--frames")
                frames = std::stoi(value());
            else if (arg == "--root")
                root = value();
            else if (arg == "--debug")
                debug = true;
            else
                throw std::runtime_error("Unknown option " + arg);
        }
        Runtime ui;
        demo::mount(ui, root);
        demo::mockData(ui);
        demo::actions(ui);
        ui.update(frame);
        if (!ui.load(demo::screenId(file, root))) {
            demo::diagnostics(ui.diagnostics());
            return 1;
        }
        demo::diagnostics(ui.diagnostics());
        SoftwareRenderer software;
        if (!output.empty()) {
            ui.update(frame);
            software.render(ui.buildDrawList(debug));
            if (!software.savePng(output))
                return 1;
            std::cout << "Rendered " << software.width() << "x" << software.height() << " to " << output
                      << '\n';
            return 0;
        }
#ifdef VOXELUI_SDL
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            std::cerr << SDL_GetError() << '\n';
            return 1;
        }
        auto *window =
            SDL_CreateWindow("VoxelUI preview — F3 bounds / F5 reload / +/- scale", int(frame.viewport.x),
                             int(frame.viewport.y), SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
        if (!window) {
            std::cerr << SDL_GetError() << '\n';
            SDL_Quit();
            return 1;
        }
        auto *renderer = SDL_CreateRenderer(window, nullptr);
        if (!renderer) {
            std::cerr << SDL_GetError() << '\n';
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }
        SDL_StartTextInput(window);
        ui.clipboard(
            []() {
                char *p = SDL_GetClipboardText();
                std::string text = p ? p : "";
                SDL_free(p);
                return text;
            },
            [](std::string text) { SDL_SetClipboardText(text.c_str()); });
        auto key = [](SDL_Keycode k) {
            switch (k) {
            case SDLK_TAB:
                return Key::tab;
            case SDLK_RETURN:
                return Key::enter;
            case SDLK_SPACE:
                return Key::space;
            case SDLK_ESCAPE:
                return Key::escape;
            case SDLK_LEFT:
                return Key::left;
            case SDLK_RIGHT:
                return Key::right;
            case SDLK_HOME:
                return Key::home;
            case SDLK_END:
                return Key::end;
            case SDLK_BACKSPACE:
                return Key::backspace;
            case SDLK_DELETE:
                return Key::del;
            case SDLK_A:
                return Key::a;
            case SDLK_C:
                return Key::c;
            case SDLK_V:
                return Key::v;
            case SDLK_X:
                return Key::x;
            case SDLK_UP:
                return Key::up;
            case SDLK_DOWN:
                return Key::down;
            default:
                return Key::none;
            }
        };
        SDL_Texture *texture = nullptr;
        int tw = 0, th = 0;
        bool running = true;
        Vec2 mouse;
        int count = 0;
        auto reloadAt = std::chrono::steady_clock::now();
        while (running) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                Event e;
                bool send = true;
                auto mods = SDL_GetModState();
                e.control = (mods & SDL_KMOD_CTRL) != 0;
                e.shift = (mods & SDL_KMOD_SHIFT) != 0;
                switch (event.type) {
                case SDL_EVENT_QUIT:
                    running = false;
                    send = false;
                    break;
                case SDL_EVENT_MOUSE_MOTION:
                    e.type = EventType::pointerMove;
                    e.position = mouse = {event.motion.x, event.motion.y};
                    break;
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                case SDL_EVENT_MOUSE_BUTTON_UP:
                    e.type = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? EventType::pointerDown
                                                                       : EventType::pointerUp;
                    e.position = mouse = {event.button.x, event.button.y};
                    e.button = event.button.button;
                    break;
                case SDL_EVENT_MOUSE_WHEEL:
                    e.type = EventType::wheel;
                    e.position = mouse;
                    e.delta = event.wheel.y;
                    break;
                case SDL_EVENT_TEXT_INPUT:
                    e.type = EventType::textInput;
                    e.text = event.text.text;
                    break;
                case SDL_EVENT_TEXT_EDITING:
                    e.type = EventType::composition;
                    e.text = event.edit.text;
                    break;
                case SDL_EVENT_KEY_DOWN:
                case SDL_EVENT_KEY_UP:
                    e.type = event.type == SDL_EVENT_KEY_DOWN ? EventType::keyDown : EventType::keyUp;
                    e.key = key(event.key.key);
                    if (event.type == SDL_EVENT_KEY_DOWN) {
                        if (event.key.key == SDLK_F3) {
                            debug = !debug;
                            send = false;
                        }
                        if (event.key.key == SDLK_F5) {
                            ui.reload();
                            demo::diagnostics(ui.diagnostics());
                            send = false;
                        }
                        if (event.key.key == SDLK_EQUALS || event.key.key == SDLK_MINUS) {
                            frame.uiScale = std::clamp(
                                frame.uiScale + (event.key.key == SDLK_EQUALS ? .1f : -.1f), .25f, 4.f);
                            send = false;
                        }
                    }
                    break;
                case SDL_EVENT_WINDOW_FOCUS_LOST:
                    e.type = EventType::focusLost;
                    break;
                default:
                    send = false;
                }
                if (send)
                    ui.processEvent(e);
            }
            int w, h, pw, ph;
            SDL_GetWindowSize(window, &w, &h);
            SDL_GetWindowSizeInPixels(window, &pw, &ph);
            frame.viewport = {float(w), float(h)};
            frame.dpiScale = w > 0 ? float(pw) / w : 1;
            frame.deltaSeconds = 1.f / 60;
            auto now = std::chrono::steady_clock::now();
            if (now - reloadAt > std::chrono::milliseconds(400)) {
                if (ui.resources().refresh()) {
                    ui.update(frame);
                    demo::diagnostics(ui.diagnostics());
                    std::string title =
                        hasErrors(ui.diagnostics())
                            ? "VoxelUI — reload failed; previous screen retained (see console)"
                            : "VoxelUI — F3 bounds / F5 reload / +/- scale";
                    SDL_SetWindowTitle(window, title.c_str());
                }
                reloadAt = now;
            }
            ui.update(frame);
            software.render(ui.buildDrawList(debug));
            if (!texture || tw != software.width() || th != software.height()) {
                if (texture)
                    SDL_DestroyTexture(texture);
                tw = software.width();
                th = software.height();
                texture =
                    SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, tw, th);
                if (!texture)
                    throw std::runtime_error(SDL_GetError());
            }
            SDL_UpdateTexture(texture, nullptr, software.pixels().data(), tw * 4);
            SDL_RenderClear(renderer);
            SDL_RenderTexture(renderer, texture, nullptr, nullptr);
            SDL_RenderPresent(renderer);
            if (frames > 0 && ++count >= frames)
                running = false;
            SDL_Delay(8);
        }
        if (texture)
            SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 0;
#else
        std::cerr << "Built without SDL3; use --headless output.png\n";
        return 2;
#endif
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
