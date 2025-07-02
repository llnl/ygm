// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#undef NDEBUG

#include <ygm/comm.hpp>
#include <ygm/container/container_traits.hpp>

#include <ygm/container/array.hpp>
#include <ygm/container/bag.hpp>
#include <ygm/container/counting_set.hpp>
#include <ygm/container/disjoint_set.hpp>
#include <ygm/container/map.hpp>
#include <ygm/container/set.hpp>

#include <ygm/detail/ygm_traits.hpp>

int main(int argc, char** argv) {
    ygm::comm world(&argc, &argv);

    ygm::container::array<int>        test_array(world, 10);
    ygm::container::bag<int>          test_bag(world);
    ygm::container::counting_set<int> test_counting_set(world);
    ygm::container::disjoint_set<int> test_disjoint_set(world);
    ygm::container::map<int, int>     test_map(world);
    ygm::container::set<int>          test_set(world);
    int                               i;

    {
        static_assert(ygm::container::is_array(test_array));
        static_assert(not ygm::container::is_array(test_bag));
        static_assert(not ygm::container::is_array(test_counting_set));
        static_assert(not ygm::container::is_array(test_disjoint_set)); 
        static_assert(not ygm::container::is_array(test_map));
        static_assert(not ygm::container::is_array(test_set));
        static_assert(not ygm::container::is_array(i));
    }
    
    {
        static_assert(ygm::container::is_bag(test_bag));
        static_assert(not ygm::container::is_bag(test_array));
        static_assert(not ygm::container::is_bag(test_counting_set));
        static_assert(not ygm::container::is_bag(test_disjoint_set)); 
        static_assert(not ygm::container::is_bag(test_map));
        static_assert(not ygm::container::is_bag(test_set));
        static_assert(not ygm::container::is_bag(i));
    }

    {
        static_assert(ygm::container::is_counting_set(test_counting_set));
        static_assert(not ygm::container::is_counting_set(test_array));
        static_assert(not ygm::container::is_counting_set(test_bag));
        static_assert(not ygm::container::is_counting_set(test_disjoint_set));
        static_assert(not ygm::container::is_counting_set(test_map));
        static_assert(not ygm::container::is_counting_set(test_set));
        static_assert(not ygm::container::is_counting_set(i));
    }

    {
        static_assert(ygm::container::is_disjoint_set(test_disjoint_set));
        static_assert(not ygm::container::is_disjoint_set(test_array));
        static_assert(not ygm::container::is_disjoint_set(test_bag));
        static_assert(not ygm::container::is_disjoint_set(test_counting_set));
        static_assert(not ygm::container::is_disjoint_set(test_map));
        static_assert(not ygm::container::is_disjoint_set(test_set));
        static_assert(not ygm::container::is_disjoint_set(i));
    }
    
    {
        static_assert(ygm::container::is_map(test_map));
        static_assert(not ygm::container::is_map(test_array));
        static_assert(not ygm::container::is_map(test_bag));
        static_assert(not ygm::container::is_map(test_counting_set));
        static_assert(not ygm::container::is_map(test_disjoint_set));
        static_assert(not ygm::container::is_map(test_set));
        static_assert(not ygm::container::is_map(i));
    }
    
    {
        static_assert(ygm::container::is_set(test_set));
        static_assert(not ygm::container::is_set(test_array));
        static_assert(not ygm::container::is_set(test_bag));
        static_assert(not ygm::container::is_set(test_counting_set));
        static_assert(not ygm::container::is_set(test_disjoint_set));
        static_assert(not ygm::container::is_set(test_map));
        static_assert(not ygm::container::is_set(i));
    }

    return 0;
}
