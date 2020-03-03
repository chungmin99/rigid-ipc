#pragma once

#include <Eigen/Core>

#include <utils/eigen_ext.hpp>

/// NOTE: Naming Convention
/// Point: Either a 2D or 3D point in space.
/// Segment: A line segment in either 2D or 3D defined by its endpoints.
/// Triangle: A triangle in 3D

namespace ccd {
namespace geometry {

    //-------------------------------------------------------------------------
    // Unsigned Distances
    //-------------------------------------------------------------------------

    template <typename T>
    inline T point_segment_distance_2D(
        const Eigen::Vector2<T>& point,
        const Eigen::Vector2<T>& segment_start,
        const Eigen::Vector2<T>& segment_end);

    template <typename T>
    inline T point_segment_distance_3D(
        const Eigen::Vector3<T>& point,
        const Eigen::Vector3<T>& segment_start,
        const Eigen::Vector3<T>& segment_end);

    template <typename T>
    inline T point_segment_distance(
        const Eigen::VectorX3<T>& point,
        const Eigen::VectorX3<T>& segment_start,
        const Eigen::VectorX3<T>& segment_end)
    {
        if (point.size() == 2) {
            return point_segment_distance_2D(
                point.template head<2>().eval(),
                segment_start.template head<2>().eval(),
                segment_end.template head<2>().eval());
        } else {
            return point_segment_distance_3D(
                point.template head<3>().eval(),
                segment_start.template head<3>().eval(),
                segment_end.template head<3>().eval());
        }
    }

    template <typename T>
    inline T segment_segment_distance(
        const Eigen::Vector3<T>& segment0_start,
        const Eigen::Vector3<T>& segment0_end,
        const Eigen::Vector3<T>& segment1_start,
        const Eigen::Vector3<T>& segment1_end);

    template <typename T>
    inline T point_triangle_distance(
        const Eigen::Vector3<T>& point,
        const Eigen::Vector3<T>& triangle_vertex0,
        const Eigen::Vector3<T>& triangle_vertex1,
        const Eigen::Vector3<T>& triangle_vertex2);

    //-------------------------------------------------------------------------
    // Signed Distances (useful for CCD)
    //-------------------------------------------------------------------------

    /// Compute the signed distance between a point and a line
    /// WARNING: Produces the same sign as euclidean distance but may be
    /// different scales.
    template <typename T>
    inline T point_line_signed_distance(
        const Eigen::Vector2<T>& point,
        const Eigen::Vector2<T>& line_point1,
        const Eigen::Vector2<T>& line_point0);

    /// Compute the signed distance between two lines
    /// WARNING: Produces the same sign as euclidean distance but may be
    /// different scales.
    /// WARNING: Parallel edges results in zero distance
    template <typename T>
    inline T line_line_signed_distance(
        const Eigen::Vector3<T>& line0_point0,
        const Eigen::Vector3<T>& line0_point1,
        const Eigen::Vector3<T>& line1_point0,
        const Eigen::Vector3<T>& line1_point1);

    /// Compute the distance between a point and a plane.
    /// Normal is assumed to be unit length.
    /// WARNING: Produces the same sign as euclidean distance but may be
    /// different scales.
    template <typename T>
    inline T point_plane_signed_distance(
        const Eigen::Vector3<T>& point,
        const Eigen::Vector3<T>& triangle_vertex0,
        const Eigen::Vector3<T>& triangle_vertex1,
        const Eigen::Vector3<T>& triangle_vertex2);

} // namespace geometry
} // namespace ccd

#include "distance.tpp"
