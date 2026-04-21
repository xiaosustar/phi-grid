#pragma once
#include <map>
#include <string>
#include <vector>
#include <variant>
#include <optional>
namespace odb{


// Matrix file data structure
enum class Polarity{
    POSITIVE,
    NEGATIVE
};

// Matrix file data structure
class MatrixStep{
public:
    int col = -1;
    std::string name;
    int id = -1;
};

// Matrix file data structure
class MatrixLayer{
public:
    int row = -1;
    std::string context;
    std::string type;
    std::string name;
    Polarity polarity = Polarity::POSITIVE;
    std::string start_name;
    std::string end_name;
    std::string old_name;
    std::string add_type;
    int color = -1;
    int id  = -1;
};

// Complete parsing results of the matrix file
class OdbMatrix{
public:
    std::vector<MatrixStep> steps;
    std::vector<MatrixLayer> layers;
};


// Netlists file data structure
enum class NetlistPointType{
    T,
    D,
    B
};

// Netlists file data structure
class NetlistPoint{
public:
    int net_id = -1;
    double size = 0;
    double x = 0, y = 0;
    NetlistPointType type = NetlistPointType::T;
    std::string top_access;
    std::string bot_access;
    std::string staggered_flag;
    int top_expose = -1;
    int bot_expose = -1;
    int expose_type = -1;
    bool is_via = false;
    int layer_id = -1;

};

// Complete parsing results of the netlists file
class OdbNetlist{
public:
    std::string units = "INCH";
    bool optimize = false;
    bool staggered = false;
    std::map<int, std::string> netlists;
    std::vector<NetlistPoint> points;
    std::string netlist_name;
};


enum class ArcDirection{
    CLOCKWISE,
    COUNTER_CLOCKWISE    
};

class ContourLinePoint{
public:
    double x = 0, y = 0;
};

class ContourArcPoint{
public:
    double x_center = 0, y_center = 0;
    double x_end = 0, y_end = 0;
    ArcDirection direction;
};

using ContourPoint = std::variant<ContourLinePoint, ContourArcPoint>;

class SurfaceContour{
public:
    char type = 'I';
    double x_start = 0, y_start = 0;
    std::vector<ContourPoint> points;
};

enum class SntType{
    VIA,
    TRC,
    PLN,
    TOP,
    BOT,
    UNKNOWN
};

class SntFid{
public:
    char layer_type;
    int layer_index;
    int feature_index;
};

class NetSnt{
public:
    SntType type = SntType::UNKNOWN;
    std::string raw_string;
    std::vector<SntFid> fids;
};

class EdaNet{
public:
    std::string name;
    int id = -1;
    std::vector<NetSnt> snts;
};

enum class PkgShapeType{
    CONTOUR,
    RECT,
    CIRCLE,
    SQUARE,
    NONE
};

class PkgShapeRect{
public:
    double x = 0, y = 0;
    double width = 0, height = 0;
};

class PkgShapeCircle{
public:
    double x = 0, y = 0;
    double radius = 0;
};

class PkgShapeSquare{
public:
    double x = 0, y = 0;
    double half_side = 0;
};

class PkgShapeContour{
public:
    std::vector<SurfaceContour> contours;
};

class PkgShape{
public:
    PkgShapeType type = PkgShapeType::NONE;
    PkgShapeRect rect{};
    PkgShapeCircle circle{};
    PkgShapeSquare square{};
    PkgShapeContour contour{};
};

class PkgPin{
public:
    std::string name;
    std::string pin_type;
    double x = 0, y = 0;
    double rotation = 0;
    std::string top_access;
    std::string bot_access;
    int id = -1;

    PkgShape pin_shape;
};



class EdaPkg{
public:
    std::string name; 
    double x_origin = 0;
    double y_min = 0, x_min = 0;
    double y_max = 0, x_max = 0;
    int id = -1; 

    PkgShape outline;

    std::vector<PkgPin> pins;
};


class EdaFgr{
public:
    std::string fgr_type;
    int index = -1;
    std::string property_type;
    std::string property_value;
    std::vector<SntFid> fids;
};

class OdbEda{
public:
    std::string hdr;
    std::string units;
    std::vector<std::string> layers;
    std::vector<EdaNet> nets;
    std::vector<EdaPkg> pkgs;  
    std::vector<EdaFgr> fgrs;  
};


class FeatureAttribute{
public:
    std::map<std::string, std::string> attributes;
    int feature_id = -1;

    std::optional<std::string> get_attrbute_name(const std::string& key) const{
        auto it = attributes.find(key);
        if(it != attributes.end()){
            return it -> second;
        }
        return std::nullopt;
    }
};

class FeatureLine{
public:
    double x_start = 0, y_start = 0;
    double x_end = 0, y_end = 0;
    double width = 0;
    Polarity polarity = Polarity::POSITIVE;
    int symbol_index = -1;
    FeatureAttribute attributes;

    double line_length() const{
        double dx = x_end - x_start;
        double dy = y_end - y_start;
        return std::sqrt(dx * dx + dy * dy);
    }
};

class FeatureArc{
public:
    double x_start = 0, y_start = 0;
    double x_center = 0, y_center = 0;
    double x_end = 0, y_end = 0;
    double width = 0;
    Polarity polarity = Polarity::POSITIVE;
    int symbol_index = -1;
    ArcDirection direction;
    FeatureAttribute attributes;

    double arc_radius() const{
        double dx = x_end - x_center;
        double dy = y_end - y_center;
        return std::sqrt(dx * dx + dy * dy);
    }

    bool is_full_circle() const{
        return (x_start == x_center && y_start == y_center);
    }
};

class FeaturePad{
public:
    double x = 0, y = 0;
    int symbol_index = -1;
    Polarity polarity = Polarity::POSITIVE;
    int rotation_index;
    int orient;
    FeatureAttribute attributes;
};

class FeatureText{
public:
    //TODO: text features
};

class FeatureSurface{
public:
    Polarity polarity = Polarity::POSITIVE;
    int symbol_index = -1;
    FeatureAttribute attributes;
    std::vector<SurfaceContour> contours;
};

using Feature = std::variant<
    FeatureLine,
    FeatureArc,
    FeaturePad,
    FeatureText,
    FeatureSurface
>;

class FeatureHeader{
public:
    std::string units = "INCH";
    int layer_id = -1;
    int feature_count = 0;
    std::map<int, std::string> symbols;
    std::map<int, std::string> attribute_names;
    std::map<int, std::string> attribute_strings;
};

class AttrlistData{
public:
    std::string units = "INCH";
    std::map<std::string, std::string> attrilists;
};

class ProfileData{
public:
    std::string units = "INCH";
    int id = -1;
    int feature_count = 0;
    std::vector<Feature> features;
};

class LayerFeature{
public:
    std::string layer_name;
    FeatureHeader header;
    std::vector<Feature> features;

    AttrlistData layer_attrlist;
    ProfileData layer_profile;
};

class OdbLayer{
public:
    std::string step_name;
    AttrlistData step_attrlist;
    ProfileData step_profile;
    std::vector<LayerFeature> layers;
};

    
}