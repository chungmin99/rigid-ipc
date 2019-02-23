#ifndef CCD_STATE_HPP
#define CCD_STATE_HPP

#include <Eigen/Dense>
#include <Eigen/Sparse>

#include <ccd/collision_detection.hpp>
#include <ccd/impact.hpp>
#include <ccd/prune_impacts.hpp>

namespace ccd {

/**
 * @brief The State class keeps the full state of the UI and the collisions.
 */
class State {
public:
    static const int kDIM = 2;

    State();
    virtual ~State() = default;

    ///@brief #V,2 vertices positions
    Eigen::MatrixX2d vertices;
    ///@brief #E,2 vertices connnectivity
    Eigen::MatrixX2i edges;
    ///@brief #V,2 vertices displacements
    Eigen::MatrixX2d displacements;

    ///@brief All edge-vertex contact
    EdgeVertexImpacts ev_impacts;

    ///@brief All edge-edge contact
    EdgeEdgeImpacts ee_impacts;

    ///@brief #E,1 contact volume for each edge
    Eigen::VectorXd volumes;

    ///@brief #E,1 impact index for each edge
    Eigen::VectorXi edges_impact;

    ///@brief #2V,NV contact gradient for each volume
    Eigen::MatrixXd volume_grad;

    ///@brief method to use for contact detection
    DetectionMethod detection_method;
    ///@brief epsilon use on volume computation
    double epsilon = 1.0;

    // SCENE CRUD
    // ----------------------------------------------------------------------
    void load_scene(const std::string filename);
    void save_scene(const std::string filename);
    void reset_scene();

    void add_vertex(const Eigen::RowVector2d& vertex);
    void add_edges(const Eigen::MatrixX2i& edges);

    void set_vertex_position(
        const int vertex_idx, const Eigen::RowVector2d& position);
    void move_vertex(const int vertex_idx, const Eigen::RowVector2d& delta);
    void move_displacement(
        const int vertex_idx, const Eigen::RowVector2d& delta);

    void reset_impacts();

    Eigen::MatrixX2d get_volume_grad();
    Eigen::MatrixX2d get_vertex_at_time();

    // SCENE CCD
    // ----------------------------------------------------------------------
    void detect_edge_vertex_collisions();
    void prune_impacts();
    void compute_collision_volumes();
    void run_full_pipeline();

    // UI
    // ----------------------------------------------------------------------
    /// @brief Background rectangle to detect clicks
    double canvas_width, canvas_height;
    /// @brief We show the scene at time=`time` between 0 and 1
    float time;
    /// @brief Current user-selection of vertex and displacement points
    std::vector<int> selected_points, selected_displacements;

    /// @brief Use for any functionallity that requires showing only one ev
    /// impact
    int current_ev_impact;

    /// @brief Use for any functionallity that requires showing only one ee
    /// impact
    int current_ee_impact;

    double min_edge_width;
};

}
#endif
