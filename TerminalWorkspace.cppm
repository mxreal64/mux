module; // Global header wrapper for standard library fallback
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <string>
#include <vector>

export module TerminalWorkspace;

import MuxParser; 
import std;

export namespace MuxUI {
    class Workspace {
    private:
        std::string text_buffer_;
        std::string status_message_ = " [Ctrl+T] Toggle View | [Ctrl+S] Save | [Ctrl+Q] Quit ";
        
        // Interactive Saving States
        bool save_mode_active_ = false;
        std::string filename_buffer_ = "output.md";

        MuxEngine engine_;
        int active_tab_ = 0; // 0 = Editor, 1 = Fullscreen Preview

        void execute_save() noexcept {
            if (filename_buffer_.empty()) {
                filename_buffer_ = "output.md";
            }
            std::ofstream file(filename_buffer_);
            if (file) {
                file << text_buffer_;
                status_message_ = " Successfully saved to " + filename_buffer_ + "! ";
            } else {
                status_message_ = " Error: Could not save target file! ";
            }
            save_mode_active_ = false;
        }

    public:
        Workspace() {
            text_buffer_ = "# Project Mux\n\nBare-metal C++26 text compilation processing.\n\n## Sub-Heading Layout\n- Light\n- Fast\n- No Chromium\n\nPress Ctrl+T to toggle your screen view seamlessly!";
            text_buffer_.reserve(1024 * 16);
        }

        void run() noexcept {
            auto screen = ftxui::ScreenInteractive::Fullscreen();
            screen.TrackMouse(true); 

            // Left Input Panel Component
            ftxui::Component input_field = ftxui::Input(&text_buffer_, "Enter markdown text...");

            // Custom Interactive Saving Box Component
            ftxui::Component save_input = ftxui::Input(&filename_buffer_, "filename.md");

            auto input_window = ftxui::Renderer(input_field, [input_field] {
                return ftxui::window(ftxui::text(" MUX EDITOR ") | ftxui::bold, input_field->Render());
            });

            // Right View Parser Rendering Engine (Updates live automatically on text change)
            auto preview_pane = ftxui::Renderer([this] {
                engine_.parse_buffer(text_buffer_);
                const auto& tokens = engine_.get_tokens();

                ftxui::Elements visual_lines;

                for (const auto& token : tokens) {
                    std::string line_str(token.text_slice);
                    switch (token.type) {
                        case MuxBlockType::Heading1:
                            // Heading 1: Upper-case transform, double-underlined for maximum weight
                            std::transform(line_str.begin(), line_str.end(), line_str.begin(), ::toupper);
                            visual_lines.push_back(ftxui::text("=== " + line_str + " ===") | ftxui::bold | ftxui::color(ftxui::Color::Orange1));
                            visual_lines.push_back(ftxui::separatorDouble());
                            break;
                        case MuxBlockType::Heading2:
                            // Heading 2: Styled bold prefix marker
                            visual_lines.push_back(ftxui::text("▶ " + line_str) | ftxui::bold | ftxui::color(ftxui::Color::Yellow));
                            break;
                        case MuxBlockType::Heading3:
                            // Heading 3: Standard bold accent color
                            visual_lines.push_back(ftxui::text(line_str) | ftxui::bold | ftxui::color(ftxui::Color::Cyan));
                            break;
                        case MuxBlockType::CodeBlockLine:
                            visual_lines.push_back(ftxui::text(line_str) | ftxui::color(ftxui::Color::Green));
                            break;
                        case MuxBlockType::Paragraph:
                            visual_lines.push_back(ftxui::paragraph(line_str));
                            break;
                        case MuxBlockType::EmptyLine:
                            visual_lines.push_back(ftxui::separatorEmpty());
                            break;
                        default:
                            break;
                    }
                }

                return ftxui::window(ftxui::text(" MUX LIVE PREVIEW ") | ftxui::bold, 
                                     ftxui::vbox(std::move(visual_lines))) 

                       | ftxui::bgcolor(ftxui::Color::Black);
            });

            // Tab router container controls screens exclusively
            auto dynamic_tabs = ftxui::Container::Tab({ input_window, preview_pane }, &active_tab_);
            
            // Compose overall nested layout incorporating tab components and status panel structures
            auto global_layout = ftxui::Container::Vertical({ dynamic_tabs, save_input });

            auto global_interface = ftxui::Renderer(global_layout, [&] {
                ftxui::Element bottom_bar;
                
                if (save_mode_active_) {
                    bottom_bar = ftxui::hbox({
                        ftxui::text(" Save as: ") | ftxui::bold | ftxui::color(ftxui::Color::Yellow),
                        save_input->Render() | ftxui::flex,
                        ftxui::text(" [Press Enter to Confirm] ") | ftxui::dim
                    });
                } else {
                    bottom_bar = ftxui::text(status_message_) | ftxui::color(ftxui::Color::BlueLight);
                }

                return ftxui::vbox(
                    dynamic_tabs->Render() | ftxui::flex,
                    ftxui::separator(),
                    bottom_bar
                ) | ftxui::bgcolor(ftxui::Color::Black);
            });

            // Intercept Global Events (Hotkeys & Dynamic Context Switches)
            auto event_handler = ftxui::CatchEvent(global_interface, [&](ftxui::Event event) {
                // Ctrl+Q to Quit
                if (event == ftxui::Event::Special("\x11")) { 
                    screen.Exit();
                    return true;
                }

                // If in save mode, give input box focus priority over hotkeys
                if (save_mode_active_) {
                    if (event == ftxui::Event::Return) {
                        execute_save();
                        return true;
                    }
                    if (event == ftxui::Event::Escape) {
                        save_mode_active_ = false;
                        status_message_ = " Saving cancelled. ";
                        return true;
                    }
                    return save_input->OnEvent(event);
                }

                // Ctrl+T to Toggle View (Swaps cleanly between Editor and Fullscreen Preview)
                if (event == ftxui::Event::Special("\x14")) { 
                    active_tab_ = (active_tab_ == 0) ? 1 : 0;
                    return true;
                }

                // Ctrl+S to Trigger Save Prompt Mode
                if (event == ftxui::Event::Special("\x13")) { 
                    save_mode_active_ = true;
                    save_input->TakeFocus();
                    return true;
                }

                return false;
            });

            screen.Loop(event_handler);
        }
    };
}
