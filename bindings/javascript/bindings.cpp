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

val createResultMultiPoints(std::vector<cavc::Polyline<double>>& multi_points, int point_type) {
    auto new_point = getNewPointFunc(point_type);
    auto ret_val = val::array();
    for (auto it = multi_points.begin(); it != multi_points.end(); ++it) {
        auto line = new_line(it->vertexes(), new_point);
        ret_val.call<void>("push", line);
    }
    
    return ret_val;
}

val parallelOffset(val points, bool isClosed, double offset, int point_type) {
    if (!points.isArray()) return val::undefined();
    
    cavc::Polyline<double> input = getInput(points, isClosed);
    
    std::vector<cavc::Polyline<double>> results = cavc::parallelOffset(input, offset);
    
    return createResultMultiPoints(results, point_type);
}

val parallelOffsetMulti(val multi_points, bool isClosed, double offset, int point_type) {
    if (!multi_points.isArray()) return val::undefined();
    
    return val::undefined();
}

std::vector<cavc::Polyline<double>> simpleJoin(std::vector<cavc::Polyline<double>>& left, std::vector<cavc::Polyline<double>>& right) {
    std::vector<cavc::Polyline<double>> result(1);
    std::vector<cavc::PlineVertex<double>>& p_left = left[0].vertexes(), p_right = right[0].vertexes();
    cavc::Polyline<double> p(p_left.size() + p_right.size());
    for (auto it = p_left.begin(); it != p_left.end(); ++it) p.push_back(*it);
    for (auto it = p_right.rbegin(); it != p_right.rend(); ++it) p.push_back(*it);
    result.push_back(p);
    return result;
}

val polygonize(val line, double width, int point_type) {
    if (!line.isArray()) return val::undefined();
    
    cavc::Polyline<double> input = getInput(line, false);
    
    width /= 2;
    std::vector<cavc::Polyline<double>> left = cavc::parallelOffset(input, width);
    std::vector<cavc::Polyline<double>> right = cavc::parallelOffset(input, width);
    
    std::vector<cavc::Polyline<double>> result;
    if (left.size() == 1 && right.size() == 1) {
        result = simpleJoin(left, right);
    }
    
    return createResultMultiPoints(result, point_type);
}

EMSCRIPTEN_BINDINGS(cavalier_contours_bindings) {
    function("parallelOffset", &parallelOffset);
    function("polygonize", &polygonize);
}
