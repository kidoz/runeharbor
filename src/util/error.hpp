// SPDX-License-Identifier: MIT
#pragma once

#include <string>

namespace runeharbor::util
{

/// Lightweight error type for use with std::expected<T, Error>.
struct Error
{
    std::string message;

    explicit Error(std::string msg) : message(std::move(msg)) {}
};

} // namespace runeharbor::util
