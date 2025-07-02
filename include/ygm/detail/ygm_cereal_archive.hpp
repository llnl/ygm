// Copyright 2019-2025 Lawrence Livermore National Security, LLC and other YGM
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <cereal/cereal.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/set.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/tuple.hpp>
#include <cereal/types/utility.hpp>
#include <cereal/types/vector.hpp>
#include <cstring>
#include <ygm/detail/byte_vector.hpp>
#include <ygm/utility/assert.hpp>

namespace cereal {
// ######################################################################
//! An output archive designed to save data in a compact binary representation
/*! This archive outputs data to a stream in an extremely compact binary
    representation with as little extra metadata as possible.

    This archive does nothing to ensure that the endianness of the saved
    and loaded data is the same.  If you need to have portability over
    architectures with different endianness, use PortableYGMOutputArchive.

    When using a binary archive and a file stream, you must use the
    std::ios::binary format flag to avoid having your data altered
    inadvertently.

    \ingroup Archives */
class YGMOutputArchive
    : public OutputArchive<YGMOutputArchive, AllowEmptyClassElision> {
 public:
  //! Construct, outputting to the provided stream
  /*! @param stream The stream to output to.  Can be a stringstream, a file
     stream, or even cout! */
  /*YGMOutputArchive(std::vector<std::byte> &stream)
      : OutputArchive<YGMOutputArchive, AllowEmptyClassElision>(this),
        vec_data(stream) {}*/

  YGMOutputArchive(ygm::detail::byte_vector &stream)
      : OutputArchive<YGMOutputArchive, AllowEmptyClassElision>(this),
        vec_data(stream) {}

  ~YGMOutputArchive() CEREAL_NOEXCEPT = default;

  //! Writes size bytes of data to the output stream
  void saveBinary(const void *data, std::streamsize size) {
    vec_data.push_bytes(data, size);

    // if (writtenSize != size)
    //   throw Exception("Failed to write " + std::to_string(size) +
    //                   " bytes to output stream! Wrote " +
    //                   std::to_string(writtenSize));
  }

 private:
  ygm::detail::byte_vector &vec_data;
};

// ######################################################################
//! An input archive designed to load data saved using YGMOutputArchive
/*  This archive does nothing to ensure that the endianness of the saved
    and loaded data is the same.  If you need to have portability over
    architectures with different endianness, use PortableYGMOutputArchive.

    When using a binary archive and a file stream, you must use the
    std::ios::binary format flag to avoid having your data altered
    inadvertently.

    \ingroup Archives */
class YGMInputArchive
    : public InputArchive<YGMInputArchive, AllowEmptyClassElision> {
 public:
  //! Construct, loading from the provided stream
  YGMInputArchive(std::byte *data, size_t capacity)
      : InputArchive<YGMInputArchive, AllowEmptyClassElision>(this),
        m_pdata(data),
        m_capacity(capacity) {}

  ~YGMInputArchive() CEREAL_NOEXCEPT = default;

  //! Reads size bytes of data from the input stream
  void loadBinary(void *const data, std::streamsize size) {
    YGM_ASSERT_DEBUG(m_position + size <= m_capacity);
    std::memcpy(data, m_pdata + m_position, size);
    m_position += size;

    // if (readSize != size)
    //   throw Exception("Failed to read " + std::to_string(size) +
    //                   " bytes from input stream! Read " +
    //                   std::to_string(readSize));
  }

  bool empty() const {
    YGM_ASSERT_DEBUG(!(m_position > m_capacity));
    return m_position == m_capacity;
  }

 private:
  std::byte *m_pdata;
  size_t     m_position = 0;
  size_t     m_capacity = 0;
};

// ######################################################################
// Common BinaryArchive serialization functions

//! Saving for POD types to binary
template <class T>
inline typename std::enable_if<std::is_arithmetic<T>::value, void>::type
CEREAL_SAVE_FUNCTION_NAME(YGMOutputArchive &ar, T const &t) {
  ar.saveBinary(std::addressof(t), sizeof(t));
}

//! Loading for POD types from binary
template <class T>
inline typename std::enable_if<std::is_arithmetic<T>::value, void>::type
CEREAL_LOAD_FUNCTION_NAME(YGMInputArchive &ar, T &t) {
  ar.loadBinary(std::addressof(t), sizeof(t));
}

//! Serializing NVP types to binary
template <class Archive, class T>
inline CEREAL_ARCHIVE_RESTRICT(YGMInputArchive, YGMOutputArchive)
    CEREAL_SERIALIZE_FUNCTION_NAME(Archive &ar, NameValuePair<T> &t) {
  ar(t.value);
}

//! Serializing SizeTags to binary
template <class Archive, class T>
inline CEREAL_ARCHIVE_RESTRICT(YGMInputArchive, YGMOutputArchive)
    CEREAL_SERIALIZE_FUNCTION_NAME(Archive &ar, SizeTag<T> &t) {
  ar(t.size);
}

//! Saving binary data
template <class T>
inline void CEREAL_SAVE_FUNCTION_NAME(YGMOutputArchive    &ar,
                                      BinaryData<T> const &bd) {
  ar.saveBinary(bd.data, static_cast<std::streamsize>(bd.size));
}

//! Loading binary data
template <class T>
inline void CEREAL_LOAD_FUNCTION_NAME(YGMInputArchive &ar, BinaryData<T> &bd) {
  ar.loadBinary(bd.data, static_cast<std::streamsize>(bd.size));
}
}  // namespace cereal

// register archives for polymorphic support
CEREAL_REGISTER_ARCHIVE(cereal::YGMOutputArchive)
CEREAL_REGISTER_ARCHIVE(cereal::YGMInputArchive)

// tie input and output archives together
CEREAL_SETUP_ARCHIVE_TRAITS(cereal::YGMInputArchive, cereal::YGMOutputArchive)
