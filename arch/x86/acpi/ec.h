#pragma once

#include <uacpi/uacpi.h>

uacpi_iteration_decision acpi_ec_init(void*, uacpi_namespace_node* node, uint32_t);
