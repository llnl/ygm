// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <filesystem>
#include <fstream>

#include <cereal/archives/binary.hpp>
// #include <cereal/archives/portable_binary.hpp>
// #include <cereal/archives/json.hpp>

#include <ygm/container/detail/base_async_insert.hpp>
#include <ygm/container/detail/base_concepts.hpp>
#include <ygm/detail/collective.hpp>
#include <ygm/utility/boost_json.hpp>
#include <ygm/utility/filesystem.hpp>

namespace ygm::container {

/*
 * @brief Tag for use in constructors to signify data is coming from saved files
 */
struct from_saved_tag_t {};
static from_saved_tag_t from_saved_tag;

/**
 * @brief Returns a container from saved container data
 *
 * @param save_path Path to saved container
 * @param check_types Flag indicating whether types should be checked before
 * loading from files (default: true)
 * @return ygm::container containing saved data
 */
template <typename T>
T from_saved(ygm::comm& comm, const std::filesystem::path& save_path,
             bool check_types = true) {
  T to_return(ygm::container::from_saved_tag, comm, save_path, check_types);

  return to_return;
}

namespace detail {

namespace save_constants {
constexpr int              manifest_version = 1;
constexpr std::string_view data_filename_prefix{"data_rank"};
};  // namespace save_constants

template <typename derived_type, typename for_all_args>
struct base_save_load;

/*
template <typename T>
concept HasExtendManifest = requires(const T& v) {
  v.extend_manifest(std::declval<boost::json::object&>());
};
*/

template <typename T>
concept HasCustomSave = requires(T& v) {
  v.custom_save(
      std::declval<std::filesystem::path>(),
      std::declval<
          typename base_save_load<T, typename T::for_all_args>::manifest_t&>());
};

template <typename T>
concept HasCustomLoad = requires(T& v) {
  v.custom_load(
      std::declval<std::filesystem::path>(),
      std::declval<
          typename base_save_load<T, typename T::for_all_args>::manifest_t&>());
};

template <typename T>
concept HasSavePrologue = requires(T& v) {
  v.save_prologue(

      std::declval<std::filesystem::path>(),
      std::declval<
          typename base_save_load<T, typename T::for_all_args>::manifest_t&>());
};

template <typename T>
concept HasSaveEpilogue = requires(T& v) {
  v.save_epilogue(
      std::declval<std::filesystem::path>(),
      std::declval<
          typename base_save_load<T, typename T::for_all_args>::manifest_t&>());
};

template <typename T>
concept HasLoadPrologue = requires(T& v) {
  v.load_prologue(
      std::declval<std::filesystem::path>(),
      std::declval<
          typename base_save_load<T, typename T::for_all_args>::manifest_t&>());
};

template <typename T>
concept HasLoadEpilogue = requires(T& v) {
  v.load_epilogue(
      std::declval<std::filesystem::path>(),
      std::declval<
          typename base_save_load<T, typename T::for_all_args>::manifest_t&>());
};

/**
 * @brief Curiously-recurring template pattern struct that provides save
 * and load functions for containers
 */
template <typename derived_type, typename for_all_args>
struct base_save_load {
 public:
  using manifest_t       = boost::json::object;
  using output_archive_t = cereal::BinaryOutputArchive;
  using input_archive_t  = cereal::BinaryInputArchive;

  // Allow friend access for from_saved function to be able to call load
  friend derived_type ygm::container::from_saved<derived_type>(
      ygm::comm&, const std::filesystem::path&, bool check_types);

  /**
   * @brief Function to save a container
   *
   * @param save_path Path to store saved container
   *
   * @details The save logic can be completely overwritten by providing
   * a `custom_save()` method in the container class. When using the
   * save operation provided in the CRTP class, the save steps
   * are
   * 1. create the manifest object in memory
   * 2. complete the `save_prologue(path, manifest)` function, if provided
   * 3. save container data to files
   * 4. complete the `save_epilogue(path, manifest)` function, if provided
   * 5. write the manifest object from memory to a file alongside the saved
   * data
   */
  void save(std::filesystem::path save_path) {
    derived_type* derived_this = static_cast<derived_type*>(this);

    derived_this->comm().barrier();

    save_path /= "";  // Append directory separator if not present
    ygm::utility::fs::make_directories(save_path);

    manifest_t manifest_obj = create_manifest();

    if constexpr (HasCustomSave<derived_type>) {
      derived_this->custom_save(save_path, manifest_obj);
    } else {
      if constexpr (HasSavePrologue<derived_type>) {
        derived_this->save_prologue(save_path, manifest_obj);
      }

      std::filesystem::path rank_path =
          save_path / (std::string(save_constants::data_filename_prefix) +
                       std::to_string(derived_this->comm().rank()));
      std::ofstream ofs(rank_path);
      // cereal::PortableBinaryOutputArchive archive(ofs);
      output_archive_t archive(ofs);

      if constexpr (SingleItemTuple<for_all_args>) {
        for (const auto& value : *derived_this) {
          archive(value);
        }
      } else if constexpr (DoubleItemTuple<for_all_args>) {
        for (const auto& [key, value] : *derived_this) {
          archive(key, value);
        }
      } else {
        derived_this->comm().cerr()
            << "Unable to loop over container to save" << std::endl;
      }

      derived_this->comm().cf_barrier();

      if constexpr (HasSaveEpilogue<derived_type>) {
        derived_this->save_epilogue(save_path, manifest_obj);
      }
    }

    write_manifest(save_path / "manifest.json", manifest_obj);
  }

 protected:
  /**
   * @brief Function to load a container
   *
   * @param save_path Path to saved container
   * @param check_types Optional bool indicating whether types should be checked
   * before desave. Defaults to true.
   *
   * @details The desave logic can be completely overwritten by
   * providing a `custom_load()` method in the container class. When
   * using the desave operation provided in the CRTP class, the
   * desave steps are:
   * 1. Read the manifest for the container
   * 2. Check the types (if check_types == true)
   * 3. complete the `load_prologue(path, manifest)` function, if
   * provided
   * 4. load from files to container
   * 5. complete the `load_epilogue(path, manifest)` function, if
   * provided The ability to run without checking types is provided for cases
   * when the typeid for the stored data does not agree with that stored in the
   * manifest. This can occur, for instance, if the save and
   * desave executables contain the same class that gets saved,
   * but with each having unique names. This is mainly included as a potential
   * option that can give access to otherwise inaccessible saved data.
   */
  void load(std::filesystem::path save_path, bool check_types = true) {
    derived_type* derived_this = static_cast<derived_type*>(this);

    save_path /= "";
    manifest_t manifest_obj = read_manifest(save_path / "manifest.json");

    if (check_types) {
      check_saved_types(manifest_obj);
    }

    if constexpr (HasCustomLoad<derived_type>) {
      derived_this->custom_save(save_path, manifest_obj);
    } else {
      if constexpr (HasLoadPrologue<derived_type>) {
        derived_this->load_prologue(save_path, manifest_obj);
      }

      std::vector<int> local_read_rank_ids = assign_saved_rank_files(save_path);

      for (const int rank_id : local_read_rank_ids) {
        std::filesystem::path rank_path =
            save_path / (std::string(save_constants::data_filename_prefix) +
                         std::to_string(rank_id));
        std::ifstream   ifs(rank_path, std::ios::binary);
        input_archive_t archive(ifs);

        boost::json::value jv = manifest_obj["comm_size"];
        bool               local_insert_flag =
            ((rank_id == derived_this->comm().rank()) and
             (jv.to_number<int64_t>() == derived_this->comm().size()));
        // This check for when the archive is empty only works with binary
        // archives.
        while (ifs.peek() != EOF) {
          if constexpr (SingleItemTuple<for_all_args>) {
            typename std::tuple_element<0, for_all_args>::type val;
            archive(val);

            // Only use local_insert if save and
            // load communicator configurations match and the
            // container derives from base_async_insert (implying the existence
            // of local_insert);
            if constexpr (std::is_base_of_v<
                              ygm::container::detail::base_async_insert_value<
                                  derived_type, for_all_args>,
                              derived_type>) {
              if (local_insert_flag) {
                derived_this->local_insert(val);
              } else {
                derived_this->async_insert(val);
              }
            } else {
              derived_this->async_insert(val);
            }
          } else if constexpr (DoubleItemTuple<for_all_args>) {
            typename std::tuple_element<0, for_all_args>::type key;
            typename std::tuple_element<1, for_all_args>::type val;
            archive(key, val);

            if constexpr (std::is_base_of_v<ygm::container::detail::
                                                base_async_insert_key_value<
                                                    derived_type, for_all_args>,
                                            derived_type>) {
              if (local_insert_flag) {
                derived_this->local_insert(key, val);
              } else {
                derived_this->async_insert(key, val);
              }
            } else {
              derived_this->async_insert(key, val);
            }
          }
        }
      }

      if constexpr (HasLoadEpilogue<derived_type>) {
        derived_this->load_epilogue(save_path, manifest_obj);
      }
    }

    derived_this->comm().barrier();
  }

  /**
   * @brief Create a manifest of basic information about a saved container
   * for writing to a file alongside saved data. Containers can update the
   * manifest through a save prologue and epilogue methods.
   */
  manifest_t create_manifest() {
    derived_type* derived_this = static_cast<derived_type*>(this);

    manifest_t manifest_obj{};
    manifest_obj["version"]   = save_constants::manifest_version;
    manifest_obj["comm_size"] = derived_this->comm().size();
    manifest_obj["container_type"] =
        typeid(typename derived_type::container_type).name();

    // TODO: Case for neither condition?
    if constexpr (SingleItemTuple<for_all_args>) {
      manifest_obj["value_type"] =
          typeid(typename derived_type::value_type).name();
    } else if constexpr (DoubleItemTuple<for_all_args>) {
      manifest_obj["key_type"] = typeid(typename derived_type::key_type).name();
      manifest_obj["mapped_type"] =
          typeid(typename derived_type::mapped_type).name();
    }

    if constexpr (HasSize<derived_type>) {
      manifest_obj["size"] = derived_this->size();
    }

    return manifest_obj;
  }

  /**
   * @brief Write a manifest file that contains basic information about
   * saved container. Containers can update the manifest before it is
   * written.
   *
   * @param manifest_path Path for manifest file
   * @param manifest_obj Manifest container basic container information
   */
  void write_manifest(const std::filesystem::path& manifest_path,
                      const manifest_t&            manifest_obj) {
    derived_type* derived_this = static_cast<derived_type*>(this);

    // Write manifest from rank 0
    if (derived_this->comm().rank0()) {
      std::ofstream ofs(manifest_path);
      ofs << manifest_obj << std::endl;
    }
  }

  /**
   * @brief Read the manifest file with basic information created when
   * saving a container. Container class can check additional
   * container-specific info added to the manifest.
   *
   * @param path Path to manifest file
   * @param check_types Boolean indicating whether type IDs are to be checked
   * when reading manifests.
   */
  manifest_t read_manifest(const std::filesystem::path& manifest_path) {
    derived_type* derived_this = static_cast<derived_type*>(this);

    manifest_t manifest_obj;

    if (derived_this->comm().rank0()) {
      std::ifstream      ifs(manifest_path);
      std::ostringstream ss;
      ss << ifs.rdbuf();
      std::string manifest_contents = ss.str();

      manifest_obj = boost::json::parse(manifest_contents).as_object();
    }

    ygm::bcast(manifest_obj, 0, derived_this->comm());

    return manifest_obj;
  }

  /**
   * @brief Check that saved types are the same as those in the container
   * being loadd into
   *
   * @param manifest_obj Manifest with information about types at save
   * time
   */
  void check_saved_types(const manifest_t& manifest_obj) {
    if constexpr (SingleItemTuple<for_all_args>) {
      YGM_ASSERT_RELEASE(typeid(typename derived_type::value_type).name() ==
                         manifest_obj.at("value_type"));
    } else if constexpr (DoubleItemTuple<for_all_args>) {
      YGM_ASSERT_RELEASE(typeid(typename derived_type::key_type).name() ==
                         manifest_obj.at("key_type"));
      YGM_ASSERT_RELEASE(typeid(typename derived_type::mapped_type).name() ==
                         manifest_obj.at("mapped_type"));
    }

    // Check tag of container used during save
    YGM_ASSERT_RELEASE(typeid(typename derived_type::container_type).name() ==
                       manifest_obj.at("container_type"));
  }

  /**
   * @brief Assign saved data files to ranks for reading
   *
   * @param save_path Path to directory of saved data files
   * @return local_read_rank_ids A vector of rank IDs representing the data
   * files for the local rank to open and load
   */
  std::vector<int> assign_saved_rank_files(
      const std::filesystem::path& save_path) {
    derived_type*    derived_this = static_cast<derived_type*>(this);
    std::vector<int> local_read_rank_ids;

    ygm::comm& c = derived_this->comm();

    auto p_local_read_rank_ids = c.make_ygm_ptr(local_read_rank_ids);

    if (c.rank0()) {
      for (const auto& dir_entry :
           std::filesystem::directory_iterator(save_path)) {
        auto filename = std::filesystem::path(dir_entry).filename();
        auto pos = filename.string().find(save_constants::data_filename_prefix);
        if (pos != std::string::npos) {
          std::string filename_str = filename.string();
          int         rank         = std::stoi(filename_str.substr(
              pos + save_constants::data_filename_prefix.size()));

          int dest = rank % c.size();
          c.async(
              dest,
              [](int file_rank, auto p_local_read_rank_ids) {
                p_local_read_rank_ids->push_back(file_rank);
              },
              rank, p_local_read_rank_ids);
        }
      }
    }

    c.barrier();

    return local_read_rank_ids;
  }
};
}  // namespace detail

}  // namespace ygm::container
