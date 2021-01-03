/**
* Python port of Milton Bradley's Merlin Electronic Game emulator
 *
 * This was inspired and ported from the work done by Dominic Thibodeau (hotkeysoft).
 * https://github.com/hotkeysoft/emulators/tree/master/TMS1000
 *
 * Thank you Dominic!!
 */
#ifndef TMS1XX0_H
#define TMS1XX0_H

#define R_WIDTH 15

typedef unsigned char BYTE;
typedef unsigned short WORD;


class ROM {
    private:
    BYTE *data_;
    int rom_size_;
    public:
    ROM();
    void load_rom(std::string filename);
    BYTE get_data(WORD index);
};

class CPUState {
    private:
    BYTE reg_a_;
    bool reg_cl_;
    BYTE reg_ca_;
    BYTE reg_cb_;
    BYTE reg_cs_;
    BYTE reg_k_;
    BYTE reg_o_;
    BYTE reg_pa_;
    BYTE reg_pb_;
    BYTE reg_pc_;
    bool reg_r_[R_WIDTH];
    bool reg_s_;
    bool reg_sl_;
    BYTE reg_sr_;
    BYTE reg_x_;
    BYTE reg_y_;
    void(*output_r_cb_)(int, bool);
    void(*output_o_cb_)(int);
    int(*input_k_cb_)(int);

    public:
    CPUState();
    void increment_pc();

    BYTE get_pc();
    void set_pc(BYTE);
    BYTE get_pb();
    void set_pb(BYTE);
    BYTE get_pa();
    void set_pa(BYTE);

    BYTE get_ca();
    void set_ca(BYTE);
    BYTE get_cb();
    void set_cb(BYTE);
    BYTE get_cs();
    void set_cs(BYTE);
    void com_cb();

    bool get_cl();
    void set_cl(bool);

    bool get_s();
    void set_s(bool);
    bool get_sl();
    void set_sl(bool);
    BYTE get_sr();
    void set_sr(BYTE);

    BYTE get_a();
    void set_a(BYTE);

    BYTE get_y();
    void set_y(BYTE);
    void inc_y();
    void dec_y();

    BYTE get_x();
    void set_x(BYTE);
    void com_x();

    BYTE get_k();
    void set_k(BYTE);

    void set_r_index(BYTE);
    void rst_r_index(BYTE);

    void set_o(BYTE);

    void set_output_r_cb(void(*)(int, bool));
    void set_output_o_cb(void(*)(int));
    void set_input_k_cb(int(*)(int));
};

class TMS1100 {
    private:
    CPUState *cpu_;
    ROM *rom_;
    BYTE *ram_;
    void(TMS1100::*op_code_func_[256])(BYTE, bool);
    BYTE op_code_constant_[256];

    void uADC_a(BYTE val);
    void uADC_y(BYTE val);

    void exec(BYTE);
    void setup_op_codes();

    // register to register
    void op_tay(BYTE, bool);
    void op_tya(BYTE, bool);
    void op_cla(BYTE, bool);

    // transfer register to memory
    void op_tam(BYTE, bool);
    void op_tamiyc(BYTE, bool);
    void op_tamdyn(BYTE, bool);
    void op_tamza(BYTE, bool);

    // memory to register
    void op_tmy(BYTE, bool);
    void op_tma(BYTE, bool);
    void op_xma(BYTE, bool);

    // arithmetic
    void op_amaac(BYTE, bool);
    void op_saman(BYTE, bool);
    void op_imac(BYTE, bool);

    void op_dman(BYTE, bool);
    void op_a_aac(BYTE, bool);

    void op_iyc(BYTE, bool);
    void op_dyn(BYTE, bool);
    void op_cpaiz(BYTE, bool);

    // arithmetic compare
    void op_alem(BYTE, bool);

    // logical compare
    void op_mnea(BYTE, bool);
    void op_mnez(BYTE, bool);
    void op_ynea(BYTE, bool);

    void op_ldp(BYTE, bool);
    void op_tcy(BYTE, bool);
    void op_ynec(BYTE, bool);
    void op_tcmiy(BYTE, bool);

    // bits in memory
    void op_comx(BYTE, bool);
    void op_comc(BYTE, bool);

    void op_sbit(BYTE, bool);
    void op_rbit(BYTE, bool);
    void op_tbit1(BYTE, bool);

    // input
    void op_knez(BYTE, bool);
    void op_tka(BYTE, bool);

    // output
    void op_setr(BYTE, bool);
    void op_rstr(BYTE, bool);
    void op_tdo(BYTE, bool);

    void op_ldx(BYTE, bool);

    // rom addressing
    void op_br(BYTE, bool);
    void op_call(BYTE, bool);
    void op_retn(BYTE, bool);

    public:
    TMS1100(ROM *);
    ~TMS1100();
    void step();

    void set_output_r_cb(void(*)(int, bool));
    void set_output_o_cb(void(*)(int));
    void set_input_k_cb(int(*)(int));
};

#endif