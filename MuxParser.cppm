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

#include <cstdint>
#include <string_view>
#include <vector>
#include <algorithm>

export module MuxParser; 

export enum class MuxBlockType : uint8_t {
    Heading1,
    Heading2,
    Heading3,
    Paragraph,
    CodeBlockLine,
    EmptyLine
};

export enum class MuxTextStyle : uint8_t {
    Normal,
    Bold,
    Italic
};

export struct MuxInlineSpan {
    MuxTextStyle style;
    std::string_view text;
};

export struct MuxToken {
    uint32_t id;
    MuxBlockType type;
    std::string_view text_slice;
};

export class MuxEngine {
private:
    std::vector<MuxToken> tokens_;

    [[nodiscard]] static bool is_whitespace_only(std::string_view str) noexcept {
        return std::all_of(str.begin(), str.end(), [](char c) {
            return c == ' ' || c == '\t' || c == '\r' || c == '\n';
        });
    }

public:
    MuxEngine() {
        tokens_.reserve(1024);
    }

    void parse_buffer(std::string_view buffer) noexcept {
        tokens_.clear();
        std::size_t position = 0;
        uint32_t token_id = 0;
        bool in_code_block = false;

        while (position < buffer.size()) {
            std::size_t next_newline = buffer.find('\n', position);
            if (next_newline == std::string_view::npos) {
                next_newline = buffer.size();
            }

            std::string_view line = buffer.substr(position, next_newline - position);

            if (!line.empty() && line.back() == '\r') {
                line.remove_suffix(1);
            }

            if (line.starts_with("```")) {
                in_code_block = !in_code_block;
                position = next_newline + 1;
                continue;
            }

            if (in_code_block) {
                tokens_.push_back({token_id++, MuxBlockType::CodeBlockLine, line});
            } else if (line.empty() || is_whitespace_only(line)) {
                if (tokens_.empty() || tokens_.back().type != MuxBlockType::EmptyLine) {
                    tokens_.push_back({token_id++, MuxBlockType::EmptyLine, line});
                }
            } else if (line.starts_with("# ")) {
                tokens_.push_back({token_id++, MuxBlockType::Heading1, line.substr(2)});
            } else if (line.starts_with("## ")) {
                tokens_.push_back({token_id++, MuxBlockType::Heading2, line.substr(3)});
            } else if (line.starts_with("### ")) {
                tokens_.push_back({token_id++, MuxBlockType::Heading3, line.substr(4)});
            } else {
                tokens_.push_back({token_id++, MuxBlockType::Paragraph, line});
            }

            position = next_newline + 1;
        }
    }

    [[nodiscard]] const std::vector<MuxToken>& get_tokens() const noexcept {
        return tokens_;
    }

    [[nodiscard]] static std::vector<MuxInlineSpan> tokenize_inline(std::string_view text) noexcept {
        std::vector<MuxInlineSpan> spans;
        spans.reserve(8);

        std::size_t pos = 0;
        while (pos < text.size()) {
            std::string_view remaining = text.substr(pos);

            if (remaining.starts_with("**")) {
                std::size_t close = text.find("**", pos + 2);
                if (close != std::string_view::npos) {
                    spans.push_back({MuxTextStyle::Bold, text.substr(pos + 2, close - (pos + 2))});
                    pos = close + 2;
                    continue;
                }
            }

            if (remaining.starts_with("*")) {
                std::size_t close = text.find("*", pos + 1);
                if (close != std::string_view::npos) {
                    spans.push_back({MuxTextStyle::Italic, text.substr(pos + 1, close - (pos + 1))});
                    pos = close + 1;
                    continue;
                }
            }

            std::size_t next_bold = text.find("**", pos);
            std::size_t next_italic = text.find("*", pos);
            std::size_t next_delim = std::size_t(-1);

            if (next_bold != std::string_view::npos && next_italic != std::string_view::npos) {
                next_delim = std::min(next_bold, next_italic);
            } else if (next_bold != std::string_view::npos) {
                next_delim = next_bold;
            } else {
                next_delim = next_italic;
            }

            if (next_delim == std::string_view::npos) {
                spans.push_back({MuxTextStyle::Normal, text.substr(pos)});
                break;
            }

            if (next_delim > pos) {
                spans.push_back({MuxTextStyle::Normal, text.substr(pos, next_delim - pos)});
            }
            pos = next_delim;
        }
        return spans;
    }
};

