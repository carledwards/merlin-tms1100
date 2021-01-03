/**
 * @file tms1xx0.cpp
 * @author Carl Edwards
 * 
 * C++ port of Milton Bradley's Merlin Electronic Game emulator
 *
 * This was inspired and ported from the work done by Dominic Thibodeau (hotkeysoft).
 * https://github.com/hotkeysoft/emulators/tree/master/TMS1000
 *
 * Thank you Dominic!!
 */
#include <iomanip>
#include <string>
#include <fstream>
#include <vector>
#include <sstream>
#include "tms1xx0.h"
using namespace std; 

#define SET1(X) (X & 0x01)
#define SET2(X) (X & 0x03)
#define SET3(X) (X & 0x07)
#define SET4(X) (X & 0x0F)
#define SET6(X) (X & 0x3F)
#define NOT4(X) ((~X) & 0x0F)
#define CURR_RAM (ram_[(cpu_->get_x() << 4) | cpu_->get_y()])
// #define DEBUG_FUNCTION printf("func: %s\n", __FUNCTION__)
#define DEBUG_FUNCTION 

static BYTE pc_sequence[64] = {
    0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x3E,
    0x3D, 0x3B, 0x37, 0x2F, 0x1E, 0x3C, 0x39, 0x33,
    0x27, 0x0E, 0x1D, 0x3A, 0x35, 0x2B, 0x16, 0x2C,
    0x18, 0x30, 0x21, 0x02, 0x05, 0x0B, 0x17, 0x2E,
    0x1C, 0x38, 0x31, 0x23, 0x06, 0x0D, 0x1B, 0x36,
    0x2D, 0x1A, 0x34, 0x29, 0x12, 0x24, 0x08, 0x11,
    0x22, 0x04, 0x09, 0x13, 0x26, 0x0C, 0x19, 0x32,
    0x25, 0x0A, 0x15, 0x2A, 0x14, 0x28, 0x10, 0x20
};

BYTE inverseSequence(BYTE addr) {
    for (BYTE i = 0; i < 64; ++i) {
        if (pc_sequence[i] == addr) {
            return i;
        }
    }
    throw runtime_error("inverseSequence: can't happen");
}

ROM::ROM() {
    rom_size_ = 0;
    data_ = NULL;
}

BYTE ROM::get_data(WORD index) {
    if (index < 0 || index >= rom_size_) {
        std::ostringstream oss;
        oss << "rom.get_data index '" << index << "' out of of range: " << rom_size_;
        throw runtime_error(oss.str());
    }
    return data_[index];
}

void ROM::load_rom(std::string filename) {
    ifstream ifd(filename, ios::binary | ios::in | ios::ate);
    if (!ifd.is_open()) {
        throw runtime_error("error opening file: " + filename);
    }
    int size = ifd.tellg();
    ifd.seekg(0, ios::beg);
    std::vector<char> buffer;
    buffer.resize(size);
    ifd.read(buffer.data(), size);

	// Rearrange the ROM according to the PC sequence
	// so it appear as linear (and then we can simply 
	// increment PC)
    unsigned char *remappedROM = new unsigned char[size];

    for (int i = 0; i < size; ++i) {
        // remappedROM[i] = buffer[i];
        // Remap memory addresses
        remappedROM[i] = buffer[i & 0xFFC0 | pc_sequence[i & 0x3F]];

        // Remap branch and jump instructions
        if (remappedROM[i] & 0x80) {
            BYTE newAddress = inverseSequence(remappedROM[i] & 0x3F);
            remappedROM[i] &= 0xC0; // Keep opcode
            remappedROM[i] |= newAddress; // new operand
        }
    }

    delete data_;
    data_ = remappedROM;
    rom_size_ = size;
}

CPUState :: CPUState() {
    output_r_cb_ = NULL;
    output_o_cb_ = NULL;
    input_k_cb_ = NULL;
    set_x(0xAA);
    set_y(0xAA);
    set_a(0xAA);
    set_s(false);
    set_sl(false);
    set_sr(0x00);
    set_pc(0x00);
    set_pa(0xFF);
    set_pb(0xFF);
    set_k(0x00);
    set_cl(false);
    set_o(0);

    for (int i = 0; i < R_WIDTH; i++) {
        reg_r_[i] = false;
    }

    set_ca(0);
    set_cb(0);
    set_cs(0);
}

void CPUState::increment_pc() {
    reg_pc_ = SET6(reg_pc_ + 1);
}

BYTE CPUState::get_pc() {
    return reg_pc_;
}

void CPUState::set_pc(BYTE pc) {
    reg_pc_ = SET6(pc);
}

BYTE CPUState::get_pa() {
    return reg_pa_;
}

void CPUState::set_pa(BYTE pa) {
    reg_pa_ = SET4(pa);
}

BYTE CPUState::get_pb() {
    return reg_pb_;
}

void CPUState::set_pb(BYTE pb) {
    reg_pb_ = SET4(pb);
}

bool CPUState::get_s() {
    return reg_s_;
}

void CPUState::set_s(bool val) {
    reg_s_ = val;
}

bool CPUState::get_sl() {
    return reg_sl_;
}

void CPUState::set_sl(bool val) {
    reg_sl_ = val;
}

BYTE CPUState::get_sr() {
    return reg_sr_;
}

void CPUState::set_sr(BYTE val) {
    reg_sr_ = val;
}

BYTE CPUState::get_a() {
    return reg_a_;
}

void CPUState::set_a(BYTE val) {
    reg_a_ = SET4(val);
}

BYTE CPUState::get_y() {
    return reg_y_;
}

void CPUState::set_y(BYTE val) {
    reg_y_ = SET4(val);
}

void CPUState::inc_y() {
    set_y(reg_y_ + 1);
}

void CPUState::dec_y() {
    set_y(reg_y_ - 1);
}

BYTE CPUState::get_x() {
    return reg_x_;
}

void CPUState::set_x(BYTE val) {
    reg_x_ = SET3(val);
}

void CPUState::com_x() {
    reg_x_ = SET3(reg_x_ ^ 4);
}

void CPUState::com_cb() {
    reg_cb_ = SET1(~reg_cb_);
}

BYTE CPUState::get_k() {
    if (input_k_cb_) {
        set_k(input_k_cb_(reg_o_));
    }
    return reg_k_;
}

void CPUState::set_k(BYTE val) {
    reg_k_ = SET4(val);
}

void CPUState::set_r_index(BYTE index) {
    if (index >=0 && index < R_WIDTH) {
        reg_r_[index] = true;
        if (output_r_cb_) {
            output_r_cb_(index, true);
        }
    }
}

void CPUState::rst_r_index(BYTE index) {
    if (index >=0 && index < R_WIDTH) {
        reg_r_[index] = false;
        if (output_r_cb_) {
            output_r_cb_(index, false);
        }
    }
}

void CPUState::set_o(BYTE val) {
    // TODO check on max bits for O
    reg_o_ = val;
    if (output_o_cb_) {
        output_o_cb_(reg_o_);
    }
}

bool CPUState::get_cl() {
    return reg_cl_;
}
void CPUState::set_cl(bool val) {
    reg_cl_ = val;
}

BYTE CPUState::get_ca() {
    return reg_ca_;
}

void CPUState::set_ca(BYTE val) {
    reg_ca_ = SET1(val);
}

BYTE CPUState::get_cb() {
    return reg_cb_;
}

void CPUState::set_cb(BYTE val) {
    reg_cb_ = SET1(val);
}

BYTE CPUState::get_cs() {
    return reg_cs_;
}

void CPUState::set_cs(BYTE val) {
    reg_cs_ = SET1(val);
}

void CPUState::set_output_r_cb(void(*output_r_cb)(int, bool)) {
    output_r_cb_ = output_r_cb;
}

void CPUState::set_output_o_cb(void(*output_o_cb)(int)) {
    output_o_cb_ = output_o_cb;
}

void CPUState::set_input_k_cb(int(*input_k_cb)(int)) {
    input_k_cb_ = input_k_cb;
}


// register to register
void TMS1100::op_tay(BYTE, bool) {
    DEBUG_FUNCTION;
    cpu_->set_y(cpu_->get_a());
}

void TMS1100::op_tya(BYTE, bool) {
    DEBUG_FUNCTION;
    cpu_->set_a(cpu_->get_y());
}

void TMS1100::op_cla(BYTE, bool) {
    DEBUG_FUNCTION;
    cpu_->set_a(0);
}

// transfer register to memory
void TMS1100::op_tam(BYTE, bool) {
    DEBUG_FUNCTION;
    CURR_RAM = cpu_->get_a();
}

void TMS1100::op_tamiyc(BYTE, bool) {
    DEBUG_FUNCTION;
    CURR_RAM = cpu_->get_a();
    cpu_->set_s(cpu_->get_y() == 0x0F);
    cpu_->inc_y();
}

void TMS1100::op_tamdyn(BYTE, bool) {
    DEBUG_FUNCTION;
    CURR_RAM = cpu_->get_a();
    cpu_->set_s(cpu_->get_y() >= 1);
    cpu_->dec_y();
}

void TMS1100::op_tamza(BYTE, bool) {
    DEBUG_FUNCTION;
    CURR_RAM = cpu_->get_a();
    cpu_->set_a(0);
}

// memory to register
void TMS1100::op_tmy(BYTE, bool) {
    DEBUG_FUNCTION;
    cpu_->set_y(CURR_RAM);
}

void TMS1100::op_tma(BYTE, bool) {
    DEBUG_FUNCTION;
    cpu_->set_a(CURR_RAM);
}

void TMS1100::op_xma(BYTE, bool) {
    DEBUG_FUNCTION;
    BYTE temp = CURR_RAM;
    CURR_RAM = cpu_->get_a();
    cpu_->set_a(temp);
}

void TMS1100::uADC_a(BYTE val) {
    DEBUG_FUNCTION;
    BYTE sum = cpu_->get_a() + val;
    cpu_->set_s(sum > 0x0F);
    cpu_->set_a(sum);
}

void TMS1100::uADC_y(BYTE val) {
    DEBUG_FUNCTION;
    BYTE sum = cpu_->get_y() + val;
    cpu_->set_s(sum > 0x0F);
    cpu_->set_y(sum);
}

// arithmetic
void TMS1100::op_amaac(BYTE, bool) {
    DEBUG_FUNCTION;
    uADC_a(CURR_RAM);
}

void TMS1100::op_saman(BYTE, bool) {
    DEBUG_FUNCTION;
    BYTE sum = NOT4(cpu_->get_a()) + CURR_RAM + 1;
    cpu_->set_s(sum > 0x0F);
    cpu_->set_a(sum);
}

void TMS1100::op_imac(BYTE, bool) {
    DEBUG_FUNCTION;
    cpu_->set_a(CURR_RAM);
    uADC_a(0x01);
}

void TMS1100::op_dman(BYTE, bool) {
    DEBUG_FUNCTION;
    cpu_->set_a(CURR_RAM);
    uADC_a(0x0F);
}

void TMS1100::op_a_aac(BYTE opcode, bool) {
    DEBUG_FUNCTION;
    uADC_a(op_code_constant_[opcode]);
}

void TMS1100::op_iyc(BYTE, bool) {
    DEBUG_FUNCTION;
    uADC_y(0x01);
}

void TMS1100::op_dyn(BYTE, bool) {
    DEBUG_FUNCTION;
    uADC_y(0x0F);
}

void TMS1100::op_cpaiz(BYTE, bool) {
    DEBUG_FUNCTION;
    BYTE sum = NOT4(cpu_->get_a()) + 1;
    cpu_->set_s(sum > 0x0F);
    cpu_->set_a(sum);
}

// arithmetic compare
void TMS1100::op_alem(BYTE, bool) {
    DEBUG_FUNCTION;
    BYTE sum = NOT4(cpu_->get_a()) + CURR_RAM + 1;
    cpu_->set_s(sum > 0x0F);
}

// logical compare
void TMS1100::op_mnea(BYTE opcode, bool) {
    DEBUG_FUNCTION;
    cpu_->set_s(CURR_RAM != cpu_->get_a());
}

void TMS1100::op_mnez(BYTE, bool) {
    DEBUG_FUNCTION;
    cpu_->set_s(CURR_RAM != 0);
}

void TMS1100::op_ynea(BYTE, bool) {
    DEBUG_FUNCTION;
    cpu_->set_s(cpu_->get_a() != cpu_->get_y());
    cpu_->set_sl(cpu_->get_s());
}

void TMS1100::op_ldp(BYTE opcode, bool) {
    DEBUG_FUNCTION;
    cpu_->set_pb(op_code_constant_[opcode]);
}

void TMS1100::op_tcy(BYTE opcode, bool) {
    DEBUG_FUNCTION;
    cpu_->set_y(op_code_constant_[opcode]);
}

void TMS1100::op_ynec(BYTE opcode, bool) {
    DEBUG_FUNCTION;
    cpu_->set_s(cpu_->get_y() != op_code_constant_[opcode]);
}

void TMS1100::op_tcmiy(BYTE opcode, bool) {
    DEBUG_FUNCTION;
    CURR_RAM = op_code_constant_[opcode];
    cpu_->inc_y();
}

// bits in memory
void TMS1100::op_comx(BYTE, bool) {
    DEBUG_FUNCTION;
    cpu_->com_x();
}

void TMS1100::op_comc(BYTE, bool) {
    DEBUG_FUNCTION;
    cpu_->com_cb();
}

void TMS1100::op_sbit(BYTE opcode, bool) {
    DEBUG_FUNCTION;
    BYTE setBit = 1 << op_code_constant_[opcode];
    CURR_RAM |= setBit;
}

void TMS1100::op_rbit(BYTE opcode, bool) {
    DEBUG_FUNCTION;
    BYTE setBit = SET4(~(1 << op_code_constant_[opcode]));
    CURR_RAM &= setBit;
}

void TMS1100::op_tbit1(BYTE opcode, bool) {
    DEBUG_FUNCTION;
    cpu_->set_s(CURR_RAM & (1 << op_code_constant_[opcode]));
}

// input
void TMS1100::op_knez(BYTE, bool) {
    DEBUG_FUNCTION;
	cpu_->set_s(cpu_->get_k() != 0);
}

void TMS1100::op_tka(BYTE, bool) {
    DEBUG_FUNCTION;
    cpu_->set_a(cpu_->get_k());
}

// output
void TMS1100::op_setr(BYTE, bool) {
    DEBUG_FUNCTION;
    if (cpu_->get_x() <= 3 && cpu_->get_y() <= 10) {
        cpu_->set_r_index(cpu_->get_y());
    }
}

void TMS1100::op_rstr(BYTE, bool) {
    DEBUG_FUNCTION;
    if (cpu_->get_x() <= 3 && cpu_->get_y() <= 10) {
        cpu_->rst_r_index(cpu_->get_y());
    }
}

void TMS1100::op_tdo(BYTE, bool) {
    DEBUG_FUNCTION;
    // LSB <=> MSB Inverted relative to fuse map (SL = MSB)
    cpu_->set_o(cpu_->get_a() | (cpu_->get_sl() ? 0x10 : 0));
}

void TMS1100::op_ldx(BYTE opcode, bool) {
    DEBUG_FUNCTION;
    cpu_->set_x(op_code_constant_[opcode]);
}

// rom addressing
void TMS1100::op_br(BYTE opcode, bool last_s) {
    DEBUG_FUNCTION;
    if (!last_s) {
        return;
    }

    cpu_->set_ca(cpu_->get_cb());
    cpu_->set_pc(SET6(opcode));
        
    if (!cpu_->get_cl()) {
        cpu_->set_pa(cpu_->get_pb());
    }
}

void TMS1100::op_call(BYTE opcode, bool last_s) {
    DEBUG_FUNCTION;
    if (!last_s) {
        return;
    }

    if (cpu_->get_cl()) {
        cpu_->set_pb(cpu_->get_pa());
    }
    else {
        cpu_->set_cs(cpu_->get_ca());
        cpu_->set_sr(cpu_->get_pc());

        // PB <=> PA
        BYTE temp = cpu_->get_pb();
        cpu_->set_pb(cpu_->get_pa());
        cpu_->set_pa(temp);

        cpu_->set_cl(true);
    }
    cpu_->set_ca(cpu_->get_cb());
    cpu_->set_pc(SET6(opcode));
}

void TMS1100::op_retn(BYTE, bool) {
    DEBUG_FUNCTION;
    cpu_->set_pa(cpu_->get_pb());
    if (cpu_->get_cl()) {
        cpu_->set_ca(cpu_->get_cs());
        cpu_->set_pc(cpu_->get_sr());
        cpu_->set_cl(false);
    }
}

// TMS1100::TMS1100(void(*output_r_cb)(int, bool),void(*output_o_cb)(int)) {
//     cpu_ = CPUState(output_r_cb, output_o_cb);
//     for (int i = 0; i <= 255; i++) {
//         op_code_func_[i] = NULL;
//     }
//     setup_op_codes();
// }

void TMS1100::setup_op_codes() {
    op_code_func_[0x20] = &TMS1100::op_tay;
    op_code_func_[0x23] = &TMS1100::op_tya;
    op_code_func_[0x7f] = &TMS1100::op_cla;

    // transfer register to memory
    op_code_func_[0x27] = &TMS1100::op_tam;
    op_code_func_[0x25] = &TMS1100::op_tamiyc;
    op_code_func_[0x24] = &TMS1100::op_tamdyn;
    op_code_func_[0x26] = &TMS1100::op_tamza;

    // memory to register
    op_code_func_[0x22] = &TMS1100::op_tmy;
    op_code_func_[0x21] = &TMS1100::op_tma;
    op_code_func_[0x03] = &TMS1100::op_xma;

    // arithmetic
    op_code_func_[0x06] = &TMS1100::op_amaac;
    op_code_func_[0x3c] = &TMS1100::op_saman;
    op_code_func_[0x3e] = &TMS1100::op_imac;

    op_code_func_[0x07] = &TMS1100::op_dman;

    // ia, a9aac, a5aac, a13aac, a3aac, a11aac, a7aac, dan, a2aac, a10aac, a6aac
    // a14aac, a4aac, a12aac, a8aac
    BYTE op_constants_0[] = {1, 9, 5, 13, 3, 11, 7, 15, 2, 10, 6, 14, 4, 12, 8};
    for (BYTE i = 0; i < sizeof(op_constants_0)/sizeof op_constants_0[0]; i++) {
        BYTE op_index = 0x70 + i;
        op_code_func_[op_index] = &TMS1100::op_a_aac;
        op_code_constant_[op_index] = op_constants_0[i];
    }

    op_code_func_[0x05] = &TMS1100::op_iyc;
    op_code_func_[0x04] = &TMS1100::op_dyn;
    op_code_func_[0x3d] = &TMS1100::op_cpaiz;

    // arithmetic compare
    op_code_func_[0x01] = &TMS1100::op_alem;

    // logical compare
    op_code_func_[0x00] = &TMS1100::op_mnea;
    op_code_func_[0x3f] = &TMS1100::op_mnez;
    op_code_func_[0x02] = &TMS1100::op_ynea;

    BYTE op_constants_1[] = {0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15};
    for (BYTE i = 0; i < sizeof(op_constants_1)/sizeof op_constants_1[0]; i++) {
        BYTE op_index = 0x10 + i;
        op_code_func_[op_index] = &TMS1100::op_ldp;
        op_code_constant_[op_index] = op_constants_1[i];
        op_index = 0x40 + i;
        op_code_func_[op_index] = &TMS1100::op_tcy;
        op_code_constant_[op_index] = op_constants_1[i];
        op_index = 0x50 + i;
        op_code_func_[op_index] = &TMS1100::op_ynec;
        op_code_constant_[op_index] = op_constants_1[i];
        op_index = 0x60 + i;
        op_code_func_[op_index] = &TMS1100::op_tcmiy;
        op_code_constant_[op_index] = op_constants_1[i];
    }

    // bits in memory
    op_code_func_[0x09] = &TMS1100::op_comx;
    op_code_func_[0x0b] = &TMS1100::op_comc;

    BYTE op_constants_2[] = {0, 2, 1, 3};
    for (BYTE i = 0; i < sizeof(op_constants_2)/sizeof op_constants_2[0]; i++) {
        BYTE op_index = 0x30 + i;
        op_code_func_[op_index] = &TMS1100::op_sbit;
        op_code_constant_[op_index] = op_constants_2[i];
        op_index = 0x34 + i;
        op_code_func_[op_index] = &TMS1100::op_rbit;
        op_code_constant_[op_index] = op_constants_2[i];
        op_index = 0x38 + i;
        op_code_func_[op_index] = &TMS1100::op_tbit1;
        op_code_constant_[op_index] = op_constants_2[i];
    }

    // input
    op_code_func_[0x0e] = &TMS1100::op_knez;
    op_code_func_[0x08] = &TMS1100::op_tka;

    // output
    op_code_func_[0x0d] = &TMS1100::op_setr;
    op_code_func_[0x0c] = &TMS1100::op_rstr;
    op_code_func_[0x0a] = &TMS1100::op_tdo;

    // ram 'x' addressing
    BYTE op_constants_3[] = {0, 4, 2, 6, 1, 5, 3, 7};
    for (BYTE i = 0; i < sizeof(op_constants_3)/sizeof op_constants_3[0]; i++) {
        BYTE op_index = 0x28 + i;
        op_code_func_[op_index] = &TMS1100::op_ldx;
        op_code_constant_[op_index] = op_constants_3[i];
    }

    // addressing
    for (BYTE i = 0; i < 0x40; i++) {
        op_code_func_[0x80 + i] = &TMS1100::op_br;
        op_code_func_[0xC0 + i] = &TMS1100::op_call;
    }

    op_code_func_[0x0f] = &TMS1100::op_retn;
}

void TMS1100::set_output_r_cb(void(*output_r_cb)(int, bool)) {
    cpu_->set_output_r_cb(output_r_cb);
}

void TMS1100::set_output_o_cb(void(*output_o_cb)(int)) {
    cpu_->set_output_o_cb(output_o_cb);
}

void TMS1100::set_input_k_cb(int(*input_k_cb)(int)) {
    cpu_->set_input_k_cb(input_k_cb);
}

void TMS1100::exec(BYTE opcode) {
    bool last_status = cpu_->get_s();
    cpu_->set_s(true);
    void(TMS1100::*func)(BYTE, bool) = op_code_func_[opcode];
    if (func) {
        (this->*func)(opcode, last_status);
    }
}

void TMS1100::step() {
    WORD rom_address = (cpu_->get_ca() << 10) | (cpu_->get_pa() << 6) | cpu_->get_pc();
    BYTE opcode = rom_->get_data(rom_address);

    // useful for debugging
    // printf("%1x:%02x %02x x:%02x y:%02x a:%02x s:%1x ram:%02x cl:%02x ca:%02x cb:%02x\n",
    //     cpu_->get_pa(), cpu_->get_pc(), opcode, cpu_->get_x(), cpu_->get_y(), cpu_->get_a(),
    //     cpu_->get_s(), CURR_RAM, cpu_->get_cl(), cpu_->get_ca(), cpu_->get_cb());

    cpu_->increment_pc();
    exec(opcode);
};

TMS1100::TMS1100(ROM *rom) {
    cpu_ = new CPUState();
    for (int i = 0; i <= 255; i++) {
        op_code_func_[i] = NULL;
    }
    setup_op_codes();
    rom_ = rom;
    ram_ = new BYTE[128];
    for (int i = 0; i < 128; i++) {
        ram_[i] = SET4(0xAA);
    }
}

TMS1100::~TMS1100() {
    delete cpu_;
    delete ram_;   
}
