#pragma once
#include <array>
#include <cmath>
#include "odb_types.hpp"

namespace odb {

using arrayd3 = std::array<double, 3>;

enum class BoundaryCondition{
    Null = 0,
    USERDEFINED = 1,
    DIRICHLET = 5,
    NEUMANN = 12,
    WALL = 20
};

class SurfaceBoundaryCondition{
public:
    BoundaryCondition bctype = BoundaryCondition::Null;
    double delta = 0.0; 
};

class LineMesh{
public:
    std::vector<arrayd3> coord;
    std::vector<double> para_coord;
    std::vector<std::array<int, 2>> segments;
    std::vector<int> regions;
    std::vector<arrayd3> point_normal;
    SurfaceBoundaryCondition boundary_condition;
};

class OdbToLineMeshConfig{
public:
    double arc_step_degree = 5.0;
    const OdbSymbol* custom_symbols = nullptr;
    bool include_signal = true;
    bool include_power_ground = true;
    bool include_drill = true;
    bool include_dielectric = true;
    bool include_document = false;
    bool include_component = false;
    bool include_mask = false;
    bool include_solder_paste = false;
    bool include_solder_mask = false;
    bool include_rout = false;
};

class LayerLineMesh{
public:
    std::string layer_name;
    std::string layer_type;
    int layer_row = -1;
    int layer_id = -1;
    LineMesh mesh;
    LineMesh profile;
};

class OdbLineMesh{
public:
    std::string step_name;
    LineMesh board_outline;
    std::vector<LayerLineMesh> layers;
};


namespace detail {

constexpr double PI = 3.14159265358979323846;
constexpr double MIL_TO_INCH = 1.0 / 1000.0;


inline bool need_process_layer(const std::string& type, const OdbToLineMeshConfig& config){
    if(type == "SIGNAL" && config.include_signal){
        return true;
    }
    if(type == "POWER_GROUND" && config.include_power_ground){
        return true;
    }
    if(type == "DRILL" && config.include_drill){
        return true;
    }
    if(type == "DIELECTRIC" && config.include_dielectric){
        return true;
    }
    if(type == "DOCUMENT" && config.include_document){
        return true;
    }
    if(type == "COMPONENT" && config.include_component){
        return true;
    }
    if(type == "MASK" && config.include_mask){
        return true;
    }
    if(type == "SOLDER_PASTE" && config.include_solder_paste){
        return true;
    }
    if(type == "SOLDER_MASK" && config.include_solder_mask){
        return true;
    }
    if(type == "ROUT" && config.include_rout){
        return true;
    }

    return false;
}

inline SurfaceBoundaryCondition default_bc_for_layer(const std::string& type){
    SurfaceBoundaryCondition bc;
    if(type == "SIGNAL"){
        bc.bctype = BoundaryCondition::NEUMANN;
    }
    else if(type == "POWER_GROUND"){
        bc.bctype = BoundaryCondition::DIRICHLET;
    }
    else if(type == "DRILL"){
        bc.bctype = BoundaryCondition::WALL;
    }
    else{
        bc.bctype = BoundaryCondition::Null;
    }
    return bc;
}

inline int add_point(LineMesh& mesh, double x, double y, double z, int region){
    mesh.coord.push_back({x, y, z});
    // mesh.point_normal.push_back({0.0, 0.0, 1.0});
    mesh.regions.push_back(region);    
    return static_cast<int>(mesh.coord.size()) - 1;
}

inline void add_segment(LineMesh& mesh, int i, int j){
    mesh.segments.push_back({i, j});
}

inline std::vector<arrayd3> discretize_arc(double x_start, double y_start,
    double x_end, double y_end,
    double x_center, double y_center,
    ArcDirection direction,
    bool is_full_circle,
    double arc_step_degree = 5.0)
{
    std::vector<arrayd3> points;
    double radius = std::sqrt((x_end - x_center) * (x_end - x_center) + (y_end - y_center) * (y_end - y_center));
    if(radius < 1e-12){
        points.push_back({x_start, y_start, 0.0});
        return points;
    }

    double arc_step_radian = arc_step_degree * PI / 180.0;

    if(is_full_circle){
        double start_angle = std::atan2(y_end - y_center, x_end - x_center);
        double total_angle = 2 * PI;
        // 对于完整圆，至少保证三个点且闭环
        int num_steps = std::max(3, static_cast<int>(total_angle / arc_step_degree));

        for(int k = 0; k < num_steps; ++k){
            double angle = start_angle + k * total_angle / num_steps;
            points.push_back({x_center + radius * std::cos(angle), y_center + radius * std::sin(angle), 0.0});
        }
    }else{
        double start_angle = std::atan2(y_start - y_center, x_start - x_center);
        double end_angle = std::atan2(y_end -y_center, x_end - x_center);

        if(direction == ArcDirection::CLOCKWISE ){
            if(end_angle > start_angle)
                end_angle -= 2.0 * PI;
        }else{
            if(end_angle < start_angle)
                end_angle += 2.0 * PI;
        }

        double total_angle = end_angle - start_angle;
        // 对于非完整圆，至少保证两个点（起点和终点）
        int num_steps = std::max(1, static_cast<int>(std::abs(total_angle) / arc_step_radian));
        for(int k = 0; k<= num_steps; ++k){
            double angle = start_angle + k * total_angle / num_steps;
            points.push_back({x_center + radius * std::cos(angle), y_center + radius * std::sin(angle), 0.0});
        }
    }

    if(points.size() >= 2){
        points.front() = {x_start, y_start, 0.0};
        points.back() = {x_end, y_end, 0.0};
    }
    
    return points;
}

inline std::vector<arrayd3> expand_pad(
    const std::string& symbol,
    double x_center, double y_center,
    double arc_step_degree = 5.0
){
    std::vector<arrayd3> points;
    const double step_radian = arc_step_degree * PI / 180.0;

    std::string sym = symbol;
    // r100 I
    if(sym.size() > 2 && sym.back() == 'I' && sym[sym.size() - 2] == ' '){
        sym = sym.substr(0, sym.size() - 2);
    }

    // ── 圆形：r<d>
    if(!sym.empty() && sym[0] == 'r' && sym.find('x') == std::string::npos){
        double diameter;
        try{
            diameter = std::stod(sym.substr(1));
        }catch(...){ goto unknown; }
        double radius = diameter * MIL_TO_INCH / 2.0;
        int num_steps = std::max(3, static_cast<int>(2.0 * PI / step_radian));
        for(int k = 0; k < num_steps; ++k){
            double angle = k * 2.0 * PI / num_steps;
            points.push_back({x_center + radius * std::cos(angle), y_center + radius * std::sin(angle), 0.0});
        }
        return points;
    }

    // ── 正方形：s<d>
    if(!sym.empty() && sym[0] == 's' && sym.find('x') == std::string::npos){
        double side;
        try{
            side = std::stod(sym.substr(1));
        }catch(...){ goto unknown; }
        double half_side = side * MIL_TO_INCH / 2.0;
        points.push_back({x_center - half_side, y_center - half_side, 0.0});
        points.push_back({x_center + half_side, y_center - half_side, 0.0});
        points.push_back({x_center + half_side, y_center + half_side, 0.0});
        points.push_back({x_center - half_side, y_center + half_side, 0.0});
        return points;
    }

    // ── 圆角矩形：rect<w>x<h>xr<r> / 普通矩形：rect<w>x<h>
    if(sym.size() > 4 && sym.substr(0, 4) == "rect"){
        size_t pos1 = sym.find('x', 4);
        if(pos1 ==std::string::npos){
            goto unknown;
        }
        // 宽高的长度包括圆角的半径长度
        double width, height, radius;
        try{
            width = std::stod(sym.substr(4, pos1 - 4)) * MIL_TO_INCH;
        }catch(...){ goto unknown; }

        size_t pos2 = sym.find('x', pos1 + 1);
        try{
            height = std::stod(sym.substr(pos1 + 1, 
            pos2 == std::string::npos ? std::string::npos : pos2 - pos1 - 1)) * MIL_TO_INCH;

        }catch(...){ goto unknown; }

        double half_width = width / 2.0;
        double half_height = height / 2.0;

        if(pos2 != std::string::npos && pos2 + 1 < sym.size() && sym[pos2 + 1] == 'r'){
            try{
                radius = std::stod(sym.substr(pos2 + 2)) * MIL_TO_INCH;
            }catch(...){ goto unknown; }

            radius = std::min(radius, std::min(half_width, half_height));

            // 四段直线 + 四个圆角弧，逆时针绘制,  底边左→右
            // points.push_back({x_center - half_width + radius, y_center - half_height, 0.0});
            // points.push_back({x_center + half_width - radius, y_center - half_height, 0.0});

            int num_steps = std::max(1,static_cast<int>(PI /2.0 / step_radian));
            for(int k = 0; k <= num_steps; ++k){
                double angle = -PI / 2.0 + k * PI / 2.0 / num_steps;
                points.push_back({x_center + half_width - radius + radius * std::cos(angle),
                    y_center - half_height + radius + radius * std::sin(angle), 0.0});
            }

            // points.push_back({x_center + half_width, y_center + half_height - radius, 0.0});
            for(int k = 0; k <= num_steps; ++k){
                double angle = 0.0 + k * PI / 2.0 / num_steps;
                points.push_back({x_center + half_width - radius + radius * std::cos(angle),
                    y_center + half_height - radius + radius * std::sin(angle), 0.0});
            }

            // points.push_back({x_center - half_width + radius, y_center + half_height, 0.0});
            for(int k = 0; k <= num_steps; ++k){
                double angle = PI / 2.0 + k * PI / 2.0 / num_steps;
                points.push_back({x_center - half_width + radius + radius * std::cos(angle),
                    y_center + half_height - radius + radius * std::sin(angle), 0.0});
            }

            // points.push_back({x_center - half_width, y_center - half_height + radius, 0.0});
            for(int k = 0; k <= num_steps; ++k){
                double angle = PI + k * PI / 2.0 / num_steps;
                points.push_back({x_center - half_width + radius + radius * std::cos(angle),
                    y_center - half_height + radius + radius * std::sin(angle), 0.0});
            }
            return points;
        }
        points.push_back({x_center - half_width, y_center - half_height, 0.0});
        points.push_back({x_center + half_width, y_center - half_height, 0.0});
        points.push_back({x_center + half_width, y_center + half_height, 0.0});
        points.push_back({x_center - half_width, y_center + half_height, 0.0});
        return points;
    } 

    // ── 跑道形：oval<w>x<h>
    if(sym.size() > 4 && sym.substr(0, 4) == "oval"){
        size_t pos = sym.find('x', 4);
        if(pos != std::string::npos){
            double width, height;
            try{
                width = std::stod(sym.substr(4, pos - 4)) * MIL_TO_INCH;
                height = std::stod(sym.substr(pos + 1)) * MIL_TO_INCH;
            }catch(...){ goto unknown; }
            // 宽的长度包括半圆的半径长度
            double radius = height / 2.0;
            double middle_width = width - height;
            int num_steps = static_cast<int>(PI / step_radian);
            // 右端半圆，逆时针
            for(int k = 0; k <= num_steps; ++k){
                double angle = -PI / 2.0 + k * PI / num_steps;
                points.push_back({x_center + middle_width + radius * std::cos(angle), y_center + radius * std::sin(angle), 0.0});
            }

            for(int k = 0; k <= num_steps; ++k){
                double angle = PI / 2.0 + k * PI / num_steps;
                points.push_back({x_center - middle_width + radius * std::cos(angle), y_center + radius * std::sin(angle), 0.0});
            }
            return points;
        }
    }
unknown:
    points.push_back({x_center, y_center, 0.0});
    return points;
}

inline void feature_to_linemesh(
    const Feature& feature,
    const FeatureHeader& header,
    LineMesh& mesh,
    int region_id,
    double arc_step_degree,
    const OdbSymbol* custom_symbols = nullptr
){
    std::visit([&](auto&& f){
        using T = std::decay_t<decltype(f)>;
        if constexpr (std::is_same_v<T, FeatureLine>){
            int i = add_point(mesh, f.x_start, f.y_start,0.0, region_id);
            int j = add_point(mesh, f.x_end, f.y_end, 0.0, region_id);
            add_segment(mesh, i , j);
        }
        else if constexpr (std::is_same_v<T, FeatureArc>){
            auto points = discretize_arc(f.x_start, f.y_start, f.x_end, f.y_end, f.x_center, f.y_center,
            f.direction, f.is_full_circle(), arc_step_degree);

            if(points.size() < 2){
                return;
            }

            int first = add_point(mesh, points[0][0], points[0][1], 0.0, region_id);
            int prev = first;
            for(size_t k = 1; k < points.size(); ++k){
                int cur = add_point(mesh, points[k][0], points[k][1], 0.0, region_id);
                add_segment(mesh, prev, cur);
                prev = cur;
            }
            if(f.is_full_circle()){
                add_segment(mesh, prev, first);
            }
        }
        else if constexpr (std::is_same_v<T, FeaturePad>){
            std::string sym_name;
            auto it = header.symbols.find(f.symbol_index);
            if(it != header.symbols.end()){
                sym_name = it -> second;     
            }

            // ── 圆环：donut_r<outer>x<inner>
            if(sym_name.size() > 7 && sym_name.substr(0, 7) == "donut_r"){
                size_t pos = sym_name.find('x', 7);
                if(pos != std::string::npos){
                    double outer_radius, inner_radius;
                    try{
                        outer_radius = std::stod(sym_name.substr(7, pos - 7)) * MIL_TO_INCH / 2.0;
                        inner_radius = std::stod(sym_name.substr(pos + 1)) * MIL_TO_INCH / 2.0;
                    }catch(...){
                        add_point(mesh, f.x, f.y, 0.0, region_id);
                        return;
                    }

                    double step_radian = arc_step_degree * PI /180.0;
                    int num_steps = std::max(3, static_cast<int>(2.0 * PI / step_radian));

                    // 外圆：逆时针
                    auto add_circle = [&](double x_center, double y_center, double radius){
                        int first = add_point(mesh, x_center + radius * std::cos(0.0), y_center + radius * std::sin(0.0), 0.0, region_id);
                        int prev = first;
                        for(int k = 1; k < num_steps; ++k){
                            double angle = k * 2.0 * PI / num_steps;
                            int cur = add_point(mesh, x_center + radius * std::cos(angle), y_center + radius * std::sin(angle), 0.0, region_id);
                            add_segment(mesh, prev, cur);
                            prev = cur;
                        }
                        add_segment(mesh, prev, first);
                    };
                    // 内圆：顺时针
                    auto add_circle_cw = [&](double x_center, double y_center, double radius){
                        int first = add_point(mesh, x_center + radius * std::cos(0.0), y_center + radius * std::sin(0.0), 0.0, region_id);
                        int prev = first;
                        for(int k = 1; k < num_steps; ++k){
                            double angle = -k * 2.0 * PI / num_steps;
                            int cur = add_point(mesh, x_center + radius * std::cos(angle), y_center + radius * std::sin(angle), 0.0, region_id);
                            add_segment(mesh, prev, cur);
                            prev = cur;
                        }
                        add_segment(mesh, prev, first);
                    };

                    add_circle(f.x, f.y, outer_radius);
                    add_circle_cw(f.x, f.y, inner_radius);
                }
            }

            if(custom_symbols){
                std::string sym = sym_name;
                if(sym.size() > 2 && sym.back() == 'I' && sym[sym.size() - 2] == ' '){
                    sym = sym.substr(0, sym.size() - 2);
                }
                
                auto parse_custom = [&](const std::string& symbol){
                    auto it = custom_symbols -> find(symbol);
                    if(it == custom_symbols -> end()){
                        return false;
                    }

                    bool found = false;

                    for(const auto& feat : it -> second.features){
                        std::visit([&](auto&& sf){
                            using ST = std::decay_t<decltype(sf)>;
                            if constexpr (std::is_same_v<ST, FeatureSurface>){
                                for(const auto& contour : sf.contours){
                                    if(contour.points.empty()){
                                        continue;
                                    }

                                    int first = add_point(mesh, f.x + contour.x_start, f.y + contour.y_start, 0.0, region_id);
                                    int prev = first;
                                    for(const auto& point : contour.points){
                                        std::visit([&](auto&& p){
                                            using PT = std::decay_t<decltype(p)>;
                                            if constexpr (std::is_same_v<PT, ContourLinePoint>){
                                                int cur = add_point(mesh, f.x + p.x, f.y + p.y, 0.0, region_id);
                                                add_segment(mesh, prev, cur);
                                                prev = cur;
                                            }else if constexpr (std::is_same_v<PT, ContourArcPoint>){
                                                double x_start = mesh.coord[prev][0];
                                                double y_start = mesh.coord[prev][1];
                                                auto arc_points = discretize_arc(x_start, y_start, f.x + p.x_end, f.y + p.y_end, f.x + p.x_center, f.y + p.y_center, p.direction, false, arc_step_degree);
                                                for(size_t k = 1; k < arc_points.size(); ++k){
                                                    int cur = add_point(mesh, arc_points[k][0], arc_points[k][1], 0.0, region_id);
                                                    add_segment(mesh, prev, cur);
                                                    prev = cur;
                                                }
                                            }
                                        }, point);
                                    }

                                    add_segment(mesh, prev, first);
                                    found = true;
                                }
                            }
                        }, feat);
                    }
                    return found;
                };
                if(parse_custom(sym)){
                    return;
                }
            }

            auto points = expand_pad(sym_name, f.x, f.y, arc_step_degree);
            if(points.size() < 2){
                add_point(mesh, f.x, f.y, 0.0, region_id);
                return;
            }

            int first = add_point(mesh, points[0][0], points[0][1], 0.0, region_id);
            int prev = first;
            for(size_t k = 1; k < points.size(); ++k){
                int cur = add_point(mesh, points[k][0], points[k][1], 0.0, region_id);
                add_segment(mesh, prev, cur);
                prev = cur;
            }
            add_segment(mesh, prev, first);
        }
        else if constexpr (std::is_same_v<T, FeatureSurface>){
            for(const auto& contour : f.contours){
                if(contour.points.empty()){
                    continue;
                }

                int first = add_point(mesh, contour.x_start, contour.y_start, 0.0, region_id);
                int prev = first;
                for(const auto& point : contour.points){
                    std::visit([&](auto&& pt){
                        using PT = std::decay_t<decltype(pt)>;
                        if constexpr (std::is_same_v<PT, ContourLinePoint>){
                            int cur = add_point(mesh, pt.x, pt.y, 0.0, region_id);
                            add_segment(mesh, prev, cur);
                            prev = cur;
                        }
                        else if constexpr (std::is_same_v<PT, ContourArcPoint>){
                            double x_start = mesh.coord[prev][0];
                            double y_start = mesh.coord[prev][1];
                            auto arc_points = discretize_arc(x_start, y_start, pt.x_end, pt.y_end, pt.x_center, pt.y_center, pt.direction, false, arc_step_degree);

                            for(size_t k = 1; k < arc_points.size(); ++k){
                                int cur = add_point(mesh, arc_points[k][0], arc_points[k][1], 0.0, region_id);
                                add_segment(mesh, prev, cur);
                                prev = cur;
                            }
                        }
                    }, point);
                }
                add_segment(mesh, prev, first);

            }
        }
        else if constexpr (std::is_same_v<T, FeatureText>){
            // TODO
        }

    }, feature);
}

inline void profile_to_linemesh(
    const ProfileData& profile,
    LineMesh& mesh,
    int region_id,
    double arc_step_degree
){
    for(const auto& feature : profile.features){
        feature_to_linemesh(feature, {}, mesh, region_id, arc_step_degree, nullptr);
    }
    mesh.boundary_condition.bctype = BoundaryCondition::WALL;
    mesh.boundary_condition.delta = 0.0;
}


inline OdbLineMesh odb_to_linemesh(
    const OdbLayer& step_layers,
    const OdbMatrix& matrix,
    const OdbToLineMeshConfig& config = {},
    const OdbSymbol* custom_symbols = nullptr
){
    OdbLineMesh step_linemesh;
    step_linemesh.step_name = step_layers.step_name;

    detail::profile_to_linemesh(step_layers.step_profile, step_linemesh.board_outline, 0, config.arc_step_degree);

    auto to_lower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c));});
        return s;
    };

    std::map<std::string, const MatrixLayer*> matrix_layer_map;
    for(const auto& layer : matrix.layers){
        matrix_layer_map[to_lower(layer.name)] = &layer;
    }

    for(const auto& layer : step_layers.layers){
        auto it = matrix_layer_map.find(to_lower(layer.layer_name));
        if(it == matrix_layer_map.end()){
            continue;
        }

        const MatrixLayer& matrixlayer = *(it -> second);
        if(!detail::need_process_layer(matrixlayer.type, config)){
            continue;
        }

        LayerLineMesh layer_mesh;
        layer_mesh.layer_name = layer.layer_name;
        layer_mesh.layer_type = matrixlayer.type;
        layer_mesh.layer_row = matrixlayer.row;
        layer_mesh.layer_id = matrixlayer.id;
        layer_mesh.mesh.boundary_condition = detail::default_bc_for_layer(matrixlayer.type);
        
        for(const auto& feature : layer.features){
            detail::feature_to_linemesh(feature, layer.header, layer_mesh.mesh, matrixlayer.id, config.arc_step_degree, custom_symbols);
        }

        detail::profile_to_linemesh(layer.layer_profile, layer_mesh.profile, matrixlayer.id, config.arc_step_degree);
        step_linemesh.layers.push_back(std::move(layer_mesh));
    }    
    return step_linemesh;
}



}


}