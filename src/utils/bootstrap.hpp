#pragma once

namespace Lunar {

class Bootstrap {
public:
    Bootstrap();
    ~Bootstrap();

    Bootstrap(const Bootstrap&) = delete;
    Bootstrap& operator=(const Bootstrap&) = delete;

    Bootstrap(Bootstrap&&) noexcept = default;
    Bootstrap& operator=(Bootstrap&&) noexcept = default;
};

} // namespace Lunar