#include "format_circuits.h"
#include "common/String.h"
#include <cmath>

String format_value(double value_, const String &unit, int prec) {
    if (fabs(value_) == 0.0)
        return String::Build("0.0 ", unit);

    int degree = (int)(log10(value_ < 0 ? -value_ : value_) / 3);
    double scaled_ = value_ * pow(1000, -degree);
    String scaled, value;

    if (prec > 0) {
        StringBuilder temp, temp2;
        temp  << Format::Precision(prec) << scaled_;
        temp2 << Format::Precision(prec) << value_;
        scaled = temp.Build();
        value = temp2.Build();
    }
    else {
        scaled = String::Build(scaled_);
        value = String::Build(value_);
    }

    if (degree < 0) {
        if (-degree > prefix_count)
            return String::Build(value, " ", unit);
        return String::Build(scaled, " ", dec_prefixes[-degree - 1], unit);
    }
    else if (degree > 0) {
        if (degree > prefix_count)
            return String::Build(value, " ", unit);
        return String::Build(scaled, " ", inc_prefixes[degree - 1], unit);
    }
    return String::Build(value, " ", unit);
}