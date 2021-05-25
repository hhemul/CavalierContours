#include "cavc/polylineoffset.hpp"
#include "cavc/polylinecombine.hpp"
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

const cavc::PlineCombineMode getMode(const int op) {
    cavc::PlineCombineMode mode;
    switch (op) {
        case 0:
            mode = cavc::PlineCombineMode::Union;
            break;
        case 1:
            mode = cavc::PlineCombineMode::Exclude;
            break;
        case 2:
            mode = cavc::PlineCombineMode::Intersect;
            break;
        case 3:
            mode = cavc::PlineCombineMode::XOR;
            break;
        default:
            mode = cavc::PlineCombineMode::Union;
    }
    return mode;
}

val combine(val polygon1, val polygon2, int op, int point_type, val ret_val) {
    if (!polygon1.isArray() || !polygon2.isArray()) return val::undefined();
    
    cavc::Polyline<double> input1 = getInput(polygon1, true);
    cavc::Polyline<double> input2 = getInput(polygon2, true);
    const cavc::PlineCombineMode mode = getMode(op);
    
    cavc::CombineResult<double> result = combinePolylines(input1, input2, mode);
    
    createResultMultiPoints(result.remaining, point_type, ret_val);
    return val((int)result.status);
}

val combineMulti(val multi_polygon1, val polygon2, int op, int point_type, val ret_val) {
    if (!multi_polygon1.isArray() || !polygon2.isArray()) return val::undefined();
    
    auto l = multi_polygon1["length"].as<unsigned>();
    for (auto i = 0; i < l; i++) combine(multi_polygon1[i][0], polygon2, op, point_type, ret_val);
    
    return ret_val;
}

void _combine(cavc::CombineResult<double> &result, cavc::Polyline<double> &p, const cavc::PlineCombineMode mode) {
    if (mode != cavc::PlineCombineMode::Union) return;
    int uncoincidentCount = 0;
    for (auto it = result.remaining.begin(); it != result.remaining.end(); ++it) {
        if (&(*it) == &p) continue;
        cavc::CombineResult<double> result = combinePolylines(*it, p, mode);
        switch (result.status) {
            case cavc::CombineStatus::CompletelyUncoincident:
                uncoincidentCount++;
                break;
            case cavc::CombineStatus::CompletelyCoincident:
                break;
            case cavc::CombineStatus::AnyIntersects:
                break;
            case cavc::CombineStatus::AInsideB:
                ;
                break;
            case cavc::CombineStatus::BInsideA:
                break;
        }
    }
}

val combineAll(val polygons, int op, int point_type, val ret_val) {
    if (!polygons.isArray()) return val::undefined();
    
    auto l = polygons["length"].as<unsigned>();
    if (l < 2) return polygons;
    
    const auto mode = getMode(op);
    cavc::CombineResult<double> result;
    result.remaining.push_back(getInput(polygons[0], true));
    
    for (int i = 1; i < l; i++) {
        cavc::Polyline<double> p = getInput(polygons[i], true);
        _combine(result, p, mode);
    }
    
    return createResultMultiPoints(result.remaining, point_type, ret_val);
}

val getAllSelfIntersections(val polygon, int point_type, val ret_val) {
    if (!polygon.isArray()) return val::undefined();
    
    cavc::Polyline<double> input = getInput(polygon, true);
    cavc::StaticSpatialIndex<double> spatialIndex = cavc::createApproxSpatialIndex(input);
    
    std::vector<cavc::PlineIntersect<double>> selfIntersects;
    cavc::allSelfIntersects(input, selfIntersects, spatialIndex);
    
    for (auto it = selfIntersects.begin(); it != selfIntersects.end(); ++it) {
        val intersection = val::array();
        intersection.call<void>("push", it->pos.x());
        intersection.call<void>("push", it->pos.y());
        intersection.call<void>("push", it->sIndex1);
        intersection.call<void>("push", it->sIndex2);
        ret_val.call<void>("push", intersection);
    }
    
    return ret_val;
}

EMSCRIPTEN_BINDINGS(cavalier_contours_bindings) {
    function("parallelOffset", &parallelOffset);
    function("polygonize", &polygonize);
    function("polygonizeMulti", &polygonizeMulti);
    function("combine", &combine);
    function("combineMulti", &combineMulti);
    function("getAllSelfIntersections", &getAllSelfIntersections);
}
