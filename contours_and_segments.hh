#include "dashing.hh"
#include "parse_numbers.hh"
#include <random>
#include <iostream>

namespace dashing
{

typedef std::vector<Segment> Segments;
typedef std::vector<Point> Contour;
typedef std::vector<Contour> Contours;

void ContourToSegments(Segments &dest,
        /* EXPLICIT COPY */ Contour src,
        double jitter = 0)
{
    if(jitter)
    {
        static auto && gen = std::minstd_rand(std::random_device{}());
        std::uniform_real_distribution<> dist(-jitter/2, jitter/2);
        for(auto &p : src)
        {
            p.x += dist(gen);
            p.y += dist(gen);
        }
    }

    for(size_t i=0; i<src.size()-1; i++)
        dest.emplace_back(Segment{src[i], src[i+1], false});

    int i = src.size();
    dest.emplace_back(Segment{src[i-1], src[0], false});
}

void ContoursToSegments(Segments &dest, Contours const &src, double jitter=0)
{
    dest.clear();

    for(auto &contour : src)
    {
        ContourToSegments( dest, contour, jitter);
    }
}

Contours ContoursFromFile(std::istream &fi)
{
    auto result = Contours{};
    auto line = std::string{};

    while(getline(fi, line)) {
        auto && coordinates = parse_numbers(line);
        if(coordinates.size() % 2 != 0)
            throw std::invalid_argument("odd number of values in segment line");
        if(coordinates.size() < 6)
            throw std::invalid_argument("too few values in segment line");

        auto contour = Contour{};
        for(size_t i=0; i<coordinates.size(); i += 2)
            contour.emplace_back(Point{coordinates[i], coordinates[i+1]});
        result.emplace_back(std::move(contour));
    }
    return result;
}

Segments SegmentsFromFile(std::istream &fi, double jitter) {
    auto && contours = ContoursFromFile(fi);
    auto result = Segments{};
    ContoursToSegments(result, contours, jitter);
    return result;
}

std::vector<Segment> SegmentsFromFile(const char *filename, double jitter) {
    std::fstream fi(filename);
    return SegmentsFromFile(fi, jitter);
}

}
