/* ----------------------------------------------------------------------------

 * GTSAM Copyright 2010, Georgia Tech Research Corporation, 
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * Authors: Frank Dellaert, et al. (see THANKS for the full author list)

 * See LICENSE for the license information

 * -------------------------------------------------------------------------- */

/**
 * @file    numericalDerivative.h
 * @brief   Some functions to compute numerical derivatives
 * @author  Frank Dellaert
 */

// \callgraph
#pragma once

#include <boost/function.hpp>
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif
#include <boost/bind.hpp>
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#include <gtsam/base/Matrix.h>
#include <gtsam/base/Manifold.h>
#include <gtsam/linear/VectorValues.h>
#include <gtsam/linear/JacobianFactor.h>
#include <gtsam/nonlinear/Values.h>

#include <CppUnitLite/TestResult.h>
#include <CppUnitLite/Test.h>
#include <CppUnitLite/Failure.h>

namespace gtsam {

/*
 * Note that all of these functions have two versions, a boost.function version and a
 * standard C++ function pointer version.  This allows reformulating the arguments of
 * a function to fit the correct structure, which is useful for situations like
 * member functions and functions with arguments not involved in the derivative:
 *
 * Usage of the boost bind version to rearrange arguments:
 *   for a function with one relevant param and an optional derivative:
 *     Foo bar(const Obj& a, boost::optional<Matrix&> H1)
 *   Use boost.bind to restructure:
 *     boost::bind(bar, _1, boost::none)
 *   This syntax will fix the optional argument to boost::none, while using the first argument provided
 *
 * For member functions, such as below, with an instantiated copy instanceOfSomeClass
 *     Foo SomeClass::bar(const Obj& a)
 * Use boost bind as follows to create a function pointer that uses the member function:
 *       boost::bind(&SomeClass::bar, ref(instanceOfSomeClass), _1)
 *
 * For additional details, see the documentation:
 *     http://www.boost.org/doc/libs/release/libs/bind/bind.html
 */

/**
 * Numerically compute gradient of scalar function
 * Class X is the input argument
 * The class X needs to have dim, expmap, logmap
 */
template<class X>
Vector numericalGradient(boost::function<double(const X&)> h, const X& x,
    double delta = 1e-5) {
  double factor = 1.0 / (2.0 * delta);

  BOOST_STATIC_ASSERT_MSG(traits::is_manifold<X>::value,
      "Template argument X must be a manifold type.");
  static const int N = traits::dimension<X>::value;
  BOOST_STATIC_ASSERT_MSG(N>0, "Template argument X must be fixed-size type.");
  typedef DefaultChart<X> ChartX;
  typedef typename ChartX::vector TangentX;

  // get chart at x
  ChartX chartX;

  // Prepare a tangent vector to perturb x with, only works for fixed size
  TangentX d;
  d.setZero();

  Vector g = zero(N); // Can be fixed size
  for (int j = 0; j < N; j++) {
    d(j) = delta;
    double hxplus = h(chartX.retract(x, d));
    d(j) = -delta;
    double hxmin = h(chartX.retract(x, d));
    d(j) = 0;
    g(j) = (hxplus - hxmin) * factor;
  }
  return g;
}

/**
 * @brief New-style numerical derivatives using manifold_traits
 * @brief Computes numerical derivative in argument 1 of unary function
 * @param h unary function yielding m-vector
 * @param x n-dimensional value at which to evaluate h
 * @param delta increment for numerical derivative
 * Class Y is the output argument
 * Class X is the input argument
 * @return m*n Jacobian computed via central differencing
 */
template<class Y, class X>
// TODO Should compute fixed-size matrix
Matrix numericalDerivative11(boost::function<Y(const X&)> h, const X& x,
    double delta = 1e-5) {
  using namespace traits;

  BOOST_STATIC_ASSERT_MSG(traits::is_manifold<Y>::value,
      "Template argument Y must be a manifold type.");
  typedef DefaultChart<Y> ChartY;
  typedef typename ChartY::vector TangentY;

  BOOST_STATIC_ASSERT_MSG(traits::is_manifold<X>::value,
      "Template argument X must be a manifold type.");
  static const int N = traits::dimension<X>::value;
  BOOST_STATIC_ASSERT_MSG(N>0, "Template argument X must be fixed-size type.");
  typedef DefaultChart<X> ChartX;
  typedef typename ChartX::vector TangentX;

  // get value at x, and corresponding chart
  Y hx = h(x);
  ChartY chartY;

  // Bit of a hack for now to find number of rows
  TangentY zeroY = chartY.local(hx, hx);
  size_t m = zeroY.size();

  // get chart at x
  ChartX chartX;

  // Prepare a tangent vector to perturb x with, only works for fixed size
  TangentX dx;
  dx.setZero();

  // Fill in Jacobian H
  Matrix H = zeros(m, N);
  double factor = 1.0 / (2.0 * delta);
  for (int j = 0; j < N; j++) {
    dx(j) = delta;
    TangentY dy1 = chartY.local(hx, h(chartX.retract(x, dx)));
    dx(j) = -delta;
    TangentY dy2 = chartY.local(hx, h(chartX.retract(x, dx)));
    dx(j) = 0;
    H.col(j) << (dy1 - dy2) * factor;
  }
  return H;
}

/** use a raw C++ function pointer */
template<class Y, class X>
Matrix numericalDerivative11(Y (*h)(const X&), const X& x,
    double delta = 1e-5) {
  return numericalDerivative11<Y, X>(boost::bind(h, _1), x, delta);
}

/**
 * Compute numerical derivative in argument 1 of binary function
 * @param h binary function yielding m-vector
 * @param x1 n-dimensional first argument value
 * @param x2 second argument value
 * @param delta increment for numerical derivative
 * @return m*n Jacobian computed via central differencing
 */
template<class Y, class X1, class X2>
Matrix numericalDerivative21(const boost::function<Y(const X1&, const X2&)>& h,
    const X1& x1, const X2& x2, double delta = 1e-5) {
  BOOST_STATIC_ASSERT_MSG(traits::is_manifold<Y>::value,
      "Template argument Y must be a manifold type.");
  BOOST_STATIC_ASSERT_MSG(traits::is_manifold<X1>::value,
      "Template argument X1 must be a manifold type.");
  return numericalDerivative11<Y, X1>(boost::bind(h, _1, x2), x1, delta);
}

/** use a raw C++ function pointer */
template<class Y, class X1, class X2>
inline Matrix numericalDerivative21(Y (*h)(const X1&, const X2&), const X1& x1,
    const X2& x2, double delta = 1e-5) {
  return numericalDerivative21<Y, X1, X2>(boost::bind(h, _1, _2), x1, x2, delta);
}

/**
 * Compute numerical derivative in argument 2 of binary function
 * @param h binary function yielding m-vector
 * @param x1 first argument value
 * @param x2 n-dimensional second argument value
 * @param delta increment for numerical derivative
 * @return m*n Jacobian computed via central differencing
 */
template<class Y, class X1, class X2>
Matrix numericalDerivative22(boost::function<Y(const X1&, const X2&)> h,
    const X1& x1, const X2& x2, double delta = 1e-5) {
  BOOST_STATIC_ASSERT_MSG(traits::is_manifold<Y>::value,
      "Template argument Y must be a manifold type.");
  BOOST_STATIC_ASSERT_MSG(traits::is_manifold<X2>::value,
      "Template argument X2 must be a manifold type.");
  return numericalDerivative11<Y, X2>(boost::bind(h, x1, _1), x2, delta);
}

/** use a raw C++ function pointer */
template<class Y, class X1, class X2>
inline Matrix numericalDerivative22(Y (*h)(const X1&, const X2&), const X1& x1,
    const X2& x2, double delta = 1e-5) {
  return numericalDerivative22<Y, X1, X2>(boost::bind(h, _1, _2), x1, x2, delta);
}

/**
 * Compute numerical derivative in argument 1 of ternary function
 * @param h ternary function yielding m-vector
 * @param x1 n-dimensional first argument value
 * @param x2 second argument value
 * @param x3 third argument value
 * @param delta increment for numerical derivative
 * @return m*n Jacobian computed via central differencing
 * All classes Y,X1,X2,X3 need dim, expmap, logmap
 */
template<class Y, class X1, class X2, class X3>
Matrix numericalDerivative31(
    boost::function<Y(const X1&, const X2&, const X3&)> h, const X1& x1,
    const X2& x2, const X3& x3, double delta = 1e-5) {
  BOOST_STATIC_ASSERT_MSG(traits::is_manifold<Y>::value,
      "Template argument Y must be a manifold type.");
  BOOST_STATIC_ASSERT_MSG(traits::is_manifold<X1>::value,
      "Template argument X1 must be a manifold type.");
  return numericalDerivative11<Y, X1>(boost::bind(h, _1, x2, x3), x1, delta);
}

template<class Y, class X1, class X2, class X3>
inline Matrix numericalDerivative31(Y (*h)(const X1&, const X2&, const X3&),
    const X1& x1, const X2& x2, const X3& x3, double delta = 1e-5) {
  return numericalDerivative31<Y, X1, X2, X3>(boost::bind(h, _1, _2, _3), x1,
      x2, x3, delta);
}

/**
 * Compute numerical derivative in argument 2 of ternary function
 * @param h ternary function yielding m-vector
 * @param x1 n-dimensional first argument value
 * @param x2 second argument value
 * @param x3 third argument value
 * @param delta increment for numerical derivative
 * @return m*n Jacobian computed via central differencing
 * All classes Y,X1,X2,X3 need dim, expmap, logmap
 */
template<class Y, class X1, class X2, class X3>
Matrix numericalDerivative32(
    boost::function<Y(const X1&, const X2&, const X3&)> h, const X1& x1,
    const X2& x2, const X3& x3, double delta = 1e-5) {
  BOOST_STATIC_ASSERT_MSG(traits::is_manifold<Y>::value,
      "Template argument Y must be a manifold type.");
  BOOST_STATIC_ASSERT_MSG(traits::is_manifold<X2>::value,
      "Template argument X2 must be a manifold type.");
  return numericalDerivative11<Y, X2>(boost::bind(h, x1, _1, x3), x2, delta);
}

template<class Y, class X1, class X2, class X3>
inline Matrix numericalDerivative32(Y (*h)(const X1&, const X2&, const X3&),
    const X1& x1, const X2& x2, const X3& x3, double delta = 1e-5) {
  return numericalDerivative32<Y, X1, X2, X3>(boost::bind(h, _1, _2, _3), x1,
      x2, x3, delta);
}

/**
 * Compute numerical derivative in argument 3 of ternary function
 * @param h ternary function yielding m-vector
 * @param x1 n-dimensional first argument value
 * @param x2 second argument value
 * @param x3 third argument value
 * @param delta increment for numerical derivative
 * @return m*n Jacobian computed via central differencing
 * All classes Y,X1,X2,X3 need dim, expmap, logmap
 */
template<class Y, class X1, class X2, class X3>
Matrix numericalDerivative33(
    boost::function<Y(const X1&, const X2&, const X3&)> h, const X1& x1,
    const X2& x2, const X3& x3, double delta = 1e-5) {
  BOOST_STATIC_ASSERT_MSG(traits::is_manifold<Y>::value,
      "Template argument Y must be a manifold type.");
  BOOST_STATIC_ASSERT_MSG(traits::is_manifold<X3>::value,
      "Template argument X3 must be a manifold type.");
  return numericalDerivative11<Y, X3>(boost::bind(h, x1, x2, _1), x3, delta);
}

template<class Y, class X1, class X2, class X3>
inline Matrix numericalDerivative33(Y (*h)(const X1&, const X2&, const X3&),
    const X1& x1, const X2& x2, const X3& x3, double delta = 1e-5) {
  return numericalDerivative33<Y, X1, X2, X3>(boost::bind(h, _1, _2, _3), x1,
      x2, x3, delta);
}

/**
 * Compute numerical Hessian matrix.  Requires a single-argument Lie->scalar
 * function.  This is implemented simply as the derivative of the gradient.
 * @param f A function taking a Lie object as input and returning a scalar
 * @param x The center point for computing the Hessian
 * @param delta The numerical derivative step size
 * @return n*n Hessian matrix computed via central differencing
 */
template<class X>
inline Matrix numericalHessian(boost::function<double(const X&)> f, const X& x,
    double delta = 1e-5) {
  BOOST_STATIC_ASSERT_MSG(traits::is_manifold<X>::value,
      "Template argument X must be a manifold type.");
  typedef boost::function<double(const X&)> F;
  typedef boost::function<Vector(F, const X&, double)> G;
  G ng = static_cast<G>(numericalGradient<X> );
  return numericalDerivative11<Vector, X>(boost::bind(ng, f, _1, delta), x,
      delta);
}

template<class X>
inline Matrix numericalHessian(double (*f)(const X&), const X& x, double delta =
    1e-5) {
  return numericalHessian(boost::function<double(const X&)>(f), x, delta);
}

/** Helper class that computes the derivative of f w.r.t. x1, centered about
 * x1_, as a function of x2
 */
template<class X1, class X2>
class G_x1 {
  const boost::function<double(const X1&, const X2&)>& f_;
  X1 x1_;
  double delta_;
public:
  G_x1(const boost::function<double(const X1&, const X2&)>& f, const X1& x1,
      double delta) :
      f_(f), x1_(x1), delta_(delta) {
  }
  Vector operator()(const X2& x2) {
    return numericalGradient<X1>(boost::bind(f_, _1, x2), x1_, delta_);
  }
};

template<class X1, class X2>
inline Matrix numericalHessian212(
    boost::function<double(const X1&, const X2&)> f, const X1& x1, const X2& x2,
    double delta = 1e-5) {
  G_x1<X1, X2> g_x1(f, x1, delta);
  return numericalDerivative11<Vector, X2>(
      boost::function<Vector(const X2&)>(
          boost::bind<Vector>(boost::ref(g_x1), _1)), x2, delta);
}

template<class X1, class X2>
inline Matrix numericalHessian212(double (*f)(const X1&, const X2&),
    const X1& x1, const X2& x2, double delta = 1e-5) {
  return numericalHessian212(boost::function<double(const X1&, const X2&)>(f),
      x1, x2, delta);
}

template<class X1, class X2>
inline Matrix numericalHessian211(
    boost::function<double(const X1&, const X2&)> f, const X1& x1, const X2& x2,
    double delta = 1e-5) {

  Vector (*numGrad)(boost::function<double(const X1&)>, const X1&,
      double) = &numericalGradient<X1>;
  boost::function<double(const X1&)> f2(boost::bind(f, _1, x2));

  return numericalDerivative11<Vector, X1>(
      boost::function<Vector(const X1&)>(boost::bind(numGrad, f2, _1, delta)),
      x1, delta);
}

template<class X1, class X2>
inline Matrix numericalHessian211(double (*f)(const X1&, const X2&),
    const X1& x1, const X2& x2, double delta = 1e-5) {
  return numericalHessian211(boost::function<double(const X1&, const X2&)>(f),
      x1, x2, delta);
}

template<class X1, class X2>
inline Matrix numericalHessian222(
    boost::function<double(const X1&, const X2&)> f, const X1& x1, const X2& x2,
    double delta = 1e-5) {

  Vector (*numGrad)(boost::function<double(const X2&)>, const X2&,
      double) = &numericalGradient<X2>;
  boost::function<double(const X2&)> f2(boost::bind(f, x1, _1));

  return numericalDerivative11<Vector, X2>(
      boost::function<Vector(const X2&)>(boost::bind(numGrad, f2, _1, delta)),
      x2, delta);
}

template<class X1, class X2>
inline Matrix numericalHessian222(double (*f)(const X1&, const X2&),
    const X1& x1, const X2& x2, double delta = 1e-5) {
  return numericalHessian222(boost::function<double(const X1&, const X2&)>(f),
      x1, x2, delta);
}

/**
 * Numerical Hessian for tenary functions
 */
/* **************************************************************** */
template<class X1, class X2, class X3>
inline Matrix numericalHessian311(
    boost::function<double(const X1&, const X2&, const X3&)> f, const X1& x1,
    const X2& x2, const X3& x3, double delta = 1e-5) {

  Vector (*numGrad)(boost::function<double(const X1&)>, const X1&,
      double) = &numericalGradient<X1>;
  boost::function<double(const X1&)> f2(boost::bind(f, _1, x2, x3));

  return numericalDerivative11<Vector, X1>(
      boost::function<Vector(const X1&)>(boost::bind(numGrad, f2, _1, delta)),
      x1, delta);
}

template<class X1, class X2, class X3>
inline Matrix numericalHessian311(double (*f)(const X1&, const X2&, const X3&),
    const X1& x1, const X2& x2, const X3& x3, double delta = 1e-5) {
  return numericalHessian311(
      boost::function<double(const X1&, const X2&, const X3&)>(f), x1, x2, x3,
      delta);
}

/* **************************************************************** */
template<class X1, class X2, class X3>
inline Matrix numericalHessian322(
    boost::function<double(const X1&, const X2&, const X3&)> f, const X1& x1,
    const X2& x2, const X3& x3, double delta = 1e-5) {

  Vector (*numGrad)(boost::function<double(const X2&)>, const X2&,
      double) = &numericalGradient<X2>;
  boost::function<double(const X2&)> f2(boost::bind(f, x1, _1, x3));

  return numericalDerivative11<Vector, X2>(
      boost::function<Vector(const X2&)>(boost::bind(numGrad, f2, _1, delta)),
      x2, delta);
}

template<class X1, class X2, class X3>
inline Matrix numericalHessian322(double (*f)(const X1&, const X2&, const X3&),
    const X1& x1, const X2& x2, const X3& x3, double delta = 1e-5) {
  return numericalHessian322(
      boost::function<double(const X1&, const X2&, const X3&)>(f), x1, x2, x3,
      delta);
}

/* **************************************************************** */
template<class X1, class X2, class X3>
inline Matrix numericalHessian333(
    boost::function<double(const X1&, const X2&, const X3&)> f, const X1& x1,
    const X2& x2, const X3& x3, double delta = 1e-5) {

  Vector (*numGrad)(boost::function<double(const X3&)>, const X3&,
      double) = &numericalGradient<X3>;
  boost::function<double(const X3&)> f2(boost::bind(f, x1, x2, _1));

  return numericalDerivative11<Vector, X3>(
      boost::function<Vector(const X3&)>(boost::bind(numGrad, f2, _1, delta)),
      x3, delta);
}

template<class X1, class X2, class X3>
inline Matrix numericalHessian333(double (*f)(const X1&, const X2&, const X3&),
    const X1& x1, const X2& x2, const X3& x3, double delta = 1e-5) {
  return numericalHessian333(
      boost::function<double(const X1&, const X2&, const X3&)>(f), x1, x2, x3,
      delta);
}

/* **************************************************************** */
template<class X1, class X2, class X3>
inline Matrix numericalHessian312(
    boost::function<double(const X1&, const X2&, const X3&)> f, const X1& x1,
    const X2& x2, const X3& x3, double delta = 1e-5) {
  return numericalHessian212<X1, X2>(
      boost::function<double(const X1&, const X2&)>(boost::bind(f, _1, _2, x3)),
      x1, x2, delta);
}

template<class X1, class X2, class X3>
inline Matrix numericalHessian313(
    boost::function<double(const X1&, const X2&, const X3&)> f, const X1& x1,
    const X2& x2, const X3& x3, double delta = 1e-5) {
  return numericalHessian212<X1, X3>(
      boost::function<double(const X1&, const X3&)>(boost::bind(f, _1, x2, _2)),
      x1, x3, delta);
}

template<class X1, class X2, class X3>
inline Matrix numericalHessian323(
    boost::function<double(const X1&, const X2&, const X3&)> f, const X1& x1,
    const X2& x2, const X3& x3, double delta = 1e-5) {
  return numericalHessian212<X2, X3>(
      boost::function<double(const X2&, const X3&)>(boost::bind(f, x1, _1, _2)),
      x2, x3, delta);
}

/* **************************************************************** */
template<class X1, class X2, class X3>
inline Matrix numericalHessian312(double (*f)(const X1&, const X2&, const X3&),
    const X1& x1, const X2& x2, const X3& x3, double delta = 1e-5) {
  return numericalHessian312(
      boost::function<double(const X1&, const X2&, const X3&)>(f), x1, x2, x3,
      delta);
}

template<class X1, class X2, class X3>
inline Matrix numericalHessian313(double (*f)(const X1&, const X2&, const X3&),
    const X1& x1, const X2& x2, const X3& x3, double delta = 1e-5) {
  return numericalHessian313(
      boost::function<double(const X1&, const X2&, const X3&)>(f), x1, x2, x3,
      delta);
}

template<class X1, class X2, class X3>
inline Matrix numericalHessian323(double (*f)(const X1&, const X2&, const X3&),
    const X1& x1, const X2& x2, const X3& x3, double delta = 1e-5) {
  return numericalHessian323(
      boost::function<double(const X1&, const X2&, const X3&)>(f), x1, x2, x3,
      delta);
}

// The benefit of this method is that it does not need to know what types are involved
// to evaluate the factor. If all the machinery of gtsam is working correctly, we should
// get the correct numerical derivatives out the other side.
template<typename FactorType>
JacobianFactor computeNumericalDerivativeJacobianFactor(const FactorType& factor,
                                                     const Values& values,
                                                     double fd_step) {
  Eigen::VectorXd e = factor.unwhitenedError(values);
  const size_t rows = e.size();

  std::map<Key, Matrix> jacobians;
  typename FactorType::const_iterator key_it = factor.begin();
  VectorValues dX = values.zeroVectors();
  for(; key_it != factor.end(); ++key_it) {
    size_t key = *key_it;
    // Compute central differences using the values struct.
    const size_t cols = dX.dim(key);
    Matrix J = Matrix::Zero(rows, cols);
    for(size_t col = 0; col < cols; ++col) {
      Eigen::VectorXd dx = Eigen::VectorXd::Zero(cols);
      dx[col] = fd_step;
      dX[key] = dx;
      Values eval_values = values.retract(dX);
      Eigen::VectorXd left = factor.unwhitenedError(eval_values);
      dx[col] = -fd_step;
      dX[key] = dx;
      eval_values = values.retract(dX);
      Eigen::VectorXd right = factor.unwhitenedError(eval_values);
      J.col(col) = (left - right) * (1.0/(2.0 * fd_step));
    }
    jacobians[key] = J;
  }

  // Next step...return JacobianFactor
  return JacobianFactor(jacobians, -e);
}

template<typename FactorType>
void testFactorJacobians(TestResult& result_,
                         const std::string& name_,
                         const FactorType& f,
                         const gtsam::Values& values,
                         double fd_step,
                         double tolerance) {
  // Check linearization
  JacobianFactor expected = computeNumericalDerivativeJacobianFactor(f, values, fd_step);
  boost::shared_ptr<GaussianFactor> gf = f.linearize(values);
  boost::shared_ptr<JacobianFactor> jf = //
      boost::dynamic_pointer_cast<JacobianFactor>(gf);

  typedef std::pair<Eigen::MatrixXd, Eigen::VectorXd> Jacobian;
  Jacobian evalJ = jf->jacobianUnweighted();
  Jacobian estJ = expected.jacobianUnweighted();
  EXPECT(assert_equal(evalJ.first, estJ.first, tolerance));
  EXPECT(assert_equal(evalJ.second, Eigen::VectorXd::Zero(evalJ.second.size()), tolerance));
  EXPECT(assert_equal(estJ.second, Eigen::VectorXd::Zero(evalJ.second.size()), tolerance));
}

} // namespace gtsam

/// \brief Check the Jacobians produced by a factor against finite differences.
/// \param factor The factor to test.
/// \param values Values filled in for testing the Jacobians.
/// \param numerical_derivative_step The step to use when computing the numerical derivative Jacobians
/// \param tolerance The numerical tolerance to use when comparing Jacobians.
#define EXPECT_CORRECT_FACTOR_JACOBIANS(factor, values, numerical_derivative_step, tolerance) \
    { gtsam::testFactorJacobians(result_, name_, factor, values, numerical_derivative_step, tolerance); }

