#include "parse_numbers.hh"
#include "dashing.hh"
namespace dashing
{
std::vector<double> parse_numbers(std::string line) {
    boost::algorithm::replace_all(line, ",", " ");
    std::istringstream fi(line);
    std::vector<double> result;
    double d;
    while((fi >> d)) result.push_back(d);
    return result;
}

PSMatrix PSMatrix::inverse() const {
    auto i = 1. / determinant();
    return PSMatrix{d*i, -b*i, -c*i, a*i, i*(c*f-e*d), i*(b*e-a*f)};
}

PSMatrix Translation(double x, double y) {
    PSMatrix r{1.,0.,0.,1.,x,y};
    return r;
}

PSMatrix Rotation(double theta) {
    double c = cos(theta), s = sin(theta);
    PSMatrix r{c,s,-s,c,0.,0.};
    return r;
}

PSMatrix XSkew(double xk) {
    PSMatrix r{1.,0.,xk,1.,0.,0.};
    return r;
}

PSMatrix YScale(double ys) {
    PSMatrix r{1.,0.,0.,ys,0.,0.};
    return r;
}

PSMatrix operator*(const PSMatrix &m1, const PSMatrix m2) {
    PSMatrix r{
            m2.a * m1.a + m2.b * m1.c,
            m2.a * m1.b + m2.b * m1.d,
            m2.c * m1.a + m2.d * m1.c,
            m2.c * m1.b + m2.d * m1.d,
            m2.e * m1.a + m2.f * m1.c + m1.e,
            m2.e * m1.b + m2.f * m1.d + m1.f};
    return r;
}

static double radians(double degrees) { return degrees * acos(0) / 90.; }

Dash::Dash(double th, double x0, double y0, double dx, double dy,
        const std::vector<double>::const_iterator dbegin,
        const std::vector<double>::const_iterator dend) : dash(dbegin, dend) {
    auto s = 0.;
    for(auto d : dash) { sum.push_back(s); s += fabs(d); }
    sum.push_back(s);

    tr = Translation(x0, y0) * Rotation(th) * XSkew(dx / dy) * YScale(dy);
    tf = tr.inverse();
}

Dash Dash::FromString(const std::string &line, double scale) {
    std::vector<double> words = parse_numbers(line);
    if(words.size() < 5)
        throw std::invalid_argument("not a valid dash specification");
    for(auto i = words.begin() + 1; i != words.end(); i++) *i *= scale;
    return Dash(radians(words.at(0)), words.at(1), words.at(2), words.at(3),
        words.at(4), words.begin() + 5, words.end());        
}

}
