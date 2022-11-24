/*=========================================================================

  Program:   Visualization Toolkit
  Module:    X3D.hxx

  Copyright (c) 2021, Los Alamos National Laboratory
  All rights reserved.
  See Copyright.md for details.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED. See the above copyright notice for more information.

=========================================================================*/
/**
   \file X3D.hxx

   Constants, typedefs, and utility functions for Reader and
   Writer classes.

   \author Mark G. Gray <gray@lanl.gov>
*/

#ifndef X3D_HXX
#define X3D_HXX

#include <array>
#include <map>
#include <string>
#include <vector>

template <typename T, std::size_t N>
std::ostream& operator<<(std::ostream& os, const std::array<T, N>& vec)
{
  os << "{";
  for (auto elem : vec)
  {
    os << elem << " ";
  }
  os << "}";
  return os;
}

template <typename T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& vec)
{
  for (auto elem : vec)
  {
    os << elem << " ";
  }
  return os;
}

namespace X3D
{
// Supported X3D versions
enum class Version
{
  v1_0,
  v1_3
};

// Magic string which must be at beginning of an X3D file
const std::string MAGIC_STRING = "x3dtoflag ascii";

// Top level section headings in X3D file in order
const std::vector<std::string> TOP_BLOCK = { "header", "matnames", "mateos", "matopc", "nodes",
  "faces", "cells", "slaved_nodes", "ghost_nodes", "cell_data", "node_data" };

// Keys in an X3D file header block in order
const std::vector<std::string> HEADER_KEYS = { "process", "numdim", "materials", "nodes", "faces",
  "elements", "ghost_nodes", "slaved_nodes", "nodes_per_slave", "nodes_per_face", "faces_per_cell",
  "node_data_fields", "cell_data_fields" };

// X3D block representation in STL data structures
// Use PascalCase for types
typedef std::map<std::string, int> Header;  // Header Data Block
typedef std::vector<std::string> Materials; // Material Data Blocks
typedef std::array<double, 3> Node;
typedef std::vector<Node> Nodes; // Coordinate Data Blck
struct Face
{
  std::vector<int> node_id;
  int face_id; // X3D local face ID
  int neighbor_process_id;
  int neighbor_face_id;
};
typedef std::vector<Face> Faces;             // Faces Data Block
typedef std::vector<std::vector<int>> Cells; // Cell Data Block
struct ConstrainedNode
{
  int vertex_id;
  std::vector<int> master;
};
typedef std::vector<ConstrainedNode> ConstrainedNodes; // Constrained Node Block
typedef std::array<int, 4> SharedNode;
typedef std::vector<SharedNode> SharedNodes; // Shared Nodes (on Parallel Boundary) Block
struct CellData                              // Cell Data Block
{
  std::vector<std::string> names;
  std::vector<int> matid;
  std::vector<int> partelm;
  std::map<std::string, std::vector<double>> fields;
};
struct NodeData // Point-centered Physical Data Block
{
  std::vector<std::string> names;
  std::map<std::string, std::vector<Node>> fields;
};

/**
   \function error_message

   Format error message for ReadError, WriteError
*/
inline std::string error_message(
  const std::string& expect, const std::string& found, const std::string& where)
{
  return "Expect: \"" + expect + "\"; found: \"" + found + "\" in " + where;
}

/**
   \function error_message

   Format error message for ReadError, WriteError
*/
inline std::string error_message(int expect, int found, const std::string& where)
{
  return error_message(std::to_string(expect), std::to_string(found), where);
}

}
#endif
