#!/usr/bin/env python3
"""
Python port of Milton Bradley's Merlin Electronic Game emulator

This was inspired and ported from the work done by Dominic Thibodeau (hotkeysoft).
https://github.com/hotkeysoft/emulators/tree/master/TMS1000

Thank you Dominic!!
"""

from datetime import datetime
from blessed import Terminal
from tms1xx0 import ROM, TMS1100

SOUND_ON = """  │=========│
  │=========│
  │=========│"""

SOUND_OFF = """  │---------│
  │---------│
  │---------│"""

GAME_TEMPLATE = """  ┌─────────┐
  │---------│
  │---------│
  │---------│
  └─────────┘
 /   ┌───┐   \\
 │   │ ~ │   │
 ├───┼───┼───┤
 │ 1 │ 2 │ 3 │
 ├───┼───┼───┤
 │ 4 │ 5 │ 6 │
 ├───┼───┼───┤
 │ 7 │ 8 │ 9 │
 ├───┼───┼───┤
 │   │ O │   │
 \\   └───┘   /
  ┌─────────┐
  │  N   S  │
  │         │
  │  H   C  │
  └─────────┘"""

LED_POSITION = [
  (7, 7),
  (9, 3),
  (9, 7),
  (9, 11),
  (11, 3),
  (11, 7),
  (11, 11),
  (13, 3),
  (13, 7),
  (13, 11),
  (15, 7),
  (16, 3),
  (16, 4),
]

LED_OFF_DEFAULT = ["~","1","2","3","4","5","6","7","8","9","0","","","",""]

SOUND_ANIMATION_MICROS = 500 * 100
SOUND_POSITION = (2,0)

# pylint: disable = invalid-name
g_input_key = None
g_input_key_count = 0
g_term = Terminal()
g_sound_start = None

def cpu_r_output_cb(y_value, on_off):
  """ called by the CPU for handling hardware 'R' output """
  led_pos = LED_POSITION[y_value]
  led_char = g_term.red(u"■") if on_off else g_term.white(LED_OFF_DEFAULT[y_value])
  print(g_term.move_yx(led_pos[0], led_pos[1]) + led_char)

def cpu_o_output_cb(o_value):
  """ called by the CPU for handling hardware 'O' output """
  # pylint: disable = global-statement
  global g_sound_start
  if g_sound_start is not None:
    diff = datetime.now() - g_sound_start
    if diff.microseconds > SOUND_ANIMATION_MICROS:
      g_sound_start = None
      print(g_term.move_yx(SOUND_POSITION[0], SOUND_POSITION[1]) + g_term.white(SOUND_OFF))
  elif o_value & 0x01: # check for the sound bit
    g_sound_start = datetime.now()
    print(g_term.move_yx(SOUND_POSITION[0], SOUND_POSITION[1]) + g_term.white(SOUND_ON))


def cpu_k_input_cb(o_reg, op_code):
  """ called by the CPU for providing the hardware user input """
  # pylint: disable = too-many-branches, global-statement
  global g_input_key, g_input_key_count
  inp = g_input_key
  k_val = 0
  if o_reg == 0:    # O0: K1(R0), K2(R1), K8(R2),  K4(R3)
    if inp == u"~":
      k_val = 1
    elif inp == u"1":
      k_val =  2
    elif inp == u"2":
      k_val =  8
    elif inp == u"3":
      k_val =  4
  elif o_reg == 4:  # O1: K1(R4), K2(R5), K8(R6),  K4(R7)
    if inp == u"4":
      k_val =  1
    elif inp == u"5":
      k_val =  2
    elif inp == u"6":
      k_val =  8
    elif inp == u"7":
      k_val =  4
  elif o_reg == 8:  # O2: K1(R8), K2(R9), K8(R10), K4(SG)
    if inp == u"8":
      k_val =  1
    elif inp == u"9":
      k_val =  2
    elif inp == u"0":
      k_val =  8
    elif inp == u"s":
      k_val =  4
  elif o_reg == 12: # O3:         K2(CT), K8(NG),  K4(HM)
    if inp == u"c":
      k_val =  2
    elif inp == u"n":
      k_val =  8
    elif inp == u"h":
      k_val =  4

  # hack to simulate a key being pressed down for 32 checks
  if k_val > 0:
    g_input_key_count = g_input_key_count - 1
    if g_input_key_count <= 0:
      g_input_key = None
      g_input_key_count = 0

  return k_val

def main():
  """ the main entry point """
  # pylint: disable = global-statement
  global g_input_key, g_input_key_count
  print(g_term.clear)
  print(g_term.white(GAME_TEMPLATE))

  rom = ROM()
  rom.load_rom('mp3404.bin')

  emu = TMS1100(rom, 128,
    r_reg_output_cb=cpu_r_output_cb,
    o_reg_output_cb=cpu_o_output_cb,
    k_reg_input_cb=cpu_k_input_cb)

  with g_term.cbreak(), g_term.hidden_cursor():
    while True:
      # block w/ timeout waiting for a keyboard input
      val = g_term.inkey(2 / 1000000)
      if not val:
        pass
      elif val.is_sequence:
        pass
      elif val:
        # keyboard inputs are a single-shot (i.e. we don't scan for key held down like
        # hardware does (for switch debounce).
        # To emulate the requirement for the Merlin hardware, we need the key to be held down
        # for a total of 32 "reads" of the same value.
        g_input_key = val.lower()
        g_input_key_count = 32

        # quickly exit the emulator
        if g_input_key == u"q":
          break

      emu.step()

if __name__ == '__main__':
  main()
