#include "cslam/back_end/utils/logger.h"

namespace cslam
{

    Logger::Logger(std::shared_ptr<rclcpp::Node> &node, const unsigned int &robot_id, const unsigned int &nb_robots, const std::string &log_folder) : robot_id_(robot_id)
    {
        node_ = node;
        auto t = std::time(nullptr);
        auto tm = std::localtime(&t);
        char time_str[100];
        std::strftime(time_str, sizeof(time_str), "%d-%m-%Y_%H-%M-%S", tm);
        std::string experiment_id = std::string(time_str) + "_experiment_robot_" + std::to_string(robot_id);
        log_folder_ = log_folder + "/" + experiment_id;
        system(("mkdir -p " + log_folder_).c_str());
        total_pgo_time_ = 0;
        nb_robots_ = nb_robots;
    }

    void Logger::add_pose_graph_log_info(const cslam_common_interfaces::msg::PoseGraph &msg)
    {
        pose_graphs_log_info_.push_back(msg);
    }

    void Logger::log_initial_global_pose_graph(const gtsam::NonlinearFactorGraph::shared_ptr &graph,
                                               const gtsam::Values::shared_ptr &initial)
    {
        initial_global_pose_graph_ = std::make_pair(graph, initial);
    }

    void Logger::log_optimized_global_pose_graph(const gtsam::NonlinearFactorGraph::shared_ptr &graph,
                                                 const gtsam::Values &result,
                                                 const unsigned int &origin_robot_id)
    {
        auto values = gtsam::Values::shared_ptr(new gtsam::Values(result));
        optimized_global_pose_graph_ = std::make_pair(graph, values);
        origin_robot_id_ = origin_robot_id;
    }

    void Logger::start_timer()
    {
        start_time_ = std::chrono::steady_clock::now();
    }

    void Logger::stop_timer()
    {
        auto end_time = std::chrono::steady_clock::now();
        elapsed_time_ = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time_).count();
        total_pgo_time_ += elapsed_time_;
    }

    void Logger::write_logs()
    {
        // Create folder
        auto t = std::time(nullptr);
        auto tm = std::localtime(&t);
        char time_str[100];
        std::strftime(time_str, sizeof(time_str), "%d-%m-%Y_%H-%M-%S", tm);
        std::string timestamp = std::string(time_str);
        std::string result_folder = log_folder_ + "/" + timestamp;
        system(("mkdir -p " + result_folder).c_str());

        // Write pgo logs (.g2o)
        try
        {
            if (initial_global_pose_graph_.second->size() > 0)
            {
                gtsam::writeG2o(*initial_global_pose_graph_.first, *initial_global_pose_graph_.second, result_folder + "/initial_global_pose_graph.g2o");
            }
            if (optimized_global_pose_graph_.second->size() > 0)
            {
                gtsam::writeG2o(*optimized_global_pose_graph_.first, *optimized_global_pose_graph_.second, result_folder + "/optimized_global_pose_graph.g2o");
            }
        }
        catch (std::exception &e)
        {
            RCLCPP_ERROR(node_->get_logger(), "Logging: Error while writing g2o files: %s", e.what());
        }

        // Write other logs (.csv)
        std::ofstream optimization_log_file;
        optimization_log_file.open(result_folder + "/log.csv");
        optimization_log_file << "robot_id," << std::to_string(robot_id_) << std::endl;
        optimization_log_file << "origin_robot_id," << std::to_string(origin_robot_id_) << std::endl;
        optimization_log_file << "nb_robots," << std::to_string(nb_robots_) << std::endl;
        unsigned int total_nb_matches = 0;
        unsigned int total_nb_failed_matches = 0;
        unsigned int total_nb_vertices_transmitted = 0;
        unsigned int total_front_end_cumulative_communication_bytes = 0;
        float total_sparsification_cumulative_computation_time = 0.0;
        for (const auto &info : pose_graphs_log_info_)
        {
            total_nb_matches += info.nb_matches;
            total_nb_failed_matches += info.nb_failed_matches;
            total_nb_vertices_transmitted += info.nb_vertices_transmitted;
            total_front_end_cumulative_communication_bytes += info.front_end_cumulative_communication_bytes;
            total_sparsification_cumulative_computation_time += info.sparsification_cumulative_computation_time;
        }
        optimization_log_file << "total_nb_matches," << std::to_string(total_nb_matches) << std::endl;
        optimization_log_file << "total_nb_failed_matches," << std::to_string(total_nb_failed_matches) << std::endl;
        optimization_log_file << "total_nb_vertices_transmitted," << std::to_string(total_nb_vertices_transmitted) << std::endl;
        optimization_log_file << "total_front_end_cumulative_communication_bytes," << std::to_string(total_front_end_cumulative_communication_bytes) << std::endl;
        optimization_log_file << "total_sparsification_cumulative_computation_time," << std::to_string(total_sparsification_cumulative_computation_time) << std::endl;
        optimization_log_file << "latest_pgo_time," << std::to_string(elapsed_time_) << std::endl;
        optimization_log_file << "total_pgo_time," << std::to_string(total_pgo_time_) << std::endl;

        optimization_log_file << "nb_edges," << std::to_string(optimized_global_pose_graph_.first->size()) << std::endl;
        optimization_log_file << "nb_vertices," << std::to_string(optimized_global_pose_graph_.second->size()) << std::endl;
        float total_error = compute_error(optimized_global_pose_graph_.first,
                                          optimized_global_pose_graph_.second);
        optimization_log_file << "total_error," << std::to_string(total_error) << std::endl;
        auto loop_closure_errors = compute_inter_robot_loop_closure_errors(optimized_global_pose_graph_.first,
                                                                           optimized_global_pose_graph_.second);
        optimization_log_file << "inter_robot_loop_closures," << std::to_string(loop_closure_errors.size()) << std::endl;
        for (const auto &error : loop_closure_errors)
        {
            optimization_log_file << "error"
                                     << "," << std::to_string(error.second) << std::endl;
        }

        optimization_log_file.close();

        // Write gps logs (.csv)
        for (const auto &info : pose_graphs_log_info_)
        {
            std::ofstream gps_log_file;
            gps_log_file.open(result_folder + "/gps_robot_" + std::to_string(info.robot_id) + ".csv");
            gps_log_file << "vertice_id,latitude,longitude,altitude" << std::endl;

            for (unsigned int i = 0; i < info.gps_values_idx.size(); i++)
            {
                gps_log_file << std::fixed << std::setprecision(10)
                             << std::to_string(info.gps_values_idx[i]) << ","
                             << std::to_string(info.gps_values[i].latitude) << ","
                             << std::to_string(info.gps_values[i].longitude) << ","
                             << std::to_string(info.gps_values[i].altitude) << std::endl;
            }
            
            gps_log_file.close();
        }

        // Clear logs
        pose_graphs_log_info_.clear();
        initial_global_pose_graph_.first.reset();
        initial_global_pose_graph_.second.reset();
        optimized_global_pose_graph_.first.reset();
        optimized_global_pose_graph_.second.reset();
        gps_values_.clear();
    }

    std::vector<std::pair<std::pair<gtsam::LabeledSymbol, gtsam::LabeledSymbol>, double>> Logger::compute_inter_robot_loop_closure_errors(const gtsam::NonlinearFactorGraph::shared_ptr &graph,
                                                                                                                                          const gtsam::Values::shared_ptr &result)
    {
        std::vector<std::pair<std::pair<gtsam::LabeledSymbol, gtsam::LabeledSymbol>, double>> loop_closure_errors;
        try
        {
            if (result->size() > 0){
                for (const auto &factor_ : *graph)
                {
                    auto factor =
                        boost::dynamic_pointer_cast<gtsam::BetweenFactor<gtsam::Pose3>>(
                            factor_);
                    if (factor)
                    {
                        auto key1 = gtsam::LabeledSymbol(factor->key1());
                        auto key2 = gtsam::LabeledSymbol(factor->key2());
                        auto robot_id1 = key1.label();
                        auto robot_id2 = key2.label();
                        if (robot_id1 != robot_id2)
                        {
                            if (result->exists(key1) && result->exists(key2))
                            {
                                auto error = factor->error(*result);
                                loop_closure_errors.push_back(std::make_pair(std::make_pair(key1, key2), error));
                            }
                        }
                    }
                }
            }            
        }
        catch (std::exception &e)
        { 
            RCLCPP_ERROR(node_->get_logger(), "Logging: Error while computing inter-robot loop closure errors connectivity: %s", e.what());
        }
        return loop_closure_errors;
    }

    double Logger::compute_error(const gtsam::NonlinearFactorGraph::shared_ptr &graph,
                                 const gtsam::Values::shared_ptr &result)
    {
        double error = 0.0;
        try
        {
            error = graph->error(*result);
        }
        catch (std::exception &e)
        { 
            RCLCPP_ERROR(node_->get_logger(), "Logging: Error while computing graph error: %s", e.what());
        }
        return error;
    }

} // namespace cslam