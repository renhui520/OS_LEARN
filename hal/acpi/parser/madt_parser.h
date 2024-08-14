#ifndef __AWA_PARSER_MADT_PARSER_H
#define __AWA_PARSER_MADT_PARSER_H

#include <hal/acpi/acpi.h>

/**
 * @brief Parse the MADT and populated into main TOC
 * 
 * @param rsdt RSDT
 * @param toc The main TOC
 */
void madt_parse(acpi_madt_t* madt, acpi_context* toc);

#endif
