// Copyright (C) 2026 mxreal64 
// 
// This program is free software: you can redistribute it and/or modify 
// it under the terms of the GNU General Public License as published by 
// the Free Software Foundation, either version 3 of the License, or 
// (at your option) any later version. // // This program is distributed in the hope that it will be useful, 
// but WITHOUT ANY WARRANTY; without even the implied warranty of 
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
// GNU General Public License for more details. 
// 
// You should have received a copy of the GNU General Public License 
// along with this program. If not, see <https://gnu.org>.

module; 
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
        
        
        bool save_mode_active_ = false;
        std::string filename_buffer_ = "output.md";

        MuxEngine engine_;
        int active_tab_ = 0; 

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
            text_buffer_ = "type";
            text_buffer_.reserve(1024 * 16);
        }

        void run() noexcept {
            auto screen = ftxui::ScreenInteractive::Fullscreen();
            screen.TrackMouse(true); 

            ftxui::Component input_field = ftxui::Input(&text_buffer_, "Enter markdown text...");

            ftxui::Component save_input = ftxui::Input(&filename_buffer_, "filename.md");

            auto input_window = ftxui::Renderer(input_field, [input_field] {
                return ftxui::window(ftxui::text(" MUX EDITOR ") | ftxui::bold, input_field->Render());
            });

            auto preview_pane = ftxui::Renderer([this] {
                engine_.parse_buffer(text_buffer_);
                const auto& tokens = engine_.get_tokens();

                ftxui::Elements visual_lines;

                for (const auto& token : tokens) {
                    std::string line_str(token.text_slice);
                    switch (token.type) {
                        case MuxBlockType::Heading1:
                            std::transform(line_str.begin(), line_str.end(), line_str.begin(), ::toupper);
                            visual_lines.push_back(ftxui::text("=== " + line_str + " ===") | ftxui::bold | ftxui::color(ftxui::Color::Orange1));
                            visual_lines.push_back(ftxui::separatorDouble());
                            break;
                        case MuxBlockType::Heading2:
                            visual_lines.push_back(ftxui::text("▶ " + line_str) | ftxui::bold | ftxui::color(ftxui::Color::Yellow));
                            break;
                        case MuxBlockType::Heading3:
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

            auto dynamic_tabs = ftxui::Container::Tab({ input_window, preview_pane }, &active_tab_);
            
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
