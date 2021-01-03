/**
 * @file python.cpp
 * @author Carl Edwards
 * 
 * Python bindings using pybind11 and the TMS1100 C++ library.
 * 
 * Compiling the Merlin library Mac:
 *   /usr/bin/clang++ -shared -std=c++2a -undefined dynamic_lookup 
 *     -g tms1xx0.cpp python.cpp `python3 -m pybind11 --includes` 
 *     -o merlin`python3-config --extension-suffix`
 */

#include "pybind11/functional.h"
#include <pybind11/pybind11.h>
#include "tms1xx0.h"

namespace py = pybind11;

/* 
 * Struct needed so we can deallocate the saved python callbacks.
 * If we don't, the python application "hangs" upon exit.
 */
struct Emulator {
    TMS1100 *cpu_ = NULL;
    std::function<void(int, bool)> py_r_cb_;
    std::function<void(int)> py_o_cb_;
    std::function<int(int)> py_k_cb_;
};

Emulator *emu_ = NULL;

void output_r_cb(int index, bool val) {
    //printf("output_r_cb: %d value: %d\n", index, val);
    if (emu_ && emu_->py_r_cb_) {
        emu_->py_r_cb_(index, val);
    }
}

void output_o_cb(int val) {
    //printf("output_o_cb: %d\n", val);
    if (emu_ && emu_->py_o_cb_) {
        emu_->py_o_cb_(val);
    }
}

int input_k_cb(int o_reg) {
    //printf("input_k_cb: %d\n", o_reg);
    if (emu_ && emu_->py_k_cb_) {
        return emu_->py_k_cb_(o_reg);
    }
    return 0;
}

void init(std::string rom_filename, std::function<void(int, bool)> r_cb, std::function<void(int)> o_cb, std::function<int(int)> k_cb) {
    if (emu_) {
        return;
    }
    emu_ = new Emulator;
    emu_->py_r_cb_ = r_cb;
    emu_->py_o_cb_ = o_cb;
    emu_->py_k_cb_ = k_cb;

    ROM *rom = new ROM();
    rom->load_rom(rom_filename);
    emu_->cpu_ = new TMS1100(rom);
    emu_->cpu_->set_output_r_cb(output_r_cb);
    emu_->cpu_->set_output_o_cb(output_o_cb);
    emu_->cpu_->set_input_k_cb(input_k_cb);
}

void step() {
    if (emu_) {
        emu_->cpu_->step();
    }
}

void deinit() {
    if (emu_) {
        if (emu_->cpu_) {
            delete emu_->cpu_;
        }
        delete emu_;
    }
    emu_ = NULL;
}

PYBIND11_MODULE(merlin, m) {
    m.doc() = "Merlin TMS1100 emulator";

    m.def("init", &init, "initialize the Merlin emulator");
    m.def("step", &step, "perform one step of the TMS1100 cpu");
    m.def("deinit", &deinit, "deinitialize the Merlin emulator");
}
