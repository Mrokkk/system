#ifndef INCLUDE_KERNEL_OPCODE_H_
#define INCLUDE_KERNEL_OPCODE_H_

#define I_GENERIC   0
#define I_READMEM   1
#define I_WRITEMEM  2

struct i_decoded {
    void *ip; /* Use also for errors */
    char instruction[32]; /* String with decoded instruction */
    char dump_size; /* Size of instruction */
    char dump[32]; /* Whole instruction bytes */
    char type; /* Type of instruction */
};

typedef int (*ip_decoder_t)(void *, struct i_decoded *);

int instruction_decoder_register(ip_decoder_t *);

#endif /* INCLUDE_KERNEL_OPCODE_H_ */
