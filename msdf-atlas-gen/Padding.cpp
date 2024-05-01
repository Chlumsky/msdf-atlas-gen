
#include "Padding.h"

namespace msdf_atlas {

void pad(msdfgen::Shape::Bounds &bounds, const Padding &padding) {
    bounds.l -= padding.l;
    bounds.b -= padding.b;
    bounds.r += padding.r;
    bounds.t += padding.t;
}

Padding operator-(const Padding &padding) {
    return Padding(-padding.l, -padding.b, -padding.r, -padding.t);
}

Padding operator+(const Padding &a, const Padding &b) {
    return Padding(a.l+b.l, a.b+b.b, a.r+b.r, a.t+b.t);
}

Padding operator-(const Padding &a, const Padding &b) {
    return Padding(a.l-b.l, a.b-b.b, a.r-b.r, a.t-b.t);
}

Padding operator*(double factor, const Padding &padding) {
    return Padding(factor*padding.l, factor*padding.b, factor*padding.r, factor*padding.t);
}

Padding operator*(const Padding &padding, double factor) {
    return Padding(padding.l*factor, padding.b*factor, padding.r*factor, padding.t*factor);
}

Padding operator/(const Padding &padding, double divisor) {
    return Padding(padding.l/divisor, padding.b/divisor, padding.r/divisor, padding.t/divisor);
}

}
