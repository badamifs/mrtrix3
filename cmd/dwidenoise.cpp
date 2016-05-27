/*
 * Copyright (c) 2008-2016 the MRtrix3 contributors
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/
 * 
 * MRtrix is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * 
 * For more details, see www.mrtrix.org
 * 
 */


#include "command.h"
#include "image.h"
#include <Eigen/Dense>
#include <Eigen/SVD>

#define DEFAULT_SIZE 5

using namespace MR;
using namespace App;


void usage ()
{
  DESCRIPTION
  + "denoise DWI data and estimate the noise level based on the optimal threshold for PCA.";

  
  AUTHOR = "Daan Christiaens (daan.christiaens@kuleuven.be) & Jelle Veraart (jelle.veraart@nyumc.org) & J-Donald Tournier (jdtournier.gmail.com)";
  
  
  ARGUMENTS
  + Argument ("dwi", "the input diffusion-weighted image.").type_image_in ()

  + Argument ("out", "the output denoised DWI image.").type_image_out ();


  OPTIONS
  + Option ("size", "set the window size of the denoising filter. (default = " + str(DEFAULT_SIZE) + ")")
    + Argument ("window").type_integer (0, 50)
  
  + Option ("noise", "the output noise map.")
    + Argument ("level").type_image_out();

}


typedef float value_type;


template <class ImageType>
class DenoisingFunctor
{
public:
  DenoisingFunctor (ImageType& dwi, int size)
    : extent(size/2),
      m(dwi.size(3)),
      n(size*size*size),
      X(m,n), Xm(m),
      pos{0, 0, 0}
  { }
  
  void operator () (ImageType& dwi, ImageType& out)
  {
    // Load data in local window
    load_data(dwi);
    // Centre data
    Xm = X.rowwise().mean();
    X.colwise() -= Xm;
    // Compute SVD
    Eigen::JacobiSVD<Eigen::MatrixXd> svd (X, Eigen::ComputeThinU | Eigen::ComputeThinV);
    Eigen::VectorXd s = svd.singularValues();
    VAR(s);
    // Simply threshold at 90% variance for now
    double thres = 0.90 * s.squaredNorm();
    double cumsum = 0.0;
    for (size_t i = 0; i < n; ++i) {
      if (cumsum <= thres)
        cumsum += s[i] * s[i];
      else
        s[i] = 0.0;
    }
    VAR(s);
    // Restore DWI data
    X = svd.matrixU() * s.asDiagonal() * svd.matrixV().adjoint();
    X += Xm;
    Eigen::MatrixXd A (m,2);
    A.col(0) = dwi.row(3).template cast<double>();
    A.col(1) = X.col(n/2);
    VAR(A);
    // Store output
    assign_pos_of(dwi).to(out);
    VAR(out.index(0));
    VAR(out.index(1));
    VAR(out.index(2));
    out.row(3) = X.col(n/2).template cast<value_type>();
  }
  
  void load_data (ImageType& dwi)
  {
    pos[0] = dwi.index(0); pos[1] = dwi.index(1); pos[2] = dwi.index(2);
    X.setZero();
    ssize_t k = 0;
    for (dwi.index(2) = pos[2]-extent; dwi.index(2) <= pos[2]+extent; ++dwi.index(2))
      for (dwi.index(1) = pos[1]-extent; dwi.index(1) <= pos[1]+extent; ++dwi.index(1))
        for (dwi.index(0) = pos[0]-extent; dwi.index(0) <= pos[0]+extent; ++dwi.index(0), ++k)
          if (! is_out_of_bounds(dwi))
            X.col(k) = dwi.row(3).template cast<double>();
    // reset image position
    dwi.index(0) = pos[0];
    dwi.index(1) = pos[1];
    dwi.index(2) = pos[2];
  }
  
private:
  int extent;
  size_t m, n;
  Eigen::MatrixXd X;
  Eigen::VectorXd Xm;
  ssize_t pos[3];
  
};



void run ()
{
  auto dwi_in = Image<value_type>::open (argument[0]).with_direct_io(3);

  auto header = Header (dwi_in);
  header.datatype() = DataType::Float32;
  auto dwi_out = Image<value_type>::create (argument[1], header);
  
  int extent = get_option_value("size", DEFAULT_SIZE);
  
  DenoisingFunctor< Image<value_type> > func (dwi_in, extent);
  
  dwi_in.index(0) = 8;
  dwi_in.index(1) = 9;
  dwi_in.index(2) = 10;
  
  func(dwi_in, dwi_out);
  

}


