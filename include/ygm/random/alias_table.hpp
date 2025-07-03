#include <ygm/comm.hpp>
#include <ygm/random/random.hpp>
#include <ygm/detail/collective.hpp> 
#include <ygm/container/detail/base_concepts.hpp> 
#include <unordered_map>
#include <map>
#include <mpi.h>
#include <cmath>
#include <algorithm>


namespace ygm::random {

template <typename T>
concept convertable_to_double = std::convertible_to<T,double>;

template<typename T>
concept is_associative_container = requires {
    typename T::key_type;
    typename T::mapped_type;
};

template<typename T>
concept is_value_container = requires {
    typename T::value_type;
};

// template<typename T>
// concept valid_stl_associative_container = requires {
//     typename T::key_type;
//     typename T::value_type;
// };


// template<typename T>
// concept valid_stl_value_container = requires {
//     typename T::value_type;
//     requires ygm::container::detail::DoubleItemTuple<T::value_type>;
// };

// template<typename T>
// concept valid_stl_container =
//     is_associative_container<T> || valid_stl_value_container<T>;

template <typename Item, typename RNG>
class alias_table {

    using self_type = alias_table<Item, RNG>;
    using weight_t = double;

    public: 

        struct item {
            Item id;
            weight_t weight;

            template <typename Archive>
            void serialize(Archive& ar) { // Needed for cereal serialization
                ar(id, weight);
            }
        };

        struct table_item { 
            double p; // prob item a is selected = p, prob item b is selected = (1 - p)
            Item a;
            Item b;
        };

 
        template<typename Container>
        requires ygm::container::detail::HasForAll<Container> &&
                 is_value_container<Container> &&
                 ygm::container::detail::DoubleItemTuple<typename Container::value_type> && 
                 convertable_to_double<std::tuple_element_t<1, typename Container::value_type>>
        alias_table(ygm::comm &comm, RNG &rng, Container &c) : 
                m_comm(comm),
                pthis(this),
                m_rng(rng),
                m_rank_dist(0, comm.size()-1) {
            c.for_all([&](const auto& id_weight_pair){
                m_local_items.push_back({std::get<0>(id_weight_pair), std::get<1>(id_weight_pair)});
            });
            comm.barrier();
            balance_weight();
            comm.barrier();
            YGM_ASSERT_RELEASE(is_balanced());
            build_alias_table();
            m_local_items.clear();
        }

        template<typename Container>
        requires ygm::container::detail::HasForAll<Container> &&
                 is_associative_container<Container> && 
                 convertable_to_double<typename Container::mapped_type>
        alias_table(ygm::comm &comm, RNG &rng, Container &c) : 
                m_comm(comm),
                pthis(this),
                m_rng(rng),
                m_rank_dist(0, comm.size()-1) {
            c.for_all([&](const auto &key, const auto &value){
                m_local_items.push_back({key, value});
            });
            comm.barrier();
            balance_weight();
            comm.barrier();
            YGM_ASSERT_RELEASE(is_balanced());
            build_alias_table();
            m_local_items.clear();
        }

    private:
 
        void balance_weight() { 

            MPI_Comm comm = m_comm.get_mpi_comm();
            double local_weight = 0.0;
            for (int i = 0; i < m_local_items.size(); i++) {
                local_weight += m_local_items[i].weight;
            }

            double global_weight;
            MPI_Allreduce(&local_weight, &global_weight, 1, MPI_DOUBLE, MPI_SUM, comm);

            double prfx_sum_weight;
            MPI_Exscan(&local_weight, &prfx_sum_weight, 1, MPI_DOUBLE, MPI_SUM, comm);


            double target_weight = global_weight / m_comm.size(); // target weight per rank
            uint32_t dest_rank = prfx_sum_weight / target_weight; 
            double curr_weight = std::fmod(prfx_sum_weight, target_weight); // Spillage weight

            std::vector<item> new_local_items;
            using ygm_items_ptr = ygm::ygm_ptr<std::vector<item>>;
            ygm_items_ptr ptr_new_items = m_comm.make_ygm_ptr(new_local_items); 
            m_comm.barrier(); // ensure ygm_ptr is created on every rank

            std::vector<item> items_to_send;
            for (uint64_t i = 0; i < m_local_items.size(); i++) { // WARNING: size of m_local_items can grow during loop
                item local_item = m_local_items[i]; 
                if (curr_weight + local_item.weight >= target_weight) {
                
                    double remaining_weight = curr_weight + local_item.weight - target_weight;
                    double weight_to_send = local_item.weight - remaining_weight;
                    curr_weight += weight_to_send;
                    item item_to_send = {local_item.id, weight_to_send};
                    items_to_send.push_back(item_to_send);

                    if ((curr_weight > 0.0001) && (dest_rank < m_comm.size())) { // Accounts for rounding errors
                        // Moves weights to dest_rank's new_local_items
                        m_comm.async(dest_rank, [](std::vector<item> items, ygm_items_ptr new_items_ptr) {
                            new_items_ptr->insert(new_items_ptr->end(), items.begin(), items.end()); 
                        }, items_to_send, ptr_new_items);
                    }
                    // Handle case where item weight is large enough to span multiple rank's alias tables
                    if (remaining_weight >= target_weight) { 
                        m_local_items.push_back({local_item.id, remaining_weight});
                        curr_weight = 0;
                    }  else {
                        curr_weight = remaining_weight;
                    }
                    items_to_send.clear();
                    if (curr_weight != 0) {
                        items_to_send.push_back({local_item.id, curr_weight});
                    }
                    dest_rank++;

                } else {
                    items_to_send.push_back(local_item);
                    curr_weight += local_item.weight;
                }
            }
            
            // Need to handle items left in items to send
            if (items_to_send.size() > 0 && curr_weight > 0.00001 && dest_rank < m_comm.size()) { // Account for floating point errors
                m_comm.async(dest_rank, [](std::vector<item> items, ygm_items_ptr new_items_ptr) {
                    new_items_ptr->insert(new_items_ptr->end(), items.begin(), items.end()); 
                }, items_to_send, ptr_new_items);
            }

            m_comm.barrier();
            std::swap(new_local_items, m_local_items);
        } 

        bool is_balanced() { 
            // Not a perfect check. Will check if every rank has the same weight for each table
            // but does not confirm that the actual target weight is met on each rank
            double local_weight = 0.0;
            for (const auto& itm : m_local_items) {
                local_weight += itm.weight;
            } 
            m_comm.barrier();
            auto equal = [](double a, double b){
                return (std::abs(a - b) < 0.00001);
            }; 
            bool balanced = ygm::is_same(local_weight, m_comm, equal);
            return balanced;
        }

        void build_alias_table() {
            // Make average weight of items 1, so coin flip is simple across all tables 
            double local_weight = 0.0;
            for (const auto& itm : m_local_items) {
                local_weight += itm.weight;
            } 
            double avg_weight = local_weight / m_local_items.size(); 

            double new_total_weight = 0;
            for (auto& itm : m_local_items) {
                itm.weight /= avg_weight;
                new_total_weight += itm.weight;
            }
            double new_avg_weight = new_total_weight / m_local_items.size(); // Should be 1
            if (std::abs((new_avg_weight - 1.0)) < 0.0001) {
                YGM_ASSERT_RELEASE(std::abs((new_avg_weight - 1.0)) < 0.00001);
            }                

            // Implementation of Vose's algorithm, utilized Keith Schwarz numerically stable version
            // https://www.keithschwarz.com/darts-dice-coins/
            std::vector<item> heavy_items;
            std::vector<item> light_items;
            for (auto& itm : m_local_items) {
                if (itm.weight < 1.0) {
                    light_items.push_back(itm);
                } else {
                    heavy_items.push_back(itm);
                }
            }

            while (!light_items.empty() && !heavy_items.empty()) {
                item l = light_items.back();
                item h = heavy_items.back(); 
                table_item tbl_itm = {l.weight, l.id, h.id};
                m_local_alias_table.push_back(tbl_itm);
                h.weight = (h.weight + l.weight) - 1;
                light_items.pop_back(); 
                if (h.weight < 1) {
                    light_items.push_back(h);
                    heavy_items.pop_back();
                }   
            }

            // Either heavy items or light_items is empty, need to flush the non empty 
            // vector and add them to the alias table with a p value of 1
            while (!heavy_items.empty()) {
                item h = heavy_items.back();
                table_item tbl_itm = {1.0, h.id, 0};
                m_local_alias_table.push_back(tbl_itm);
                heavy_items.pop_back();
            }
            while (!light_items.empty()) {
                item l = light_items.back();
                table_item tbl_itm = {1.0, l.id, 0};
                m_local_alias_table.push_back(tbl_itm);
                light_items.pop_back();
            }
            m_comm.barrier();
        }
    
    public:

        template <typename Visitor, typename... VisitorArgs>
        void async_sample(Visitor visitor, const VisitorArgs &...args) { // Sample should be provided a lambda/functor that takes as an argument the item type 

            auto sample_wrapper = [](auto ptr_a_tbl, const VisitorArgs &...args) {
                std::uniform_int_distribution<uint64_t> table_item_dist(0, ptr_a_tbl->m_local_alias_table.size()-1);
                table_item tbl_itm = ptr_a_tbl->m_local_alias_table[table_item_dist(ptr_a_tbl->m_rng)];
                Item s;
                if (tbl_itm.p == 1) {
                    s = tbl_itm.a;
                } else {
                    std::uniform_real_distribution<float> zero_one_dist(0.0, 1.0);
                    float f = zero_one_dist(ptr_a_tbl->m_rng);
                    if (f < tbl_itm.p) {
                        s = tbl_itm.a;
                    } else {
                        s = tbl_itm.b;
                    }
                }
                Visitor *vis = nullptr;
                ygm::meta::apply_optional(*vis, std::make_tuple(ptr_a_tbl), std::forward_as_tuple(s, args...));
            };

            uint32_t dest_rank = m_rank_dist(m_rng);
            m_comm.async(dest_rank, sample_wrapper, pthis, std::forward<const VisitorArgs>(args)...);
        }        

    private:
        ygm::comm&                                          m_comm;
        ygm::ygm_ptr<self_type>                             pthis;
        std::uniform_int_distribution<uint32_t>             m_rank_dist;
        RNG&                                                m_rng;
        std::vector<item>                                   m_local_items;
        std::vector<table_item>                             m_local_alias_table;
};

}  // namespace ygm::random