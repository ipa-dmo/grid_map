/*
 * Signeddistancefield.hpp
 *
 *  Created on: Aug 16, 2017
 *      Author: Takahiro Miki
 *   Institute: ETH Zurich, Autonomous Systems Lab
 */

#include <grid_map_sdf/SignedDistanceField.hpp>
#include <grid_map_core/GridMap.hpp>

// signed distance field
#include "dt.h"


using namespace grid_map;
using namespace Eigen;

namespace grid_map_sdf{

SignedDistanceField::SignedDistanceField():maxDistance_(100)
{

}

SignedDistanceField::~SignedDistanceField()
{

}

Size SignedDistanceField::getMapSize(){
  return size_;
}

double SignedDistanceField::getMapResolution(){
  return resolution_;
}

Position SignedDistanceField::getMapPosition(){
  return mapPosition_;
}

void SignedDistanceField::calculateSignedDistanceField(GridMap &map, std::string layer, double heightClearance){
  resolution_ = map.getResolution();
  mapPosition_ = map.getPosition();
  size_ = map.getSize();
  MatrixXf mat = map.get(layer); 

  // Get min and max height and fill the max height in the nan area.
  double minHeight = 10000;
  double maxHeight = -10000;
  std::vector<Eigen::Array2i> nanIndices;
  for (int y = 0; y < mat.cols(); y++) {
    for (int x = 0; x < mat.rows(); x++) {
      if(!std::isnan(mat(x, y))){
        if(mat(x, y) > maxHeight)
          maxHeight = mat(x, y);
        if(mat(x, y) < minHeight)
          minHeight = mat(x, y);
      }
      else{
        nanIndices.push_back(Eigen::Array2i(x, y));
      }
    }
  }
  for(Eigen::Array2i index: nanIndices){
    mat(index.x(), index.y()) = maxHeight;
  }
  // Height range of the signed distance field is higher than the max height
  maxHeight += heightClearance;

  MatrixXf sdfElevationAbove = MatrixXf::Ones(mat.rows(), mat.cols()) * maxDistance_; 
  MatrixXf sdfElevationBelow = MatrixXf::Ones(mat.rows(), mat.cols()) * maxDistance_; 
  MatrixXf sdfLayer = MatrixXf::Zero(mat.rows(), mat.cols()); 
  // MatrixXf sdfLayer(mat.rows(), mat.cols()); 
  std::vector<MatrixXf> sdf;
  // std::vector<MatrixXf> sdfResult;

  // Calculate signed distance field in downwards direction.
  // std::vector<MatrixXf> sdfElevationBelowList;
  // for(float j = maxHeight; j > minHeight; j-=resolution_){
  //   for (int y = 0; y < sdfElevationBelow.cols(); y++) {
  //     for (int x = 0; x < sdfElevationBelow.rows(); x++) {
  //       if(sdfElevationBelow(x, y) == maxDistance_ && mat(x, y) > j)
  //         sdfElevationBelow(x, y) = j - mat(x, y);
  //       else if(sdfElevationBelow(x, y) != maxDistance_)
  //         sdfElevationBelow(x, y) -= resolution_;
  //     }
  //   }
  //   sdfElevationBelowList.push_back(sdfElevationBelow);
  // }
  zIndexStartHeight_ = minHeight;

  // Calculate signed distance field from bottom.
  for(float i = minHeight; i < maxHeight; i+=resolution_){
    Eigen::Matrix<bool, Eigen::Dynamic, Eigen::Dynamic>  obstacleFreeField = mat.array() < i;
    Eigen::Matrix<bool, Eigen::Dynamic, Eigen::Dynamic>  obstacleField = obstacleFreeField.array() < 1;
    MatrixXf sdfObstacle = get2dSDF(obstacleField);
    MatrixXf sdfObstacleFree = get2dSDF(obstacleFreeField);
    MatrixXf sdf2d;
    // If 2d sdfObstacleFree calculation failed, neglect this sdf
    // to avoid extreme small distances(-INF).
    if ((sdfObstacleFree.array() > 100000).any())
      sdf2d = sdfObstacle;
    else
      sdf2d = sdfObstacle - sdfObstacleFree;
    sdf2d *= resolution_;
    // MatrixXf sdfElevationBelow = sdfElevationBelowList.back();
    // sdfElevationBelowList.pop_back();
    for (int y = 0; y < sdfElevationAbove.cols(); y++) {
      for (int x = 0; x < sdfElevationAbove.rows(); x++) {
        // Update sdfElevation from the layer below.
        if(sdfElevationAbove(x, y) == maxDistance_ && mat(x, y) < i)
          sdfElevationAbove(x, y) = i - mat(x, y);
        else if(sdfElevationAbove(x, y) != maxDistance_ && mat(x, y) < i)
          sdfElevationAbove(x, y) = sdfLayer(x, y) + resolution_;
        // SDF is the minimum distance between 2d, elevationAbove and elevationBelow
        // std::cout << "col, row " << sdfElevationAbove.cols() << ", " << sdfElevationAbove.rows() << std::endl;
        // std::cout << x << ", " << y << ", " << i << std::endl;
        // std::cout << "[" << sdfLayer(x, y) << ", " << mat(x, y) << ", " << sdf2d(x, y) << ", " << sdfElevationAbove(x, y) << ", " << sdfElevationBelow(x, y) << "]" << std::endl;
        if (sdf2d(x, y) == 0){
          float negativeMinDistance = i - mat(x, y);
          sdfLayer(x, y) = negativeMinDistance;
        }
        else if (sdf2d(x, y) < 0){
          // double negativeMinDistance = -std::min(fabs(sdf2d(x, y)), fabs(sdfElevationBelow(x, y)));
          // std::cout << "sdf elevation below, -" << sdfElevationBelow(x, y) << ", " << mat(x, y) - i << std::endl;
          float negativeMinDistance = -std::min(fabs(sdf2d(x, y)), fabs(mat(x, y) - i));
          sdfLayer(x, y) = negativeMinDistance;
        }
        else{
          sdfLayer(x, y) = std::min(sdf2d(x, y), sdfElevationAbove(x, y));
        }
        // std::cout << resolution_ << std::endl;
        // std::cout << x << ", " << y << ", " << i << std::endl;
        // if (i == 1.3)
        //   std::cout << sdfLayer(x, y) << ", " << mat(x, y) << ", " << sdf2d(x, y) << ", " << sdfElevationAbove(x, y) << std::endl;
        // std::cout << std::min_element({sdf2d(x, y), sdfElevationAbove(x, y), sdfElevationBelow(x, y)}) << std::endl;
        // sdfLayer(x, y) = std::min({sdf2d(x, y), sdfElevationAbove(x, y), sdfElevationBelow(x, y)});
      }
    }
    sdf.push_back(sdfLayer);
  }
  // // Calculate signed distance field in downwards direction.
  // MatrixXf sdfLayerPrev = sdf.back();
  // for(float j = maxHeight; j > minHeight; j-=resolution_){
  //   MatrixXf sdfLayerj = sdf.back();
  //   sdf.pop_back();
  //   for (int y = 0; y < sdfElevationBelow.cols(); y++) {
  //     for (int x = 0; x < sdfElevationBelow.rows(); x++) {
  //       if(sdfElevationBelow(x, y) == maxDistance_ && mat(x, y) > j)
  //         sdfElevationBelow(x, y) = j - mat(x, y);
  //       else if(sdfElevationBelow(x, y) != maxDistance_)
  //         sdfElevationBelow(x, y) = sdfLayerj(x, y) - resolution_;
  //       if(sdfLayerj(x, y) < 0)
  //         sdfLayerj(x, y) = -std::min(fabs(sdfLayerj(x, y)), fabs(sdfElevationBelow(x, y)));
  //     }
  //   }
  //   sdfResult.push_back(sdfLayerj);
  //   sdfLayerPrev = sdfLayerj;
  // }
  // std::reverse(sdfResult.begin(), sdfResult.end());
  // data_ = sdfResult;
  data_ = sdf;
}

Eigen::MatrixXf SignedDistanceField::get2dSDF(Eigen::Matrix<bool, Eigen::Dynamic, Eigen::Dynamic>& mat){

  image<uchar> *input = new image<uchar>(mat.cols(), mat.rows(), true);

  for (int y = 0; y < input->height(); y++) {
    for (int x = 0; x < input->width(); x++) {
      imRef(input, x, y) = mat(x, y);
    }
  }
  // compute dt
  image<float> *out = dt(input);

  Eigen::MatrixXf result(mat.rows(), mat.cols());

  // take square roots
  for (int y = 0; y < out->height(); y++) {
    for (int x = 0; x < out->width(); x++) {
      result(x, y) = sqrt(imRef(out, x, y));
    }
  }
  return result;
}

double SignedDistanceField::getDistanceAt(Eigen::Vector3f position){
  double xCenter = size_.x() / 2.0;
  double yCenter = size_.y() / 2.0;
  int i = std::round(xCenter - (position.x() - mapPosition_.x()) / resolution_);
  int j = std::round(yCenter - (position.y() - mapPosition_.y()) / resolution_);
  int k = std::round((position.z() - zIndexStartHeight_) / resolution_);
  // std::cout << "xCenter, yCenter = " << xCenter << ", " << yCenter << std::endl;
  // std::cout << "position.x, mapPosition x= " << position.x() << ", " << mapPosition_.x() << std::endl;
  // std::cout << "position.y, mapPosition y = " << position.y() << ", " << mapPosition_.y() << std::endl;
  // std::cout << "i, j, k = " << i << ", " << j << ", " << k << std::endl;
  i = std::max(i, 0);
  i = std::min(i, size_.x() - 1);
  j = std::max(j, 0);
  j = std::min(j, size_.y() - 1);
  k = std::max(k, 0);
  k = std::min(k, (int)data_.size() - 1);
  // Eigen::Vector3f gradient = getDistanceGradientAt(position);
  // std::cout << "gradient = " << gradient << std::endl;
  // double xp = mapPosition_.x() + ((size_.x() - i) - xCenter) * resolution_;
  // double yp = mapPosition_.y() + ((size_.y() - j) - yCenter) * resolution_;
  // double zp = zIndexStartHeight_ + k * resolution_;
  // Eigen::Vector3f error = position - Eigen::Vector3f(xp, yp, zp);
  // std::cout << "error = " << position << " - " << "(" << xp << ", " << yp << ", " << zp << " = " << error << std::endl;
  // std::cout << "error d " << gradient.dot(error) << std::endl;
  return data_[k](i, j);
}

double SignedDistanceField::getInterpolatedDistanceAt(Eigen::Vector3f position){
  double xCenter = size_.x() / 2.0;
  double yCenter = size_.y() / 2.0;
  int i = std::round(xCenter - (position.x() - mapPosition_.x()) / resolution_);
  int j = std::round(yCenter - (position.y() - mapPosition_.y()) / resolution_);
  int k = std::round((position.z() - zIndexStartHeight_) / resolution_);
  // std::cout << "xCenter, yCenter = " << xCenter << ", " << yCenter << std::endl;
  // std::cout << "position.x, mapPosition x= " << position.x() << ", " << mapPosition_.x() << std::endl;
  // std::cout << "position.y, mapPosition y = " << position.y() << ", " << mapPosition_.y() << std::endl;
  // std::cout << "i, j, k = " << i << ", " << j << ", " << k << std::endl;
  i = std::max(i, 0);
  i = std::min(i, size_.x() - 1);
  j = std::max(j, 0);
  j = std::min(j, size_.y() - 1);
  k = std::max(k, 0);
  k = std::min(k, (int)data_.size() - 1);
  Eigen::Vector3f gradient = getDistanceGradientAt(position);
  // std::cout << "gradient = " << gradient << std::endl;
  double xp = mapPosition_.x() + ((size_.x() - i) - xCenter) * resolution_;
  double yp = mapPosition_.y() + ((size_.y() - j) - yCenter) * resolution_;
  double zp = zIndexStartHeight_ + k * resolution_;
  Eigen::Vector3f error = position - Eigen::Vector3f(xp, yp, zp);
  // std::cout << "error = " << position << " - " << "(" << xp << ", " << yp << ", " << zp << " = " << error << std::endl;
  // std::cout << "error d " << gradient.dot(error) << std::endl;
  return data_[k](i, j) + gradient.dot(error);
}


Eigen::Vector3f SignedDistanceField::getDistanceGradientAt(Eigen::Vector3f position){
  double dx = (getDistanceAt(position + Vector3f(resolution_, 0, 0)) - getDistanceAt(position - Vector3f(resolution_, 0, 0))) / (2 * resolution_);
  double dy = (getDistanceAt(position + Vector3f(0, resolution_, 0)) - getDistanceAt(position - Vector3f(0, resolution_, 0))) / (2 * resolution_);
  double dz = (getDistanceAt(position + Vector3f(0, 0, resolution_)) - getDistanceAt(position - Vector3f(0, 0, resolution_))) / (2 * resolution_);
  return Vector3f(dx, dy, dz);
}

void SignedDistanceField::convertToPointCloud(pcl::PointCloud<pcl::PointXYZI>& points){
  double xCenter = size_.x() / 2.0;
  double yCenter = size_.y() / 2.0;
  for (int z = 0; z < data_.size(); z++){
    for (int y = 0; y < size_.y(); y++) {
      for (int x = 0; x < size_.x(); x++) {
        double xp = mapPosition_.x() + ((size_.x() - x) - xCenter) * resolution_;
        double yp = mapPosition_.y() + ((size_.y() - y) - yCenter) * resolution_;
        double zp = zIndexStartHeight_ + z * resolution_;
        pcl::PointXYZI p;
        p.x = xp;
        p.y = yp;
        p.z = zp;
        p.intensity = data_[z](x, y);
        points.push_back(p);
      }
    }
  }
  return;
}


} /* namespace */
