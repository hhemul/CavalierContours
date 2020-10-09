#include "cavc/polylineoffset.hpp"
#include <emscripten/bind.h>

using namespace emscripten;

typedef val (*new_point_func)(cavc::PlineVertex<double>&);

val new_point_by_array(cavc::PlineVertex<double> &vertex) {
    val point = val::array();
    point.call<void>("push", vertex.x());
    point.call<void>("push", vertex.y());
    point.call<void>("push", vertex.bulge());
    return point;
}

val new_point_by_object(cavc::PlineVertex<double> &vertex) {
    val point = val::object();
    point.set<std::string, double>("x", vertex.x());
    point.set<std::string, double>("y", vertex.y());
    point.set<std::string, double>("bulge", vertex.bulge());
    return point;
}

new_point_func getNewPointFunc(int point_type) {
    switch (point_type) {
        case 0: return &new_point_by_object;
        case 1: return &new_point_by_array;
    }
    return &new_point_by_object;
}

val new_line(std::vector<cavc::PlineVertex<double>> &vertexes, new_point_func new_point) {
    auto line = val::array();
    for (auto it = vertexes.begin(); it != vertexes.end(); ++it) {
        val point = new_point(*it);
        line.call<void>("push", point);
    }
    return line;
}

cavc::Polyline<double> getInput(const val& points, bool isClosed) {
    cavc::Polyline<double> input;
    auto l = points["length"].as<unsigned>();
    
    for (unsigned i = 0; i < l; i++) {
        val point = points[i];
        double x, y, bulge;
        if (point.isArray()) {
            auto point_items = point["length"].as<unsigned>();
            x = point[0].as<double>();
            y = point[1].as<double>();
            bulge = point_items < 3 ? 0 : point[2].as<double>();
        } else {
            x = point["x"].as<double>();
            y = point["y"].as<double>();
            auto b = point["bulge"];
            bulge = (!b.isNull() && !b.isUndefined()) ? b.as<double>() : 0;
        }
        input.addVertex(x, y, bulge);
    }
    
    input.isClosed() = isClosed;

    return input;
}

val createResultMultiPoints(std::vector<cavc::Polyline<double>>& multi_points, int point_type, val ret_val = val::array()) {
    auto new_point = getNewPointFunc(point_type);
    for (auto it = multi_points.begin(); it != multi_points.end(); ++it) {
        if (it->vertexes().size() > 1) {
            auto line = new_line(it->vertexes(), new_point);
            ret_val.call<void>("push", line);
        }
    }
    
    return ret_val;
}

val parallelOffset(val points, bool isClosed, double offset, int point_type, val ret_val) {
    if (!points.isArray()) return val::undefined();
    
    cavc::Polyline<double> input = getInput(points, isClosed);
    
    std::vector<cavc::Polyline<double>> results = cavc::parallelOffset(input, offset);
    
    return createResultMultiPoints(results, point_type, ret_val);
}

val parallelOffsetMulti(val multi_points, bool isClosed, double offset, int point_type, val ret_val) {
    if (!multi_points.isArray()) return val::undefined();
    
    auto l = multi_points["length"].as<unsigned>();
    for (auto i = 0; i < l; i++) parallelOffset(multi_points[i], isClosed, offset, point_type, ret_val);
    
    return val::undefined();
}

val polygonize(val line, double width, int point_type, bool allowSelfIntersection, val ret_val) {
    if (!line.isArray()) return val::undefined();
    
    cavc::Polyline<double> input = getInput(line, false);
    
    width /= 2;
    std::vector<cavc::Polyline<double>> result = cavc::polygonize(input, width, allowSelfIntersection);
    
    return createResultMultiPoints(result, point_type, ret_val);
}
    
val polygonizeMulti(val multi_line, double width, int point_type, bool allowSelfIntersection, val ret_val) {
    if (!multi_line.isArray()) return val::undefined();
        
    auto l = multi_line["length"].as<unsigned>();
    for (auto i = 0; i < l; i++) polygonize(multi_line[i], width, point_type, allowSelfIntersection, ret_val);
    
    return ret_val;
}

EMSCRIPTEN_BINDINGS(cavalier_contours_bindings) {
    function("parallelOffset", &parallelOffset);
    function("polygonize", &polygonize);
    function("polygonizeMulti", &polygonizeMulti);
}
