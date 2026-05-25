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

module; // Global module fragment - put traditional includes here

#include <cstdint>
#include <string_view>
#include <vector>

export module MuxParser; // Module boundary definition starts here

export enum class MuxBlockType : uint8_t {
    Heading1,
    Heading2,
    Heading3,
    Paragraph,
    CodeBlockLine,
    EmptyLine
};

export struct MuxToken {
    uint32_t id;
    MuxBlockType type;
    std::string_view text_slice;
};

export class MuxEngine {
private:
    std::vector<MuxToken> tokens_;

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

            if (line.starts_with("```")) {
                in_code_block = !in_code_block;
                position = next_newline + 1;
                continue;
            }

            if (in_code_block) {
                tokens_.push_back({token_id++, MuxBlockType::CodeBlockLine, line});
            } else if (line.empty()) {
                tokens_.push_back({token_id++, MuxBlockType::EmptyLine, line});
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
};
