/* ----------------------------------------------------------------------------

 * GTSAM Copyright 2010, Georgia Tech Research Corporation,
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * Authors: Frank Dellaert, et al. (see THANKS for the full author list)

 * See LICENSE for the license information

 * -------------------------------------------------------------------------- */

/**
 * @file    testHybridEstimation.cpp
 * @brief   Unit tests for end-to-end Hybrid Estimation
 * @author  Varun Agrawal
 */

#include <gtsam/geometry/Pose2.h>
#include <gtsam/hybrid/HybridBayesNet.h>
#include <gtsam/hybrid/HybridNonlinearFactorGraph.h>
#include <gtsam/hybrid/HybridNonlinearISAM.h>
#include <gtsam/hybrid/HybridSmoother.h>
#include <gtsam/hybrid/MixtureFactor.h>
#include <gtsam/inference/Symbol.h>
#include <gtsam/linear/GaussianBayesNet.h>
#include <gtsam/linear/GaussianFactorGraph.h>
#include <gtsam/linear/JacobianFactor.h>
#include <gtsam/linear/NoiseModel.h>
#include <gtsam/nonlinear/NonlinearFactorGraph.h>
#include <gtsam/nonlinear/PriorFactor.h>
#include <gtsam/slam/BetweenFactor.h>

// Include for test suite
#include <CppUnitLite/TestHarness.h>

#include "Switching.h"

using namespace std;
using namespace gtsam;

using symbol_shorthand::X;

Ordering getOrdering(HybridGaussianFactorGraph& factors,
                     const HybridGaussianFactorGraph& newFactors) {
  factors += newFactors;
  // Get all the discrete keys from the factors
  KeySet allDiscrete = factors.discreteKeys();

  // Create KeyVector with continuous keys followed by discrete keys.
  KeyVector newKeysDiscreteLast;
  const KeySet newFactorKeys = newFactors.keys();
  // Insert continuous keys first.
  for (auto& k : newFactorKeys) {
    if (!allDiscrete.exists(k)) {
      newKeysDiscreteLast.push_back(k);
    }
  }

  // Insert discrete keys at the end
  std::copy(allDiscrete.begin(), allDiscrete.end(),
            std::back_inserter(newKeysDiscreteLast));

  const VariableIndex index(factors);

  // Get an ordering where the new keys are eliminated last
  Ordering ordering = Ordering::ColamdConstrainedLast(
      index, KeyVector(newKeysDiscreteLast.begin(), newKeysDiscreteLast.end()),
      true);
  return ordering;
}

/****************************************************************************/
// Test approximate inference with an additional pruning step.
TEST(HybridEstimation, Incremental) {
  size_t K = 15;
  std::vector<double> measurements = {0, 1, 2, 2, 2, 2,  3,  4,  5,  6, 6,
                                      7, 8, 9, 9, 9, 10, 11, 11, 11, 11};
  // Ground truth discrete seq
  std::vector<size_t> discrete_seq = {1, 1, 0, 0, 0, 1, 1, 1, 1, 0,
                                      1, 1, 1, 0, 0, 1, 1, 0, 0, 0};
  Switching switching(K, 1.0, 0.1, measurements, "1/1 1/1");
  HybridSmoother smoother;
  HybridNonlinearFactorGraph graph;
  Values initial;

  // Add the X(0) prior
  graph.push_back(switching.nonlinearFactorGraph.at(0));
  initial.insert(X(0), switching.linearizationPoint.at<double>(X(0)));

  HybridGaussianFactorGraph linearized;
  HybridGaussianFactorGraph bayesNet;

  for (size_t k = 1; k < K; k++) {
    // Motion Model
    graph.push_back(switching.nonlinearFactorGraph.at(k));
    // Measurement
    graph.push_back(switching.nonlinearFactorGraph.at(k + K - 1));

    initial.insert(X(k), switching.linearizationPoint.at<double>(X(k)));

    bayesNet = smoother.hybridBayesNet();
    linearized = *graph.linearize(initial);
    Ordering ordering = getOrdering(bayesNet, linearized);

    smoother.update(linearized, ordering, 3);
    graph.resize(0);
  }

  HybridValues delta = smoother.hybridBayesNet().optimize();

  Values result = initial.retract(delta.continuous());

  DiscreteValues expected_discrete;
  for (size_t k = 0; k < K - 1; k++) {
    expected_discrete[M(k)] = discrete_seq[k];
  }
  EXPECT(assert_equal(expected_discrete, delta.discrete()));

  Values expected_continuous;
  for (size_t k = 0; k < K; k++) {
    expected_continuous.insert(X(k), measurements[k]);
  }
  EXPECT(assert_equal(expected_continuous, result));
}

/**
 * @brief A function to get a specific 1D robot motion problem as a linearized
 * factor graph. This is the problem P(X|Z, M), i.e. estimating the continuous
 * positions given the measurements and discrete sequence.
 *
 * @param K The number of timesteps.
 * @param measurements The vector of measurements for each timestep.
 * @param discrete_seq The discrete sequence governing the motion of the robot.
 * @param measurement_sigma Noise model sigma for measurements.
 * @param between_sigma Noise model sigma for the between factor.
 * @return GaussianFactorGraph::shared_ptr
 */
GaussianFactorGraph::shared_ptr specificProblem(
    size_t K, const std::vector<double>& measurements,
    const std::vector<size_t>& discrete_seq, double measurement_sigma = 0.1,
    double between_sigma = 1.0) {
  NonlinearFactorGraph graph;
  Values linearizationPoint;

  // Add measurement factors
  auto measurement_noise = noiseModel::Isotropic::Sigma(1, measurement_sigma);
  for (size_t k = 0; k < K; k++) {
    graph.emplace_shared<PriorFactor<double>>(X(k), measurements.at(k),
                                              measurement_noise);
    linearizationPoint.insert<double>(X(k), static_cast<double>(k + 1));
  }

  using MotionModel = BetweenFactor<double>;

  // Add "motion models".
  auto motion_noise_model = noiseModel::Isotropic::Sigma(1, between_sigma);
  for (size_t k = 0; k < K - 1; k++) {
    auto motion_model = boost::make_shared<MotionModel>(
        X(k), X(k + 1), discrete_seq.at(k), motion_noise_model);
    graph.push_back(motion_model);
  }
  GaussianFactorGraph::shared_ptr linear_graph =
      graph.linearize(linearizationPoint);
  return linear_graph;
}

/**
 * @brief Get the discrete sequence from the integer `x`.
 *
 * @tparam K Template parameter so we can set the correct bitset size.
 * @param x The integer to convert to a discrete binary sequence.
 * @return std::vector<size_t>
 */
template <size_t K>
std::vector<size_t> getDiscreteSequence(size_t x) {
  std::bitset<K - 1> seq = x;
  std::vector<size_t> discrete_seq(K - 1);
  for (size_t i = 0; i < K - 1; i++) {
    // Save to discrete vector in reverse order
    discrete_seq[K - 2 - i] = seq[i];
  }
  return discrete_seq;
}

/**
 * @brief Helper method to get the probPrimeTree
 * as per the new elimination scheme.
 *
 * @param graph The HybridGaussianFactorGraph to eliminate.
 * @return AlgebraicDecisionTree<Key>
 */
AlgebraicDecisionTree<Key> probPrimeTree(
    const HybridGaussianFactorGraph& graph) {
  HybridBayesNet::shared_ptr bayesNet;
  HybridGaussianFactorGraph::shared_ptr remainingGraph;
  Ordering continuous(graph.continuousKeys());
  std::tie(bayesNet, remainingGraph) =
      graph.eliminatePartialSequential(continuous);

  auto last_conditional = bayesNet->at(bayesNet->size() - 1);
  DiscreteKeys discrete_keys = last_conditional->discreteKeys();

  const std::vector<DiscreteValues> assignments =
      DiscreteValues::CartesianProduct(discrete_keys);

  std::reverse(discrete_keys.begin(), discrete_keys.end());

  vector<VectorValues::shared_ptr> vector_values;
  for (const DiscreteValues& assignment : assignments) {
    VectorValues values = bayesNet->optimize(assignment);
    vector_values.push_back(boost::make_shared<VectorValues>(values));
  }
  DecisionTree<Key, VectorValues::shared_ptr> delta_tree(discrete_keys,
                                                         vector_values);

  std::vector<double> probPrimes;
  for (const DiscreteValues& assignment : assignments) {
    double error = 0.0;
    VectorValues delta = *delta_tree(assignment);
    for (auto factor : graph) {
      if (factor->isHybrid()) {
        auto f = boost::static_pointer_cast<GaussianMixtureFactor>(factor);
        error += f->error(delta, assignment);

      } else if (factor->isContinuous()) {
        auto f = boost::static_pointer_cast<HybridGaussianFactor>(factor);
        error += f->inner()->error(delta);
      }
    }
    probPrimes.push_back(exp(-error));
  }
  AlgebraicDecisionTree<Key> probPrimeTree(discrete_keys, probPrimes);
  return probPrimeTree;
}

/****************************************************************************/
/**
 * Test for correctness of different branches of the P'(Continuous | Discrete).
 * The values should match those of P'(Continuous) for each discrete mode.
 */
TEST(HybridEstimation, Probability) {
  constexpr size_t K = 4;
  std::vector<double> measurements = {0, 1, 2, 2};

  // This is the correct sequence
  // std::vector<size_t> discrete_seq = {1, 1, 0};

  double between_sigma = 1.0, measurement_sigma = 0.1;

  std::vector<double> expected_errors, expected_prob_primes;
  for (size_t i = 0; i < pow(2, K - 1); i++) {
    std::vector<size_t> discrete_seq = getDiscreteSequence<K>(i);

    GaussianFactorGraph::shared_ptr linear_graph = specificProblem(
        K, measurements, discrete_seq, measurement_sigma, between_sigma);

    auto bayes_net = linear_graph->eliminateSequential();

    VectorValues values = bayes_net->optimize();

    expected_errors.push_back(linear_graph->error(values));
    expected_prob_primes.push_back(linear_graph->probPrime(values));
  }

  Switching switching(K, between_sigma, measurement_sigma, measurements);
  auto graph = switching.linearizedFactorGraph;
  Ordering ordering = getOrdering(graph, HybridGaussianFactorGraph());

  AlgebraicDecisionTree<Key> expected_probPrimeTree = probPrimeTree(graph);

  // Eliminate continuous
  Ordering continuous_ordering(graph.continuousKeys());
  HybridBayesNet::shared_ptr bayesNet;
  HybridGaussianFactorGraph::shared_ptr discreteGraph;
  std::tie(bayesNet, discreteGraph) =
      graph.eliminatePartialSequential(continuous_ordering);

  // Get the last continuous conditional which will have all the discrete keys
  auto last_conditional = bayesNet->at(bayesNet->size() - 1);
  DiscreteKeys discrete_keys = last_conditional->discreteKeys();

  const std::vector<DiscreteValues> assignments =
      DiscreteValues::CartesianProduct(discrete_keys);

  // Reverse discrete keys order for correct tree construction
  std::reverse(discrete_keys.begin(), discrete_keys.end());

  // Create a decision tree of all the different VectorValues
  DecisionTree<Key, VectorValues::shared_ptr> delta_tree =
      graph.continuousDelta(discrete_keys, bayesNet, assignments);

  AlgebraicDecisionTree<Key> probPrimeTree =
      graph.continuousProbPrimes(discrete_keys, bayesNet, assignments);

  EXPECT(assert_equal(expected_probPrimeTree, probPrimeTree));

  // Test if the probPrimeTree matches the probability of
  // the individual factor graphs
  for (size_t i = 0; i < pow(2, K - 1); i++) {
    std::vector<size_t> discrete_seq = getDiscreteSequence<K>(i);
    Assignment<Key> discrete_assignment;
    for (size_t v = 0; v < discrete_seq.size(); v++) {
      discrete_assignment[M(v)] = discrete_seq[v];
    }
    EXPECT_DOUBLES_EQUAL(expected_prob_primes.at(i),
                         probPrimeTree(discrete_assignment), 1e-8);
  }

  // remainingGraph->add(DecisionTreeFactor(discrete_keys, probPrimeTree));

  // Ordering discrete(graph.discreteKeys());
  // // remainingGraph->print("remainingGraph");
  // // discrete.print();
  // auto discreteBayesNet = remainingGraph->eliminateSequential(discrete);
  // bayesNet->add(*discreteBayesNet);
  // // bayesNet->print();

  // HybridValues hybrid_values = bayesNet->optimize();
  // hybrid_values.discrete().print();
}

/* ************************************************************************* */
int main() {
  TestResult tr;
  return TestRegistry::runAllTests(tr);
}
/* ************************************************************************* */
