/*=========================================================================

 Program:   Visualization Toolkit
 Module:    VARvtkVTI.h

 Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
 All rights reserved.
 See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

 This software is distributed WITHOUT ANY WARRANTY; without even
 the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 PURPOSE.  See the above copyright notice for more information.

 =========================================================================*/

/*
 * VARvtkVTI.h : class that supports ImageData schema in VTK XML format .vti
 *                  extends abstract ADIOS2xmlVTK
 *
 *  Created on: May 1, 2019
 *      Author: William F Godoy godoywf@ornl.gov
 */

#ifndef VTK_IO_ADIOS2_VAR_SCHEMA_VTK_VARvtkVTI_h
#define VTK_IO_ADIOS2_VAR_SCHEMA_VTK_VARvtkVTI_h

#include <map>
#include <string>
#include <vector>

#include "VAR/schema/vtk/VARvtkBase.h"

#include "vtkImageData.h"
#include "vtkNew.h"

namespace var
{
namespace schema
{
class VARvtkVTI : public VARvtkBase
{
public:
  VARvtkVTI(const std::string& schema, adios2::IO& io, adios2::Engine& engine);
  ~VARvtkVTI();

private:
  /** Could be extended in a container, this is a per-rank ImageData */
  vtkNew<vtkImageData> ImageData;
  /** Store the Whole Extent in physical dimensions, row-major */
  adios2::Dims WholeExtent;

  adios2::Dims GetShape(const types::DataSetType type);
  adios2::Box<adios2::Dims> GetSelection(const types::DataSetType type);

  void DoFill(vtkMultiBlockDataSet* multiBlock, const size_t step) final;
  void ReadPiece(const size_t step, const size_t pieceID) final;

  void Init() final;

#define declare_type(T)                                                                            \
  void SetDimensions(                                                                              \
    adios2::Variable<T> variable, const types::DataArray& dataArray, const size_t step) final;
  VTK_IO_ADIOS2_VAR_ARRAY_TYPE(declare_type)
#undef declare_type

  template<class T>
  void SetDimensionsCommon(
    adios2::Variable<T> variable, const types::DataArray& dataArray, const size_t step);
};

} // end namespace schema
} // end namespace var

#endif /* VTK_IO_ADIOS2_VAR_SCHEMA_VTK_VARvtkVTI_h */