/*

Copyright (c) 2005-2016, University of Oxford.
All rights reserved.

University of Oxford means the Chancellor, Masters and Scholars of the
University of Oxford, having an administrative office at Wellington
Square, Oxford OX1 2JD, UK.

This file is part of Aboria.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
 * Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
 * Neither the name of the University of Oxford nor the names of its
   contributors may be used to endorse or promote products derived from this
   software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef RBF_PARAM_SWEEP_H_
#define RBF_PARAM_SWEEP_H_

//#include "hilbert/hilbert.h"
#include <sys/resource.h>
#include <sys/time.h>

#include <chrono>
#include <cxxtest/TestSuite.h>
#include <fstream>
typedef std::chrono::system_clock Clock;
#include "Aboria.h"
using namespace Aboria;

class RbfParamSweepTest : public CxxTest::TestSuite {
public:
  ABORIA_VARIABLE(function, double, "function_eval");

  struct matern_kernel {
    double m_scale;
    double m_sigma;
    static constexpr const char *m_name = "matern";
    void set_sigma(const double sigma) {
      m_sigma = sigma;
      m_scale = std::sqrt(3.0) / sigma;
    }
    template <unsigned int D>
    double operator()(const Vector<double, D> &a,
                      const Vector<double, D> &b) const {
      const double r = (b - a).norm();
      return (1.0 + m_scale * r) * std::exp(-r * m_scale);
    };
  };

  struct output_files {
    std::ofstream out_op_setup_time;
    std::ofstream out_op_apply_time;
    std::ofstream out_op_apply_error;

    const int width = 12;
    static const int Nops = 2;
    std::ofstream out_solve_setup_time[Nops];
    std::ofstream out_solve_solve_time[Nops];
    std::ofstream out_solve_iterations[Nops];
    std::ofstream out_solve_error[Nops];
    std::ofstream out_solve_test_error[Nops];

    output_files(const std::string &name) {

      auto solve_header = [&](auto &out) {
        out << std::setw(width) << "N " << std::setw(width) << "sigma "
            << std::setw(width) << "D " << std::setw(width) << "order "
            << std::setw(width) << "chol " << std::setw(width) << "diag "
            << std::setw(width) << "srtz " << std::setw(width) << "nystrom "
            << std::endl;
      };

      auto op_header = [&](auto &out) {
        out << std::setw(width) << "N " << std::setw(width) << "sigma "
            << std::setw(width) << "D " << std::setw(width) << "order "
            << std::setw(width) << "matrix " << std::setw(width) << "dense "
            << std::setw(width) << "fmm " << std::setw(width) << "h2 "
            << std::endl;
      };

      out_op_setup_time.open(name + "_op_setup_time.txt", std::ios::out);
      op_header(out_op_setup_time);
      out_op_apply_time.open(name + "_op_apply_time.txt", std::ios::out);
      op_header(out_op_apply_time);
      out_op_apply_error.open(name + "_op_apply_error.txt", std::ios::out);
      op_header(out_op_apply_error);

      std::string op_names[2] = {"_matrix_", "_h2_"};
      for (int i = 0; i < Nops; ++i) {
        out_solve_setup_time[i].open(
            name + op_names[i] + "solve_setup_time.txt", std::ios::out);
        solve_header(out_solve_setup_time[i]);
        out_solve_solve_time[i].open(
            name + op_names[i] + "solve_solve_time.txt", std::ios::out);
        solve_header(out_solve_solve_time[i]);
        out_solve_iterations[i].open(
            name + op_names[i] + "solve_iterations.txt", std::ios::out);
        solve_header(out_solve_iterations[i]);
        out_solve_error[i].open(name + op_names[i] + "solve_error.txt",
                                std::ios::out);
        solve_header(out_solve_error[i]);
        out_solve_test_error[i].open(
            name + op_names[i] + "solve_test_error.txt", std::ios::out);
        solve_header(out_solve_test_error[i]);
      }
    }

    void new_line_end() {
      out_op_setup_time << std::endl;
      out_op_apply_time << std::endl;
      out_op_apply_error << std::endl;
      for (int i = 0; i < Nops; ++i) {
        out_solve_setup_time[i] << std::endl;
        out_solve_solve_time[i] << std::endl;
        out_solve_iterations[i] << std::endl;
        out_solve_error[i] << std::endl;
        out_solve_test_error[i] << std::endl;
      }
    }

    void new_line_start(const size_t N, const double sigma, const size_t D,
                        const size_t order) {
      auto start = [&](auto &out) {
        out << std::setw(width) << N << " " << std::setw(width) << sigma << " "
            << std::setw(width) << D << " " << std::setw(width) << order;
      };

      start(out_op_setup_time);
      start(out_op_apply_time);
      start(out_op_apply_error);
      for (int i = 0; i < Nops; ++i) {
        start(out_solve_setup_time[i]);
        start(out_solve_solve_time[i]);
        start(out_solve_iterations[i]);
        start(out_solve_error[i]);
        start(out_solve_test_error[i]);
      }
    }

    void close() {
      out_op_setup_time.close();
      out_op_apply_time.close();
      out_op_apply_error.close();
      for (int i = 0; i < Nops; ++i) {
        out_solve_setup_time[i].close();
        out_solve_solve_time[i].close();
        out_solve_iterations[i].close();
        out_solve_error[i].close();
        out_solve_test_error[i].close();
      }
    }
  };

  template <unsigned int D> auto rosenbrock(size_t N, size_t Ntest) {
    typedef Particles<std::tuple<function>, D, std::vector, KdtreeNanoflann>
        Particles_t;
    typedef position_d<D> position;

    Particles_t knots(N + Ntest);

    auto funct = [](const auto &x) {
      double ret = 0;
      // rosenbrock functions all D
      for (size_t i = 0; i < D - 1; ++i) {
        ret += 100 * std::pow(x[i + 1] - std::pow(x[i], 2), 2) +
               std::pow(1 - x[i], 2);
      }
      return ret;
    };

    std::default_random_engine generator(0);
    std::uniform_real_distribution<double> distribution(0.0, 1.0);

    for (size_t i = 0; i < N + Ntest; ++i) {
      for (size_t d = 0; d < D; ++d) {
        get<position>(knots)[i][d] = distribution(generator);
      }
      get<function>(knots)[i] = funct(get<position>(knots)[i]);
    }
    return knots;
  }

  template <typename Op, typename TrueOp, typename TestOp>
  void helper_operator_matrix(const Op &G, const TrueOp &Gtrue,
                              const Eigen::VectorXd &phi, const TestOp &Gtest,
                              const Eigen::VectorXd &phi_test,
                              output_files &out, bool do_solve) {

    if (do_solve) {
      std::cout << "SOLVING CHOL" << std::endl;
      auto t0 = Clock::now();
      auto solver = G.llt();
      auto t1 = Clock::now();
      Eigen::VectorXd gamma = solver.solve(phi);
      auto t2 = Clock::now();

      for (int i = 0; i < 2; ++i) {
        out.out_solve_iterations[i] << " " << std::setw(out.width) << 1;
        out.out_solve_error[i] << " " << std::setw(out.width) << 0;
        out.out_solve_setup_time[i]
            << " " << std::setw(out.width)
            << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0)
                   .count();
        out.out_solve_solve_time[i]
            << " " << std::setw(out.width)
            << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1)
                   .count();
      }
    } else {
      for (int i = 0; i < 2; ++i) {
        out.out_solve_iterations[i] << " " << std::setw(out.width) << -1;
        out.out_solve_error[i] << " " << std::setw(out.width) << -1;
        out.out_solve_setup_time[i] << " " << std::setw(out.width) << -1;
        out.out_solve_solve_time[i] << " " << std::setw(out.width) << -1;
      }
    }
  }

  template <typename Op, typename TrueOp, typename TestOp>
  void helper_operator(const Op &G, const TrueOp &Gtrue,
                       const Eigen::VectorXd &phi, const TestOp &Gtest,
                       const Eigen::VectorXd &phi_test, const int max_iter,
                       const size_t Nbuffer, output_files &out, int do_solve) {

    Eigen::VectorXd gamma = Eigen::VectorXd::Random(N);
    auto t0 = Clock::now();
    Eigen::VectorXd phi_random = G * gamma;
    auto t1 = Clock::now();

    std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    Eigen::VectorXd phi_random_true = Gtrue * gamma;

    out.out_op_apply_error << " " << std::setw(out.width)
                           << (phi_random - phi_random_true).norm() /
                                  phi_random_true.norm();
    out.out_op_apply_time
        << " " << std::setw(out.width)
        << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0)
               .count();

    if (do_solve) {

      std::cout << "SOLVING IDENTITY" << std::endl;
      Eigen::BiCGSTAB<Op, Eigen::IdentityPreconditioner> bicg;

      bicg.setMaxIterations(max_iter);
      t0 = Clock::now();
      bicg.compute(G);
      t1 = Clock::now();
      Eigen::VectorXd gamma = bicg.solve(phi);
      auto t2 = Clock::now();
      out.out_solve_iterations[do_solve - 1] << " " << std::setw(out.width)
                                             << bicg.iterations();
      out.out_solve_error[do_solve - 1] << " " << std::setw(out.width)
                                        << bicg.error();
      out.out_solve_setup_time[do_solve - 1]
          << " " << std::setw(out.width)
          << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0)
                 .count();
      out.out_solve_solve_time[do_solve - 1]
          << " " << std::setw(out.width)
          << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1)
                 .count();

      Eigen::VectorXd phi_proposed = Gtest * gamma;

      out.out_solve_test_error[do_solve - 1]
          << " " << std::setw(out.width)
          << (phi_proposed - phi_test).norm() / phi_test.norm();

      std::cout << "SOLVING Schwartz" << std::endl;
      Eigen::BiCGSTAB<Op, SchwartzPreconditioner<Eigen::LLT<Eigen::MatrixXd>>>
          bicg2;
      bicg2.setMaxIterations(max_iter);
      bicg2.preconditioner().set_max_buffer_n(Nbuffer);
      t0 = Clock::now();
      bicg2.compute(G);
      t1 = Clock::now();
      gamma = bicg2.solve(phi);
      t2 = Clock::now();

      out.out_solve_iterations[do_solve - 1] << " " << std::setw(out.width)
                                             << bicg2.iterations();
      out.out_solve_error[do_solve - 1] << " " << std::setw(out.width)
                                        << bicg2.error();
      out.out_solve_setup_time[do_solve - 1]
          << " " << std::setw(out.width)
          << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0)
                 .count();
      out.out_solve_solve_time[do_solve - 1]
          << " " << std::setw(out.width)
          << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1)
                 .count();

      phi_proposed = Gtest * gamma;

      out.out_solve_test_error[do_solve - 1]
          << " " << std::setw(out.width)
          << (phi_proposed - phi_test).norm() / phi_test.norm();

      Eigen::BiCGSTAB<Op, NystromPreconditioner<Eigen::LLT<Eigen::MatrixXd>>>
          bicg3;
      bicg3.setMaxIterations(max_iter);
      bicg3.preconditioner().set_number_of_random_particles(Nbuffer *
                                                            std::sqrt(N));
      bicg3.preconditioner().set_lambda(1e-5);
      t0 = Clock::now();
      bicg3.compute(G);
      t1 = Clock::now();
      gamma = bicg3.solve(phi);
      t2 = Clock::now();

      out.out_solve_iterations[do_solve - 1] << " " << std::setw(out.width)
                                             << bicg3.iterations();
      out.out_solve_error[do_solve - 1] << " " << std::setw(out.width)
                                        << bicg3.error();
      out.out_solve_setup_time[do_solve - 1]
          << " " << std::setw(out.width)
          << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0)
                 .count();
      out.out_solve_solve_time[do_solve - 1]
          << " " << std::setw(out.width)
          << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1)
                 .count();

      phi_proposed = Gtest * gamma;

      out.out_solve_test_error[do_solve - 1]
          << " " << std::setw(out.width)
          << (phi_proposed - phi_test).norm() / phi_test.norm();
    }
  }

  template <size_t Order, typename Particles_t, typename Kernel>
  void helper_param_sweep(const Particles_t &particles, const size_t Ntest,
                          const Kernel &kernel, const double jitter,
                          output_files &out) {
#ifdef HAVE_EIGEN

    const int width = 11;
    char name[50] = "program name";
    char *argv[] = {name, NULL};
    int argc = sizeof(argv) / sizeof(char *) - 1;
    char **argv2 = &argv[0];
    init_h2lib(&argc, &argv2);

    out.new_line_start(particles.size() - Ntest, kernel.m_sigma,
                       Particles_t::dimension, Order);

    const int max_iter = 1000;
    const size_t n_subdomain = 200;
    const int Nbuffer = 4 * n_subdomain;
    const unsigned int D = Particles_t::dimension;
    typedef position_d<D> position;

    Particles_t knots(particles.size() - Ntest);
    Particles_t test(Ntest);

    bbox<D> knots_box;
    for (size_t i = 0; i < knots.size(); ++i) {
      knots[i] = particles[i];
      knots_box = knots_box + bbox<D>(get<position>(knots)[i]);
    }

    bbox<D> test_box;
    for (size_t i = 0; i < test.size(); ++i) {
      test[i] = particles[knots.size() + i];
      test_box = test_box + bbox<D>(get<position>(test)[i]);
    }

    knots.init_neighbour_search(knots_box.bmin, knots_box.bmax,
                                Vector<bool, D>::Constant(false), n_subdomain);
    test.init_neighbour_search(test_box.bmin, test_box.bmax,
                               Vector<bool, D>::Constant(false), n_subdomain);
    std::cout << "FINISHED INIT NEIGHBOUR" << std::endl;

    auto self_kernel = [&](auto a, auto b) {
      double ret = kernel(get<position>(a), get<position>(b));
      if (get<id>(a) == get<id>(b)) {
        ret += jitter;
      }
      return ret;
    };

    typedef Eigen::Map<Eigen::Matrix<double, Eigen::Dynamic, 1>> map_type;
    map_type phi(get<function>(knots).data(), knots.size());
    map_type phi_test(get<function>(test).data(), test.size());

    auto t0 = Clock::now();
    auto Gmatrix = create_matrix_operator(knots, knots, self_kernel);
    auto t1 = Clock::now();
    out.out_op_setup_time
        << " " << std::setw(width)
        << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0)
               .count();
    auto Gmatrix_test = create_matrix_operator(test, knots, self_kernel);

    std::cout << "APPLYING MATRIX OPERATOR" << std::endl;
    helper_operator_matrix(Gmatrix.get_first_kernel().get_matrix(), Gmatrix,
                           phi, Gmatrix_test, phi_test, out, true);
    helper_operator(Gmatrix, Gmatrix, phi, Gmatrix_test, phi_test, max_iter,
                    Nbuffer, out, 1);

    std::cout << "CREATING DENSE OPERATOR" << std::endl;
    t0 = Clock::now();
    auto Gdense = create_dense_operator(knots, knots, self_kernel);
    t1 = Clock::now();
    out.out_op_setup_time
        << " " << std::setw(width)
        << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0)
               .count();

    auto Gdense_test = create_dense_operator(test, knots, self_kernel);

    std::cout << "APPLYING DENSE OPERATOR" << std::endl;
    helper_operator(Gdense, Gmatrix, phi, Gdense_test, phi_test, max_iter,
                    Nbuffer, out, 0);

    std::cout << "CREATING FMM OPERATOR" << std::endl;
    t0 = Clock::now();
    auto G_FMM = create_fmm_operator<Order>(knots, knots, kernel, self_kernel);
    t1 = Clock::now();
    out.out_op_setup_time
        << " " << std::setw(width)
        << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0)
               .count();

    auto G_FMM_test =
        create_fmm_operator<Order>(test, knots, kernel, self_kernel);

    std::cout << "APPLYING FMM OPERATOR" << std::endl;
    helper_operator(G_FMM, Gmatrix, phi, G_FMM_test, phi_test, max_iter,
                    Nbuffer, out, 0);

    std::cout << "CREATING H2 OPERATOR" << std::endl;
    t0 = Clock::now();
    auto G_H2 = create_h2_operator(knots, knots, Order, kernel, self_kernel);
    t1 = Clock::now();
    out.out_op_setup_time
        << " " << std::setw(width)
        << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0)
               .count();

    auto G_H2_test =
        create_h2_operator(test, knots, Order, kernel, self_kernel);

    helper_operator(G_H2, Gmatrix, phi, G_H2_test, phi_test, max_iter, Nbuffer,
                    out, 2);

    out.new_line_end();

#endif // HAVE_EIGEN
  }

  void test_param_sweep(void) {
    std::cout << "-------------------------------------------\n"
              << "Running precon param sweep             ....\n"
              << "------------------------------------------" << std::endl;

    /*
    double gscale = std::pow(0.1, 2);
    auto gaussian_kernel = [&](const auto &a, const auto &b) {
      return std::exp(-(b - a).squaredNorm() * gscale);
    };
    */
    matern_kernel kernel;
    output_files out(kernel.m_name);

    const size_t Ntest = 1000;
    const double jitter = 1e-5;

    for (int N = 1000; N < 300000; N *= 2) {
      for (double sigma = 0.1; sigma < 2.0; sigma += 0.4) {
        kernel.set_sigma(sigma);

        helper_param_sweep<2>(rosenbrock<8>(N, Ntest), Ntest, kernel, jitter,
                              out);
        helper_param_sweep<2>(rosenbrock<6>(N, Ntest), Ntest, kernel, jitter,
                              out);
        helper_param_sweep<4>(rosenbrock<4>(N, Ntest), Ntest, kernel, jitter,
                              out);
        helper_param_sweep<5>(rosenbrock<3>(N, Ntest), Ntest, kernel, jitter,
                              out);
        helper_param_sweep<8>(rosenbrock<2>(N, Ntest), Ntest, kernel, jitter,
                              out);
      }
    }
    out.close();

    /*
    double c = 1.0 / 0.1;
    double invc = 1.0 / c;
    auto wendland_kernel = [&](const auto &a, const auto &b) {
      const double r = (b - a).norm();
      if (r < 2 * invc) {
        return std::pow(2.0 - r * c, 4) * (1.0 + 2.0 * r * c);
      } else {
        return 0.0;
      }
    };
    */
  }
};

#endif /* RBF_PARAM_SWEEP_H_ */
