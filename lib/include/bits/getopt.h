#pragma once

int getopt(int argc, char* const* argv, const char* optstring);

extern char* optarg;
extern int optind;
extern int opterr;
extern int optopt;
