#ifndef __AWA_ACPI_MADT_H
#define __AWA_ACPI_MADT_H

#include "sdt.h"

#define ACPI_MADT_LAPIC 0x0  // Local APIC
#define ACPI_MADT_IOAPIC 0x1 // I/O APIC
#define ACPI_MADT_INTSO 0x2  // Interrupt Source Override

/**
 * @brief ACPI Interrupt Controller Structure (ICS) Header
 *
 */
typedef struct
{
    uint8_t type;
    uint8_t length;
} __attribute__((packed)) acpi_ics_hdr_t;

/**
 * @brief ACPI Processor Local APIC Structure (PLAS)
 * This structure tell information about our Local APIC per processor. Including
 * the MMIO addr.
 *
 */
typedef struct
{
    acpi_ics_hdr_t header;
    uint8_t processor_id;
    uint8_t apic_id;
    uint32_t flags;
} __attribute__((packed)) acpi_apic_t;

/**
 * @brief ACPI IO APIC Structure (IOAS)
 *
 * This structure tell information about our I/O APIC on motherboard. Including
 * the MMIO addr.
 *
 */
typedef struct
{
    acpi_ics_hdr_t header;
    uint8_t ioapic_id;
    uint8_t reserved;
    uint32_t ioapic_addr;
    // The global system interrupt offset for this IOAPIC. (Kind of IRQ offset for a slave IOAPIC)
    uint32_t gis_offset;
} __attribute__((packed)) acpi_ioapic_t;

/**
 * @brief ACPI Interrupt Source Override (INTSO)
 *
 * According to the ACPI Spec, the IRQ config between APIC and 8259 PIC can be
 * assumed to be identically mapped. However, some manufactures may have their
 * own preference and hence expections may be introduced. This structure provide
 * information on such exception.
 *
 */
typedef struct
{
    acpi_ics_hdr_t header;
    uint8_t bus;
    // source, which is the original IRQ back in the era of IBM PC/AT, the 8259
    // PIC
    uint8_t source;
    // global system interrupt. The override of source in APIC mode
    uint32_t gsi;
    uint16_t flags;
} __attribute__((packed)) acpi_intso_t;

typedef struct
{
    acpi_sdthdr_t header;
    void* apic_addr;
    uint32_t flags;
    // Here is a bunch of packed ICS reside here back-to-back.
} __attribute__((packed)) acpi_madt_t;

typedef struct
{
    void* apic_addr;
    acpi_apic_t* apic;
    acpi_ioapic_t* ioapic;
    acpi_intso_t** irq_exception;
} __attribute__((packed)) acpi_madt_toc_t;

#endif
