//============================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//============================================================================

#include <vtkm/cont/ArrayHandleConcatenate.h>
#include <vtkm/cont/ArrayHandleIndex.h>

#include <vtkm/cont/testing/Testing.h>

namespace
{

constexpr vtkm::Id ARRAY_SIZE = 4;

void TestConcatOfConcat()
{
  vtkm::cont::ArrayHandleIndex array1(ARRAY_SIZE);
  vtkm::cont::ArrayHandleIndex array2(2 * ARRAY_SIZE);

  vtkm::cont::ArrayHandleConcatenate<vtkm::cont::ArrayHandleIndex, vtkm::cont::ArrayHandleIndex>
    array3(array1, array2);

  vtkm::cont::ArrayHandleIndex array4(ARRAY_SIZE);
  vtkm::cont::ArrayHandleConcatenate<
    vtkm::cont::ArrayHandleConcatenate<vtkm::cont::ArrayHandleIndex,  // 1st
                                       vtkm::cont::ArrayHandleIndex>, // ArrayHandle
    vtkm::cont::ArrayHandleIndex>                                     // 2nd ArrayHandle
    array5;
  {
    array5 = vtkm::cont::make_ArrayHandleConcatenate(array3, array4);
  }

  vtkm::cont::printSummary_ArrayHandle(array5, std::cout, true);

  VTKM_TEST_ASSERT(array5.GetNumberOfValues() == 4 * ARRAY_SIZE);

  // Check the values in array5. If array5 is correct, all the `ArrayHandleConcatinate`s
  // (such as in array3) must be working.
  auto portal = array5.ReadPortal();
  for (vtkm::Id index = 0; index < ARRAY_SIZE; ++index)
  {
    VTKM_TEST_ASSERT(portal.Get(index) == index);
    VTKM_TEST_ASSERT(portal.Get(index + (3 * ARRAY_SIZE)) == index);
  }
  for (vtkm::Id index = 0; index < (2 * ARRAY_SIZE); ++index)
  {
    VTKM_TEST_ASSERT(portal.Get(index + ARRAY_SIZE) == index);
  }
}

void TestConcatenateEmptyArray()
{
  std::vector<vtkm::Float64> vec;
  for (vtkm::Id i = 0; i < ARRAY_SIZE; i++)
  {
    vec.push_back(vtkm::Float64(i) * 1.5);
  }

  using CoeffValueType = vtkm::Float64;
  using CoeffArrayTypeTmp = vtkm::cont::ArrayHandle<CoeffValueType>;
  using ArrayConcat = vtkm::cont::ArrayHandleConcatenate<CoeffArrayTypeTmp, CoeffArrayTypeTmp>;
  using ArrayConcat2 = vtkm::cont::ArrayHandleConcatenate<ArrayConcat, CoeffArrayTypeTmp>;

  CoeffArrayTypeTmp arr1 = vtkm::cont::make_ArrayHandle(vec, vtkm::CopyFlag::Off);
  CoeffArrayTypeTmp arr2, arr3;

  ArrayConcat arrConc(arr2, arr1);
  ArrayConcat2 arrConc2(arrConc, arr3);

  vtkm::cont::printSummary_ArrayHandle(arrConc2, std::cout, true);

  VTKM_TEST_ASSERT(arrConc2.GetNumberOfValues() == ARRAY_SIZE);
}

void TestConcatenateFill()
{
  using T = vtkm::FloatDefault;
  vtkm::cont::ArrayHandle<T> array1;
  vtkm::cont::ArrayHandle<T> array2;
  array1.Allocate(ARRAY_SIZE);
  array2.Allocate(ARRAY_SIZE);

  auto concatArray = vtkm::cont::make_ArrayHandleConcatenate(array1, array2);

  const T value0 = TestValue(0, T{});
  const T value1 = TestValue(1, T{});
  const T value2 = TestValue(2, T{});

  VTKM_STATIC_ASSERT_MSG((ARRAY_SIZE % 2) == 0, "ARRAY_SIZE must be even for this test.");

  concatArray.Fill(value2, 3 * ARRAY_SIZE / 2);
  concatArray.Fill(value1, ARRAY_SIZE / 2, 3 * ARRAY_SIZE / 2);
  concatArray.Fill(value0, 0, ARRAY_SIZE / 2);

  vtkm::cont::printSummary_ArrayHandle(concatArray, std::cout, true);

  auto portal = concatArray.ReadPortal();
  for (vtkm::Id index = 0; index < (ARRAY_SIZE / 2); ++index)
  {
    VTKM_TEST_ASSERT(portal.Get(index) == value0);
  }
  for (vtkm::Id index = (ARRAY_SIZE / 2); index < (3 * ARRAY_SIZE / 2); ++index)
  {
    VTKM_TEST_ASSERT(portal.Get(index) == value1);
  }
  for (vtkm::Id index = (3 * ARRAY_SIZE / 2); index < (2 * ARRAY_SIZE); ++index)
  {
    VTKM_TEST_ASSERT(portal.Get(index) == value2);
  }
}

void TestArrayHandleConcatenate()
{
  TestConcatOfConcat();
  TestConcatenateEmptyArray();
  TestConcatenateFill();
}

} // anonymous namespace

int UnitTestArrayHandleConcatenate(int argc, char* argv[])
{
  //TestConcatenateEmptyArray();
  return vtkm::cont::testing::Testing::Run(TestArrayHandleConcatenate, argc, argv);
}
