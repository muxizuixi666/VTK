/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkVolumeRayCastMIPFunction.cxx
  Language:  C++
  Date:      $Date$
  Version:   $Revision$


Copyright (c) 1993-1998 Ken Martin, Will Schroeder, Bill Lorensen.

This software is copyrighted by Ken Martin, Will Schroeder and Bill Lorensen.
The following terms apply to all files associated with the software unless
explicitly disclaimed in individual files. This copyright specifically does
not apply to the related textbook "The Visualization Toolkit" ISBN
013199837-4 published by Prentice Hall which is covered by its own copyright.

The authors hereby grant permission to use, copy, and distribute this
software and its documentation for any purpose, provided that existing
copyright notices are retained in all copies and that this notice is included
verbatim in any distributions. Additionally, the authors grant permission to
modify this software and its documentation for any purpose, provided that
such modifications are not distributed without the explicit consent of the
authors and that existing copyright notices are retained in all copies. Some
of the algorithms implemented by this software are patented, observe all
applicable patent law.

IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY FOR
DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT
OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY DERIVATIVES THEREOF,
EVEN IF THE AUTHORS HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES, INCLUDING,
BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE IS PROVIDED ON AN
"AS IS" BASIS, AND THE AUTHORS AND DISTRIBUTORS HAVE NO OBLIGATION TO PROVIDE
MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.


=========================================================================*/
#include <math.h>

#include "vtkVolumeRayCastMIPFunction.h"
#include "vtkVolume.h"

#define vtkRoundFuncMacro(x)   (int)((x)+0.5)

// Macro for trilinear interpolation - do four linear interpolations on
// edges, two linear interpolations between pairs of edges, then a final
// interpolation between faces
#define vtkTrilinFuncMacro(v,x,y,z,a,b,c,d,e,f,g,h)         \
        t00 =   a + (x)*(b-a);      \
        t01 =   c + (x)*(d-c);      \
        t10 =   e + (x)*(f-e);      \
        t11 =   g + (x)*(h-g);      \
        t0  = t00 + (y)*(t01-t00);  \
        t1  = t10 + (y)*(t11-t10);  \
        v   =  t0 + (z)*(t1-t0);

// This is the templated function that actually casts a ray and computes
// the maximum value.  It is valid for unsigned char and unsigned short,
template <class T>
static void CastMaxScalarValueRay( T *data_ptr,
				   VTKRayCastRayInfo *rayInfo,
				   VTKRayCastVolumeInfo *volumeInfo )
{
  float     triMax, triValue;
  int       max;
  float     max_opacity;
  int       loop;
  int       xinc, yinc, zinc;
  int       voxel[3], prev_voxel[3];
  float     ray_position[3];
  T         A, B, C, D, E, F, G, H;
  float     t00, t01, t10, t11, t0, t1;
  int       Binc, Cinc, Dinc, Einc, Finc, Ginc, Hinc;
  float     xoff, yoff, zoff;
  T         *dptr;
  int       num_steps;
  float     *ray_increment;
  float     *grayArray, *RGBArray;
  float     *scalarArray;
  T         nnValue, nnMax;

  num_steps = rayInfo->NumberOfStepsToTake;
  ray_increment = rayInfo->TransformedIncrement;

  grayArray = volumeInfo->Volume->GetGrayArray();
  RGBArray = volumeInfo->Volume->GetRGBArray();
  scalarArray = volumeInfo->Volume->GetScalarOpacityArray();

  xinc = volumeInfo->DataIncrement[0];
  yinc = volumeInfo->DataIncrement[1];
  zinc = volumeInfo->DataIncrement[2];

  // Initialize the ray position and voxel location
  memcpy( ray_position, rayInfo->TransformedStart, 3*sizeof(float) );

  // If we have nearest neighbor interpolation
  if ( volumeInfo->InterpolationType == VTK_NEAREST_INTERPOLATION )
    {
    voxel[0] = vtkRoundFuncMacro( ray_position[0] );
    voxel[1] = vtkRoundFuncMacro( ray_position[1] );
    voxel[2] = vtkRoundFuncMacro( ray_position[2] );

    // Access the value at this voxel location
    nnMax = *(data_ptr + voxel[2] * zinc +
	      voxel[1] * yinc + voxel[0] );

    // Increment our position and compute our voxel location
    ray_position[0] += ray_increment[0];
    ray_position[1] += ray_increment[1];
    ray_position[2] += ray_increment[2];
    voxel[0] = vtkRoundFuncMacro( ray_position[0] );
    voxel[1] = vtkRoundFuncMacro( ray_position[1] );
    voxel[2] = vtkRoundFuncMacro( ray_position[2] );

    // For each step along the ray
    for ( loop = 1; loop < num_steps; loop++ )
      {	    
      // Access the value at this voxel location
      nnValue = *(data_ptr + voxel[2] * zinc +
		  voxel[1] * yinc + voxel[0] );

      // If this is greater than the max, this is the new max.
      if ( nnValue > nnMax )
	{
	nnMax = nnValue;
	}
      
      // Increment our position and compute our voxel location
      ray_position[0] += ray_increment[0];
      ray_position[1] += ray_increment[1];
      ray_position[2] += ray_increment[2];
      voxel[0] = vtkRoundFuncMacro( ray_position[0] );
      voxel[1] = vtkRoundFuncMacro( ray_position[1] );
      voxel[2] = vtkRoundFuncMacro( ray_position[2] );
      }
    max = (int)nnMax;
    }
  // We are using trilinear interpolation
  else if ( volumeInfo->InterpolationType == VTK_LINEAR_INTERPOLATION )
    {
    voxel[0] = (int)( ray_position[0] );
    voxel[1] = (int)( ray_position[1] );
    voxel[2] = (int)( ray_position[2] );

    // Compute the increments to get to the other 7 voxel vertices from A
    Binc = xinc;
    Cinc = yinc;
    Dinc = xinc + yinc;
    Einc = zinc;
    Finc = zinc + xinc;
    Ginc = zinc + yinc;
    Hinc = zinc + xinc + yinc;
  
    // Set values for the first pass through the loop
    dptr = data_ptr + voxel[2] * zinc + voxel[1] * yinc + voxel[0];
    A = *(dptr);
    B = *(dptr + Binc);
    C = *(dptr + Cinc);
    D = *(dptr + Dinc);
    E = *(dptr + Einc);
    F = *(dptr + Finc);
    G = *(dptr + Ginc);
    H = *(dptr + Hinc);

    // Compute our offset in the voxel, and use that to trilinearly
    // interpolate a value
    xoff = ray_position[0] - (float) voxel[0];
    yoff = ray_position[1] - (float) voxel[1];
    zoff = ray_position[2] - (float) voxel[2];
    vtkTrilinFuncMacro( triMax, xoff, yoff, zoff, A, B, C, D, E, F, G, H );

    // Keep the voxel location so that we know when we've moved into a
    // new voxel
    memcpy( prev_voxel, voxel, 3*sizeof(int) );

    // Increment our position and compute our voxel location
    ray_position[0] += ray_increment[0];
    ray_position[1] += ray_increment[1];
    ray_position[2] += ray_increment[2];      
    voxel[0] = (int)( ray_position[0] );
    voxel[1] = (int)( ray_position[1] );
    voxel[2] = (int)( ray_position[2] );

    // For each step along the ray
    for ( loop = 1; loop < num_steps; loop++ )
      {	    
      // Have we moved into a new voxel? If so we need to recompute A-H
      if ( prev_voxel[0] != voxel[0] ||
	   prev_voxel[1] != voxel[1] ||
	   prev_voxel[2] != voxel[2] )
	{
	dptr = data_ptr + voxel[2] * zinc + voxel[1] * yinc + voxel[0];

	A = *(dptr);
	B = *(dptr + Binc);
	C = *(dptr + Cinc);
	D = *(dptr + Dinc);
	E = *(dptr + Einc);
	F = *(dptr + Finc);
	G = *(dptr + Ginc);
	H = *(dptr + Hinc);

	memcpy( prev_voxel, voxel, 3*sizeof(float) );
	}

      // Compute our offset in the voxel, and use that to trilinearly
      // interpolate a value
      xoff = ray_position[0] - (float) voxel[0];
      yoff = ray_position[1] - (float) voxel[1];
      zoff = ray_position[2] - (float) voxel[2];
      vtkTrilinFuncMacro( triValue, xoff, yoff, zoff, A, B, C, D, E, F, G, H );

      // If this value is greater than max, it is the new max
      if ( triValue > triMax )
	{
	triMax = triValue;
	}

      // Increment our position and compute our voxel location
      ray_position[0] += ray_increment[0];
      ray_position[1] += ray_increment[1];
      ray_position[2] += ray_increment[2];      
      voxel[0] = (int)( ray_position[0] );
      voxel[1] = (int)( ray_position[1] );
      voxel[2] = (int)( ray_position[2] );
      }
    max = (int)triMax;
    }

  if ( max < 0 ) 
    {
    max = 0;
    }
  else if ( max > volumeInfo->Volume->GetArraySize() - 1 )
    {
    max = volumeInfo->Volume->GetArraySize() - 1;
    }

  max_opacity = scalarArray[max];
  
  // Set the return pixel value.  
  if( volumeInfo->ColorChannels == 1 )
    {
    rayInfo->Color[0] = max_opacity * grayArray[max];
    rayInfo->Color[1] = max_opacity * grayArray[max];
    rayInfo->Color[2] = max_opacity * grayArray[max];
    rayInfo->Color[3] = max_opacity;
    }
  else if ( volumeInfo->ColorChannels == 3 )
    {
    rayInfo->Color[0] = max_opacity * RGBArray[max*3];
    rayInfo->Color[1] = max_opacity * RGBArray[max*3+1];
    rayInfo->Color[2] = max_opacity * RGBArray[max*3+2];
    rayInfo->Color[3] = max_opacity;
    }

  rayInfo->Depth = 
    ( max_opacity > 0 )?(volumeInfo->CenterDistance):(VTK_LARGE_FLOAT);

  rayInfo->NumberOfStepsTaken = num_steps;
}


// This is the templated function that actually casts a ray and computes
// the maximum value.  It is valid for unsigned char and unsigned short,
template <class T>
static void CastMaxOpacityRay( T *data_ptr,
			       VTKRayCastRayInfo *rayInfo,
			       VTKRayCastVolumeInfo *volumeInfo )
{
  float     max;
  float     opacity;
  float     value;
  int       max_value;
  int       loop;
  int       xinc, yinc, zinc;
  int       voxel[3];
  int       prev_voxel[3];
  float     ray_position[3];
  T         A, B, C, D, E, F, G, H;
  float     t00, t01, t10, t11, t0, t1;
  int       Binc, Cinc, Dinc, Einc, Finc, Ginc, Hinc;
  float     xoff, yoff, zoff;
  T         *dptr;
  int       steps_this_ray = 0;
  float     *SOTF;
  int       num_steps;
  float     *ray_start, *ray_increment;
  float     *grayArray, *RGBArray;

  num_steps = rayInfo->NumberOfStepsToTake;
  ray_start = rayInfo->TransformedStart;
  ray_increment = rayInfo->TransformedIncrement;


  SOTF = volumeInfo->Volume->GetScalarOpacityArray();
  grayArray = volumeInfo->Volume->GetGrayArray();
  RGBArray = volumeInfo->Volume->GetRGBArray();

  // Set the max value.  This will not always be correct and should be fixed
  max = -999999.0;

  xinc = volumeInfo->DataIncrement[0];
  yinc = volumeInfo->DataIncrement[1];
  zinc = volumeInfo->DataIncrement[2];

  // Initialize the ray position and voxel location
  ray_position[0] = ray_start[0];
  ray_position[1] = ray_start[1];
  ray_position[2] = ray_start[2];

  // If we have nearest neighbor interpolation
  if ( volumeInfo->InterpolationType == VTK_NEAREST_INTERPOLATION )
    {

    voxel[0] = vtkRoundFuncMacro( ray_position[0] );
    voxel[1] = vtkRoundFuncMacro( ray_position[1] );
    voxel[2] = vtkRoundFuncMacro( ray_position[2] );

    // For each step along the ray
    for ( loop = 0; loop < num_steps; loop++ )
      {	    
      // We've taken another step
      steps_this_ray++;
      
      // Access the value at this voxel location
      value = *(data_ptr + voxel[2] * zinc +
		voxel[1] * yinc + voxel[0] );

      if ( value < 0 ) 
	{
	value = 0;
	}
      else if ( value > volumeInfo->Volume->GetArraySize() - 1 )
	{
	value = volumeInfo->Volume->GetArraySize() - 1;
	}

      opacity = SOTF[(int)value];
 
      // If this is greater than the max, this is the new max.
      if ( opacity > max ) 
	{
	max = opacity;
	max_value = value;
	}

      // Increment our position and compute our voxel location
      ray_position[0] += ray_increment[0];
      ray_position[1] += ray_increment[1];
      ray_position[2] += ray_increment[2];
      voxel[0] = vtkRoundFuncMacro( ray_position[0] );
      voxel[1] = vtkRoundFuncMacro( ray_position[1] );
      voxel[2] = vtkRoundFuncMacro( ray_position[2] );
      }
    }
  // We are using trilinear interpolation
  else if ( volumeInfo->InterpolationType == VTK_LINEAR_INTERPOLATION )
    {
    voxel[0] = (int)( ray_position[0] );
    voxel[1] = (int)( ray_position[1] );
    voxel[2] = (int)( ray_position[2] );

    // Compute the increments to get to the other 7 voxel vertices from A
    Binc = xinc;
    Cinc = yinc;
    Dinc = xinc + yinc;
    Einc = zinc;
    Finc = zinc + xinc;
    Ginc = zinc + yinc;
    Hinc = zinc + xinc + yinc;
  
    // Set values for the first pass through the loop
    dptr = data_ptr + voxel[2] * zinc + voxel[1] * yinc + voxel[0];
    A = *(dptr);
    B = *(dptr + Binc);
    C = *(dptr + Cinc);
    D = *(dptr + Dinc);
    E = *(dptr + Einc);
    F = *(dptr + Finc);
    G = *(dptr + Ginc);
    H = *(dptr + Hinc);

    // Keep the voxel location so that we know when we've moved into a
    // new voxel
    prev_voxel[0] = voxel[0];
    prev_voxel[1] = voxel[1];
    prev_voxel[2] = voxel[2];

    // For each step along the ray
    for ( loop = 0; loop < num_steps; loop++ )
      {	    
      // We've taken another step
      steps_this_ray++;

      // Have we moved into a new voxel? If so we need to recompute A-H
      if ( prev_voxel[0] != voxel[0] ||
	   prev_voxel[1] != voxel[1] ||
	   prev_voxel[2] != voxel[2] )
	{
	dptr = data_ptr + voxel[2] * zinc + voxel[1] * yinc + voxel[0];

	A = *(dptr);
	B = *(dptr + Binc);
	C = *(dptr + Cinc);
	D = *(dptr + Dinc);
	E = *(dptr + Einc);
	F = *(dptr + Finc);
	G = *(dptr + Ginc);
	H = *(dptr + Hinc);

	prev_voxel[0] = voxel[0];
	prev_voxel[1] = voxel[1];
	prev_voxel[2] = voxel[2];
	}

      // Compute our offset in the voxel, and use that to trilinearly
      // interpolate a value
      xoff = ray_position[0] - (float) voxel[0];
      yoff = ray_position[1] - (float) voxel[1];
      zoff = ray_position[2] - (float) voxel[2];
      vtkTrilinFuncMacro( value, xoff, yoff, zoff, A, B, C, D, E, F, G, H );

      if ( value < 0 ) 
	{
	value = 0;
	}
      else if ( value > volumeInfo->Volume->GetArraySize() - 1 )
	{
	value = volumeInfo->Volume->GetArraySize() - 1;
	}

      opacity = SOTF[(int)value];
 
      // If this is greater than the max, this is the new max.
      if ( opacity > max ) 
	{
	max = opacity;
	max_value = value;
	}
      
      // Increment our position and compute our voxel location
      ray_position[0] += ray_increment[0];
      ray_position[1] += ray_increment[1];
      ray_position[2] += ray_increment[2];      
      voxel[0] = (int)( ray_position[0] );
      voxel[1] = (int)( ray_position[1] );
      voxel[2] = (int)( ray_position[2] );
      }
    }

  // Set the return pixel value.  The depth value is currently useless and
  // should be fixed.
  if( volumeInfo->ColorChannels == 1 )
    {
    rayInfo->Color[0] = max * grayArray[max_value];
    rayInfo->Color[1] = max * grayArray[max_value];
    rayInfo->Color[2] = max * grayArray[max_value];
    rayInfo->Color[3] = max;
    }
  else if ( volumeInfo->ColorChannels == 3 )
    {
    rayInfo->Color[0] = max * RGBArray[max_value*3];
    rayInfo->Color[1] = max * RGBArray[max_value*3+1];
    rayInfo->Color[2] = max * RGBArray[max_value*3+2];
    rayInfo->Color[3] = max;
    }

  rayInfo->Depth = 
    ( max > 0 )?(volumeInfo->CenterDistance):(VTK_LARGE_FLOAT);

  rayInfo->NumberOfStepsTaken = steps_this_ray;
}

// Construct a new vtkVolumeRayCastMIPFunction 
vtkVolumeRayCastMIPFunction::vtkVolumeRayCastMIPFunction()
{
  this->MaximizeMethod = VTK_MAXIMIZE_SCALAR_VALUE;
}

// Destruct the vtkVolumeRayCastMIPFunction
vtkVolumeRayCastMIPFunction::~vtkVolumeRayCastMIPFunction()
{
}

// This is called from RenderAnImage (in vtkDepthPARCMapper.cxx)
// It uses the integer data type flag that is passed in to
// determine what type of ray needs to be cast (which is handled
// by a templated function. 
void vtkVolumeRayCastMIPFunction::CastRay( VTKRayCastRayInfo *rayInfo,
					   VTKRayCastVolumeInfo *volumeInfo)
{
  void *data_ptr;
  
  data_ptr = volumeInfo->ScalarDataPointer;

  if ( this->MaximizeMethod == VTK_MAXIMIZE_SCALAR_VALUE )
    {
    switch ( volumeInfo->ScalarDataType )
      {
      case VTK_UNSIGNED_CHAR:
	CastMaxScalarValueRay( (unsigned char *)data_ptr, rayInfo, volumeInfo );
	break;
      case VTK_UNSIGNED_SHORT:
	CastMaxScalarValueRay( (unsigned short *)data_ptr, rayInfo, volumeInfo );
      }  
    }
  else
    {
    switch ( volumeInfo->ScalarDataType )
      {
      case VTK_UNSIGNED_CHAR:
	CastMaxOpacityRay( (unsigned char *)data_ptr, rayInfo, volumeInfo );
	break;
      case VTK_UNSIGNED_SHORT:
	CastMaxOpacityRay( (unsigned short *)data_ptr, rayInfo, volumeInfo );
      }  
    }
}

float vtkVolumeRayCastMIPFunction::GetZeroOpacityThreshold( vtkVolume *vtkNotUsed(vol) )
{
  return ( 1.0 );
}

// This is an update method that is called from Render (in
// vtkDepthPARCMapper.cxx).  It allows the specific mapper type to
// update any local caster variables.  In this case, nothing needs
// to be done here
void vtkVolumeRayCastMIPFunction::SpecificFunctionInitialize( 
                                      vtkRenderer *vtkNotUsed(ren), 
				      vtkVolume *vtkNotUsed(vol),
				      VTKRayCastVolumeInfo *vtkNotUsed(volumeInfo),
				      vtkVolumeRayCastMapper *vtkNotUsed(mapper) )
{
}

// Print method for vtkVolumeRayCastMIPFunction
void vtkVolumeRayCastMIPFunction::PrintSelf(ostream& os, vtkIndent indent)
{
  vtkVolumeRayCastFunction::PrintSelf(os,indent);

  os << indent << "Maximize Method: " << this->GetMaximizeMethodAsString()
     << "\n";
}
