/* ----------------------------------------------------------------------------

 * GTSAM Copyright 2010, Georgia Tech Research Corporation,
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * Authors: Frank Dellaert, et al. (see THANKS for the full author list)

 * See LICENSE for the license information

 * -------------------------------------------------------------------------- */

/**
 * @file   SmartStereoProjectionFactorPP.h
 * @brief  Smart stereo factor on poses and extrinsic calibration
 * @author Luca Carlone
 * @author Frank Dellaert
 */

#include <gtsam_unstable/slam/SmartStereoProjectionFactorPP.h>

namespace gtsam {

SmartStereoProjectionFactorPP::SmartStereoProjectionFactorPP(
    const SharedNoiseModel& sharedNoiseModel,
    const SmartStereoProjectionParams& params)
    : Base(sharedNoiseModel, params) {} // note: no extrinsic specified!

void SmartStereoProjectionFactorPP::add(
    const StereoPoint2& measured,
    const Key& w_P_body_key, const Key& body_P_cam_key,
    const boost::shared_ptr<Cal3_S2Stereo>& K) {
  // we index by cameras..
  Base::add(measured, w_P_body_key);
  // but we also store the extrinsic calibration keys in the same order
  body_P_cam_keys_.push_back(body_P_cam_key);
  K_all_.push_back(K);
}

void SmartStereoProjectionFactorPP::add(
    const std::vector<StereoPoint2>& measurements,
    const KeyVector& w_P_body_keys, const KeyVector& body_P_cam_keys,
    const std::vector<boost::shared_ptr<Cal3_S2Stereo>>& Ks) {
  assert(measurements.size() == poseKeys.size());
  assert(w_P_body_keys.size() == body_P_cam_keys.size());
  assert(w_P_body_keys.size() == Ks.size());
  // we index by cameras..
  Base::add(measurements, w_P_body_keys);
  // but we also store the extrinsic calibration keys in the same order
  body_P_cam_keys_.insert(body_P_cam_keys_.end(), body_P_cam_keys.begin(), body_P_cam_keys.end());
  K_all_.insert(K_all_.end(), Ks.begin(), Ks.end());
}

void SmartStereoProjectionFactorPP::add(
    const std::vector<StereoPoint2>& measurements,
    const KeyVector& w_P_body_keys, const KeyVector& body_P_cam_keys,
    const boost::shared_ptr<Cal3_S2Stereo>& K) {
  assert(poseKeys.size() == measurements.size());
  assert(w_P_body_keys.size() == body_P_cam_keys.size());
  for (size_t i = 0; i < measurements.size(); i++) {
    Base::add(measurements[i], w_P_body_keys[i]);
    body_P_cam_keys_.push_back(body_P_cam_keys[i]);
    K_all_.push_back(K);
  }
}
void SmartStereoProjectionFactorPP::print(
    const std::string& s, const KeyFormatter& keyFormatter) const {
  std::cout << s << "SmartStereoProjectionFactorPP, z = \n ";
  for (size_t i = 0; i < K_all_.size(); i++) {
    K_all_[i]->print("calibration = ");
    std::cout << " extrinsic pose key: " << keyFormatter(body_P_cam_keys_[i]);
  }
  Base::print("", keyFormatter);
}

bool SmartStereoProjectionFactorPP::equals(const NonlinearFactor& p,
                                             double tol) const {
  const SmartStereoProjectionFactorPP* e =
      dynamic_cast<const SmartStereoProjectionFactorPP*>(&p);

  return e && Base::equals(p, tol) &&
      body_P_cam_keys_ == e->getExtrinsicPoseKeys();
}

double SmartStereoProjectionFactorPP::error(const Values& values) const {
  if (this->active(values)) {
    return this->totalReprojectionError(cameras(values));
  } else {  // else of active flag
    return 0.0;
  }
}

SmartStereoProjectionFactorPP::Base::Cameras
SmartStereoProjectionFactorPP::cameras(const Values& values) const {
  assert(keys_.size() == K_all_.size());
  assert(keys_.size() == body_P_cam_keys_.size());
  Base::Cameras cameras;
  for (size_t i = 0; i < keys_.size(); i++) {
    Pose3 w_P_body = values.at<Pose3>(keys_[i]);
    Pose3 body_P_cam = values.at<Pose3>(body_P_cam_keys_[i]);
    Pose3 w_P_cam = w_P_body.compose(body_P_cam);
    cameras.push_back(StereoCamera(w_P_cam, K_all_[i]));
  }
  return cameras;
}

}  // \ namespace gtsam
