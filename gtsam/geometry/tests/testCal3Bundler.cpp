/* ----------------------------------------------------------------------------

 * GTSAM Copyright 2010, Georgia Tech Research Corporation,
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * Authors: Frank Dellaert, et al. (see THANKS for the full author list)

 * See LICENSE for the license information

 * -------------------------------------------------------------------------- */

/**
 * @file  testCal3Bundler.cpp
 * @brief Unit tests for Bundler calibration model.
 */

#include <CppUnitLite/TestHarness.h>
#include <gtsam/base/Testable.h>
#include <gtsam/base/TestableAssertions.h>
#include <gtsam/base/numericalDerivative.h>
#include <gtsam/geometry/Cal3Bundler.h>

using namespace gtsam;

GTSAM_CONCEPT_TESTABLE_INST(Cal3Bundler)
GTSAM_CONCEPT_MANIFOLD_INST(Cal3Bundler)

static Cal3Bundler K(500, 1e-3, 1e-3, 1000, 2000);
static Point2 p(2, 3);

/* ************************************************************************* */
TEST(Cal3Bundler, Vector) {
  Cal3Bundler K;
  Vector expected(3);
  expected << 1, 0, 0;
  CHECK(assert_equal(expected, K.vector()));
}

/* ************************************************************************* */
TEST(Cal3Bundler, Uncalibrate) {
  Vector v = K.vector();
  double r = p.x() * p.x() + p.y() * p.y();
  double distortion = 1.0 + v[1] * r + v[2] * r * r;
  double u = K.px() + v[0] * distortion * p.x();
  double v_coord = K.py() + v[0] * distortion * p.y();
  Point2 expected(u, v_coord);
  Point2 actual = K.uncalibrate(p);
  CHECK(assert_equal(expected, actual));
}

/* ************************************************************************* */
TEST(Cal3Bundler, Calibrate) {
  Point2 pn(0.5, 0.5);
  Point2 pi = K.uncalibrate(pn);
  Point2 pn_hat = K.calibrate(pi);
  CHECK(traits<Point2>::Equals(pn, pn_hat, 1e-5));
}

/* ************************************************************************* */
Point2 uncalibrate_(const Cal3Bundler& k, const Point2& pt) {
  return k.uncalibrate(pt);
}

Point2 calibrate_(const Cal3Bundler& k, const Point2& pt) {
  return k.calibrate(pt);
}

/* ************************************************************************* */
TEST(Cal3Bundler, DUncalibrateDefault) {
  Cal3Bundler trueK(1, 0, 0);
  Matrix Dcal, Dp;
  Point2 actual = trueK.uncalibrate(p, Dcal, Dp);
  Point2 expected(p);  // Since K is identity, uncalibrate should return p
  CHECK(assert_equal(expected, actual, 1e-7));
  Matrix numerical1 = numericalDerivative21(uncalibrate_, trueK, p);
  Matrix numerical2 = numericalDerivative22(uncalibrate_, trueK, p);
  CHECK(assert_equal(numerical1, Dcal, 1e-7));
  CHECK(assert_equal(numerical2, Dp, 1e-7));
}

/* ************************************************************************* */
TEST(Cal3Bundler, DCalibrateDefault) {
  Cal3Bundler trueK(1, 0, 0);
  Matrix Dcal, Dp;
  Point2 pn(0.5, 0.5);
  Point2 pi = trueK.uncalibrate(pn);
  Point2 actual = trueK.calibrate(pi, Dcal, Dp);
  CHECK(assert_equal(pn, actual, 1e-7));
  Matrix numerical1 = numericalDerivative21(calibrate_, trueK, pi);
  Matrix numerical2 = numericalDerivative22(calibrate_, trueK, pi);
  CHECK(assert_equal(numerical1, Dcal, 1e-5));
  CHECK(assert_equal(numerical2, Dp, 1e-5));
}

/* ************************************************************************* */
TEST(Cal3Bundler, DUncalibratePrincipalPoint) {
  Cal3Bundler K(5, 0, 0, 2, 2);
  Matrix Dcal, Dp;
  Point2 actual = K.uncalibrate(p, Dcal, Dp);
  Point2 expected(2.0 + 5.0 * p.x(), 2.0 + 5.0 * p.y());
  CHECK(assert_equal(expected, actual, 1e-7));
  Matrix numerical1 = numericalDerivative21(uncalibrate_, K, p);
  Matrix numerical2 = numericalDerivative22(uncalibrate_, K, p);
  CHECK(assert_equal(numerical1, Dcal, 1e-7));
  CHECK(assert_equal(numerical2, Dp, 1e-7));
}

/* ************************************************************************* */
TEST(Cal3Bundler, DCalibratePrincipalPoint) {
  Cal3Bundler K(2, 0, 0, 2, 2);
  Matrix Dcal, Dp;
  Point2 pn(0.5, 0.5);
  Point2 pi = K.uncalibrate(pn);
  Point2 actual = K.calibrate(pi, Dcal, Dp);
  CHECK(assert_equal(pn, actual, 1e-7));
  Matrix numerical1 = numericalDerivative21(calibrate_, K, pi);
  Matrix numerical2 = numericalDerivative22(calibrate_, K, pi);
  CHECK(assert_equal(numerical1, Dcal, 1e-5));
  CHECK(assert_equal(numerical2, Dp, 1e-5));
}

/* ************************************************************************* */
TEST(Cal3Bundler, DUncalibrate) {
  Matrix Dcal, Dp;
  Point2 actual = K.uncalibrate(p, Dcal, Dp);
  // Compute expected value manually
  Vector v = K.vector();
  double r2 = p.x() * p.x() + p.y() * p.y();
  double distortion = 1.0 + v[1] * r2 + v[2] * r2 * r2;
  Point2 expected(
      K.px() + v[0] * distortion * p.x(),
      K.py() + v[0] * distortion * p.y());
  CHECK(assert_equal(expected, actual, 1e-7));

  Matrix numerical1 = numericalDerivative21(uncalibrate_, K, p);
  Matrix numerical2 = numericalDerivative22(uncalibrate_, K, p);
  CHECK(assert_equal(numerical1, Dcal, 1e-7));
  CHECK(assert_equal(numerical2, Dp, 1e-7));
}

/* ************************************************************************* */
TEST(Cal3Bundler, DCalibrate) {
  Matrix Dcal, Dp;
  Point2 pn(0.5, 0.5);
  Point2 pi = K.uncalibrate(pn);
  Point2 actual = K.calibrate(pi, Dcal, Dp);
  CHECK(assert_equal(pn, actual, 1e-7));
  Matrix numerical1 = numericalDerivative21(calibrate_, K, pi);
  Matrix numerical2 = numericalDerivative22(calibrate_, K, pi);
  CHECK(assert_equal(numerical1, Dcal, 1e-5));
  CHECK(assert_equal(numerical2, Dp, 1e-5));
}

/* ************************************************************************* */
TEST(Cal3Bundler, assert_equal) { CHECK(assert_equal(K, K, 1e-7)); }

/* ************************************************************************* */
TEST(Cal3Bundler, Retract) {
  Cal3Bundler expected(510, 2e-3, 2e-3, 1000, 2000);
  EXPECT_LONGS_EQUAL(3, expected.dim());

  EXPECT_LONGS_EQUAL(Cal3Bundler::Dim(), 3);
  EXPECT_LONGS_EQUAL(expected.dim(), 3);

  Vector3 d;
  d << 10, 1e-3, 1e-3;
  Cal3Bundler actual = K.retract(d);
  CHECK(assert_equal(expected, actual, 1e-7));
  CHECK(assert_equal(d, K.localCoordinates(actual), 1e-7));
}

/* ************************************************************************* */
TEST(Cal3Bundler, Print) {
  Cal3Bundler cal(1, 2, 3, 4, 5);
  std::stringstream os;
  os << "f: " << cal.fx() << ", k1: " << cal.k1() << ", k2: " << cal.k2()
     << ", px: " << cal.px() << ", py: " << cal.py();

  EXPECT(assert_stdout_equal(os.str(), cal));
}

/* ************************************************************************* */
int main() {
  TestResult tr;
  return TestRegistry::runAllTests(tr);
}
/* ************************************************************************* */
