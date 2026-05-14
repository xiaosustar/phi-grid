// #include <algorithm>
#include <iostream>
#include <sstream>
// #include <vector>
#include <iomanip>
#include <type_traits>
#include "odb_types.hpp"
#include "odb_parser.hpp"
#include "odb_to_linemesh.hpp"

void print_matrix(const odb::OdbMatrix& matrix);
void print_netlist(const odb::OdbNetlist& netlist);
void print_eda(const odb::OdbEda& eda);
void print_features(const std::vector<odb::Feature>& features);
void print_symbols(const odb::OdbSymbol& symbols);
void print_linemesh(const std::vector<odb::OdbLineMesh>& linemeshes);

int main(int argc, char **argv){
    if(argc < 2){
        std::cerr << "Usage: " << "<odb_root_dir> [step_name] [layer1,layer2,...]\n" << std::endl;
        return 1;
    }

    std::string odb_root_dir = argv[1];
    std::string step_name_filter = (argc > 2) ? argv[2] :  "";

    std::vector<std::string> layer_filter;
    if(argc > 3){    
        std::istringstream ss(argv[3]);
        std::string layer;
        while(std::getline(ss, layer, ',')){
            if(!layer.empty()){
                layer_filter.push_back(layer);
            }
        }
    }

    try{
        // 1. Parse Matrix
        auto matrix = odb::parse_matrix_from_dir(odb_root_dir);
        print_matrix(matrix);

        // 2. Parse Netlist
        auto netlists_map = odb::parse_netlist_from_dir(odb_root_dir, step_name_filter);
        std::cout << "\nParsed " << netlists_map.size() << " netlist(s) from ODB++ project.\n";
        for(const auto& [netlist_name, netlist] : netlists_map){
            print_netlist(netlist);
        }

        // 3. Parse EDA
        auto edas_map = odb::parse_eda_from_dir(odb_root_dir, step_name_filter);
        std::cout << "\nParsed " << edas_map.size() << " EDA file(s) from ODB++ project.\n";
        for(const auto& [eda_name, eda] : edas_map){
            print_eda(eda);
        }
        
        // 4. Parse Layers/Features
        auto layers_map = odb::parse_layer_from_dir(odb_root_dir, step_name_filter, layer_filter);
        std::cout << "\nParsed " << layers_map.size() << " layer step(s) from ODB++ project.\n";
       
        std::vector<odb::Feature> all_features;
        for(const auto& [step_name, layer_data] : layers_map){
            std::cout << "  Step: " << step_name << " (" << layer_data.layers.size() << " layers)\n";
            for (const auto& layer : layer_data.layers) {
                all_features.insert(all_features.end(), layer.features.begin(), layer.features.end());
            }
        }
        print_features(all_features);

        // 5. Parse Symbols
        auto symbol = odb::parse_symbol_from_dir(odb_root_dir);
        std::cout << "\nParsed " << symbol.size() << " symbols from ODB++ project.\n";
        print_symbols(symbol);

        // 6. Convert to LineMesh
        odb::OdbToLineMeshConfig mesh_config;
        mesh_config.arc_step_degree = 1.0;
        mesh_config.custom_symbols = &symbol;
        std::vector<odb::OdbLineMesh> linemeshes;
        for(const auto& [step_name, layer] : layers_map){
            auto linemesh = odb::detail::odb_to_linemesh(layer, matrix, mesh_config, &symbol);
            linemeshes.push_back(linemesh);
        }
        print_linemesh(linemeshes);
        
        
        
    }catch(const std::exception& e){
        std::cerr << "Errorr: " << e.what()  << std::endl;
    }
    return 0;
}


// ═════════════════════════════════════════════════════════════════
// Matrix print
// ═════════════════════════════════════════════════════════════════

void print_matrix(const odb::OdbMatrix& matrix) {
    std::cout << "\n============================= Matrix Summary =============================\n";

    std::cout << "Steps (" << matrix.steps.size() << "):\n";
    for (const auto& s : matrix.steps) {
        std::cout << "  - col=" << std::setw(4) << s.col
                  << " name=" << std::left << std::setw(24) << s.name
                  << " id=" << s.id << "\n";
    }
    std::cout << "\n";

    std::cout << "Layers (" << matrix.layers.size() << "):\n";
    std::cout << std::left
              << std::setw(6)  << "ROW"
              << std::setw(12) << "CONTEXT"
              << std::setw(14) << "TYPE"
              << std::setw(20) << "NAME"
              << std::setw(12) << "POLARITY"
              << std::setw(12) << "ADD_TYPE"
              << std::setw(8)  << "COLOR"
              << "ID\n";
    std::cout << std::string(96, '-') << "\n";

    for (const auto& l : matrix.layers) {
        std::cout << std::left
                  << std::setw(6)  << l.row
                  << std::setw(12) << l.context
                  << std::setw(14) << l.type
                  << std::setw(20) << l.name
                  << std::setw(12) << (l.polarity == odb::Polarity::POSITIVE ? "POSITIVE" : "NEGATIVE")
                  << std::setw(12) << l.add_type
                  << std::setw(8)  << l.color
                  << l.id << "\n";
    }
    std::cout << "========================================================================\n";
}


// ═════════════════════════════════════════════════════════════════
// Netlist 打印
// ═════════════════════════════════════════════════════════════════

void print_netlist(const odb::OdbNetlist& netlist) {
    std::cout << "\n============================ Netlist Summary =============================\n";
    std::cout << "Netlist Name: " << netlist.netlist_name << "\n";
    std::cout << "units=" << netlist.units
              << "  optimize=" << (netlist.optimize ? "y" : "n")
              << "  staggered=" << (netlist.staggered ? "y" : "n") << "\n";
    std::cout << "net count=" << netlist.netlists.size()
              << "  point count=" << netlist.points.size() << "\n\n";

    std::cout << "Net Names Sample (up to 5):\n";
    int shown = 0;
    for (const auto& [id, name] : netlist.netlists) {
        std::cout << "  - id=" << std::setw(6) << id
                  << " name=" << name << "\n";
        if (++shown >= 5) {
            std::cout << "  ...\n";
            break;
        }
    }
    std::cout << "\n";

    int cnt_t = 0, cnt_d = 0, cnt_b = 0, cnt_via = 0;
    for (const auto& p : netlist.points) {
        if (p.type == odb::NetlistPointType::T) cnt_t++;
        if (p.type == odb::NetlistPointType::D) cnt_d++;
        if (p.type == odb::NetlistPointType::B) cnt_b++;
        if (p.is_via) cnt_via++;
    }
    std::cout << "Point Type Count:"
              << " T=" << cnt_t
              << " D=" << cnt_d
              << " B=" << cnt_b
              << " via=" << cnt_via << "\n\n";

    std::cout << "Point Sample (up to 10):\n";
    std::cout << std::left
              << std::setw(6)  << "TYPE"
              << std::setw(8)  << "NET_ID"
              << std::setw(22) << "NET_NAME"
              << std::setw(12) << "X"
              << std::setw(12) << "Y"
              << std::setw(10) << "SIZE"
              << std::setw(8)  << "LAYER"
              << "VIA\n";
    std::cout << std::string(96, '-') << "\n";

    shown = 0;
    for (const auto& p : netlist.points) {
        std::string tname = (p.type == odb::NetlistPointType::T) ? "T"
                          : (p.type == odb::NetlistPointType::D) ? "D" : "B";
        std::string net_name = "?";
        auto it = netlist.netlists.find(p.net_id);
        if (it != netlist.netlists.end()) {
            net_name = it->second;
        }

        std::cout << std::left
                  << std::setw(6)  << tname
                  << std::setw(8)  << p.net_id
                  << std::setw(22) << net_name
                  << std::setw(12) << p.x
                  << std::setw(12) << p.y
                  << std::setw(10) << p.size
                  << std::setw(8)  << p.layer_id
                  << (p.is_via ? "Y" : "N") << "\n";

        if (++shown >= 10) {
            if (netlist.points.size() > 10) {
                std::cout << "...\n";
            }
            break;
        }
    }
    std::cout << "========================================================================\n";
}

// ═════════════════════════════════════════════════════════════════
// EDA 打印
// ═════════════════════════════════════════════════════════════════

const char* snt_type_to_string(odb::SntType type) {
    switch (type) {
        case odb::SntType::VIA: return "VIA";
        case odb::SntType::TRC: return "TRC";
        case odb::SntType::PLN: return "PLN";
        case odb::SntType::TOP: return "TOP";
        case odb::SntType::BOT: return "BOT";
        default: return "UNKNOWN";
    }
}

const char* pkg_shape_type_to_string(odb::PkgShapeType type) {
    switch (type) {
        case odb::PkgShapeType::CONTOUR: return "CONTOUR";
        case odb::PkgShapeType::RECT: return "RECT";
        case odb::PkgShapeType::CIRCLE: return "CIRCLE";
        case odb::PkgShapeType::SQUARE: return "SQUARE";
        default: return "NONE";
    }
}

void print_eda(const odb::OdbEda& eda) {
    std::cout << "\n============================== EDA Summary ===============================\n";
    std::cout << "hdr=" << eda.hdr << "  units=" << eda.units << "\n";
    std::cout << "layer count=" << eda.layers.size()
              << "  net count=" << eda.nets.size()
              << "  pkg count=" << eda.pkgs.size() << "\n\n";

    std::cout << "Layer Sample (up to 10):\n";
    std::cout << std::left << std::setw(6) << "IDX" << "NAME\n";
    std::cout << std::string(96, '-') << "\n";
    for (size_t i = 0; i < eda.layers.size() && i < 10; ++i) {
        std::cout << std::left << std::setw(6) << i << eda.layers[i] << "\n";
    }
    if (eda.layers.size() > 10) {
        std::cout << "...\n";
    }
    std::cout << "\n";

    int cnt_via = 0, cnt_trc = 0, cnt_pln = 0, cnt_top = 0, cnt_bot = 0, cnt_unknown = 0;
    int total_fids = 0;
    for (const auto& net : eda.nets) {
        for (const auto& snt : net.snts) {
            total_fids += static_cast<int>(snt.fids.size());
            switch (snt.type) {
                case odb::SntType::VIA: cnt_via++; break;
                case odb::SntType::TRC: cnt_trc++; break;
                case odb::SntType::PLN: cnt_pln++; break;
                case odb::SntType::TOP: cnt_top++; break;
                case odb::SntType::BOT: cnt_bot++; break;
                default: cnt_unknown++; break;
            }
        }
    }
    std::cout << "SNT Count: VIA=" << cnt_via
              << " TRC=" << cnt_trc
              << " PLN=" << cnt_pln
              << " TOP=" << cnt_top
              << " BOT=" << cnt_bot
              << " UNKNOWN=" << cnt_unknown
              << "  total_fids=" << total_fids << "\n\n";

    std::cout << "NET Sample (up to 10):\n";
    std::cout << std::left
              << std::setw(24) << "NAME"
              << std::setw(8)  << "ID"
              << std::setw(8)  << "SNTS"
              << "FIRST_SNT\n";
    std::cout << std::string(96, '-') << "\n";

    for (size_t i = 0; i < eda.nets.size() && i < 10; ++i) {
        const auto& net = eda.nets[i];
        std::string first_snt = net.snts.empty() ? "-" : snt_type_to_string(net.snts.front().type);
        std::cout << std::left
                  << std::setw(24) << net.name
                  << std::setw(8)  << net.id
                  << std::setw(8)  << net.snts.size()
                  << first_snt << "\n";
    }
    if (eda.nets.size() > 10) {
        std::cout << "...\n";
    }
    std::cout << "\n";

    int total_pins = 0;
    for (const auto& pkg : eda.pkgs) {
        total_pins += static_cast<int>(pkg.pins.size());
    }
    std::cout << "PIN total=" << total_pins << "\n\n";

    std::cout << "PKG Sample (up to 10):\n";
    std::cout << std::left
              << std::setw(40) << "NAME"
              << std::setw(8)  << "ID"
              << std::setw(8)  << "PINS"
              << std::setw(12) << "X_ORIGIN"
              << "OUTLINE\n";
    std::cout << std::string(96, '-') << "\n";

    for (size_t i = 0; i < eda.pkgs.size() && i < 10; ++i) {
        const auto& pkg = eda.pkgs[i];
        std::cout << std::left
                  << std::setw(40) << pkg.name
                  << std::setw(8)  << pkg.id
                  << std::setw(8)  << pkg.pins.size()
                  << std::setw(12) << pkg.x_origin
                  << pkg_shape_type_to_string(pkg.outline.type) << "\n";
    }
    if (eda.pkgs.size() > 10) {
        std::cout << "...\n";
    }

    // FGR (Graphic Objects) output
    int total_fgr_fids = 0;
    for (const auto& fgr : eda.fgrs) {
        total_fgr_fids += static_cast<int>(fgr.fids.size());
    }
    std::cout << "FGR Count: " << eda.fgrs.size() << "  total_fids=" << total_fgr_fids << "\n\n";

    if (!eda.fgrs.empty()) {
        std::cout << "FGR Sample (up to 10):\n";
        std::cout << std::left
                  << std::setw(8)  << "IDX"
                  << std::setw(12) << "TYPE"
                  << std::setw(20) << "PROPERTY_TYPE"
                  << std::setw(30) << "PROPERTY_VALUE"
                  << "FID_COUNT\n";
        std::cout << std::string(96, '-') << "\n";

        for (size_t i = 0; i < eda.fgrs.size() && i < 10; ++i) {
            const auto& fgr = eda.fgrs[i];
            std::cout << std::left
                      << std::setw(8)  << fgr.index
                      << std::setw(12) << fgr.fgr_type
                      << std::setw(20) << fgr.property_type
                      << std::setw(30) << (fgr.property_value.empty() ? "-" : fgr.property_value)
                      << fgr.fids.size() << "\n";
        }
        if (eda.fgrs.size() > 10) {
            std::cout << "...\n";
        }
        std::cout << "\n";

        // Show first FGR's FIDs as example
        if (!eda.fgrs.empty() && !eda.fgrs[0].fids.empty()) {
            std::cout << "FIDs of first FGR (up to 10):\n";
            std::cout << std::left
                      << std::setw(8) << "LAYER"
                      << std::setw(12) << "LAYER_IDX"
                      << "FEATURE_IDX\n";
            std::cout << std::string(96, '-') << "\n";
            
            for (size_t i = 0; i < eda.fgrs[0].fids.size() && i < 10; ++i) {
                const auto& fid = eda.fgrs[0].fids[i];
                std::cout << std::left
                          << std::setw(8) << fid.layer_type
                          << std::setw(12) << fid.layer_index
                          << fid.feature_index << "\n";
            }
            if (eda.fgrs[0].fids.size() > 10) {
                std::cout << "...\n";
            }
            std::cout << "\n";
        }
    }

    std::cout << "========================================================================\n";
}

// ═════════════════════════════════════════════════════════════════
// Features 统计
// ═════════════════════════════════════════════════════════════════

class FeatureCounter {
public:
    int lines = 0, arcs = 0, pads = 0, texts = 0, surfaces = 0;

    void count(const odb::Feature& f) {
        std::visit([&](auto&& v) {
            using T = std::decay_t<decltype(v)>;
            if      constexpr (std::is_same_v<T, odb::FeatureLine>)    lines++;
            else if constexpr (std::is_same_v<T, odb::FeatureArc>)     arcs++;
            else if constexpr (std::is_same_v<T, odb::FeaturePad>)     pads++;
            else if constexpr (std::is_same_v<T, odb::FeatureText>)    texts++;
            else if constexpr (std::is_same_v<T, odb::FeatureSurface>) surfaces++;
        }, f);
    }

    int total() const { return lines + arcs + pads + texts + surfaces; }
};

void print_features(const std::vector<odb::Feature>& features) {
    if (features.empty()) {
        std::cout << "\n============================= Features Summary ============================\n";
        std::cout << "No features found.\n";
        std::cout << "========================================================================\n";
        return;
    }

    FeatureCounter cnt;
    for (const auto& f : features) {
        cnt.count(f);
    }

    std::cout << "\n============================= Features Summary ============================\n";
    std::cout << "Total Features: " << cnt.total() << "\n"
              << "  - LINE:    " << std::setw(6) << cnt.lines << "\n"
              << "  - ARC:     " << std::setw(6) << cnt.arcs << "\n"
              << "  - PAD:     " << std::setw(6) << cnt.pads << "\n"
              << "  - TEXT:    " << std::setw(6) << cnt.texts << "\n"
              << "  - SURFACE: " << std::setw(6) << cnt.surfaces << "\n\n";

    auto fmt4 = [](double v) {
        std::ostringstream os;
        os << std::fixed << std::setprecision(4) << v;
        return os.str();
    };

    std::cout << std::left
              << std::setw(10) << "TYPE"
              << std::setw(18) << "P1(X,Y)"
              << std::setw(18) << "P2(X,Y)"
              << std::setw(20) << "EXTRA1(Width/Sym)"
              << std::setw(20) << "EXTRA2(Length/Dir)"
              << "ID\n";
    std::cout << std::string(110, '-') << "\n";

    int shown = 0;
    for (const auto& f : features) {
        std::visit([&](auto&& v) {
            using T = std::decay_t<decltype(v)>;

            if constexpr (std::is_same_v<T, odb::FeatureLine>) {
                std::cout << std::left
                          << std::setw(10) << "LINE"
                          << std::setw(18) << ("(" + fmt4(v.x_start) + "," + fmt4(v.y_start) + ")")
                          << std::setw(18) << ("(" + fmt4(v.x_end) + "," + fmt4(v.y_end) + ")")
                          << std::setw(20) << ("w=" + fmt4(v.width))
                          << std::setw(20) << ("len=" + fmt4(v.line_length()))
                          << v.attributes.feature_id << "\n";
            }
            else if constexpr (std::is_same_v<T, odb::FeatureArc>) {
                std::string dir = (v.direction == odb::ArcDirection::CLOCKWISE) ? "CW" : "CCW";
                std::string full = v.is_full_circle() ? "full" : "-";
                std::cout << std::left
                          << std::setw(10) << "ARC"
                          << std::setw(18) << ("(" + fmt4(v.x_start) + "," + fmt4(v.y_start) + ")")
                          << std::setw(18) << ("(" + fmt4(v.x_end) + "," + fmt4(v.y_end) + ")")
                          << std::setw(20) << ("r=" + fmt4(v.arc_radius()))
                          << std::setw(20) << ("dir=" + dir + "," + full)
                          << v.attributes.feature_id << "\n";
            }
            else if constexpr (std::is_same_v<T, odb::FeaturePad>) {
                std::cout << std::left
                          << std::setw(10) << "PAD"
                          << std::setw(18) << ("(" + fmt4(v.x) + "," + fmt4(v.y) + ")")
                          << std::setw(18) << "-"
                          << std::setw(20) << ("sym=$" + std::to_string(v.symbol_index))
                          << std::setw(20) << ("rot=" + std::to_string(v.orient))
                          << v.attributes.feature_id << "\n";
            }
            else if constexpr (std::is_same_v<T, odb::FeatureText>) {
                std::cout << std::left
                          << std::setw(10) << "TEXT"
                          << std::setw(18) << "-"
                          << std::setw(18) << "-"
                          << std::setw(20) << "(empty)"
                          << std::setw(20) << "-"
                          << "-" << "\n";
            }
            else if constexpr (std::is_same_v<T, odb::FeatureSurface>) {
                int pts = 0;
                for (const auto& c : v.contours) pts += static_cast<int>(c.points.size());

                std::cout << std::left
                          << std::setw(10) << "SURFACE"
                          << std::setw(18) << "-"
                          << std::setw(18) << "-"
                          << std::setw(20) << ("ctr=" + std::to_string(v.contours.size()))
                          << std::setw(20) << ("pts=" + std::to_string(pts))
                          << v.attributes.feature_id << "\n";
            }
        }, f);

        if (++shown >= 10) {
            if (features.size() > 10) {
                std::cout << "...\n";
            }
            break;
        }
    }
    std::cout << "========================================================================\n";
}

// ═════════════════════════════════════════════════════════════════
// Symbols 打印
// ═════════════════════════════════════════════════════════════════

void print_symbols(const odb::OdbSymbol& symbols) {
    std::cout << "\n============================= Symbols Summary ============================\n";
    std::cout << "Total Symbols: " << symbols.size() << "\n\n";

    if (symbols.empty()) {
        std::cout << "No symbols found.\n";
        std::cout << "========================================================================\n";
        return;
    }

    std::cout << std::left
              << std::setw(30) << "SYMBOL_NAME"
              << std::setw(12) << "FEATURES"
              << "KEY\n";
    std::cout << std::string(96, '-') << "\n";

    int shown = 0;
    for (const auto& [key, sym] : symbols) {
        std::cout << std::left
                  << std::setw(30) << sym.symbol_name
                  << std::setw(12) << sym.features.size()
                  << key << "\n";

        if (++shown >= 10) {
            if (symbols.size() > 10) {
                std::cout << "...\n";
            }
            break;
        }
    }
    std::cout << "========================================================================\n";
}

// ═════════════════════════════════════════════════════════════════
// LineMesh 打印
// ═════════════════════════════════════════════════════════════════

const char* bc_type_to_string(odb::BoundaryCondition type) {
    switch (type) {
        case odb::BoundaryCondition::Null: return "NULL";
        case odb::BoundaryCondition::USERDEFINED: return "USERDEFINED";
        case odb::BoundaryCondition::DIRICHLET: return "DIRICHLET";
        case odb::BoundaryCondition::NEUMANN: return "NEUMANN";
        case odb::BoundaryCondition::WALL: return "WALL";
        default: return "UNKNOWN";
    }
}

void print_linemesh(const std::vector<odb::OdbLineMesh>& linemeshes) {
    if (linemeshes.empty()) {
        std::cout << "\n============================== LineMesh Summary =============================\n";
        std::cout << "No linemeshes found.\n";
        std::cout << "========================================================================\n";
        return;
    }

    std::cout << "\n============================== LineMesh Summary =============================\n";
    std::cout << "Total Steps: " << linemeshes.size() << "\n\n";
    
    // 合并所有layers并打印
    std::vector<odb::LayerLineMesh> all_layers;
    size_t total_board_coords = 0;
    
    for (const auto& linemesh : linemeshes) {
        std::cout << "Step: " << linemesh.step_name << "\n";
        std::cout << "  Board Outline: " << linemesh.board_outline.coord.size() << " coords, "
                  << linemesh.board_outline.segments.size() << " segments\n";
        total_board_coords += linemesh.board_outline.coord.size();
        
        for (const auto& layer : linemesh.layers) {
            all_layers.push_back(layer);
        }
    }
    std::cout << "  Total Board Outline Coords: " << total_board_coords << "\n";
    std::cout << "  Total Layers: " << all_layers.size() << "\n\n";

    auto fmt4 = [](double v) {
        std::ostringstream os;
        os << std::fixed << std::setprecision(4) << v;
        return os.str();
    };

    std::cout << std::left
              << std::setw(24) << "LAYER_NAME"
              << std::setw(16) << "LAYER_TYPE"
              << std::setw(10) << "ROW"
              << std::setw(10) << "ID"
              << std::setw(12) << "MESH_COORDS"
              << std::setw(12) << "MESH_SEGS"
              << std::setw(12) << "BOUNDARY_C"
              << "PROFILE_PTS\n";
    std::cout << std::string(110, '-') << "\n";

    for (const auto& layer : all_layers) {
        std::string mesh_coords = std::to_string(layer.mesh.coord.size());
        std::string mesh_segs = std::to_string(layer.mesh.segments.size());
        std::string profile_pts = std::to_string(layer.profile.coord.size());
        std::string bc_type = bc_type_to_string(layer.mesh.boundary_condition.bctype);

        std::cout << std::left
                  << std::setw(24) << layer.layer_name
                  << std::setw(16) << layer.layer_type
                  << std::setw(10) << layer.layer_row
                  << std::setw(10) << layer.layer_id
                  << std::setw(12) << mesh_coords
                  << std::setw(12) << mesh_segs
                  << std::setw(12) << bc_type
                  << profile_pts << "\n";
    }
    std::cout << "\n";

    // Show Board Outline details
    std::cout << "=== Board Outline Details ===\n";
    if (!linemeshes.empty() && !linemeshes[0].board_outline.coord.empty()) {
        const auto& board_outline = linemeshes[0].board_outline;
        std::cout << "Total Coordinates: " << board_outline.coord.size() << "\n";
        std::cout << "Total Segments: " << board_outline.segments.size() << "\n";
        
        // Find coordinate range
        double min_x = board_outline.coord[0][0], max_x = min_x;
        double min_y = board_outline.coord[0][1], max_y = min_y;
        double min_z = board_outline.coord[0][2], max_z = min_z;
        
        for (const auto& pt : board_outline.coord) {
            min_x = std::min(min_x, pt[0]); max_x = std::max(max_x, pt[0]);
            min_y = std::min(min_y, pt[1]); max_y = std::max(max_y, pt[1]);
            min_z = std::min(min_z, pt[2]); max_z = std::max(max_z, pt[2]);
        }
        std::cout << "Coordinate Range:\n";
        std::cout << "  X: [" << fmt4(min_x) << ", " << fmt4(max_x) << "]\n";
        std::cout << "  Y: [" << fmt4(min_y) << ", " << fmt4(max_y) << "]\n";
        std::cout << "  Z: [" << fmt4(min_z) << ", " << fmt4(max_z) << "]\n\n";
        
        std::cout << "Sample Coordinates (up to 5):\n";
        std::cout << std::left
                  << std::setw(12) << "X"
                  << std::setw(12) << "Y"
                  << "Z\n";
        std::cout << std::string(40, '-') << "\n";
        
        for (size_t i = 0; i < board_outline.coord.size() && i < 5; ++i) {
            const auto& pt = board_outline.coord[i];
            std::cout << std::left
                      << std::setw(12) << fmt4(pt[0])
                      << std::setw(12) << fmt4(pt[1])
                      << fmt4(pt[2]) << "\n";
        }
        if (board_outline.coord.size() > 5) {
            std::cout << "  ...\n";
        }
    } else {
        std::cout << "No board outline found.\n";
    }
    std::cout << "\n";

    // Show detailed info for first layer as example
    if (!all_layers.empty()) {
        const auto& first_layer = all_layers[0];
        std::cout << "First Layer Details: " << first_layer.layer_name << "\n";
        std::cout << "  Mesh Coordinates (up to 5):\n";
        std::cout << std::left
                  << std::setw(12) << "X"
                  << std::setw(12) << "Y"
                  << "Z\n";
        std::cout << std::string(40, '-') << "\n";

        for (size_t i = 0; i < first_layer.mesh.coord.size() && i < 5; ++i) {
            const auto& pt = first_layer.mesh.coord[i];
            std::cout << std::left
                      << std::setw(12) << fmt4(pt[0])
                      << std::setw(12) << fmt4(pt[1])
                      << fmt4(pt[2]) << "\n";
        }
        if (first_layer.mesh.coord.size() > 5) {
            std::cout << "  ...\n";
        }
        std::cout << "\n";

        if (!first_layer.mesh.segments.empty()) {
            std::cout << "  Segments (up to 5):\n";
            std::cout << std::left
                      << std::setw(12) << "PT1_IDX"
                      << "PT2_IDX\n";
            std::cout << std::string(30, '-') << "\n";

            for (size_t i = 0; i < first_layer.mesh.segments.size() && i < 5; ++i) {
                const auto& seg = first_layer.mesh.segments[i];
                std::cout << std::left
                          << std::setw(12) << seg[0]
                          << seg[1] << "\n";
            }
            if (first_layer.mesh.segments.size() > 5) {
                std::cout << "  ...\n";
            }
        }
        std::cout << "\n";
    }

    std::cout << "========================================================================\n";
}