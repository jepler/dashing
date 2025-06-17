#include <sstream>
#include <utility> // std::pair ??

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "dashing.hh"

namespace py = pybind11;

namespace {
    py::tuple PSMatrixAsTuple(const dashing::PSMatrix & m) {
        return py::make_tuple(m.a, m.b, m.c, m.d, m.e, m.f);
    }

    py::str PSMatrixRepr(const dashing::PSMatrix &m) {
        return py::str("PSMatrix") + py::str(PSMatrixAsTuple(m));
    }

    py::tuple dashAsTuple(const dashing::Dash d) {
        return py::make_tuple(d.tf, d.dash);
    }

    py::str dashRepr(const dashing::Dash &m) {
        return py::str("dash") + py::str(dashAsTuple(m));
    }


    dashing::HatchPattern hatchPatternFromString(std::string s, dashing::F scale) {
        std::istringstream is(s);
        return dashing::HatchPattern::FromFile(is, scale);
    }

    dashing::HatchPattern hatchPatternFromFile(py::object open_file, dashing::F scale) {
        auto content = open_file.attr("read")();
        return hatchPatternFromString(content.cast<std::string>(), scale);
    }

    py::tuple hatchPatternAsTuple(const dashing::HatchPattern &pattern) {
        return py::tuple(py::make_iterator(pattern.d.begin(), pattern.d.end()));
    }

    py::str hatchPatternRepr(const dashing::HatchPattern &pattern) {
        return py::str("HatchPattern") + py::str(hatchPatternAsTuple(pattern));
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
        .value("GreaterThanZero", WindingRule::GreaterThanZero)
        ;

    py::class_<dashing::HatchPattern>(m, "HatchPattern",R"pbdoc(
    Encapsulate an autocad-style hatch pattern
    )pbdoc")
        .def(py::init<const std::vector<dashing::Dash>&>())
        .def("as_tuple", hatchPatternAsTuple)
        .def_static("fromString", hatchPatternFromString)
        .def_static("fromFile", hatchPatternFromFile)
        .def("hatch", hatchPatternHatch)
        .def("transformed", [](dashing::HatchPattern a, const dashing::PSMatrix b) {
                auto b_inv = b.inverse();
                for(auto &dash: a.d) {
                    dash.tf = b * dash.tf;
                    dash.tr = dash.tr * b_inv;
                }
                return a;
            })
        .def("__repr__", hatchPatternRepr)
        .def("__str__", hatchPatternRepr)
        ;

    py::class_<dashing::Dash>(m, "Dash")
        .def(py::init<const dashing::PSMatrix &, const std::vector<dashing::F> &>())
        .def("as_tuple", dashAsTuple)
        .def("__repr__", dashRepr)
        .def("__str__", dashRepr)
        ;

    py::class_<dashing::PSMatrix>(m, "PSMatrix")
        .def("as_tuple", PSMatrixAsTuple)
        .def("__repr__", PSMatrixRepr)
        .def("__str__", PSMatrixRepr)
        .def("determinant", &dashing::PSMatrix::determinant)
        .def("inverse", &dashing::PSMatrix::inverse)
        .def("identity", []() { return dashing::PSMatrix(1,0,0,1,0,0); })
        .def_static("translation", dashing::Translation)
        .def_static("rotation", dashing::Rotation)
        .def_static("xSkew", dashing::XSkew)
        .def_static("yScale", dashing::YScale)
        .def("__mul__", [](const dashing::PSMatrix &a, const dashing::PSMatrix b) { return a * b; });
        ;
}
