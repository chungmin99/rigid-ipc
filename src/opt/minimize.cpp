#include <opt/minimize.hpp>

#ifdef BUILD_WITH_IPOPT
#include <opt/IpEigenInterfaceTNLP.hpp>
#endif

#include <ccd/not_implemented_error.hpp>

namespace ccd {
namespace opt {
    bool OptimizationProblem::validate_problem()
    {
        bool valid = true;
        valid = valid && (num_vars == x0.rows());
        valid = valid && (num_vars == x_lower.rows() || x_lower.size() == 0);
        valid = valid && (num_vars == x_upper.rows() || x_upper.size() == 0);
        valid = valid
            && (num_constraints == g_lower.rows() || g_lower.size() == 0);
        valid = valid
            && (num_constraints == g_upper.rows() || g_upper.size() == 0);
        valid = valid && ((g == nullptr) == (jac_g == nullptr));
        valid = valid && (f != nullptr);
        valid = valid && (grad_f != nullptr);

        if (!valid) {
            return false;
        }

        if (x_lower.size() == 0) {
            x_lower.resize(num_vars);
            x_lower.setConstant(NO_LOWER_BOUND); // no-lower-bound
        }
        if (x_upper.size() == 0) {
            x_upper.resize(num_vars);
            x_upper.setConstant(NO_UPPER_BOUND); // no-upper-bound
        }
        if (g_lower.size() == 0) {
            g_lower.resize(num_constraints);
            g_lower.setConstant(NO_LOWER_BOUND); // no-lower-bound
        }
        if (g_upper.size() == 0) {
            g_upper.resize(num_constraints);
            g_upper.setConstant(NO_UPPER_BOUND); // no-upper-bound
        }
        return true;
    }
    OptimizationProblem::OptimizationProblem() {}
    OptimizationProblem::OptimizationProblem(const Eigen::VectorXd& x0,
        const callback_f f, const callback_grad_f grad_f,
        const Eigen::VectorXd& x_lower, const Eigen::VectorXd& x_upper,
        const int num_constraints, const callback_g& g,
        const callback_jac_g& jac_g, const Eigen::VectorXd& g_lower,
        const Eigen::VectorXd& g_upper, const callback_intermediate callback)
        : num_vars(int(x0.rows()))
        , num_constraints(num_constraints)
        , x0(x0)
        , x_lower(x_lower)
        , x_upper(x_upper)
        , g_lower(g_lower)
        , g_upper(g_upper)
        , f(f)
        , grad_f(grad_f)
        , g(g)
        , jac_g(jac_g)
        , verbosity(0)
        , max_iter(3000)
        , intermediate_cb(callback)
    {
    }

    OptimizationResult minimize(
        const OptimizationMethod& method, OptimizationProblem& problem)
    {
        if(!problem.validate_problem()){
            return OptimizationResult();
        }


        switch (method) {
        case MMA:
        case SLSQP:
            throw NotImplementedError("NLOPT not connected");
        case IP:
            return ccd::opt::minimize_ipopt(problem);
        }
    }
} // namespace opt
} // namespace ccd
