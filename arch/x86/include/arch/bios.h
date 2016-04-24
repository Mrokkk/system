#ifndef __BIOS_H_
#define __BIOS_H_

#include <arch/real_mode.h>

struct bios_data_area {
    unsigned short com_ports[4];
    unsigned short lpt_ports[3];
    unsigned short ebda_base;
    struct {
        unsigned short floppy_drives:1;
        unsigned short fpu:1;
        unsigned short ps2_mouse_integrated:1;
        unsigned short reserved1:1;
        unsigned short initial_video_mode:2;
        unsigned short nr_floppy_drives:2;
        unsigned short game_port:1;
        unsigned short nr_com_ports:3;
        unsigned short reserved2:2;
        unsigned short nr_lpt_ports:2;
    } equipment_word;
    unsigned char post_reserved1;
    unsigned int base_mem;
    struct {
        unsigned char right_shift_status:1;
        unsigned char left_shift_status:1;
        unsigned char control_status:1;
        unsigned char alt_status:1;
        unsigned char scroll_lock_status:1;
        unsigned char num_lock_status:1;
        unsigned char caps_lock_status:1;
        unsigned char insert_status:1;
    } keyboard_state_flags1, keyboard_state_flags2;
    unsigned char irq_region;
    unsigned short keyboard_next_char;
    unsigned short keyboard_next_free;
    unsigned char keyboard_buffer[32];
    unsigned char drive_calibration_state;
    unsigned char floppy_motor_status;
    unsigned char floppy_motor_time;
    unsigned char floppy_status;
    unsigned char floppy_controller_register0;
    unsigned char floppy_controller_register1;
    unsigned char floppy_controller_register2;
    unsigned char floppy_nr_cylinders;
    unsigned char floppy_nr_heads;
    unsigned char floppy_nr_sectors;
    unsigned char floppy_nr_bytes_written;
    unsigned char display_mode;
    unsigned short display_columns;
    unsigned short display_buffer_size;
    unsigned short display_current_page_address;
    unsigned short display_cursor_pos_page1;
    unsigned short display_cursor_pos_page2;
    unsigned short display_cursor_pos_page3;
    unsigned short display_cursor_pos_page4;
    unsigned short display_cursor_pos_page5;
    unsigned short display_cursor_pos_page6;
    unsigned short display_cursor_pos_page7;
    unsigned short display_cursor_pos_page8;
    unsigned short display_cursor_line; /* ??? */
    unsigned short display_current_page;
    unsigned short display_base_address;
    unsigned char display_mode_register;
    unsigned char display_palette_register; /* ??? */
    unsigned short additional_rom_offset;
    unsigned short additional_rom_segment;
    unsigned char post_reserved;
    unsigned int system_counter;
    unsigned char noon; /* ??? */
    unsigned char break_state;
    unsigned short mem_test_marker;
    unsigned char nr_hard_drives;
    unsigned short int13h_region;
    unsigned char lpt1_delay;
    unsigned char lpt2_delay;
    unsigned char lpt3_delay;
    unsigned char lpt4_delay;
    unsigned char com1_delay;
    unsigned char com2_delay;
    unsigned char com3_delay;
    unsigned char com4_delay;
    unsigned short keyboard_buffer_start;
    unsigned short keyboard_buffer_end;
    unsigned short lines_on_screen;
    unsigned short char_height;
    unsigned short graphic_card_state;
    unsigned char floppy_params;
    unsigned char hdd_state_register;
    unsigned char hdd_error_register;
    unsigned char hdd_irq_marker;

} __attribute__((packed));

#define BDA_BASE_ADDRESS    0x400
#define EBDA_BASE_ADDRESS    0x9FC00

void bios_int10h();
void bios_int15h();

#endif /* ARCH_X86_BIOS_H_ */
