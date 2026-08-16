#pragma once

#include <string>
#include <string_view>

struct ContentId {
    std::string value;

    friend bool operator==(const ContentId&, const ContentId&) = default;
};

bool is_valid_content_id(std::string_view id);
std::string canonical_content_segment(std::string_view text);
ContentId make_content_id(std::string_view category, std::string_view local_name);
