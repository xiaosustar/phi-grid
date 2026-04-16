#pragma once
#include <filesystem>
#include <fstream>
#include <map>
#include "odb_types.hpp"

namespace fs = std::filesystem;

namespace odb{


// ═════════════════════════════════════════════════════════════════
// Internal utility function 
// ═════════════════════════════════════════════════════════════════
namespace detail{


/**
 * Trims leading and trailing whitespace from the given string.
 * Whitespace includes spaces, tabs, newlines, carriage returns, form feeds, and vertical tabs.
 * 
 * @param str The input string to trim.
 * @return A trimmed string, or an empty string if the input is all whitespace.
 */   
inline std::string trim(const std::string& str){
    auto start = str.find_first_not_of(" \t\r\n\f\v");

    if(start == std::string::npos){
        return "";
    }
    auto end = str.find_last_not_of(" \t\r\n\f\v");

    return str.substr(start, end - start + 1);
}

/**
 * Parses a block of key-value pairs from the input stream.
 * Reads until a closing brace '}' is found.
 * Lines must be in "KEY=VALUE" format; others are ignored.
 *
 * @param infile The input file stream.
 * @return A map of parsed key-value pairs.
 */
inline std::map<std::string, std::string> parse_block(std::ifstream& infile){
    std::map<std::string, std::string> block_data;
    std::string line;
    while(std::getline(infile, line)){
        line = trim(line);
        if(line == "}"){
            break;
        }
        if(line.empty()){
            continue;
        }
        auto equal_pos = line.find('=');
        if(equal_pos == std::string::npos){
            continue;
        }
        block_data[trim(line.substr(0, equal_pos))] = trim(line.substr(equal_pos + 1));
    }
    return block_data;
}

/**
 * @brief Split a string by whitespace
 *
 * @param str Input string
 * @return Vector of tokens separated by spaces/tabs/newlines
 */
inline std::vector<std::string> split_by_blank(const std::string& str)
{
    std::vector<std::string> result;
    std::istringstream instream(str);
    std::string token; 
    while(instream >> token){
        result.push_back(token);
    }
    return result;
}


}


/**
 * Parse a single matrix file.
 * @param path  The path to the matrix file (usually <root>/matrix/matrix)
 */
inline OdbMatrix parse_matrix_file(const std::string& matrix_file_path){
    OdbMatrix matrix;

    std::ifstream infile(matrix_file_path);
    if(!infile.is_open()){
       throw std::runtime_error("Failed to open matrix file: " + matrix_file_path); 
    }

    std::string line;
    while(std::getline(infile, line)){
        line = detail::trim(line);

        if(line.empty()){
            continue;
        }

        // parse STEP sections.
        if(line.substr(0, 4) == "STEP"){
            while(line.find('{') == std::string::npos){
                if(!std::getline(infile, line)){
                    //TODO: better error handling
                    break;
                }
            }

            auto step_data = detail::parse_block(infile);

            MatrixStep step;

            if(step_data.count("COL"))
                step.col = std::stoi(step_data["COL"]);
            if(step_data.count("NAME"))
                step.name = step_data["NAME"];
            if(step_data.count("ID"))
                try { step.id = std::stoi(step_data["ID"]); } catch(...) {}

            matrix.steps.push_back(step);
        }
        else if(line.substr(0, 5) == "LAYER"){
            while(line.find('{') == std::string::npos){
                if(!std::getline(infile,line)){
                    //TODO: better error handling
                    break;
                }
            }
            auto layer_data = detail::parse_block(infile);

            MatrixLayer layer;

            if(layer_data.count("ROW"))
                layer.row = std::stoi(layer_data["ROW"]);
            if(layer_data.count("CONTEXT"))
                layer.context = layer_data["CONTEXT"];
            if(layer_data.count("TYPE"))
                layer.type = layer_data["TYPE"];
            if(layer_data.count("NAME"))
                layer.name = layer_data["NAME"];
            if(layer_data.count("POLARITY"))
                layer.polarity = (layer_data["POLARITY"] == "POSITIVE") ? Polarity::Positive:Polarity::Negative;
            if(layer_data.count("START_NAME"))
                layer.start_name = layer_data["START_NAME"];
            if(layer_data.count("END_NAME"))
                layer.end_name = layer_data["END_NAME"];
            if(layer_data.count("OLD_NAME"))
                layer.old_name = layer_data["OLD_NAME"];
            if(layer_data.count("ADD_TYPE"))
                layer.add_type = layer_data["ADD_TYPE"];
            if(layer_data.count("COLOR"))
                try { layer.color = std::stoi(layer_data["COLOR"]); } catch(...) {} 
            if(layer_data.count("ID"))
                try { layer.id = std::stoi(layer_data["ID"]); } catch(...) {}
            matrix.layers.push_back(layer);
        }
    }
    std::sort(matrix.layers.begin(), matrix.layers.end(), [](const MatrixLayer& a, const MatrixLayer& b){
        return a.row < b.row;
    });
    return matrix;
}

/**
 * Read the matrix file from the ODB++ project directory.
 * Automatically detects two directory structures:
 *   Structure A: <root>/matrix/matrix
 *   Structure B: <root>/<job_name>/matrix/matrix
 */
inline OdbMatrix parse_matrix_from_dir(const std::string& odb_root_dir){
    fs::path root(odb_root_dir);

    auto potential_matrix_path_A = root / "matrix" / "matrix";

    // Structure A: <root>/matrix/matrix
    if(fs::exists(potential_matrix_path_A)){
        return parse_matrix_file(potential_matrix_path_A.string());
    }

    // Structure B: <root>/<job_name>/matrix/matrix
    for(const auto& entry : fs::directory_iterator(root)){
        if(!entry.is_directory()){
            continue;
        }
        auto potential_matrix_path_B = entry.path() / "matrix" / "matrix";
        if(fs::exists(potential_matrix_path_B)){
            return parse_matrix_file(potential_matrix_path_B.string());
        }
    }
    throw std::runtime_error("Matrix file not found in ODB++ project directory:");
}

/**
 * @brief Parse a netlist file.
 * @param netlist_file_path Path to the netlist file.
 * @param netlist_name Optional netlist name (default is empty).
 * @return Parsed OdbNetlist object.
 */
inline OdbNetlist parse_netlist_file(const std::string& netlist_file_path, const std::string& netlist_name = ""){
    OdbNetlist netlist;
    netlist.netlist_name = netlist_name;

    std::ifstream infile(netlist_file_path);

    if(!infile.is_open()){
        throw std::runtime_error("Failed to open netlist file: " + netlist_file_path);
    }

    std::string line;

    bool points_section = false;

    while(std::getline(infile, line)){
        line = detail::trim(line);
        if(line.empty()){
            continue;
        }

        if(line.substr(0, 6) == "UNITS="){
            netlist.units = line.substr(6);
            continue;
        }

        if(line.size() > 2 && line[0] == 'H' && line[1] == ' '){
            auto split_line = detail::split_by_blank(line);
            if(split_line.size() >= 3){
                netlist.optimize = (split_line[2] == "y");
            }
            if(split_line.size() >= 5){
                netlist.staggered = (split_line[4] == "y");
            }
            continue;
        }

        if(line[0] == '$'){
            auto pos = line.find(' ');
            if(pos != std::string::npos){
                try{
                    int net_id = std::stoi(line.substr(1,pos - 1));
                    netlist.netlists[net_id] = detail::trim(line.substr(pos + 1));
                }catch(...){}
            }
            continue;
        }

        if(line[0] == '#'){
            if(line.find("Netlist point") != std::string::npos){
                points_section = true;        
            }
            continue;
        }

        if(!points_section){
            continue;
        }

        if(!std::isdigit(static_cast<unsigned char>(line[0]))){
            continue;
        }

        NetlistPoint point;

        auto pos = line.rfind("ld=");

        if(pos != std::string::npos){
            try{
                point.layer_id = std::stoi(line.substr(pos + 3));
            }catch(...){}
            line = detail::trim(line.substr(0, pos));
        }

        auto split_line = detail::split_by_blank(line);

        if(split_line.size() < 11){
            continue;
        }

        try{
            point.net_id = std::stoi(split_line[0]);
            point.size = std::stod(split_line[1]);
            point.x = std::stod(split_line[2]);
            point.y = std::stod(split_line[3]);
        }catch(...){}

        std::string type = split_line[4];
        if(type == "T")
            point.type = NetlistPointType::T;
        else if(type == "D")
            point.type = NetlistPointType::D;
        else if(type == "B")
            point.type = NetlistPointType::B;
        else 
            continue;
        
        point.top_access = split_line[5];
        point.bot_access = split_line[6];
        point.staggered_flag = split_line[7];

        try{
            point.top_expose = std::stoi(split_line[8]);
            point.bot_expose = std::stoi(split_line[9]);
            point.expose_type = std::stoi(split_line[10]);
        }catch(...){}

        if(split_line.size() >= 12 &&split_line[11] == "v"){
            point.is_via = true;
        }

        netlist.points.push_back(point);
    }
    return netlist;
}

/**
 * @brief Parse netlists from an ODB directory
 *
 * @param odb_root_dir Path to ODB root directory
 * @param specified_step_name Optional step name (empty = all steps)
 *   Structure A: <root>/steps/netlists/<netlist_name>/netlist
 *   Structure B: <root>/<job_name>/steps/netlists/<netlist_name>/netlist
 * @return OdbNetlist
 */
inline std::map<std::string, OdbNetlist> parse_netlist_from_dir(const std::string& odb_root_dir, const std::string& specified_step_name = ""){
    std::map<std::string, OdbNetlist> netlist;

    fs::path root(odb_root_dir);

    if(!fs::exists(root)){
        throw std::runtime_error("ODB++ root directory does not exist: " + odb_root_dir);
    }
    // Locate the "steps" directory (same logic as parse_from_dir)
    fs::path steps_dir;
    // Structure A: <root>/steps
    auto potential_step_dir_A = root / "steps";

    if(fs::exists(potential_step_dir_A) && fs::is_directory(potential_step_dir_A)){
        steps_dir = potential_step_dir_A;
    }
    else{
        // Structure B: <root>/<job_name>/steps
        for(auto& entry : fs::directory_iterator(root)){
            if(!entry.is_directory()){
                continue;
            }
            auto potential_step_dir_B = entry.path() / "steps";
            if(fs::exists(potential_step_dir_B) && fs::is_directory(potential_step_dir_B)){
                steps_dir = potential_step_dir_B;
                break;
            }

        }

    }

    if(steps_dir.empty()){
        throw std::runtime_error("Steps directory not found in ODB++ project directory: " + odb_root_dir);
    }

    for(auto& step_entry : fs::directory_iterator(steps_dir)){
        if(!step_entry.is_directory()){
            continue;
        }

        std::string step_name = step_entry.path().filename().string();

        // Filter out step_entry with different specified_step_name
        if(!specified_step_name.empty()){
           std::string a = step_name, b = specified_step_name;
           std::transform(a.begin(), a.end(), a.begin(), [](unsigned char c){ return static_cast<char>(::tolower(c));});
           std::transform(b.begin(), b.end(), b.begin(), [](unsigned char c){ return static_cast<char>(::tolower(c));});
           if(a != b){
               continue;
           }
        }
        fs::path netlist_dir = step_entry.path() / "netlists";

        if(!fs::exists(netlist_dir)){
            continue;
        }

        for(auto& netlist_entry : fs::directory_iterator(netlist_dir)){
            if(!netlist_entry.is_directory()){
                continue;
            }
            std::string netlist_name = netlist_entry.path().filename().string();

            fs::path netlist_file = netlist_entry.path() / "netlist";

            if(!fs::exists(netlist_file) || !fs::file_size(netlist_file)){
                continue;
            }

            try{
                auto result = parse_netlist_file(netlist_file.string(), netlist_name);
                netlist[netlist_name] = std::move(result);           
            }catch(const std::exception& e){
                std::cerr << "Failed to parse netlist file: " << netlist_file.string() << " Error: " << e.what() << std::endl;
            }
        }
    }
    return netlist;
}

/**
 * @brief Parse a single EDA data file.
 * @param eda_file_path Path to the EDA data file (usually <root>/steps/<step>/eda/data)
 * @return Parsed OdbEda object.
 */
inline OdbEda parse_eda_file(const std::string& eda_file_path){
    OdbEda eda;
    std::ifstream infile(eda_file_path);

    if(!infile.is_open()){
        throw std::runtime_error("Failed to open eda file: " + eda_file_path);
    }

    std::string line;

    bool in_pkg_section = false;

    EdaNet* cur_net = nullptr;
    NetSnt* cur_snt = nullptr;

    EdaPkg* cur_pkg = nullptr;
    PkgPin* cur_pin = nullptr;
    PkgShape* cur_shape = nullptr;

    bool in_ct = false;
    bool in_ct_outline = false; // 是否在 CT...CE 块中
    SurfaceContour ct_contour; // 是否在 OB...OE 中

    auto flush_ct_contour = [&](){
        if(in_ct_outline && cur_shape){
            cur_shape -> contour.contours.push_back(ct_contour);
            ct_contour = SurfaceContour{};
            in_ct_outline = false;
        }
    };

    while(std::getline(infile, line)){
        line = detail::trim(line);
        if(line.empty()){
            continue;
        }

        if(line[0] == '#'){
            if(line.find("PKG") != std::string::npos && !in_pkg_section){
                std::string tmp = line;
                tmp.erase(std::remove(tmp.begin(), tmp.end(), '#'), tmp.end());
                tmp = detail::trim(tmp);
                if(tmp.substr(0, 3) == "PKG"){
                    in_pkg_section = true;
                }
            }
            continue;
        }

        auto split_line = detail::split_by_blank(line);
        if(split_line.empty())
            continue;

        const std::string& first_token = split_line[0];
        
        if(first_token == "HDR"){
            eda.hdr = line.substr(4);
            continue;
        }

        if(first_token == "UNITS=INCH"){
            eda.units = detail::trim(line.substr(6));
            continue;
        }

        if(first_token == "LYR"){
            for(size_t i= 1; i < split_line.size(); i++){
                eda.layers.push_back(split_line[i]);
            }
            continue;
        }

        // 解析 NET 块
        if(!in_pkg_section){
            if(first_token == "NET"){
                std::string name_part = split_line[1];
                auto pos = name_part.find(';');
                if(pos != std::string::npos){
                    name_part = name_part.substr(0, pos);
                }

                int id = -1;
                auto id_pos = line.find("ID=");
                if(id_pos != std::string::npos){
                    try{
                        id = std::stoi(line.substr(id_pos + 3));
                    }catch(...){}
                }
                eda.nets.push_back(EdaNet{name_part, id});
                cur_net = &eda.nets.back();
                cur_snt = nullptr;
                continue;
            }

            if(first_token == "SNT" && cur_net){
                NetSnt snt;
                snt.raw_string = line.substr(4);
                
                if(split_line.size() >= 2){
                    const std::string& type = split_line[1];
                    if(type == "VIA")
                        snt.type = SntType::VIA;
                    else if(type == "TRC")
                        snt.type = SntType::TRC;
                    else if(type == "PLN")
                        snt.type = SntType::PLN;
                    else if(type == "TOP")
                        snt.type = SntType::TOP;
                    else if(type == "BOT")
                        snt.type = SntType::BOT;
                    else
                        snt.type = SntType::UNKNOWN;
                }

                cur_net -> snts.push_back(std::move(snt));
                cur_snt = &cur_net -> snts.back();
                continue;
            }

            if(first_token == "FID" && cur_snt && split_line.size() >= 4){
                SntFid fid;
                fid.layer_type = split_line[1][0];
                try{
                    fid.layer_index = std::stoi(split_line[2]);
                    fid.feature_index = std::stoi(split_line[3]);
                    cur_snt -> fids.push_back(fid);
                }catch(...){}
                continue;
            }
        }
        // 解析 PKG 块
        else{
            if(first_token == "PKG" && split_line.size() >= 7){
                flush_ct_contour();
                cur_pin = nullptr;
                cur_shape = nullptr;

                EdaPkg pkg;
                pkg.name = split_line[1];
                try{
                    pkg.x_origin = std::stod(split_line[2]);
                    pkg.y_min = std::stod(split_line[3]);
                    pkg.x_min = std::stod(split_line[4]);
                    pkg.y_max = std::stod(split_line[5]);
                    std::string x_max_str = split_line[6];
                    auto split_pos = x_max_str.find(';');
                    if(split_pos != std::string::npos){
                        pkg.x_max = std::stod(x_max_str.substr(0, split_pos));
                    }
                }catch(...){}
                
                auto id_pos = line.find("ID=");
                if(id_pos != std::string::npos){
                    try{
                        pkg.id = std::stoi(line.substr(id_pos + 3));
                    }catch(...){}
                }

                eda.pkgs.push_back(std::move(pkg));
                cur_pkg = &eda.pkgs.back();
                cur_shape = &cur_pkg -> outline;
                continue;
            }

            if(first_token == "RC"&& cur_shape && split_line.size() >= 5){
                cur_shape -> type = PkgShapeType::RECT;
                try{
                    cur_shape -> rect.x = std::stod(split_line[1]);
                    cur_shape -> rect.y = std::stod(split_line[2]);
                    cur_shape -> rect.width = std::stod(split_line[3]);
                    cur_shape -> rect.height = std::stod(split_line[4]);
                }catch(...){}
                continue;
            }

            if(first_token == "CR" && cur_shape && split_line.size() >= 4){
                cur_shape -> type = PkgShapeType::CIRCLE;
                try{
                    cur_shape -> circle.x = std::stod(split_line[1]);
                    cur_shape -> circle.y = std::stod(split_line[2]);
                    cur_shape -> circle.radius = std::stod(split_line[3]);
                }catch(...){}
            }

            if(first_token == "SQ" && cur_shape && split_line.size() >= 4){
                cur_shape -> type = PkgShapeType::SQUARE;
                try{
                    cur_shape -> square.x = std::stod(split_line[1]);
                    cur_shape -> square.y = std::stod(split_line[2]);
                    cur_shape -> square.half_side = std::stod(split_line[3]); 
                }catch(...){}
            }
            
            if(first_token == "PIN" && cur_pkg && split_line.size() >= 8){
                flush_ct_contour();
                in_ct = false;

                PkgPin pin;
                pin.name = split_line[1];
                pin.pin_type = split_line[2];
                try{
                    pin.x = std::stod(split_line[3]);
                    pin.y = std::stod(split_line[4]);
                    pin.rotation = std::stod(split_line[5]);
                }catch(...){}
                pin.top_access = split_line[6];
                pin.bot_access = split_line[7];

                auto id_pos = line .find("ID=");
                if(id_pos != std::string::npos){
                    try{
                        pin.id = std::stoi(line.substr(id_pos + 3));
                    }catch(...){}
                }

                cur_pkg -> pins.push_back(std::move(pin));
                cur_pin = &cur_pkg -> pins.back();
                cur_shape = &cur_pin -> pin_shape;
                continue;
            }

            if(first_token == "CT"){
                if(cur_shape){
                    cur_shape ->type = PkgShapeType::CONTOUR;
                    in_ct = true;
                    continue;
                }
            }

            if(first_token == "CE"){
                flush_ct_contour();
                in_ct = false;
                continue;
            }

            if(in_ct){
                if(first_token == "OB"){
                    flush_ct_contour();
                    ct_contour.x_start = std::stod(split_line[1]);
                    ct_contour.y_start = std::stod(split_line[2]);
                    ct_contour.type = (split_line.size() >= 4 && split_line[3] == "H") ? 'H' : 'I';
                    ct_contour.points.clear();
                    in_ct_outline = true;
                    continue;
                }

                if(first_token == "OS" && in_ct_outline && split_line.size() >= 3){
                    ct_contour.points.push_back(ContourLinePoint{std::stod(split_line[1]), std::stod(split_line[2])});
                    continue;
                }

                if(first_token == "OC" && in_ct_outline && split_line.size() >= 6){
                    ContourArcPoint arc;
                    arc.x_center = std::stod(split_line[1]);
                    arc.y_center = std::stod(split_line[2]);
                    arc.x_end = std::stod(split_line[3]);
                    arc.y_end = std::stod(split_line[4]);
                    arc.direction = (split_line[5] == "Y") ? ArcDirection::CLOCKWISE : ArcDirection::COUNTER_CLOCKWISE;
                    ct_contour.points.push_back(arc);
                    continue;
                }

                if(first_token == "OE"){
                    flush_ct_contour();
                    continue;
                }
            }
        }
    }
    return eda;
}

/**
 * @brief Parse eda from an ODB directory
 *
 * @param odb_root_dir Path to ODB root directory
 * @param filter_step_name Optional eda name (empty = all eda)
 *   Structure A: <root>/steps/eda/data
 *   Structure B: <root>/<job_name>/steps/eda/data
 * @return Map of step name to OdbEda
 */
inline OdbEda parse_eda_from_dir(const std::string& odb_root_dir, const std::string& filter_step_name = ""){
    fs::path root(odb_root_dir);
    if(!fs::exists(root)){
        throw std::runtime_error("ODB++ root directory does not exist: " + odb_root_dir);
    }  

    fs::path steps_dir;
    auto potential_step_dir_A = root / "steps"; 
    if(fs::exists(potential_step_dir_A) && fs::is_directory(potential_step_dir_A)){
        steps_dir = potential_step_dir_A;
    }
    else{
        for(auto& entry : fs::directory_iterator(root)){
            if(!entry.is_directory()){
                continue;
            }
            auto potential_step_dir_B = entry.path() / "steps";
            if(fs::exists(potential_step_dir_B) && fs::is_directory(potential_step_dir_B)){
                steps_dir = potential_step_dir_B;
                break;
            }
        }
    }

    if(steps_dir.empty()){
        throw std::runtime_error("Steps directory not found in ODB++ project directory: " + odb_root_dir);
    }

    for(auto& step_entry : fs::directory_iterator(steps_dir)){
        if(!step_entry.is_directory()){
            continue;
        }
        std::string step_name = step_entry.path().filename().string();
        if(!filter_step_name.empty()){
            std::string a = step_name, b = filter_step_name;
            std::transform(a.begin(), a.end(), a.begin(), [](unsigned char c){ return static_cast<char>(::tolower(c));});
            std::transform(b.begin(), b.end(), b.begin(), [](unsigned char c){ return static_cast<char>(::tolower(c));});
            if(a != b){
                continue;
            }
        }
        fs::path eda_file = step_entry.path() / "eda" / "data";
        if(fs::exists(eda_file) && fs::file_size(eda_file) > 0){
            return parse_eda_file(eda_file.string());
        }
    }

    throw std::runtime_error("EDA file not found in ODB++ project directory: " + odb_root_dir);
}


inline LayerFeature parse_stream(std::ifstream& in_file, const std::string& layer_name){
    LayerFeature features_result;

    features_result.layer_name = layer_name;
    FeatureHeader& header = features_result.header;
    auto& features = features_result.features;

    std::string line;
    FeatureSurface* cur_surf = nullptr;
    SurfaceContour cur_contour;
    bool in_contour = false;

    auto flush_contour = [&](){
        if(in_contour && cur_surf){
            cur_surf -> contours.push_back(cur_contour);
            cur_contour = SurfaceContour{};
            in_contour = false;
        }
    };

    while(std::getline(in_file, line)){
        line = detail::trim(line);
        if(line.empty() || line[0] == '#'){
            continue;
        }

        if(line.substr(0, 6) == "UNITS="){
            header.units = detail::trim(line.substr(6));
            continue;
        }

        if(line.substr(0, 3) == "ID="){
            try{
                header.layer_id = std::stoi(line.substr(3));
            }catch(...){}
        }

        if(line.size() > 2 && line[0] == 'F' && line[1] == ' '){
            try{
                header.feature_count = std::stoi(line.substr(2));
            }catch(...){}
        }

        if(line[0] == '$'){
            auto pos = line.find(' ');
            if(pos != std::string::npos){
                try{
                    int num = std::stoi(line.substr(1, pos - 1));
                    header.symbols[num] = detail::trim(line.substr(pos + 1));
                }catch(...){}
            }
            continue;
        }

        if(line[0] == '@'){
            auto pos = line.find(' ');
            if(pos != std::string::npos){
                try{
                    int num = std::stoi(line.substr(1, pos -1));
                    header.attribute_names[num] = detail::trim(line.substr(pos + 1));
                }catch(...){}
                continue;
            }
        }

        if(line[0] == '&'){
            auto pos = line.find(' ');
            if(pos != std::string::npos){
                try{
                    int num = std::stoi(line.substr(1, pos -1));
                    header.attribute_names[num] = detail::trim(line.substr(pos + 1));
                }catch(...){}
            }
            continue;
        }

        if(line.size() > 3 && line[0] == 'O' && line[1] == 'B' && line[2] == ' '){
            flush_contour();
            auto split_line = detail::split_by_blank(line);

            if(split_line.size() >= 3){
                cur_contour.x_start = std::stod(split_line[1]);
                cur_contour.y_start = std::stod(split_line[2]);
                
                if(split_line.size() >= 4){
                    cur_contour.type = (split_line[3] == "H") ? 'H' :'I'; 
                }

                cur_contour.points.clear();
                in_contour = true;
            }
            continue;
        }

        
    }
}

inline LayerFeature parse_feature_file(const std::string& path, const std::string& layer_name = ""){
    std::ifstream infile(path);
    if(!infile.is_open()){
        throw std::runtime_error("Failed to open feature file: " + path);
    }
    return parse_stream(infile, layer_name.empty() ? path : layer_name);
}

/*
 * @brief Parse attrlist file
 * @param attrlist_file_path Path to the attrlist file (usually <root>/steps/<step>/attrlist)
 * @return Parsed AttrlistData object
 */
inline AttrlistData parse_attrlist_file(const std::string& attrlist_file_path){
    AttrlistData attrlist;
    std::ifstream infile(attrlist_file_path);
    if(!infile.is_open()){
        throw std::runtime_error("Failed to open attrlist file: " + attrlist_file_path);
    }

    std::string line;

    while(std::getline(infile, line)){
        line = detail::trim(line);
        if(line.empty()){
            continue;
        }

        if(line.substr(0, 6) == "UNITS="){
            attrlist.units = detail::trim(line.substr(6));
            continue;
        }
        
        auto equal_pos = line.find('=');
        if(equal_pos != std::string::npos){
            std::string key = detail::trim(line.substr(0, equal_pos));
            std::string value = detail::trim(line.substr(equal_pos + 1));
            attrlist.attrilists[key] = value;
        }
    }
    return attrlist;
}

/*
* @brief Parse profile file
* @param profile_file_path Path to the profile file (usually <root>/steps/<step>/profile)
* @return Parsed ProfileData object
*/
inline ProfileData parse_profile_file(const std::string& profile_file_path){
    ProfileData profile;
    std::ifstream infile(profile_file_path);
    if(!infile.is_open()){
        throw std::runtime_error("Failed to open profile file: " + profile_file_path);
    }

    std::string line;

    while(std::getline(infile, line)){
        line = detail::trim(line);
        if(line.empty()){
            continue;
        }

        if(line[0] == '#'){
            continue;
        }

        if(line.substr(0, 6) == "UNITS="){
            profile.units = detail::trim(line.substr(6));
        }

        if(line.substr(0, 3) == "ID="){
            try{
                profile.id = std::stoi(line.substr(3));
            }catch(...){}
            continue;
        }

        if(line.size() > 2 && line[0] == 'F' && line[1] == ' '){
            try{
                profile.feature_count = std::stoi(line.substr(2));
            }catch(...){}
            continue;
        }
    }

    infile.clear();
    infile.seekg(0);

    if(!infile){
        infile.open(profile_file_path);
        if(!infile.is_open()){
            throw std::runtime_error("Failed to reopen profile file: " + profile_file_path);
        }
    }

    LayerFeature tmp_layer = parse_stream(infile, "profile");
    profile.features = std::move(tmp_layer.features);

    return profile;
}


inline OdbLayer parse_layer_from_dir(const std::string& odb_root_dir, const std::string& filter_step_name = "", const std::vector<std::string>& filter_layer_name = {}){
    OdbLayer layers;
    fs::path root(odb_root_dir);

    if(!fs::exists(root)){
        throw std::runtime_error("ODB++ root directory does not exist: " + odb_root_dir);
    }

    fs::path steps_dir;
    auto potential_step_dir_A = root / "steps";
    if(fs::exists(potential_step_dir_A) && fs::is_directory(potential_step_dir_A)){
        steps_dir = potential_step_dir_A;      
    }
    else{
        for(auto& entry : fs::directory_iterator(root)){
            if(!entry.is_directory()){
                continue;
            }
            auto potential_step_dir_B = entry.path() / "steps";
            if(fs::exists(potential_step_dir_B) && fs::is_directory(potential_step_dir_B)){
               steps_dir = potential_step_dir_B;
               break;
            }
        }
    }

    if(steps_dir.empty()){
        throw std::runtime_error("Steps directory not found in ODB++ project directory: " + odb_root_dir);
    }

    for(auto& step_entry : fs::directory_iterator(steps_dir)){
        if(!step_entry.is_directory()){
            continue;
        }

        std::string step_name = step_entry.path().filename().string();

        if(!filter_step_name.empty()){
            std::string a = step_name, b = filter_step_name;
            std::transform(a.begin(), a.end(), a.begin(), [](unsigned char c)
            { return static_cast<char>(::tolower(c));});
            std::transform(b.begin(), b.end(), b.begin(), [](unsigned char c)
            { return static_cast<char>(::tolower(c));});
            if(a != b){
                continue;
            }
        }

        AttrlistData step_attrlist;
        ProfileData step_profile;
        // 读取与 layers 同级的 attrlist/profile
        fs::path step_attrlist_path = step_entry.path() / "attrlist";
        if(fs::exists(step_attrlist_path) && fs::is_regular_file(step_attrlist_path)){
            try{
                step_attrlist = parse_attrlist_file(step_attrlist_path.string());
            }catch(const std::exception& e){
                std::cerr << "Failed to parse step attrlist file: " << step_attrlist_path.string() << " Error: " << e.what() << std::endl;
            }
        }
        fs::path step_profile_path = step_entry.path() / "profile";
        if(fs::exists(step_profile_path) && fs::is_regular_file(step_profile_path)){
            try{
                step_profile = parse_profile_file(step_profile_path.string());
            }catch(const std::exception& e){
                std::cerr << "Failed to parse step profile file: " << step_profile_path.string() << " Error: " << e.what() << std::endl;
            }
        }

        if(layers.step_name.empty()){
            layers.step_name = step_name;
            layers.step_attrlist =std::move(step_attrlist);
            layers.step_profile = std::move(step_profile);
        }

        fs::path layer_dir = step_entry.path() / "layers";
        if(!fs::exists(layer_dir)){
            continue;
        }

        for(auto& layer_entry : fs::directory_iterator(layer_dir)){
            if(!layer_entry.is_directory()){
                continue;
            }
            std::string layer_name = layer_entry.path().filename().string();

            if(!filter_layer_name.empty()){
                bool found_flag = false;
                for(auto& filter : filter_layer_name){
                    std::string a = layer_name, b= filter;
                    std::transform(a.begin(), a.end(), a.begin(), [](unsigned char c){ return static_cast<char>(::tolower(c));});
                    std::transform(b.begin(), b.end(), b.begin(), [](unsigned char c){ return static_cast<char>(::tolower(c));});
                    if(a == b){
                        found_flag = true;
                        break;
                    }
                }

                if(!found_flag){
                    continue;
                }
            }

            fs::path feature_file = layer_entry.path() / "feature";
            if(!fs::exists(feature_file)){
                continue;
            }

            if(fs::file_size(feature_file) == 0){
                continue;
            }

            try{
                auto layer_feature = parse_feature_file(feature_file.string(), layer_name);

                auto layer_attrlist_path = layer_entry.path() / "attrlist";
                if(fs::exists(layer_attrlist_path) && fs::is_regular_file(layer_attrlist_path)){
                    try{
                        layer_feature.layer_attrlist = parse_attrlist_file(layer_attrlist_path.string());
                    }catch(const std::exception& e){
                        std::cerr << "Failed to parse layer attrlist file: " << layer_attrlist_path.string() << " Error: " << e.what() << std::endl;
                    }
                }

                fs::path layer_profile_path = layer_entry.path() / "profile";
                if(fs::exists(layer_profile_path) && fs::is_regular_file(layer_profile_path)){
                    try{
                        layer_feature.layer_profile = parse_profile_file(layer_profile_path.string());
                    }catch(const std::exception& e){
                        std::cerr << "Failed to parse layer profile file: " << layer_profile_path.string() << " Error: " << e.what() << std::endl;
                    }
                }

                layers.layers.push_back(std::move(layer_feature));
            }catch (const std::exception& e){
            std::cerr << "Failed to parse feature file for layer: " << layer_name << " Error: " << e.what() << std::endl;
        }
        }
    }
}


}