#include <sys/apic.h>
#include <sys/pic.h>
#include <io/ports.h>
#include <mm/vmm.h>
#include <klibc/vector.h>
#include <drivers/serial.h>
#include <io/msr.h>

extern uint32_t lapic_base;
vector_t cpu_vector;
vector_t iso_vector;
vector_t ioapic_vector;
vector_t nmi_vector;

uint8_t cpu_count = 0;

extern uint32_t ioapic_read(void* ioapic_base, uint32_t addr);
extern void ioapic_write(void* ioapic_base, uint32_t addr, uint32_t data);
extern uint32_t read_lapic(lapic_reg_t reg);
extern void write_lapic(lapic_reg_t reg, uint32_t data);


uint32_t parse_madt() {
    madt_t *madt = (madt_t *)search_sdt_header("APIC");

    if (!madt) {
        sprintf("No MADT...\n");
        return 1;
    }

    sprintf("[MADT] Found MADT 0x%08x\n", madt);
    vector_init(&cpu_vector);
    vector_init(&iso_vector);
    vector_init(&ioapic_vector);
    vector_init(&nmi_vector);
    uint32_t bytes_for_entries = madt->header.length - (sizeof(madt->header) + sizeof(madt->local_apic_addr) + sizeof(madt->apic_flags));
    lapic_base = madt->local_apic_addr;

    sprintf("[MADT] MADT entries:\n");
    for (uint32_t e = 0; e < bytes_for_entries; e++) {
        uint8_t type = madt->entries[e++];
        uint8_t size = madt->entries[e++];
        
        switch (type) {
            case 0: {
                madt_ent0_t* ent = (madt_ent0_t*)&(madt->entries[e]);
                sprintf("   LAPIC: ACPI ID %3d, APIC ID %3d, flags 0x%08x\n", ent->acpi_processor_id, ent->apic_id, ent->cpu_flags);
                vector_add(&cpu_vector, (void*)ent);
            } break;
            case 1: {
                madt_ent1_t* ent = (madt_ent1_t*)&(madt->entries[e]);
                sprintf("   IOAPIC: id %3d, addr 0x%08x, GSI Base 0x%08x\n", ent->ioapic_id, ent->ioapic_addr, ent->gsi_base);
                vector_add(&ioapic_vector, (void*)ent);

                /* Map the IOAPICs into virtual memory */
                vmm_map((void*)ent->ioapic_addr, (void*)ent->ioapic_addr, 2, VMM_PRESENT | VMM_WRITE);
            } break;
            case 2: {
                madt_ent2_t* ent = (madt_ent2_t*)&(madt->entries[e]);
                sprintf("   ISO: bus %3d, IRQ %3d, GSI %3d, flags 0x%08x\n", ent->bus_src, ent->gsi, ent->flags);
                vector_add(&iso_vector, (void*)ent);
            } break;
            case 4: {
                madt_ent4_t* ent = (madt_ent4_t*)&(madt->entries[e]);
                sprintf("   NMI: Processor ID %3d, LINT %3d, Flags %3d\n", ent->acpi_processor_id, ent->lint, ent->flags);
                vector_add(&nmi_vector, (void *)ent);
            } break;
            case 5: {
                madt_ent5_t* ent = (madt_ent5_t*)&(madt->entries[e]);
                sprintf("   LAPIC override: addr 0x%08x\n", ent->local_apic_override);
                lapic_base = ent->local_apic_override;
            } break;
            default: {
                sprintf("   Unsupported entry %3u\n", type);
            } break;
        }

        e += size - 3;
    }

    sprintf("[APIC] LAPIC addr: 0x%08x\n", lapic_base);
    sprintf("[APIC] CPUs: %u, ISOs: %u, IOAPICs: %u, NMIs: %u\n", cpu_vector.items_count, iso_vector.items_count, ioapic_vector.items_count, nmi_vector.items_count);

    vmm_map((void*)lapic_base, (void*)lapic_base, 2, VMM_PRESENT | VMM_WRITE);

    return 0;
}

uint8_t apic_get_gsi_max(madt_ent1_t* ioapic) {
    void* ioapic_addr = (void*)ioapic->ioapic_addr;

    uint32_t data = ioapic_read(ioapic_addr, 1) >> 16;
    uint8_t ret = (uint8_t)data;
    return ret & ~(1 << 7);
}

madt_ent1_t* apic_find_valid_ioapic(uint32_t gsi) {
    madt_ent1_t* valid_ioapic = (madt_ent1_t*)0;

    madt_ent1_t** ioapics = (madt_ent1_t**)vector_items(&ioapic_vector);
    for (uint32_t i = 0; i < ioapic_vector.items_count; i++) {
        madt_ent1_t* ioapic = ioapics[i];
        uint32_t gsi_max = (uint32_t)apic_get_gsi_max(ioapic) + ioapic->gsi_base;

        if (ioapic->gsi_base <= gsi && gsi_max >= gsi)
            valid_ioapic = ioapic;
    }

    return valid_ioapic;
}

uint8_t apic_write_redirection_table(madt_ent1_t* ioapic, uint32_t gsi, uint64_t data) {
    if (!ioapic)
        return 1;

    void* valid_addr = (void*)ioapic->ioapic_addr;
    uint32_t reg = ((gsi - ioapic->gsi_base) * 2) + 16;

    ioapic_write(valid_addr, reg + 0, (uint32_t)data);
    ioapic_write(valid_addr, reg + 1, (uint32_t)(data >> 32));
    return 0;
}

uint64_t apic_read_redirection_table(madt_ent1_t* ioapic, uint32_t gsi) {
    if (!ioapic)
        return REDIRECT_TABLE_BAD_READ;

    void* valid_addr = (void*)ioapic->ioapic_addr;
    uint32_t reg = ((gsi - ioapic->gsi_base) * 2) + 16;

    uint64_t data_read = (uint64_t)ioapic_read(valid_addr, reg + 0);
    data_read |= (uint64_t)(ioapic_read(valid_addr, reg + 1)) << 32;
    return data_read;
}

void mask_gsi(madt_ent1_t* ioapic, uint32_t gsi) {
    uint64_t current_data = apic_read_redirection_table(ioapic, gsi);
    if (current_data == REDIRECT_TABLE_BAD_READ)
        return;

    apic_write_redirection_table(ioapic, gsi, current_data | (1 << 16));
}

void redirect_gsi(madt_ent1_t* ioapic, uint8_t irq, uint32_t gsi, uint16_t flags, uint8_t apic) {
    uint64_t redirect = (uint64_t)irq + 32;

    if (flags & 1 << 1)
        redirect |= 1 << 13;

    if (flags & 1 << 3)
        redirect |= 1 << 15;

    redirect |= ((uint64_t)apic) << 56;
    apic_write_redirection_table(ioapic, gsi, redirect);
}

void apic_configure_ap() {
    write_msr(APIC_BASE_MSR, (read_msr(APIC_BASE_MSR) | APIC_BASE_MSR_ENABLE) & ~(1<<10));
    write_lapic(LAPIC_SIVR, read_lapic(LAPIC_SIVR) | 0x1FF);
}

uint32_t apic_configure() {
    pic_disable();

    if (parse_madt())
        return 1;

    apic_configure_ap();

    madt_ent1_t** ioapics = (madt_ent1_t**)vector_items(&ioapic_vector);
    madt_ent2_t** isos = (madt_ent2_t**)vector_items(&iso_vector);
    uint8_t mapped_irqs[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    for (uint32_t i = 0; i < ioapic_vector.items_count; i++) {
        madt_ent1_t* ioapic = ioapics[i];

        sprintf("[APIC][IOAPIC#%u] Masked GSI's {", ioapic->ioapic_id);
        for (uint32_t gsi = ioapic->gsi_base; gsi < apic_get_gsi_max(ioapic); gsi++) {
            mask_gsi(ioapic, gsi);
            sprintf(" %d", gsi);
        }
        sprintf(" }\n");

        uint32_t gsi_max = (uint32_t)apic_get_gsi_max(ioapic) + ioapic->gsi_base;

        sprintf("[APIC][IOAPIC#%u] Mapped ISO to GSI's {\n", ioapic->ioapic_id);
        for (uint32_t i = 0; i < iso_vector.items_count; i++) {
            madt_ent2_t* iso = isos[i];

            if (ioapic->gsi_base <= iso->gsi && gsi_max >= iso->gsi) {
                redirect_gsi(ioapic, iso->irq_src, iso->gsi, iso->flags, 0);
                if (iso->gsi < 16)
                    mapped_irqs[iso->gsi] = 1;

                sprintf("   %3u GSI -> %3u IRQ (LAPIC %u)\n", iso->gsi, iso->irq_src, 0);
            }
        }
        sprintf("}\n");
    }

    sprintf("[APIC] Mapped GSI's {\n");
    for (uint8_t i = 0; i < 16; i++) {
        if (!mapped_irqs[i]) {
            madt_ent1_t* ioapic = apic_find_valid_ioapic(i);
            redirect_gsi(ioapic, i, (uint32_t)i, 0, 0);

            sprintf("   %3u GSI -> %3u IRQ (LAPIC %u)\n", i, i, 0);
        }
    }
    sprintf("}\n");

    return 0;
}

void apic_EOI() {
    write_lapic(LAPIC_EOI, 0);
}