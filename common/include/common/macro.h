#pragma once

#define GET_1ND_ARG(_1, ...)                        _1
#define GET_2ND_ARG(_1, _2, ...)                    _2
#define GET_3RD_ARG(_1, _2, _3, ...)                _3
#define GET_4TH_ARG(_1, _2, _3, _4, ...)            _4
#define GET_5TH_ARG(_1, _2, _3, _4, _5, ...)        _5
#define GET_6TH_ARG(_1, _2, _3, _4, _5, _6, ...)    _6

#define MACRO_CHOOSER_2(m1, m2, ...) \
    GET_3RD_ARG(__VA_ARGS__, m2, m1)

#define MACRO_CHOOSER_3(m1, m2, m3, ...) \
    GET_4TH_ARG(__VA_ARGS__, m3, m2, m1)

#define MACRO_CHOOSER_4(m1, m2, m3, m4, ...) \
    GET_5TH_ARG(__VA_ARGS__, m4, m3, m2, m1)

#define MACRO_CHOOSER_5(m1, m2, m3, m4, m5, ...) \
    GET_6TH_ARG(__VA_ARGS__, m5, m4, m3, m2, m1)

#define REAL_VAR_MACRO_2(m1, m2, ...) \
    MACRO_CHOOSER_2(m1, m2, __VA_ARGS__)(__VA_ARGS__)

#define REAL_VAR_MACRO_3(m1, m2, m3, ...) \
    MACRO_CHOOSER_3(m1, m2, m3, __VA_ARGS__)(__VA_ARGS__)

#define REAL_VAR_MACRO_4(m1, m2, m3, m4, ...) \
    MACRO_CHOOSER_4(m1, m2, m3, m4, __VA_ARGS__)(__VA_ARGS__)

#define REAL_VAR_MACRO_5(m1, m2, m3, m4, m5, ...) \
    MACRO_CHOOSER_5(m1, m2, m3, m4, m5, __VA_ARGS__)(__VA_ARGS__)
