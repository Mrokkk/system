#ifndef INCLUDE_KERNEL_MACRO_H_
#define INCLUDE_KERNEL_MACRO_H_

#define GET_1ND_ARG(arg1, ...) \
    arg1

#define GET_2ND_ARG(arg1, arg2, ...) \
    arg2

#define GET_3RD_ARG(arg1, arg2, arg3, ...) \
    arg3

#define GET_4RD_ARG(arg1, arg2, arg3, arg4, ...) \
    arg4

#define MACRO_CHOOSER_2(macro1, macro2, ...) \
    GET_3RD_ARG(__VA_ARGS__, macro2, macro1)

#define MACRO_CHOOSER_3(macro1, macro2, macro3, ...) \
    GET_4RD_ARG(__VA_ARGS__, macro3, macro2, macro1)

#define REAL_VAR_MACRO_2(macro1, macro2, ...) \
    MACRO_CHOOSER_2(macro1, macro2, __VA_ARGS__)(__VA_ARGS__)

#define REAL_VAR_MACRO_3(macro1, macro2, macro3, ...) \
    MACRO_CHOOSER_2(macro1, macro2, macro3,  __VA_ARGS__)(__VA_ARGS__)

#endif /* INCLUDE_KERNEL_MACRO_H_ */
