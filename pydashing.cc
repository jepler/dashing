#include <sstream>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "dashing.hh"

namespace py = pybind11;

namespace {
    dashing::HatchPattern HatchPatternFromString(std::string s, dashing::F scale) {
        std::istringstream is(s);
        return dashing::HatchPattern::FromFile(is, scale);
    }

    dashing::HatchPattern HatchPatternFromFile(py::object open_file, dashing::F scale) {
        auto content = open_file.attr("read")();
        return HatchPatternFromString(content.cast<std::string>(), scale);
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

    py::class_<dashing::HatchPattern>(m, "HatchPattern",R"pbdoc(
    Encapsulate an autocad-style hatch pattern
    )pbdoc")
        .def_static("fromString", HatchPatternFromString)
        .def_static("fromFile", HatchPatternFromFile);
}
