#pragma once

namespace LunarCore {

class Bootstrap {
  public:
    Bootstrap();
    ~Bootstrap();

    Bootstrap(const Bootstrap &) = delete;
    Bootstrap &operator=(const Bootstrap &) = delete;
    Bootstrap(Bootstrap &&) = delete;
    Bootstrap &operator=(Bootstrap &&) = delete;

  private:
    bool m_curl_initialized = false;
};

} // namespace LunarCore