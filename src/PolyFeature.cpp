#include "PolyFeature.h"

PolyFeature::PolyFeature()
        : TopoFeature(), _poly(), _base_heights() {}

PolyFeature::PolyFeature(const int outputLayerID)
        : TopoFeature(outputLayerID), _poly(), _base_heights() {}

PolyFeature::PolyFeature(const nlohmann::json& poly)
        : TopoFeature(), _base_heights() {
    for (auto& polyEdges : poly) {
        Polygon_2 tempPoly;
        for (auto& coords : polyEdges) {
            tempPoly.push_back(Point_2(coords[0], coords[1]));
        }
        CGAL::internal::pop_back_if_equal_to_front(tempPoly);

        if (_poly._rings.empty()) {
            if (tempPoly.is_clockwise_oriented()) tempPoly.reverse_orientation();
        } else {
            if (tempPoly.is_counterclockwise_oriented()) tempPoly.reverse_orientation();
        }
        _poly._rings.push_back(tempPoly);
    }
}

PolyFeature::PolyFeature(const nlohmann::json& poly, const int outputLayerID)
        : PolyFeature(poly) {
    _outputLayerID = outputLayerID;
    if (_outputLayerID  >= _numOfOutputLayers) _numOfOutputLayers = _outputLayerID + 1;
}

PolyFeature::~PolyFeature() = default;

void PolyFeature::calc_footprint_elevation_nni(const DT& dt) {
    typedef std::vector<std::pair<DT::Point, double>> Point_coordinate_vector;
    DT::Face_handle fh = nullptr;
    for (auto& ring: _poly.rings()) {
        std::vector<double> ringHeights;
        for (auto& polypt: ring) {
            Point_coordinate_vector coords;
            DT::Point pt(polypt.x(), polypt.y(), 0);
            fh = dt.locate(pt, fh);
            CGAL::Triple<std::back_insert_iterator<Point_coordinate_vector>, double, bool> result =
                    CGAL::natural_neighbor_coordinates_2(dt, pt, std::back_inserter(coords), fh);

            if (!result.third) {
//                throw std::runtime_error("Trying to interpolate the point that lies outside the convex hull!");
                this->deactivate();
                return;
            }

            double height = 0;
            for (auto& coord : coords) {
                height += coord.first.z() * coord.second / result.second;
            }
            ringHeights.push_back(height);
        }
        _base_heights.push_back(ringHeights);
    }
}

void PolyFeature::calc_footprint_elevation_linear(const DT& dt) {
    typedef CGAL::Barycentric_coordinates::Triangle_coordinates_2<iProjection_traits>   Triangle_coordinates;
    DT::Face_handle fh = nullptr;
    for (auto& ring : _poly.rings()) {
        std::vector<double> ringHeights;
        for (auto& polypt : ring) {
            DT::Point pt(polypt.x(), polypt.y(), 0);

            DT::Locate_type lt;
            int li;
            fh = dt.locate(pt, lt, li, fh);
            if (lt == DT::OUTSIDE_CONVEX_HULL) {
                this->deactivate();
                return;
            }

            Triangle_coordinates triangle_coordinates(fh->vertex(0)->point(),
                                                      fh->vertex(1)->point(),
                                                      fh->vertex(2)->point());
            std::vector<double> coords;
            triangle_coordinates(pt, std::back_inserter(coords));

            double h = 0;
            for (int i = 0; i < 3; ++i) {
                h += fh->vertex(i)->point().z() * coords[i];
            }
            ringHeights.push_back(h);
        }
        _base_heights.push_back(ringHeights);
    }
}

void PolyFeature::clear_feature() {
    _base_heights.clear();
    _mesh.clear();
}

Polygon_with_holes_2& PolyFeature::get_poly() {
    return _poly;
}

const Polygon_with_holes_2& PolyFeature::get_poly() const {
    return _poly;
}

const std::vector<std::vector<double>>& PolyFeature::get_base_heights() const {
    return _base_heights;
}