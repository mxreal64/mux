// Copyright (C) 2026 mxreal64
// Licensed under the GPL-3.0 License

module;

#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/screen/color.hpp>

export module TerminalWorkspace;

import std;
import MuxParser;

export namespace MuxUI {

class Workspace {
private:
    MuxEngine engine_;
    std::string parse_time_str_ = "Parsed in: 0.000 μs";
    std::string text_buffer_;
    std::string file_path_;

    int active_tab_ = 1; // 0 = Editor, 1 = Preview
    bool save_mode_active_ = false;
    std::string save_path_target_ = "untitled.md";

    // --- SCROLL STATE VARIABLES ---
    int scroll_offset_ = 0;
    int max_visible_lines_ = 20; // Automatically adjusted at runtime

    ftxui::Component editor_input_field_;
    ftxui::Component save_input_field_;
    ftxui::Component global_layout_container_;

    void load_file_to_buffer() noexcept {
        if (file_path_.empty()) return;
        std::ifstream file(file_path_, std::ios::in | std::ios::binary);
        if (!file.is_open()) return;

        text_buffer_ = std::string((std::istreambuf_iterator<char>(file)), 
                                    std::istreambuf_iterator<char>());
    }

public:
    explicit Workspace(std::string_view initial_file) : file_path_(initial_file) {
        load_file_to_buffer();
        
        editor_input_field_ = ftxui::Input(&text_buffer_, "Write your Markdown here...");
        save_input_field_ = ftxui::Input(&save_path_target_, "Enter path...");

        global_layout_container_ = ftxui::Container::Vertical({
            editor_input_field_,
            save_input_field_
        });

        execute_parsing_pipeline();
    }

    void execute_parsing_pipeline() noexcept {
        auto start_time = std::chrono::high_resolution_clock::now();
        engine_.parse_buffer(text_buffer_);
        auto end_time = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
        parse_time_str_ = std::format("Parsed in: {:.3f} μs", duration / 1000.0);
    }

    ftxui::Element RenderPane() {
        ftxui::Elements preview_rows;
        const auto& tokens = engine_.get_tokens();

        // 1. Slice your token loop to strictly follow your scroll offset
        for (std::size_t i = static_cast<std::size_t>(scroll_offset_); i < tokens.size(); ++i) {
            const auto& token = tokens[i];

            if (token.type == MuxBlockType::HorizontalRule) {
                preview_rows.push_back(ftxui::separator());
                continue;
            }
            if (token.type == MuxBlockType::EmptyLine) {
                preview_rows.push_back(ftxui::text(""));
                continue;
            }

            auto inline_spans = MuxEngine::tokenize_inline(token.text_slice);
            ftxui::Elements row_spans;

            for (const auto& span : inline_spans) {
                ftxui::Element span_el = ftxui::text(std::string(span.text));
                switch (span.style) {
                    case MuxTextStyle::Bold:        span_el = ftxui::bold(span_el); break;
                    case MuxTextStyle::Italic:      span_el = ftxui::italic(span_el); break;
                    case MuxTextStyle::InlineCode:  span_el = span_el | ftxui::color(ftxui::Color::Green) | ftxui::bgcolor(ftxui::Color::RGB(18, 18, 18)); break;
                    default: break;
                }
                row_spans.push_back(span_el);
            }

            ftxui::Element assembled_row = ftxui::hbox(std::move(row_spans));

            switch (token.type) {
                case MuxBlockType::Heading1:      preview_rows.push_back(ftxui::bold(assembled_row) | ftxui::color(ftxui::Color::Yellow)); break;
                case MuxBlockType::Heading2:      preview_rows.push_back(ftxui::bold(assembled_row) | ftxui::color(ftxui::Color::Cyan)); break;
                case MuxBlockType::Heading3:      preview_rows.push_back(ftxui::bold(assembled_row) | ftxui::color(ftxui::Color::BlueLight)); break;
                case MuxBlockType::BulletItem:    preview_rows.push_back(ftxui::hbox({ftxui::text(" • ") | ftxui::color(ftxui::Color::Yellow), assembled_row})); break;
                case MuxBlockType::CodeBlockLine: preview_rows.push_back(ftxui::hbox({ftxui::text("  "), assembled_row | ftxui::color(ftxui::Color::Green)})); break;
                default:                          preview_rows.push_back(assembled_row); break;
            }
        }

        // 2. Wrap rows cleanly using standard layout constraints
        auto visible_viewport = ftxui::vbox(std::move(preview_rows)) | ftxui::flex;

        ftxui::Element center_view;
        if (active_tab_ == 0) {
            center_view = ftxui::window(ftxui::text(" EDITOR ") | ftxui::bold, editor_input_field_->Render() | ftxui::flex);
        } else {
            center_view = ftxui::window(ftxui::text(" MUX LIVE PREVIEW ") | ftxui::bold, visible_viewport) | ftxui::bgcolor(ftxui::Color::Black);
        }

        ftxui::Element bottom_bar;
        if (save_mode_active_) {
            bottom_bar = ftxui::hbox({
                ftxui::text(" Save Path: ") | ftxui::bold | ftxui::color(ftxui::Color::Yellow),
                save_input_field_->Render() | ftxui::flex
            });
        } else {
            bottom_bar = ftxui::hbox({
                ftxui::text(" [Ctrl+T] Toggle View | [Ctrl+S] Save | [Ctrl+Q] Quit ") | ftxui::color(ftxui::Color::BlueLight),
                ftxui::filler(),
                ftxui::text(parse_time_str_) | ftxui::color(ftxui::Color::GrayLight)
            });
        }

        return ftxui::vbox(ftxui::Elements{ 
            center_view | ftxui::flex, 
            ftxui::separator(), 
            bottom_bar 
        });
    }

    bool HandleEvent(ftxui::Event event) {
        if (event == ftxui::Event::CtrlT) {
            active_tab_ = (active_tab_ == 0) ? 1 : 0;
            // Reset offset when jumping back and forth
            scroll_offset_ = 0; 
            return true;
        }
        if (event == ftxui::Event::CtrlS) {
            save_mode_active_ = !save_mode_active_;
            return true;
        }

        // --- INTERCEPT NAVIGATION INPUT CODES IN PREVIEW MODE ---
        if (active_tab_ == 1) {
            const int total_lines = static_cast<int>(engine_.get_tokens().size());

            if (event == ftxui::Event::ArrowDown) {
                if (scroll_offset_ < total_lines - 5) {
                    scroll_offset_++;
                    return true;
                }
            }
            if (event == ftxui::Event::ArrowUp) {
                if (scroll_offset_ > 0) {
                    scroll_offset_--;
                    return true;
                }
            }
            
            // Intercept mouse wheel actions
            if (event.is_mouse()) {
                if (event.mouse().button == ftxui::Mouse::WheelDown) {
                    if (scroll_offset_ < total_lines - 5) {
                        scroll_offset_ += 2; // Fast scroll down
                        if (scroll_offset_ > total_lines - 5) scroll_offset_ = total_lines - 5;
                        return true;
                    }
                }
                if (event.mouse().button == ftxui::Mouse::WheelUp) {
                    if (scroll_offset_ > 0) {
                        scroll_offset_ -= 2; // Fast scroll up
                        if (scroll_offset_ < 0) scroll_offset_ = 0;
                        return true;
                    }
                }
            }
        }

        if (active_tab_ == 0) {
            bool handled = editor_input_field_->OnEvent(event);
            if (handled) execute_parsing_pipeline();
            return handled;
        }

        return global_layout_container_->OnEvent(event);
    }

    void run() noexcept {
        auto screen = ftxui::ScreenInteractive::Fullscreen();
        
        // Feed window constraints into layout bounds dynamically on render loops
        auto main_renderer = ftxui::Renderer(global_layout_container_, [&] { 
            max_visible_lines_ = screen.dimy() - 4; // Track terminal size limits
            return RenderPane(); 
        });
        
        auto root_loop = ftxui::CatchEvent(main_renderer, [&](ftxui::Event event) {
            if (event == ftxui::Event::CtrlQ) {
                screen.ExitLoopClosure()();
                return true;
            }
            return HandleEvent(event);
        });

        screen.Loop(root_loop);
    }
};

} // namespace MuxUI
