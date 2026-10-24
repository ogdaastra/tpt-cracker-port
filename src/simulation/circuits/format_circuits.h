#ifndef FORMAT_CIRCUIT_H
#define FORMAT_CIRCUIT_H

class String;

const char inc_prefixes[] = {'k', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y'};
const char dec_prefixes[] = {'m', 'u', 'n', 'p', 'f', 'a', 'z', 'y'};
const int prefix_count = 8;

String format_value(double value, const String &unit, int prec=-1);

#endif