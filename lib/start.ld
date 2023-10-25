ENTRY(_start)

__start = 0x00001000;

SECTIONS
{
    . = __start;
    .interp         : { *(.interp) }
    .text : { *(.text) }

    .plt            : { *(.plt) *(.iplt) }
    .plt.got        : { *(.plt.got) }
    .plt.sec        : { *(.plt.sec) }

    .rela.init      : { *(.rela.init) }
    .rela.text      : { *(.rela.text .rela.text.* .rela.gnu.linkonce.t.*) }
    .rela.fini      : { *(.rela.fini) }
    .rela.rodata    : { *(.rela.rodata .rela.rodata.* .rela.gnu.linkonce.r.*) }
    .rela.data.rel.ro   : { *(.rela.data.rel.ro .rela.data.rel.ro.* .rela.gnu.linkonce.d.rel.ro.*) }
    .rela.data      : { *(.rela.data .rela.data.* .rela.gnu.linkonce.d.*) }
    .rela.tdata     : { *(.rela.tdata .rela.tdata.* .rela.gnu.linkonce.td.*) }
    .rela.tbss      : { *(.rela.tbss .rela.tbss.* .rela.gnu.linkonce.tb.*) }
    .rela.ctors     : { *(.rela.ctors) }
    .rela.dtors     : { *(.rela.dtors) }
    .rela.got       : { *(.rela.got) }
    .rela.bss       : { *(.rela.bss .rela.bss.* .rela.gnu.linkonce.b.*) }
    .rela.ldata     : { *(.rela.ldata .rela.ldata.* .rela.gnu.linkonce.l.*) }
    .rela.lbss      : { *(.rela.lbss .rela.lbss.* .rela.gnu.linkonce.lb.*) }
    .rela.lrodata   : { *(.rela.lrodata .rela.lrodata.* .rela.gnu.linkonce.lr.*) }
    .rela.ifunc     : { *(.rela.ifunc) }
    .rela.plt       :
    {
        *(.rela.plt)
        PROVIDE_HIDDEN (__rela_iplt_start = .);
        *(.rela.iplt)
        PROVIDE_HIDDEN (__rela_iplt_end = .);
    }
    .relr.dyn : { *(.relr.dyn) }


    .note.gnu.build-id  : { *(.note.gnu.build-id) }
    .hash           : { *(.hash) }
    .gnu.hash       : { *(.gnu.hash) }

    . = ALIGN(4K);
    .data : { *(.data) }
    .bss : {
        _sbss = .;
        *(.bss)
        _ebss = .;
    }
}
