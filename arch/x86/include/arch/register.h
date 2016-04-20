#ifndef __REGISTER_H_
#define __REGISTER_H_

#include <kernel/compiler.h>
#include <arch/processor.h>

#define eflags_cf_pos       0
#define eflags_pf_pos       2
#define eflags_af_pos       4
#define eflags_zf_pos       6
#define eflags_sf_pos       7
#define eflags_tf_pos       8
#define eflags_if_pos       9
#define eflags_df_pos       10
#define eflags_of_pos       11
#define eflags_iopl_pos     12
#define eflags_nt_pos       14
#define eflags_rf_pos       16
#define eflags_vm_pos       17
#define eflags_ac_pos       18
#define eflags_vif_pos      19
#define eflags_vip_pos      20
#define eflags_id_pos       21

#define cr0_pe_pos          0
#define cr0_mp_pos          1
#define cr0_em_pos          2
#define cr0_ts_pos          3
#define cr0_et_pos          4
#define cr0_ne_pos          5
#define cr0_wp_pos          16
#define cr0_am_pos          18
#define cr0_nw_pos          29
#define cr0_cd_pos          30
#define cr0_pg_pos          31

#define cr4_vme_pos         0
#define cr4_pvi_pos         1
#define cr4_tsd_pos         2
#define cr4_de_pos          3
#define cr4_pse_pos         4
#define cr4_pae_pos         5
#define cr4_mce_pos         6
#define cr4_pge_pos         7
#define cr4_pce_pos         8
#define cr4_osfxsr_pos      9
#define cr4_osxmmexcpt_pos  10
#define cr4_vmxe_pos        13
#define cr4_smxe_pos        14
#define cr4_pcide_pos       17
#define cr4_osxsave_pos     18
#define cr4_smep_pos        20
#define cr4_smap_pos        21

#define register_bit_mask(reg_bit) (1 << reg_bit##_pos)
#define register_bit_pos(reg_bit) reg_bit##_pos
#define register_name(reg) #reg

#ifndef __ASSEMBLER__

#define general_reg_union(letter) \
    {                                   \
        unsigned int e##letter##x;      \
        unsigned short letter##x;       \
        struct {                        \
            unsigned char letter##l;    \
            unsigned char letter##h;    \
        };                              \
    }

#define index_reg_union(letter) \
    {                                   \
        unsigned int e##letter##i;      \
        unsigned short letter##i;       \
    }

struct regs_struct {
    union general_reg_union(a);
    union general_reg_union(b);
    union general_reg_union(c);
    union general_reg_union(d);
    union index_reg_union(s);
    union index_reg_union(d);
    unsigned int eflags;
} __attribute__ ((packed));

int eflags_bits_string_get(unsigned int eflags, char *buffer);
int cr0_bits_string_get(unsigned int cr0, char *buffer);
int cr4_bits_string_get(unsigned int cr4, char *buffer);

extern unsigned long eip_get();

extern inline unsigned int dr0_get() {
    unsigned int rv;
    asm volatile("mov %%dr0, %0" : "=r" (rv));
    return rv;
}

extern inline unsigned int dr1_get() {
    unsigned int rv;
    asm volatile("mov %%dr2, %0" : "=r" (rv));
    return rv;
}

extern inline unsigned int dr2_get() {
    unsigned int rv;
    asm volatile("mov %%dr2, %0" : "=r" (rv));
    return rv;
}

extern inline unsigned int dr3_get() {
    unsigned int rv;
    asm volatile("mov %%dr3, %0" : "=r" (rv));
    return rv;
}

#define eflags_get() \
    ({                                              \
        unsigned int rv;                            \
        asm volatile("pushf; pop %0" : "=r" (rv));  \
        rv;                                         \
    })

#define cr0_get() \
    ({                                              \
        unsigned int rv;                            \
        asm volatile("mov %%cr0, %0" : "=r" (rv));  \
        rv;                                         \
    })

#define cr2_get() \
    ({                                              \
        unsigned int rv;                            \
        asm volatile("mov %%cr2, %0" : "=r" (rv));  \
        rv;                                         \
    })

#define cr3_get() \
    ({                                              \
        unsigned int rv;                            \
        asm volatile("mov %%cr3, %0" : "=r" (rv));  \
        rv;                                         \
    })

#define cr4_get() \
    ({                                              \
        unsigned int rv;                            \
        asm volatile("mov %%cr4, %0" : "=r" (rv));  \
        rv;                                         \
    })

#define cs_get() \
    ({                                              \
        unsigned int rv;                            \
        asm volatile("mov %%cs, %0" : "=r" (rv));   \
        rv;                                         \
    })

#define ds_get() \
    ({                                              \
        unsigned int rv;                            \
        asm volatile("mov %%ds, %0" : "=r" (rv));   \
        rv;                                         \
    })

#define es_get() \
    ({                                              \
        unsigned int rv;                            \
        asm volatile("mov %%es, %0" : "=r" (rv));   \
        rv;                                         \
    })

#define fs_get() \
    ({                                              \
        unsigned int rv;                            \
        asm volatile("mov %%fs, %0" : "=r" (rv));   \
        rv;                                         \
    })

#define gs_get() \
    ({                                              \
        unsigned int rv;                            \
        asm volatile("mov %%gs, %0" : "=r" (rv));   \
        rv;                                         \
    })

#define esp_get() \
    ({                                              \
        unsigned int rv;                            \
        asm volatile("mov %%esp, %0" : "=r" (rv));  \
        rv;                                         \
    })

#endif /* __ASSEMBLER__ */

#endif /* __REGISTER_H_ */
