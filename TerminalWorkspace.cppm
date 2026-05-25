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
#include <chrono>

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
        std::string open_filepath_ = "";
        double parse_time_us_ = 0.0;

        MuxEngine engine_;
        int active_tab_ = 0; 

        void execute_save() noexcept {
            if (filename_buffer_.empty()) {
                filename_buffer_ = "output.md";
            }
            std::ofstream file(filename_buffer_);
            if (file) {
                file << text_buffer_;
                open_filepath_ = filename_buffer_;
                status_message_ = " Successfully saved to " + filename_buffer_ + "! ";
            } else {
                status_message_ = " Error: Could not save target file! ";
            }
            save_mode_active_ = false;
        }

        void append_inline_spans(ftxui::Elements& target, std::string_view text_slice) {
            auto spans = MuxEngine::tokenize_inline(text_slice);
            for (const auto& span : spans) {
                if (span.style == MuxTextStyle::Bold) {
                    target.push_back(ftxui::text(std::string(span.text)) | ftxui::bold);
                } else if (span.style == MuxTextStyle::Italic) {
                    target.push_back(ftxui::text(std::string(span.text)) | ftxui::italic);
                } else if (span.style == MuxTextStyle::InlineCode) {
                    target.push_back(ftxui::text(std::string(span.text)) | ftxui::color(ftxui::Color::Yellow) | ftxui::bgcolor(ftxui::Color::Palette256(235)));
                } else {
                    target.push_back(ftxui::text(std::string(span.text)));
                }
            }
        }

    public:
        Workspace(const std::string& target_path = "") {
            text_buffer_.reserve(1024 * 16);
            if (!target_path.empty()) {
                open_filepath_ = target_path;
                filename_buffer_ = target_path;
                std::ifstream file(target_path);
                if (file) {
                    std::string line;
                    while (std::getline(file, line)) {
                        text_buffer_ += line + "\n";
                    }
                    status_message_ = " Loaded " + target_path + " successfully! ";
                } else {
                    status_message_ = " New File: " + target_path;
                }
            } else {
                text_buffer_ = "";
            }
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
                auto start = std::chrono::high_resolution_clock::now();
                engine_.parse_buffer(text_buffer_);
                auto end = std::chrono::high_resolution_clock::now();
                parse_time_us_ = std::chrono::duration<double, std::micro>(end - start).count();

                const auto& tokens = engine_.get_tokens();
                ftxui::Elements visual_lines;

                for (std::size_t i = 0; i < tokens.size(); ++i) {
                    const auto& token = tokens[i];

                    if (token.type == MuxBlockType::CodeBlockLine) {
                        ftxui::Elements code_lines;
                        while (i < tokens.size() && tokens[i].type == MuxBlockType::CodeBlockLine) {
                            code_lines.push_back(ftxui::text(std::string(tokens[i].text_slice)) | ftxui::color(ftxui::Color::Green));
                            ++i;
                        }
                        --i;
                        visual_lines.push_back(ftxui::vbox(std::move(code_lines)) | ftxui::bgcolor(ftxui::Color::Palette256(234)) | ftxui::borderLight);
                        continue;
                    }

                    switch (token.type) {
                        case MuxBlockType::Heading1: {
                            std::string upper_str(token.text_slice);
                            std::transform(upper_str.begin(), upper_str.end(), upper_str.begin(), ::toupper);
                            visual_lines.push_back(ftxui::text("=== " + upper_str + " ===") | ftxui::bold | ftxui::color(ftxui::Color::Orange1));
                            visual_lines.push_back(ftxui::separatorDouble());
                            break;
                        }
                        case MuxBlockType::Heading2:
                            visual_lines.push_back(ftxui::text("▶ " + std::string(token.text_slice)) | ftxui::bold | ftxui::color(ftxui::Color::Yellow));
                            break;
                        case MuxBlockType::Heading3:
                            visual_lines.push_back(ftxui::text(std::string(token.text_slice)) | ftxui::bold | ftxui::color(ftxui::Color::Cyan));
                            break;
                        case MuxBlockType::HorizontalRule:
                            visual_lines.push_back(ftxui::separatorLight());
                            break;
                        case MuxBlockType::BulletItem: {
                            ftxui::Elements inline_elements;
                            inline_elements.push_back(ftxui::text("  • ") | ftxui::color(ftxui::Color::Cyan) | ftxui::bold);
                            append_inline_spans(inline_elements, token.text_slice);
                            visual_lines.push_back(ftxui::hbox(std::move(inline_elements)) | ftxui::flex);
                            break;
                        }
                        case MuxBlockType::Paragraph: {
                            ftxui::Elements inline_elements;
                            append_inline_spans(inline_elements, token.text_slice);
                            visual_lines.push_back(ftxui::hbox(std::move(inline_elements)) | ftxui::flex);
                            break;
                        }
                        case MuxBlockType::EmptyLine:
                            visual_lines.push_back(ftxui::text(" "));
                            break;
                        default:
                            break;
                    }
                }

                return ftxui::window(ftxui::text(" MUX LIVE PREVIEW ") | ftxui::bold, 
                                     ftxui::vbox({
                                         ftxui::vbox(std::move(visual_lines)),
                                         ftxui::filler()
                                     })) | ftxui::bgcolor(ftxui::Color::Black);
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
                    std::string perf_str = " | Parsed in: " + std::to_string(parse_time_us_).substr(0, 5) + " μs ";
                    bottom_bar = ftxui::hbox({
                        ftxui::text(status_message_) | ftxui::color(ftxui::Color::BlueLight),
                        ftxui::filler(),
                        ftxui::text(perf_str) | ftxui::color(ftxui::Color::GreenLight) | ftxui::dim
                    });
                }

                return ftxui::vbox(
                    dynamic_tabs->Render() | ftxui::flex,
                    ftxui::separator(),
                    bottom_bar
                ) | ftxui::bgcolor(ftxui::Color::Black);
            });

            auto event_handler = ftxui::CatchEvent(global_interface, [&](ftxui::Event event) {
                if (event == ftxui::Event::Special("\x11")) { 
                    screen.Exit();
                    return true;
                }
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
                if (event == ftxui::Event::Special("\x14")) { 
                    active_tab_ = (active_tab_ == 0) ? 1 : 0;
                    return true;
                }
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
