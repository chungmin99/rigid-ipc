// Time-of-impact computation for rigid bodies with angular trajectories.
#pragma once

#include <constants.hpp>
#include <physics/rigid_body.hpp>

/**
 * @namespace ipc::rigid:
 * @brief
 */
namespace ipc::rigid {

/// Find time-of-impact between two rigid bodies
bool compute_edge_vertex_time_of_impact(
    const RigidBody& bodyA,
    const Pose<double>& poseA_t0, // Pose of bodyA at t=0
    const Pose<double>& poseA_t1, // Pose of bodyA at t=1
    size_t vertex_id,                      // In bodyA
    const RigidBody& bodyB,
    const Pose<double>& poseB_t0, // Pose of bodyB at t=0
    const Pose<double>& poseB_t1, // Pose of bodyB at t=1
    size_t edge_id,                        // In bodyB
    double& toi,
    double earliest_toi = 1, // Only search for collision in [0, earliest_toi]
    double toi_tolerance = Constants::RIGID_CCD_TOI_TOL);

/// Find time-of-impact between two rigid bodies
bool compute_edge_edge_time_of_impact(
    const RigidBody& bodyA,
    const Pose<double>& poseA_t0, // Pose of bodyA at t=0
    const Pose<double>& poseA_t1, // Pose of bodyA at t=1
    size_t edgeA_id,                       // In bodyA
    const RigidBody& bodyB,
    const Pose<double>& poseB_t0, // Pose of bodyB at t=0
    const Pose<double>& poseB_t1, // Pose of bodyB at t=1
    size_t edgeB_id,                       // In bodyB
    double& toi,
    double earliest_toi = 1, // Only search for collision in [0, earliest_toi]
    double toi_tolerance = Constants::RIGID_CCD_TOI_TOL);

/// Find time-of-impact between two rigid bodies
bool compute_face_vertex_time_of_impact(
    const RigidBody& bodyA,
    const Pose<double>& poseA_t0, // Pose of bodyA at t=0
    const Pose<double>& poseA_t1, // Pose of bodyA at t=1
    size_t vertex_id,                      // In bodyA
    const RigidBody& bodyB,
    const Pose<double>& poseB_t0, // Pose of bodyB at t=0
    const Pose<double>& poseB_t1, // Pose of bodyB at t=1
    size_t face_id,                        // In bodyB
    double& toi,
    double earliest_toi = 1, // Only search for collision in [0, earliest_toi]
    double toi_tolerance = Constants::RIGID_CCD_TOI_TOL);

} // namespace ipc::rigid
