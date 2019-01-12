/******************************************************************************
 * Copyright 2018 NVIDIA Corporation. All rights reserved.
 *****************************************************************************/
/// \file
/// \brief Scene element for sparse volumes.

#ifndef NVIDIA_INDEX_ISPARSE_VOLUME_SCENE_ELEMENT_H
#define NVIDIA_INDEX_ISPARSE_VOLUME_SCENE_ELEMENT_H

#include <mi/base/interface_declare.h>
#include <mi/dice.h>

#include <nv/index/idistributed_data.h>

namespace nv
{
namespace index
{

/// Interface for sparse volume scene elements.
/// @ingroup nv_index_scene_description_shape
///
class ISparse_volume_scene_element
  : public mi::base::Interface_declare<0xbb6ad4b7, 0xd42c, 0x47f7, 0xb4, 0x46, 0xe1, 0xb, 0x1, 0xb,
      0x76, 0x1b, nv::index::IDistributed_data>
{
public:
  /// Returns the tag of the import strategy that is used for
  /// loading the dataset.
  ///
  /// \return tag of the IDistributed_data_import_strategy
  virtual mi::neuraylib::Tag_struct get_import_callback() const = 0;
};

} // namespace index
} // namespace nv

#endif // NVIDIA_INDEX_ISPARSE_VOLUME_SCENE_ELEMENT_H
