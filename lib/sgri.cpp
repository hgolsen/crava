/*#include <math.h>
#include <time.h>
#include <stdio.h>
*/
//#include <iostream>
#include <fstream>
using std::ifstream;
#include <string>
#include <assert.h>

#include "lib/global_def.h"
#include "lib/sgri.h"
#include "lib/lib_misc.h"
#include "lib/log.h"
#include "lib/fileio.hpp"
#include "lib/exception.hpp"
#include "lib/grid.hpp"

using namespace std;
using namespace NRLib2;

Sgri::Sgri(char * fileName, char *errText, int &errCode) :
  Grid<float>(0,0,0),
  x0_(RMISSING),
  y0_(RMISSING),
  z0_(RMISSING),
  dX_(0.0),
  dY_(0.0),
  dZ_(0.0),
  scaleX_(1.0),
  scaleY_(1.0),
  scaleZ_(1.0),
  rotAngle_(0.0),
  dipAngle_(0.0),
  hasComplex_(0)
{
  if (!readHeaderFile(fileName, errText, errCode))
    return;

  ci_ = static_cast<int> (- x0_ / dX_); //Index for x=0 or closest gridpoint below
  cj_ = static_cast<int> (- y0_ / dY_); //Index for y=0 or closest gridpoint below
  ck_ = static_cast<int> (- z0_ / dZ_); //Index for z=0 or closest gridpoint below
  centerX_ = x0_ + ci_ * dX_; //X-value at ci_
  centerY_ = y0_ + cj_ * dY_; //Y-value at cj_
  centerZ_ = z0_ + ck_ * dZ_; //Z-value at ck_

  xMax_ = x0_ + (GetNI()-1) * dX_;
  yMax_ = y0_ + (GetNJ()-1) * dY_;
  zMax_ = z0_ + (GetNK()-1) * dZ_;
  
  ifstream binFile(binFileName_, ios::in | ios::binary);
  if(!binFile) {
    if (errCode == 0) 
      sprintf(errText, "Error: Could not open file %s for reading.\n", binFileName_);
    else 
      sprintf(errText, "%sError: Could not open file %s for reading.\n", errText, binFileName_);
    errCode = 1;
    return;
  }
  binFile.close();

  readBinaryFile(GetN(), errText, errCode);

  return;
}

Sgri::~Sgri() 
{
  delete [] axisLabels_;
  delete [] gridLabels_;
}

bool
Sgri::readHeaderFile(char * fileName, char *errText, int &errCode)
{
  ifstream headerFile(fileName, ios::in); //checkFileOpen performed previously
  int i;
  bool retValue = true;
  string tmpStr;

  //Reading record 1: Version header
  getline(headerFile, tmpStr);
  //Reading record 2: Grid dimension
  headerFile >> dim_;
  if (dim_ < 1 || dim_ > 3) {
    if (errCode == 0)
      sprintf(errText,"Error: Dimension of wavelet in file %s must be 1,2 or 3.\n", fileName);
    else 
      sprintf(errText,"%sError: Dimension of wavelet in file %s must be 1,2 or 3.\n", errText,fileName);
    errCode = 2;
    return (false);
  }
  else if (dim_ == 1) {
    LogKit::writeLog("\nWarning  !!!\n");
    LogKit::writeLog("Dimension for wavelet set to 1. Z-direction for 1D wavelet grid chosen.\n");
    LogKit::writeLog("\n");
  }
  getline(headerFile, tmpStr);
  //Reading record 3 ... 3+dim: Axis labels + grid value label
  axisLabels_ = new string[dim_];
  for (i=0; i<dim_; i++)
    getline(headerFile, axisLabels_[i]);
  getline(headerFile, gridValLabel_);
  int config = IMISSING;
  enum Planes {XZ, ZX, YZ, ZY};
  if (dim_ == 2) { //Must find the right axis configuration
    bool map[2][3];
    string dir[6] = {"X", "Y", "Z", "x", "y", "z"};
    for (i=0; i<2; i++) {
      int nHits = 0;
      for (int j=0; j<3; j++) {
        if ((axisLabels_[i].find(dir[j]) != string::npos) || (axisLabels_[i].find(dir[j+3]) != string::npos)) {
          map[i][j] = true;
          nHits++;
        }
        else
          map[i][j] = false;
      }
      if (nHits == 0) {
        if (errCode == 0)  
          sprintf(errText,"Error: Axis label %d does not contain either X, Y or Z. Must have exactly one direction specified.\n", i);
        else 
          sprintf(errText,"%sError: Axis label %d does not contain either X, Y or Z. Must have exactly one direction specified.\n", errText, i);
        errCode=2;
        return(false);
      }
      else if (nHits > 1) {
        if (errCode == 0)  
          sprintf(errText,"Error: Axis label %d contains at least to of the letters X, Y and Z. Must have exactly one direction specified.\n", i);
        else 
          sprintf(errText,"%sError: Axis label %d contains at least to of the letters X, Y and Z. Must have exactly one direction specified.\n", errText, i);
        errCode=2;
        return(false);
      }
    }
    if ((map[0][0] && map[1][0]) || (map[0][0] && map[1][1]) || (map[0][1] && map[1][0]) || (map[0][1] && map[1][1]) || (map[0][2] && map[1][2])) {
      if (errCode == 0)  
        sprintf(errText,"Error: Axis configuration can not be XX, YY, ZZ, XY or YX.\n");
      else 
        sprintf(errText,"%sError: Axis configuration can not be XX, YY, ZZ, XY or YX.\n", errText);
      errCode=2;
      return(false);
    }
    else {
      if (map[0][0] && map[1][2])
        config = XZ;
      else if (map[0][2] && map[1][0])
        config = ZX;
      else if (map[0][1] && map[1][0])
        config = YZ;
      else
        config = ZY;
    }
  }
  //Reading record 4+dim: Number of grids
  headerFile >> nGrid_;
  if (nGrid_ < 1) {
    if (errCode == 0)  
      sprintf(errText,"Error: Number of grids read from waveletfile %s must be > 0.\n", fileName);
    else 
      sprintf(errText,"%sError: Number of grids read from waveletfile %s must be > 0.\n", errText,fileName);
    errCode=2;
    return (false);
  }
  getline(headerFile, tmpStr);
  //Reading record 5+dim ... 5+dim+ngrid-1: Grid labels
  gridLabels_ = new string[nGrid_];
  for (i=0; i<nGrid_; i++)
    getline(headerFile, gridLabels_[i]);
  float *dValues1 = new float[dim_];
  float *dValues2 = new float[dim_];
  float *dValues3 = new float[dim_];
  int *iValues = new int[dim_];
  //Reading record 5+dim+ngrid: Scaling factor of grid values
  for (i=0; i<dim_; i++)
    headerFile >> dValues1[i];
  getline(headerFile,tmpStr);
  //Reading record 6+dim+ngrid: Number of samples in each dir.
  for (i=0; i<dim_; i++)
    headerFile >> iValues[i];
  getline(headerFile,tmpStr);
  //Reading record 7+dim+ngrid: Grid sampling in each dir.
  for (i=0; i<dim_; i++)
    headerFile >> dValues2[i];
  getline(headerFile,tmpStr);
  //Reading record 8+dim+ngrid: First point coord.
  for (i=0; i<dim_; i++)
    headerFile >> dValues3[i];
  getline(headerFile, tmpStr);

  int nX=1;
  int nY=1; 
  int nZ=1;
  bool xActive = false;
  bool yActive = false;
  if (dim_ == 3) {
    scaleX_ = dValues1[0];
    nX		  = iValues[0];
    dX_		  = dValues2[0];
    x0_		  = dValues3[0];
    scaleY_ = dValues1[1];
    nY		  = iValues[1];
    dY_		  = dValues2[1];
    y0_		  = dValues3[1];
    scaleZ_ = dValues1[2];
    nZ		  = iValues[2];
    dZ_		  = dValues2[2];
    z0_		  = dValues3[2];
  }
  else if (dim_ == 2) { 
    if (config == XZ) {
      xActive = true;
      scaleX_ = dValues1[0];
      nX		  = iValues[0];
      dX_		  = dValues2[0];
      x0_		  = dValues3[0];
      scaleZ_ = dValues1[1];
      nZ      = iValues[1];
      dZ_     = dValues2[1];
      z0_     = dValues3[1];
    }
    if (config == ZX) {
      xActive = true;
      scaleZ_ = dValues1[0];
      nZ		  = iValues[0];
      dZ_		  = dValues2[0];
      z0_		  = dValues3[0];
      scaleX_ = dValues1[1];
      nX      = iValues[1];
      dX_     = dValues2[1];
      x0_     = dValues3[1];
    }
    if (config == YZ) {
      yActive = true;
      scaleY_ = dValues1[0];
      nY		  = iValues[0];
      dY_		  = dValues2[0];
      y0_		  = dValues3[0];
      scaleZ_ = dValues1[1];
      nZ      = iValues[1];
      dZ_     = dValues2[1];
      z0_     = dValues3[1];
    }
    if (config == ZY) {
      yActive = true;
      scaleZ_ = dValues1[0];
      nZ		  = iValues[0];
      dZ_		  = dValues2[0];
      z0_		  = dValues3[0];
      scaleY_ = dValues1[1];
      nY      = iValues[1];
      dY_     = dValues2[1];
      y0_     = dValues3[1];
    }
  }
  else { //dim_ = 1 - currently not an active option
    scaleZ_ = dValues1[0];
    nZ		= iValues[0];
    dZ_		= dValues2[0];
    z0_		= dValues3[0];
  }
  if (nX < 1) {
    if (errCode == 0)  
      sprintf(errText,"Error: Number of samples in X-dir in waveletfile %s must be >= 1.\n", fileName);
    else 
      sprintf(errText,"%sError: Number of samples in X-dir in waveletfile %s must be >= 1.\n", errText,fileName);
    errCode=2;
    retValue = false;
  }
  if (nY < 1) {
    if (errCode == 0)  
      sprintf(errText,"Error: Number of samples in Y-dir in waveletfile %s must be >= 1.\n", fileName);
    else 
      sprintf(errText,"%sError: Number of samples in Y-dir in waveletfile %s must be >= 1.\n", errText,fileName);
    errCode=2;
    retValue = false;
  }
  if (nZ < 1) {
    if (errCode == 0)  
      sprintf(errText,"Error: Number of samples in Z-dir in waveletfile %s must be >= 1.\n", fileName);
    else 
      sprintf(errText,"%sError: Number of samples in Z-dir in waveletfile %s must be >= 1.\n", errText,fileName);
    errCode=2;
    retValue = false;
  }
  if (xActive && (dX_ <= 0.0)) {
    if (errCode == 0)  
      sprintf(errText,"Error: Grid sampling in X-dir in waveletfile %s must be > 0.0.\n", fileName);
    else 
      sprintf(errText,"%sError: Grid sampling in X-dir in waveletfile %s must be > 0.0.\n", errText,fileName);
    errCode=2;
    retValue = false;
  }
  if (yActive && (dY_ <= 0.0)) {
    if (errCode == 0)  
      sprintf(errText,"Error: Grid sampling in Y-dir in waveletfile %s must be > 0.0.\n", fileName);
    else 
      sprintf(errText,"%sError: Grid sampling in Y-dir in waveletfile %s must be > 0.0.\n", errText,fileName);
    errCode=2;
    retValue = false;
  }  
  if (dZ_ <= 0.0) {
    if (errCode == 0)  
      sprintf(errText,"Error: Grid sampling in Z-dir in waveletfile %s must be > 0.0.\n", fileName);
    else 
      sprintf(errText,"%sError: Grid sampling in Z-dir in waveletfile %s must be > 0.0.\n", errText,fileName);
    errCode=2;
    retValue = false;
  }
  if (xActive && (x0_ > 0.0)) {
    if (errCode == 0)  
      sprintf(errText,"Error: Grid origin in X-dir in waveletfile %s must be <= 0.0.\n", fileName);
    else 
      sprintf(errText,"%sError: Grid origin in X-dir in waveletfile %s must be <= 0.0.\n", errText,fileName);
    errCode=2;
    retValue = false;
  }
  if (yActive && (y0_ > 0.0)) {
    if (errCode == 0)  
      sprintf(errText,"Error: Grid origin in Y-dir in waveletfile %s must be <= 0.0.\n", fileName);
    else 
      sprintf(errText,"%sError: Grid origin in Y-dir in waveletfile %s must be <= 0.0.\n", errText,fileName);
    errCode=2;
    retValue = false;
  }
  if (z0_ > 0.0) {
    if (errCode == 0)  
      sprintf(errText,"Error: Grid origin in Z-dir in waveletfile %s must be <= 0.0.\n", fileName);
    else 
      sprintf(errText,"%sError: Grid origin in Z-dir in waveletfile %s must be <= 0.0.\n", errText,fileName);
    errCode=2;
    retValue = false;
  }
  if (xActive && ((x0_ + (nX * dX_)) < 0.0)) {
    if (errCode == 0)  
      sprintf(errText,"Error: Grid end-point in X-dir in waveletfile %s must be >= 0.0.\n", fileName);
    else 
      sprintf(errText,"%sError: Grid end-point in X-dir in waveletfile %s must be >= 0.0.\n", errText,fileName);
    errCode=2;
    retValue = false;
  }
  if (yActive && ((y0_ + (nY * dY_)) < 0.0)) {
    if (errCode == 0)  
      sprintf(errText,"Error: Grid end-point in Y-dir in waveletfile %s must be >= 0.0.\n", fileName);
    else 
      sprintf(errText,"%sError: Grid end-point in Y-dir in waveletfile %s must be >= 0.0.\n", errText,fileName);
    errCode=2;
    retValue = false;
  }
  if (z0_ + (nZ * dZ_) < 0.0) {
    if (errCode == 0)  
      sprintf(errText,"Error: Grid end-point in Z-dir in waveletfile %s must be >= 0.0.\n", fileName);
    else 
      sprintf(errText,"%sError: Grid end-point in Z-dir in waveletfile %s must be >= 0.0.\n", errText,fileName);
    errCode=2;
    retValue = false;
  }
  delete [] dValues1;
  delete [] dValues2;
  delete [] dValues3;
  delete [] iValues;
  Resize(nX,nY,nZ);
  //Reading record 9+dim+ngrid: Angle of rotation
  if (dim_ > 1) {
    headerFile >> dipAngle_;
    if (dim_ > 2)
      headerFile >> rotAngle_;
  }
  getline(headerFile, tmpStr);
  //Reading record 10+dim+ngrid: Undef value
  headerFile >> unDef_;
  getline(headerFile, tmpStr);
  //Reading record 11+dim+ngrid: Filename of binary file
  headerFile >> binFileName_;
  getline(headerFile, tmpStr);
  //Reading record 12+dim+ngrid: Complex values
  headerFile >> hasComplex_;
  if (hasComplex_ != 0 && hasComplex_ != 1) {
    if (errCode == 0)  
      sprintf(errText,"Error: Code for complex grid in waveletfile %s must be 0 or 1.\n", fileName);
    else 
      sprintf(errText,"%sError: Code for complex grid in waveletfile %s must be 0 or 1.\n", errText,fileName);
    errCode=2;
    retValue = false;
  }
  //The remaining records are not relevant, stop reading

  return (retValue);
}

void
Sgri::readBinaryFile(int n, char *errText, int &errCode)
{
  ifstream binFile(binFileName_,ios::in | ios::binary); //Check opening of file before calling this function
 
  if (nGrid_ > 1) {
    LogKit::writeLog("\nWarning  !!!\n");
    LogKit::writeLog("%d %s", nGrid_, "grids read from Sgri-file. Only the first is used.\n");
    LogKit::writeLog("\n");
  }
  if (hasComplex_) {
    if (errCode == 0)  
      sprintf(errText,"Error: Complex grid given in waveletfile %s. Only handling of real grids implemented.\n", binFileName_);
    else 
      sprintf(errText,"%sError: Complex grid given in waveletfile %s. Only handling of real grids implemented.\n", errText, binFileName_);
    errCode=2;
    return;
  }
  
  try {
    ReadBinaryFloatArray(binFile, begin(), n);
  }
  catch (NRLib2::Exception& e) {
    if (errCode == 0)  
      sprintf(errText, "Error: Reading from binary waveletfile %s. %s\n", binFileName_, e.what().c_str());
    else 
      sprintf(errText, "%sError: Reading from binary waveletfile %s. %s\n", errText, binFileName_, e.what().c_str());
    errCode=2;
  }
  
  return;
}

float
Sgri::getWaveletValue(float x, float y, float z) const
{
  if (x < x0_ || x > xMax_ || y < y0_ || y > yMax_ || z < z0_ || z > zMax_)
    return (0.0);

  int i0, i1, j0, j1, k0, k1;
  i0 = static_cast<int> ((x-centerX_) / dX_) + ci_;
  if (i0 + 1 == GetNI())
    i1 = i0;
  else
    i1 = i0 + 1;
  j0 = static_cast<int> ((y-centerY_) / dY_) + cj_;
  if (j0 + 1 == GetNJ())
    j1 = j0;
  else
    j1 = j0 + 1;
  k0 = static_cast<int> ((z-centerZ_) / dZ_) + ck_;
  if (k0 + 1 == GetNK())
    k1 = k0;
  else
    k1 = k0 + 1;

  float C000 = (*this)(i0,j0,k0);
  float C100 = (*this)(i1,j0,k0);
  float C010 = (*this)(i0,j1,k0);
  float C110 = (*this)(i1,j1,k0);
  float C001 = (*this)(i0,j0,k1);
  float C101 = (*this)(i1,j0,k1);
  float C011 = (*this)(i0,j1,k1);
  float C111 = (*this)(i1,j1,k1);

  float xLow = x0_ + i0*dX_;
  float xFrac = (x - xLow) / dX_;
  float C00 = C000 * (1.0f - xFrac) + C100 * xFrac;
  float C10 = C010 * (1.0f - xFrac) + C110 * xFrac;
  float C01 = C001 * (1.0f - xFrac) + C101 * xFrac;
  float C11 = C011 * (1.0f - xFrac) + C111 * xFrac;

  float yLow = y0_ + j0*dY_;
  float yFrac = (y - yLow) / dY_;
  float C0 = C00 * (1.0f - yFrac) + C10 * yFrac;
  float C1 = C01 * (1.0f - yFrac) + C11 * yFrac;
  
  float zLow = z0_ + k0*dZ_;
  float zFrac = (z - zLow) / dZ_;
  float C = C0 * (1.0f - zFrac) + C1 * zFrac;

  return(C);
}

bool               
Sgri::sizeOk(float xLim, float yLim, float zLim)
{
  bool ok = true;

  if (x0_ < -xLim || xMax_ > xLim || y0_ < -yLim || y0_ > yLim || z0_ < -zLim || z0_ > zLim)
    ok = false;

  return (ok);
}