#pragma once

void sighan();
#define die(...) { printf(__VA_ARGS__); sighan(); }
#define die_perror(...) { perror(__VA_ARGS__); sighan(); }
