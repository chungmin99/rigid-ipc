#include "rigid_body_problem.hpp"

#include <iostream>

#include <finitediff.hpp>
#include <utils/flatten.hpp>
#include <utils/tensor.hpp>

#include <io/read_rb_scene.hpp>
#include <io/serialize_json.hpp>

#include <logger.hpp>
#include <profiler.hpp>

namespace ccd {

namespace physics {

    RigidBodyProblem::RigidBodyProblem()
        : RigidBodyProblem("rigid_body_problem")
    {
    }

    RigidBodyProblem::RigidBodyProblem(const std::string& name)
        : coefficient_restitution(0)
        , collision_eps(2)
        , name_(name)
    {
    }

    void RigidBodyProblem::settings(const nlohmann::json& params)
    {
        collision_eps = params["collision_eps"].get<double>();
        coefficient_restitution =
            params["coefficient_restitution"].get<double>();

        std::vector<physics::RigidBody> rbs;
        io::read_rb_scene(params, rbs);

        init(rbs);
        m_assembler.init(rbs);

        io::from_json(params["gravity"], gravity_);
        assert(gravity_.size() >= dim());
        gravity_.conservativeResize(dim());
    }

    nlohmann::json RigidBodyProblem::settings() const
    {
        nlohmann::json json;

        json["collision_eps"] = collision_eps;
        json["coefficient_restitution"] = coefficient_restitution;
        json["gravity"] = io::to_json(gravity_);
        return json;
    }

    void RigidBodyProblem::init(const std::vector<RigidBody> rbs)
    {
        m_assembler.init(rbs);

        Fcollision.resize(m_assembler.num_vertices(), dim());
        Fcollision.setZero();
        update_constraint();

        if (dim() == 2) {
            for (size_t i = 0; i < m_assembler.m_rbs.size(); ++i) {
                auto& rb = m_assembler.m_rbs[i];
                spdlog::info(
                    "rb={} mass={} innertia={}", i, rb.mass,
                    rb.moment_of_inertia(0));
            }
        }
    }

    nlohmann::json RigidBodyProblem::state() const
    {
        nlohmann::json json;
        std::vector<nlohmann::json> rbs;
        Eigen::VectorXd p = Eigen::VectorXd::Zero(
            Pose<double>::dim_to_pos_ndof(dim())); // Linear momentum
        Eigen::VectorXd L = Eigen::VectorXd::Zero(
            Pose<double>::dim_to_rot_ndof(dim())); // Angular momentum
        double T = 0.0;                            // Kinetic energy
        double G = 0.0;                            // Potential energy

        for (auto& rb : m_assembler.m_rbs) {
            nlohmann::json jrb;
            jrb["position"] = io::to_json(Eigen::VectorXd(rb.pose.position));
            jrb["rotation"] = io::to_json(Eigen::VectorXd(rb.pose.rotation));
            jrb["linear_velocity"] =
                io::to_json(Eigen::VectorXd(rb.velocity.position));
            jrb["angular_velocity"] =
                io::to_json(Eigen::VectorXd(rb.velocity.rotation));
            rbs.push_back(jrb);

            // momentum
            p += rb.mass * rb.velocity.position;
            L += rb.moment_of_inertia * rb.velocity.rotation;

            T += 0.5 * rb.mass * rb.velocity.position.squaredNorm();
            T += 0.5 * rb.velocity.rotation.transpose() * rb.moment_of_inertia
                * rb.velocity.rotation;

            if (!rb.is_dof_fixed[0] && !rb.is_dof_fixed[1]) {
                G -= rb.mass * gravity_.dot(rb.pose.position);
            }
        }
        // Another way of compting total energy
        // Eigen::MatrixXd vel = m_assembler.world_velocities();
        // ccd::flatten(vel);
        // Eigen::VectorXd vel_ = vel;
        // double kinetic = 1.0 / 2.0  * (vel_.transpose() *
        // m_assembler.m_mass_matrix * vel_)[0];

        json["rigid_bodies"] = rbs;
        json["linear_momentum"] = io::to_json(p);
        json["angular_momentum"] = io::to_json(L);
        json["kinetic_energy"] = T;
        json["potential_energy"] = G;
        return json;
    }

    void RigidBodyProblem::state(const nlohmann::json& args)
    {
        nlohmann::json json;
        auto& rbs = args["rigid_bodies"];
        assert(rbs.size() == m_assembler.m_rbs.size());
        size_t i = 0;
        for (auto& jrb : args["rigid_bodies"]) {
            io::from_json(jrb["position"], m_assembler.m_rbs[i].pose.position);
            io::from_json(jrb["rotation"], m_assembler.m_rbs[i].pose.rotation);
            io::from_json(
                jrb["linear_velocity"], m_assembler.m_rbs[i].velocity.position);
            io::from_json(
                jrb["angular_velocity"],
                m_assembler.m_rbs[i].velocity.rotation);
            i++;
        }
    }

    void RigidBodyProblem::update_dof()
    {
        poses_t0 = m_assembler.rb_poses_t0();
        poses_t1 = m_assembler.rb_poses_t1();
        x0 = this->poses_to_dofs(poses_t0);
        num_vars_ = x0.size();
    }

    bool RigidBodyProblem::simulation_step(const double time_step)
    {
        tbb::parallel_for_each(m_assembler.m_rbs, [&](RigidBody& rb) {
            rb.pose_prev = rb.pose;
            rb.velocity_prev = rb.velocity;
            rb.pose = rb_next_pose(rb, time_step);
            rb.velocity = (rb.pose - rb.pose_prev) / time_step;
        });

        Fcollision.setZero();

        update_dof();

        return detect_collisions(
            poses_t0, poses_t1, CollisionCheck::CONSERVATIVE);
    }

    void RigidBodyProblem::update_constraint()
    {
        update_dof();

        // Compute the collisions
        original_impacts.clear();
        constraint().initialize();
        constraint().construct_collision_set(
            m_assembler, poses_t0, poses_t1 - poses_t0, original_impacts);

        tbb::parallel_sort(
            original_impacts.ev_impacts.begin(),
            original_impacts.ev_impacts.end(),
            ccd::compare_impacts_by_time<ccd::EdgeVertexImpact>);
        tbb::parallel_sort(
            original_impacts.ee_impacts.begin(),
            original_impacts.ee_impacts.end(),
            ccd::compare_impacts_by_time<ccd::EdgeEdgeImpact>);
        tbb::parallel_sort(
            original_impacts.fv_impacts.begin(),
            original_impacts.fv_impacts.end(),
            ccd::compare_impacts_by_time<ccd::FaceVertexImpact>);
    }

    opt::OptimizationResults RigidBodyProblem::solve_constraints()
    {
        return solver().solve();
    }

    void RigidBodyProblem::init_solve() { return solver().init_solve(); }

    opt::OptimizationResults RigidBodyProblem::step_solve()
    {
        return solver().step_solve();
    }

    bool RigidBodyProblem::take_step(
        const Eigen::VectorXd& dof, const double time_step)
    {
        // This need to be done BEFORE updating poses
        // -------------------------------------
        if (coefficient_restitution > -1) {
            solve_velocities();
        }

        // update final pose
        // -------------------------------------
        m_assembler.set_rb_poses(this->dofs_to_poses(dof));
        Poses<double> poses_q1 = m_assembler.rb_poses_t1();

        // This need to be done AFTER updating poses
        if (coefficient_restitution < 0) {
            tbb::parallel_for_each(m_assembler.m_rbs, [&](RigidBody& rb) {
                rb.velocity = (rb.pose - rb.pose_prev) / time_step;
            });
        }

        return detect_collisions(poses_t0, poses_q1, CollisionCheck::EXACT);
    }

    void RigidBodyProblem::solve_velocities()
    {
        if (dim() != 2) {
            throw NotImplementedError(
                "RigidBodyProblem::solve_velocities() not implmented for 3D!");
        }

        // precompute normal directions (since velocities will change i can't do
        // it after
        Eigen::MatrixXd normals(original_impacts.ev_impacts.size(), 2);

        for (long i = 0; i < normals.rows(); ++i) {
            const EdgeVertexImpact& ev_impact =
                original_impacts.ev_impacts[size_t(i)];

            double toi = ev_impact.time;

            const long edge_id = ev_impact.edge_index;
            const long a_id = ev_impact.vertex_index;
            const int b0_id = m_assembler.m_edges.coeff(edge_id, 0);
            const int b1_id = m_assembler.m_edges.coeff(edge_id, 1);

            const size_t body_B_id =
                size_t(m_assembler.m_vertex_to_body_map(b0_id));
            bool is_oriented = m_assembler.m_rbs[body_B_id].is_oriented;

            Eigen::Vector2d n_toi;
            Eigen::VectorX3d e_toi; // edge vector at toi
            switch (constraint().trajectory_type) {
            case TrajectoryType::LINEARIZED: {
                // Use linearized trajectories
                Eigen::VectorX3d e_v0_t0 =
                    m_assembler.world_vertex(poses_t0, b0_id);
                Eigen::VectorX3d e_v0_t1 =
                    m_assembler.world_vertex(poses_t1, b0_id);
                Eigen::VectorX3d e_v0_toi = (e_v0_t1 - e_v0_t0) * toi + e_v0_t0;

                Eigen::VectorX3d e_v1_t0 =
                    m_assembler.world_vertex(poses_t0, b1_id);
                Eigen::VectorX3d e_v1_t1 =
                    m_assembler.world_vertex(poses_t1, b1_id);
                Eigen::VectorX3d e_v1_toi = (e_v1_t1 - e_v1_t0) * toi + e_v1_t0;

                e_toi = e_v1_toi - e_v0_toi;
            } break;

            case TrajectoryType::SCREWING: {
                // Use nonlinear trajectory
                long edge_body_id = m_assembler.edge_id_to_body_id(edge_id);

                Pose<double> pose_toi = Pose<double>::lerp(
                    poses_t0[edge_body_id], poses_t1[edge_body_id], toi);

                e_toi = m_assembler.world_vertex(pose_toi, b1_id)
                    - m_assembler.world_vertex(pose_toi, b0_id);
            } break;
            }
            n_toi << -e_toi(1), e_toi(0); // 90deg ccw rotation
            n_toi.normalize();

            if (is_oriented) {
                n_toi = -n_toi;
            } else {
                // check normal points towards A
                Eigen::Vector2d va = m_assembler.world_vertex(poses_t0, a_id);
                Eigen::Vector2d vb = m_assembler.world_vertex(poses_t0, b0_id);
                if ((va - vb).transpose() * n_toi <= 0.0) {
                    n_toi *= -1;
                }
            }
            normals.row(i) = n_toi.transpose();
        }

#ifndef NDEBUG
        double prev_toi = -1;
#endif
        for (long i = 0; i < normals.rows(); ++i) {
            const EdgeVertexImpact& ev_impact =
                original_impacts.ev_impacts[size_t(i)];

            double toi = ev_impact.time;
#ifndef NDEBUG
            assert(prev_toi <= toi);
            prev_toi = toi;
#endif
            double alpha = ev_impact.alpha;

            // global ids of the vertices
            const long edge_id = ev_impact.edge_index;
            const long a_id = ev_impact.vertex_index;
            const int b0_id = m_assembler.m_edges.coeff(edge_id, 0);
            const int b1_id = m_assembler.m_edges.coeff(edge_id, 1);

            const size_t body_A_id =
                size_t(m_assembler.m_vertex_to_body_map(a_id));
            const size_t body_B_id =
                size_t(m_assembler.m_vertex_to_body_map(b0_id));

            // local (rigid body) ids of the vertices
            const long r_A_id = a_id - m_assembler.m_body_vertex_id[body_A_id];
            const long r_B0_id =
                b0_id - m_assembler.m_body_vertex_id[body_B_id];
            const long r_B1_id =
                b1_id - m_assembler.m_body_vertex_id[body_B_id];

            auto& body_A = m_assembler.m_rbs[body_A_id];
            auto& body_B = m_assembler.m_rbs[body_B_id];

            // The velocities of the center of mass at the time of collision!!
            Pose<double> vel_A_prev =
                Pose<double>::lerp(body_A.velocity_prev, body_A.velocity, toi);
            Pose<double> vel_B_prev =
                Pose<double>::lerp(body_B.velocity_prev, body_B.velocity, toi);

            // The masss
            const double inv_m_A =
                body_A.is_dof_fixed.head(dim()).any() ? 0.0 : 1.0 / body_A.mass;
            const double inv_m_B =
                body_B.is_dof_fixed.head(dim()).any() ? 0.0 : 1.0 / body_B.mass;

            if (dim() != 2) {
                throw NotImplementedError(
                    "Resitution not implemented in 3D yet!");
            }

            // The moment of inertia
            int rot_ndof = Pose<double>::dim_to_rot_ndof(dim());
            const Eigen::MatrixXd inv_I_A =
                body_A.is_dof_fixed.tail(rot_ndof).any()
                ? Eigen::MatrixXd::Zero(rot_ndof, rot_ndof)
                : Eigen::MatrixXd(body_A.moment_of_inertia.inverse());
            const Eigen::MatrixXd inv_I_B =
                body_B.is_dof_fixed.tail(rot_ndof).any()
                ? Eigen::MatrixXd::Zero(rot_ndof, rot_ndof)
                : Eigen::MatrixXd(body_B.moment_of_inertia.inverse());

            // Vectors from the center of mass to the collision point
            // (90deg rotation counter clockwise)
            //
            // (1) first get vertices position wrt rigid bodies
            const Eigen::Vector2d r0_A = body_A.vertices.row(r_A_id);
            const Eigen::Vector2d r0_B0 =
                body_B.vertices.row(r_B0_id); // edge vertex 0
            const Eigen::Vector2d r0_B1 =
                body_B.vertices.row(r_B1_id); // edge vertex 1
            const Eigen::Vector2d r0_B = r0_B0 + alpha * (r0_B1 - r0_B0);

            // (2) and the angular displacement at time of collision
            const Pose<double> pose_Atoi =
                Pose<double>::lerp(body_A.pose_prev, body_A.pose, toi);
            const Pose<double> pose_Btoi =
                Pose<double>::lerp(body_B.pose_prev, body_B.pose, toi);
            // (3) then the vectors are given by r = R(\theta_{t})*r_0
            const Eigen::VectorXd r_Aperp_toi =
                pose_Atoi.construct_rotation_matrix_gradient()[0] * r0_A;
            const Eigen::VectorXd r_Bperp_toi =
                pose_Btoi.construct_rotation_matrix_gradient()[0] * r0_B;

            // The collision point velocities BEFORE collision
            const Eigen::VectorXd v_Aprev =
                vel_A_prev.position + vel_A_prev.rotation(0) * r_Aperp_toi;
            const Eigen::VectorXd v_Bprev =
                vel_B_prev.position + vel_B_prev.rotation(0) * r_Bperp_toi;

            // The relative veolicity magnitud BEFORE collision
            const Eigen::VectorXd& n_toi = normals.row(i);

            const double vrel_prev_toi =
                (v_Aprev - v_Bprev).transpose() * n_toi;
            if (vrel_prev_toi >= 0.0) {
                continue;
            }
            // solve for the impulses
            const double nr_A_toi = n_toi.transpose() * r_Aperp_toi;
            const double nr_B_toi = n_toi.transpose() * r_Bperp_toi;
            const double K =
                (inv_m_A + inv_m_B //
                 + inv_I_A(0) * nr_A_toi * nr_A_toi
                 + inv_I_B(0) * nr_B_toi * nr_B_toi);

            const double j =
                -1.0 / K * (1.0 + coefficient_restitution) * vrel_prev_toi;

            // update
            Eigen::Vector2d V_A_delta = inv_m_A * j * n_toi;
            Eigen::Vector2d V_B_delta = -inv_m_B * j * n_toi;
            double w_A_delta = inv_I_A(0) * j * nr_A_toi;
            double w_B_delta = -inv_I_B(0) * j * nr_B_toi;

            if (!(body_A.is_dof_fixed[0] || body_A.is_dof_fixed[1])) {
                body_A.velocity.position = vel_A_prev.position + V_A_delta;
            }

            if (!(body_B.is_dof_fixed[0] || body_B.is_dof_fixed[1])) {
                body_B.velocity.position = vel_B_prev.position + V_B_delta;
            }

            if (!body_A.is_dof_fixed[2]) {
                body_A.velocity.rotation(0) =
                    vel_A_prev.rotation(0) + w_A_delta;
            }

            if (!body_B.is_dof_fixed[2]) {
                body_B.velocity.rotation(0) =
                    vel_B_prev.rotation(0) + w_B_delta;
            }
        }
    }

    Pose<double> RigidBodyProblem::rb_next_pose(
        const RigidBody& rb, const double time_step) const
    {
        // TODO: Use a better time-integrator
        Pose<double> pose = rb.pose;

        // qᵗ⁺¹ = qᵗ + h * q̇ᵗ + h² * g + h² * m⁻¹fₑ
        pose.position += time_step
            * (rb.velocity.position
               + time_step * (gravity_ + rb.force.position / rb.mass));

        // TODO: Fix this once the angular veolicity is fixed
        pose.rotation += time_step
            * (rb.velocity.rotation
               + time_step
                   * rb.moment_of_inertia.lu().solve(rb.force.rotation));

        pose.position =
            (rb.is_dof_fixed.head(pose.pos_ndof()))
                .select(rb.pose.position, pose.position); // reset fixed dof
        pose.rotation =
            (rb.is_dof_fixed.tail(pose.rot_ndof()))
                .select(rb.pose.rotation, pose.rotation); // reset fixed dof
        return pose;
    }

    bool RigidBodyProblem::detect_collisions(
        const Poses<double>& poses_q0,
        const Poses<double>& poses_q1,
        const CollisionCheck check_type) const
    {
        ConcurrentImpacts impacts;

        double scale =
            check_type == CollisionCheck::EXACT ? 1.0 : (1.0 + collision_eps);
        Poses<double> displacements = (poses_q1 - poses_q0) * scale;

        constraint().construct_collision_set(
            m_assembler, poses_q0, displacements, impacts);

        return impacts.size();
    }

    ////////////////////////////////////////////////////////////////////////////
    /// Functional
    ////////////////////////////////////////////////////////////////////////////

    double RigidBodyProblem::eval_f(const Eigen::VectorXd& sigma)
    {
        Eigen::VectorXd diff = sigma - this->poses_to_dofs(poses_t1);
        const Eigen::SparseMatrix<double>& invS = m_assembler.m_dof_to_pose;
        const Eigen::SparseMatrix<double>& M = m_assembler.m_rb_mass_matrix;
        return 0.5 * diff.transpose() * invS.transpose() * M * invS * diff;
    }

    Eigen::VectorXd RigidBodyProblem::eval_grad_f(const Eigen::VectorXd& sigma)
    {
        Eigen::VectorXd grad_f;
        Eigen::VectorXd diff = sigma - this->poses_to_dofs(poses_t1);

        const Eigen::SparseMatrix<double>& invS = m_assembler.m_dof_to_pose;
        const Eigen::SparseMatrix<double>& M = m_assembler.m_rb_mass_matrix;

        grad_f = invS.transpose() * M * invS * diff;

#ifdef WITH_DERIVATIVE_CHECK
        Eigen::VectorXd grad_f_approx = eval_grad_f_approx(*this, sigma);
        if (!fd::compare_gradient(grad_f, grad_f_approx)) {
            spdlog::trace("finite gradient check failed for f");
        }
#endif
        return grad_f;
    }

    Eigen::SparseMatrix<double>
    RigidBodyProblem::eval_hessian_f(const Eigen::VectorXd& sigma)
    {
        const Eigen::SparseMatrix<double>& invS = m_assembler.m_dof_to_pose;
        const Eigen::SparseMatrix<double>& M = m_assembler.m_rb_mass_matrix;

        Eigen::SparseMatrix<double> hessian_f;
        hessian_f = invS.transpose() * M * invS.transpose();

#ifdef WITH_DERIVATIVE_CHECK
        Eigen::MatrixXd hessian_f_approx = eval_hess_f_approx(*this, sigma);
        if (!fd::compare_jacobian(hessian_f, hessian_f_approx)) {
            spdlog::trace("finite hessian check failed for f");
        }
#endif
        return hessian_f;
    }

} // namespace physics
} // namespace ccd
