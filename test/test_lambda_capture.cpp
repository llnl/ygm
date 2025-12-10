
#include <ygm/comm.hpp>
#include <ygm/container/map.hpp>

#include <string>

int main(int argc, char **argv) {
  ygm::comm world(&argc, &argv);

  // Capture in comm::async
  {
    int a = 12;
    world.async(0, [a]() { YGM_ASSERT_RELEASE(a == 12); });
  }

  // Capture in comm::async_bcast
  {
    int a = 12;
    if (world.rank0()) {
      world.async_bcast([a]() { YGM_ASSERT_RELEASE(a == 12); });
    }
  }

  // Capture in container::map::async_visit
  {
    ygm::container::map<std::string, int> my_map(world);

    int a = world.rank();
    my_map.async_visit("key", [a]([[maybe_unused]] const std::string &key,
                                  int &val) { val += a; });

    world.barrier();
    my_map.for_all(
        [&world]([[maybe_unused]] const std::string &key, const int &val) {
          YGM_ASSERT_RELEASE(val == world.size() * (world.size() - 1) / 2);
        });
  }

  return 0;
}
