
#define YGM_SKIP_ASYNC_LAMBDA_COMPLIANCE 1

#include <ygm/comm.hpp>
#include <ygm/container/map.hpp>

#include <iostream>

int main(int argc, char **argv) {
  ygm::comm world(&argc, &argv);

  {
    using key_type   = int;
    using value_type = int;

    struct my_functor {
      my_functor() { std::cout << "Constructing functor" << std::endl; }

      my_functor([[maybe_unused]] const my_functor &other) {
        std::cout << "Copy constructing functor" << std::endl;
      }

      my_functor([[maybe_unused]] my_functor &&other) {
        std::cout << "Move constructing functor" << std::endl;
      }

      ~my_functor() { std::cout << "Destructing functor" << std::endl; }

      void operator()([[maybe_unused]] const key_type &k, value_type &v) {
        ++v;
      }
    };

    ygm::container::map<key_type, value_type> my_map(world);
    if (world.rank0()) {
      my_map.async_insert(0, 0);
    }

    world.cout0("\nMap async_visit with functor object");
    if (world.rank0()) {
      my_functor f;
      my_map.async_visit(0, f);
    }
    world.barrier();
    if (world.rank0()) {
      my_map.async_visit(
          0, []([[maybe_unused]] const key_type &k, const value_type &v) {
            YGM_ASSERT_RELEASE(v == 1);
          });
    }
    world.barrier();

    world.cout0("\nMap async_visit with temporary");
    if (world.rank0()) {
      my_map.async_visit(0, my_functor());
    }
    world.barrier();
    if (world.rank0()) {
      my_map.async_visit(
          0, []([[maybe_unused]] const key_type &k, const value_type &v) {
            YGM_ASSERT_RELEASE(v == 2);
          });
    }
    world.barrier();

    world.cout0("\nMap local_visit with functor object");
    if (world.rank0()) {
      my_functor f;
      my_map.local_visit(0, f);
    }
    world.barrier();
    if (world.rank0()) {
      my_map.async_visit(
          0, []([[maybe_unused]] const key_type &k, const value_type &v) {
            YGM_ASSERT_RELEASE(v == 3);
          });
    }
    world.barrier();

    world.cout0("\nMap local_visit with temporary");
    if (world.rank0()) {
      my_map.local_visit(0, my_functor());
    }
    world.barrier();
    if (world.rank0()) {
      my_map.async_visit(
          0, []([[maybe_unused]] const key_type &k, const value_type &v) {
            YGM_ASSERT_RELEASE(v == 4);
          });
    }
    world.barrier();
  }
  return 0;
}
