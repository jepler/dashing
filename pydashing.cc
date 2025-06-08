#include <sstream>
#include <utility> // std::pair ??

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "dashing.hh"

namespace py = pybind11;

namespace {
    dashing::HatchPattern hatchPatternFromString(std::string s, dashing::F scale) {
        std::istringstream is(s);
        return dashing::HatchPattern::FromFile(is, scale);
    }

    dashing::HatchPattern hatchPatternFromFile(py::object open_file, dashing::F scale) {
        auto content = open_file.attr("read")();
        return hatchPatternFromString(content.cast<std::string>(), scale);
    }


    enum WindingRule {
        EvenOdd, NonZero, GreaterThanZero
    };

    using pyPoint = std::pair<dashing::F, dashing::F>;
    using pySegment = std::pair<pyPoint, pyPoint>;

    std::vector<pySegment> hatchPatternHatch(const dashing::HatchPattern &pattern, std::vector<pySegment> pyBoundary, WindingRule wr) {
        std::vector<dashing::Segment> boundary;
        for(auto i: pyBoundary) {
            boundary.push_back(dashing::Segment{i.first.first, i.first.second, i.second.first, i.second.second, false});
        }
        std::vector<pySegment> result;
        auto cb = [&result](dashing::Segment s) {
            result.push_back(pySegment{{s.p.x, s.p.y}, {s.q.x, s.q.y}});
        };
        switch(wr) {
            case EvenOdd:
                dashing::xyhatch(pattern, boundary, cb, [](int i) { return i & 1; });
                break;
            case NonZero:
                dashing::xyhatch(pattern, boundary, cb, [](int i) { return i != 0; });
                break;
            case GreaterThanZero:
                dashing::xyhatch(pattern, boundary, cb, [](int i) { return i > 0; });
                break;
            default:
                throw std::runtime_error("Invalid winding rule");
        }
        return result;
    }
}

PYBIND11_MODULE(dashing, m) {
    py::options options;
    options.enable_enum_members_docstring();  
    options.enable_function_signatures();
    options.enable_user_defined_docstrings();

    m.doc() = R"pbdoc(
    Python wrapper for autocad-style hatch patterns
    )pbdoc";

    py::enum_<WindingRule>(m, "WindingRule")
    .value("EvenOdd", WindingRule::EvenOdd)
    .value("NonZero", WindingRule::NonZero)
    .value("GreaterThanZero", WindingRule::GreaterThanZero);

    py::class_<dashing::HatchPattern>(m, "HatchPattern",R"pbdoc(
    Encapsulate an autocad-style hatch pattern
    )pbdoc")
        .def_static("fromString", hatchPatternFromString)
        .def_static("fromFile", hatchPatternFromFile)
        .def("hatch", hatchPatternHatch);
}
