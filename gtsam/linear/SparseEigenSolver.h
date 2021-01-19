/* ----------------------------------------------------------------------------

 * GTSAM Copyright 2010, Georgia Tech Research Corporation,
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * Authors: Frank Dellaert, et al. (see THANKS for the full author list)

 * See LICENSE for the license information

 * -------------------------------------------------------------------------- */

/**
 * @file SparseEigenSolver.h
 *
 * @brief Eigen SparseSolver based linear solver backend for GTSAM
 *
 * @date Aug 2019
 * @author Mandy Xie
 * @author Fan Jiang
 * @author Frank Dellaert
 */

#pragma once

#include <gtsam/linear/GaussianFactorGraph.h>
#include <gtsam/linear/VectorValues.h>
#include <gtsam/linear/LinearSolver.h>
#include <Eigen/Sparse>
#include <string>

namespace gtsam {

  /**
   * Eigen SparseSolver based Backend class
   */
  class GTSAM_EXPORT SparseEigenSolver : public LinearSolver {
  public:

    typedef enum {
      QR,
      CHOLESKY
    } SparseEigenSolverType;


    explicit SparseEigenSolver(SparseEigenSolver::SparseEigenSolverType type, const Ordering &ordering);

    bool isIterative() override;

    bool isSequential() override;

    VectorValues solve(const GaussianFactorGraph &gfg) override;

    static Eigen::SparseMatrix<double>
    sparseJacobianEigen(const GaussianFactorGraph &gfg, const Ordering &ordering);

  protected:

    SparseEigenSolverType solverType = QR;

    Ordering ordering;
  };
}  // namespace gtsam
