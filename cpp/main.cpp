/**
 * @file main.cpp
 * @author Carl Edwards
 * 
 * Simple console app using the TMS1100 C++ library.
 * 
 * Python port of Milton Bradley's Merlin Electronic Game emulator
 *
 * This was inspired and ported from the work done by Dominic Thibodeau (hotkeysoft).
 * https://github.com/hotkeysoft/emulators/tree/master/TMS1000
 *
 * Thank you Dominic!!
 */
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <unistd.h>
#include "tms1xx0.h"

using namespace std;

void output_r_cb(int index, bool val) {
    printf("output_r_cb: %2d value: %d\n", index, val);
}

void output_o_cb(int value) {
    //printf("output_o_cb: %d\n", value);
}

void run_emulator() {
    ROM *rom = new ROM();
    rom->load_rom("mp3404.bin");

    TMS1100 emu = TMS1100(rom);
    emu.set_output_r_cb(&output_r_cb);
    emu.set_output_o_cb(&output_o_cb);
    while(1) {
        emu.step();
        usleep(75);
    }
}

int main()
{
    cout << showbase // show the 0x prefix
        << internal // fill between the prefix and the number
        << setfill('0'); // fill with 0s

    try {
        run_emulator();
    } catch(runtime_error &re) {
        cout << "unexpected error: " << re.what() << endl;
    }
}
