
#include <filesystem>
#include <ygm/detail/logger.hpp>
#include <ygm/utility/assert.hpp>

class file_cleanup {
 public:
  file_cleanup(std::filesystem::path &p) : m_p(p) {
    // Only use this for files that do not currently exist
    YGM_ASSERT_RELEASE(std::filesystem::exists(m_p) == false);
  }

  ~file_cleanup() { std::filesystem::remove(m_p); }

 private:
  std::filesystem::path m_p;
};

int main() {
  // Test files are not created until necessary
  {
    std::filesystem::path p("./test_log");
    file_cleanup          c(p);

    ygm::detail::logger l(0, p);

    YGM_ASSERT_RELEASE(std::filesystem::exists(l.get_path()) == false);

    l.set_log_level(ygm::log_level::off);
    l.log(ygm::log_level::info, "Do not log");
    YGM_ASSERT_RELEASE(std::filesystem::exists(l.get_path()) == false);

    l.set_log_level(ygm::log_level::info);
    l.log(ygm::log_level::info, "Do log");
    l.flush();
    YGM_ASSERT_RELEASE(std::filesystem::exists(l.get_path()) == true);
    YGM_ASSERT_RELEASE(std::filesystem::is_empty(l.get_path()) == false);
  }

  // Test logging level ordering
  {
    std::filesystem::path p("./test_log");
    file_cleanup          c(p);

    ygm::detail::logger l(0, p);

    YGM_ASSERT_RELEASE(std::filesystem::exists(l.get_path()) == false);

    l.set_log_level(ygm::log_level::warn);
    l.log(ygm::log_level::info, "Do not log");
    YGM_ASSERT_RELEASE(std::filesystem::exists(l.get_path()) == false);

    l.set_log_level(ygm::log_level::debug);
    l.log(ygm::log_level::info, "Do log");
    YGM_ASSERT_RELEASE(std::filesystem::exists(l.get_path()) == true);
  }

  // Test setting new log file
  {
    std::filesystem::path p("./test_log");
    file_cleanup          c(p);

    ygm::detail::logger l(0, p);

    YGM_ASSERT_RELEASE(std::filesystem::exists(l.get_path()) == false);

    l.set_log_level(ygm::log_level::warn);
    l.log(ygm::log_level::info, "Do not log");
    YGM_ASSERT_RELEASE(std::filesystem::exists(l.get_path()) == false);

    std::filesystem::path p2("./test_log2");
    file_cleanup          c2(p2);

    l.set_path(p2);
    YGM_ASSERT_RELEASE(l.get_path() == p2);
    YGM_ASSERT_RELEASE(std::filesystem::exists(l.get_path()) == false);

    l.log(ygm::log_level::info, "Do not log");
    YGM_ASSERT_RELEASE(std::filesystem::exists(l.get_path()) == false);

    l.set_log_level(ygm::log_level::debug);
    l.log(ygm::log_level::info, "Do log");
    YGM_ASSERT_RELEASE(std::filesystem::exists(l.get_path()) == true);
  }

  // Test logs do not get overwritten when opening existing file
  {
    std::filesystem::path p("./test_log");
    file_cleanup          c(p);

    ygm::detail::logger l(0, p);

    YGM_ASSERT_RELEASE(std::filesystem::exists(l.get_path()) == false);

    l.set_log_level(ygm::log_level::warn);
    l.log(ygm::log_level::info, "Do not log");
    YGM_ASSERT_RELEASE(std::filesystem::exists(l.get_path()) == false);

    l.set_log_level(ygm::log_level::debug);
    l.log(ygm::log_level::info, "Do log");
    YGM_ASSERT_RELEASE(std::filesystem::exists(l.get_path()) == true);

    std::filesystem::path p2("./test_log2");
    file_cleanup          c2(p2);

    l.set_path(p2);
    YGM_ASSERT_RELEASE(l.get_path() == p2);
    YGM_ASSERT_RELEASE(std::filesystem::exists(l.get_path()) == false);

    l.set_path(p);
    YGM_ASSERT_RELEASE(std::filesystem::is_empty(p) == false);
  }

  // Test log files do not get created when logging to cout
  {
    std::filesystem::path p("./test_log");

    ygm::detail::logger l(8, p);

    l.set_logger_target(ygm::logger_target::stdout);
    l.set_log_level(ygm::log_level::debug);
    l.log(ygm::log_level::warn, "Log to stdout");
    YGM_ASSERT_RELEASE(std::filesystem::exists(l.get_path()) == false);
  }

  return 0;
}
