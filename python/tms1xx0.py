"""
Python port of TMS1100 emulator

This was inspired and ported from the work done by Dominic Thibodeau (hotkeysoft).
https://github.com/hotkeysoft/emulators/tree/master/TMS1000

Thank you Dominic!!
"""

def _set1(value):
  return value & 0x01

def _set2(value):
  return value & 0x03

def _set3(value):
  return value & 0x07

def _set4(value):
  return value & 0x0f

def _set6(value):
  return value & 0x3f

def _not4(value):
  return (~value) & 0x0f

def _getw(value):
  return _set6(value)

class ROM:
  """ ROM for TMS1xx0 microprocessors """

  # The program counter is not linear, it increments in
  # the following sequence
  _PC_SEQUENCE = (
    0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x3E,
    0x3D, 0x3B, 0x37, 0x2F, 0x1E, 0x3C, 0x39, 0x33,
    0x27, 0x0E, 0x1D, 0x3A, 0x35, 0x2B, 0x16, 0x2C,
    0x18, 0x30, 0x21, 0x02, 0x05, 0x0B, 0x17, 0x2E,
    0x1C, 0x38, 0x31, 0x23, 0x06, 0x0D, 0x1B, 0x36,
    0x2D, 0x1A, 0x34, 0x29, 0x12, 0x24, 0x08, 0x11,
    0x22, 0x04, 0x09, 0x13, 0x26, 0x0C, 0x19, 0x32,
    0x25, 0x0A, 0x15, 0x2A, 0x14, 0x28, 0x10, 0x20
  )
  data = []

  def __init__(self):
    pass

  def _inverse_pc_sequence(self, addr):
    for i in range(0, 64):
      if self._PC_SEQUENCE[i] == addr:
        return i
    raise Exception("should not happen: pc sequence not found")

  def load_rom(self, filename):
    """ Loads a TMS1xx0 ROM from a binary file """
    self.data = []
    with open(filename, 'rb') as rom_file:
      raw_rom = rom_file.read()
      remapped_rom = [0]*len(raw_rom)
      for i in range(len(raw_rom)):
        remapped_rom[i] = raw_rom[i & 0xFFC0 | self._PC_SEQUENCE[i & 0x3F]]
        if remapped_rom[i] & 0x80 == 0x80:
          new_addr = self._inverse_pc_sequence(remapped_rom[i] & 0x3F)
          remapped_rom[i] = remapped_rom[i] & 0xC0 # keep op code
          remapped_rom[i] = remapped_rom[i] | new_addr # new operand
      self.data = remapped_rom

class _CPU:
  reg_x = 0
  reg_y = 0
  reg_a = 0
  reg_s = False
  reg_sl = False
  reg_k = 0
  reg_o = 0
  reg_pa = 0
  reg_pb = 0
  reg_pc = 0
  reg_sr = 0
  reg_cl = False
  reg_ca = 0
  reg_cb = 0
  reg_cs = 0
  reg_r = None

  def _reset(self, r_width):
    self.reg_x = _set2(0xAA)
    self.reg_y = _set4(0xAA)
    self.reg_a = _set4(0xAA)
    self.reg_s = False
    self.reg_sl = False
    self.reg_k = 0
    self.reg_o = 0
    self.reg_pa = _set4(0xFF)
    self.reg_pb = _set4(0xFF)
    self.reg_pc = 0
    self.reg_sr = 0
    self.reg_cl = False
    self.reg_ca = 0
    self.reg_cb = 0
    self.reg_cs = 0
    self.reg_r = [False]*r_width

  def __init__(self, r_width):
    self._reset(r_width)

class TMS1100:
  """ Emulates a TMS1100 microprocessor

      r_reg_output_cb:   void fun(int index, bool on_off)
      o_reg_output_cb:   void fun(int value)
      k_reg_input_cb:   int fun(int cpu_o_reg, int op_code)
  """

  # function pointers to op code implementation
  _op_code_fn = [None] * 256

  # constants for participating op codes
  _op_code_con = [None] * 256

  _rom = None
  _cpu = None
  _ram = None
  _r_reg_output_cb = None
  _o_reg_output_cb = None
  _k_reg_input_cb = None

  def _op_tay(self, _op_code, _last_status):
    # ATN, AUTY
    self._cpu.reg_y = self._cpu.reg_a

  def _op_tya(self, _op_code, _last_status):
    # YTP, AUTA
    self._cpu.reg_a = self._cpu.reg_y

  def _op_cla(self, _op_code, _last_status):
    # AUTA
    self._cpu.reg_a = 0

  def _op_tam(self, _op_code, _last_status):
    # STO
    self._set_current_ram(self._cpu.reg_a)

  def _op_tamiyc(self, _op_code, _last_status):
    # STO, YTP, CIN, C8, AUTY
    self._set_current_ram(self._cpu.reg_a)
    self._cpu.reg_s = self._cpu.reg_y == 0x0F
    self._cpu.reg_y = _set4(self._cpu.reg_y + 1)

  def _op_tamdyn(self, _op_code, _last_status):
    # STO, YTP, CIN, C8, AUTY
    self._set_current_ram(self._cpu.reg_a)
    self._cpu.reg_s = self._cpu.reg_y >= 1
    self._cpu.reg_y = _set4(self._cpu.reg_y - 1)

  def _op_tamza(self, _op_code, _last_status):
    # STO, AUTA
    self._set_current_ram(self._cpu.reg_a)
    self._cpu.reg_a = 0

  def _op_tmy(self, _op_code, _last_status):
    # MTP, AUTY
    self._cpu.reg_y = self._get_current_ram()

  def _op_tma(self, _op_code, _last_status):
    # MTP, AUTY
    self._cpu.reg_a = self._get_current_ram()

  def _op_xma(self, _op_code, _last_status):
    # MTP, STO, AUTA
    temp = self._get_current_ram()
    self._set_current_ram(self._cpu.reg_a)
    self._cpu.reg_a = temp

  def _op_amaac(self, _op_code, _last_status):
    # MTP, ATN, C8, AUTA
    value = self._cpu.reg_a + self._get_current_ram()
    self._cpu.reg_s = value > 0x0F
    self._cpu.reg_a = _set4(value)

  def _op_saman(self, _op_code, _last_status):
    # MTP, NATN, CIN, C8, AUTA
    value = _not4(self._cpu.reg_a) + self._get_current_ram() + 1
    self._cpu.reg_s = value > 0x0F
    self._cpu.reg_a = _set4(value)

  def _op_imac(self, _op_code, _last_status):
    # MTP, CIN, C8, AUTA
    self._cpu.reg_a = self._get_current_ram()
    value = self._cpu.reg_a + 1
    self._cpu.reg_s = value > 0x0F
    self._cpu.reg_a = _set4(value)

  def _op_dman(self, _op_code, _last_status):
    # MTP, 15TN, C8, AUTA
    self._cpu.reg_a = self._get_current_ram()
    value = self._cpu.reg_a + 0x0F
    self._cpu.reg_s = value > 0x0F
    self._cpu.reg_a = _set4(value)

  def _op_iac(self, _op_code, _last_status):
    value = self._cpu.reg_a + 1
    self._cpu.reg_s = value > 0x0F
    self._cpu.reg_a = _set4(value)

  def _op_dan(self, _op_code, _last_status):
    # CKP, ATN, CIN, C8, AUTA
    value = self._cpu.reg_a + 15
    self._cpu.reg_s = value > 0x0F
    self._cpu.reg_a = _set4(value)

  def _op_a9aac(self, _op_code, _last_status):
    value = self._cpu.reg_a + 9
    self._cpu.reg_s = value > 0x0F
    self._cpu.reg_a = _set4(value)

  def _op_a5aac(self, _op_code, _last_status):
    value = self._cpu.reg_a + 5
    self._cpu.reg_s = value > 0x0F
    self._cpu.reg_a = _set4(value)

  def _op_a13aac(self, _op_code, _last_status):
    value = self._cpu.reg_a + 13
    self._cpu.reg_s = value > 0x0F
    self._cpu.reg_a = _set4(value)

  def _op_a3aac(self, _op_code, _last_status):
    value = self._cpu.reg_a + 3
    self._cpu.reg_s = value > 0x0F
    self._cpu.reg_a = _set4(value)

  def _op_a11aac(self, _op_code, _last_status):
    value = self._cpu.reg_a + 11
    self._cpu.reg_s = value > 0x0F
    self._cpu.reg_a = _set4(value)

  def _op_a7aac(self, _op_code, _last_status):
    value = self._cpu.reg_a + 7
    self._cpu.reg_s = value > 0x0F
    self._cpu.reg_a = _set4(value)

  def _op_a2aac(self, _op_code, _last_status):
    value = self._cpu.reg_a + 2
    self._cpu.reg_s = value > 0x0F
    self._cpu.reg_a = _set4(value)

  def _op_a10aac(self, _op_code, _last_status):
    value = self._cpu.reg_a + 10
    self._cpu.reg_s = value > 0x0F
    self._cpu.reg_a = _set4(value)

  def _op_a6aac(self, _op_code, _last_status):
    value = self._cpu.reg_a + 6
    self._cpu.reg_s = value > 0x0F
    self._cpu.reg_a = _set4(value)

  def _op_a14aac(self, _op_code, _last_status):
    value = self._cpu.reg_a + 14
    self._cpu.reg_s = value > 0x0F
    self._cpu.reg_a = _set4(value)

  def _op_a4aac(self, _op_code, _last_status):
    value = self._cpu.reg_a + 4
    self._cpu.reg_s = value > 0x0F
    self._cpu.reg_a = _set4(value)

  def _op_a12aac(self, _op_code, _last_status):
    value = self._cpu.reg_a + 12
    self._cpu.reg_s = value > 0x0F
    self._cpu.reg_a = _set4(value)

  def _op_a8aac(self, _op_code, _last_status):
    value = self._cpu.reg_a + 8
    self._cpu.reg_s = value > 0x0F
    self._cpu.reg_a = _set4(value)

  def _op_iyc(self, _op_code, _last_status):
    # YTP, CIN, C8, AUTY
    value = self._cpu.reg_y + 1
    self._cpu.reg_s = value > 0x0F
    self._cpu.reg_y = _set4(value)

  def _op_dyn(self, _op_code, _last_status):
    # YTP, 15TN, C8, AUTY
    value = self._cpu.reg_y + 0x0F
    self._cpu.reg_s = value > 0x0F
    self._cpu.reg_y = _set4(value)

  def _op_cpaiz(self, _op_code, _last_status):
    # NATN, CIN, C8, AUTA
    value = _not4(self._cpu.reg_a) + 1
    self._cpu.reg_s = value > 0x0F
    self._cpu.reg_a = _set4(value)

  def _op_alem(self, _op_code, _last_status):
    # MTP, NATN, CIN, C8
    value = _not4(self._cpu.reg_a) + self._get_current_ram() + 1
    self._cpu.reg_s = value > 0x0F

  def _op_mnea(self, _op_code, _last_status):
    # MTP, ATN, NE
    self._cpu.reg_s = self._get_current_ram() != self._cpu.reg_a

  def _op_mnez(self, _op_code, _last_status):
    # MTP, NE
    self._cpu.reg_s = self._get_current_ram() != 0

  def _op_ynea(self, _op_code, _last_status):
    # YTP, ATN, NE, STSL
    self._cpu.reg_s = self._cpu.reg_a != self._cpu.reg_y
    self._cpu.reg_sl = self._cpu.reg_s

  def _op_ynec(self, op_code, _last_status):
    # YTP, CKN, NE
    constant = self._op_code_con[op_code]
    assert constant is not None, "op code %02x should have constant set" % op_code
    self._cpu.reg_s = self._cpu.reg_y != constant

  def _op_sbit(self, op_code, _last_status):
    constant = self._op_code_con[op_code]
    assert constant is not None, "op code %02x should have constant set" % op_code
    self._set_current_ram(self._get_current_ram() | 1 << constant)

  def _op_rbit(self, op_code, _last_status):
    constant = self._op_code_con[op_code]
    assert constant is not None, "op code %02x should have constant set" % op_code
    self._set_current_ram(self._get_current_ram() & _set4(~(1 << constant)))

  def _op_tbit1(self, op_code, _last_status):
    constant = self._op_code_con[op_code]
    assert constant is not None, "op code %02x should have constant set" % op_code
    self._cpu.reg_s = (self._get_current_ram() & (1 << constant)) > 0

  def _op_setr(self, _op_code, _last_status):
    if self._cpu.reg_x <= 3 and self._cpu.reg_y <= 10:
      self._cpu.reg_r[self._cpu.reg_y] = True
      if self._r_reg_output_cb:
        self._r_reg_output_cb(self._cpu.reg_y, True)

  def _op_rstr(self, _op_code, _last_status):
    if self._cpu.reg_x <= 3 and self._cpu.reg_y <= 10:
      self._cpu.reg_r[self._cpu.reg_y] = False
      if self._r_reg_output_cb:
        self._r_reg_output_cb(self._cpu.reg_y, False)

  def _op_knez(self, op_code, _last_status):
    if self._k_reg_input_cb:
      self._cpu.reg_k = _set4(self._k_reg_input_cb(self._cpu.reg_o & 0xFF) & 0x0F)
    self._cpu.reg_s = _set4(self._cpu.reg_k) != 0

  def _op_tka(self, op_code, _last_status):
    # CKP, AUTA
    if self._k_reg_input_cb:
      self._cpu.reg_k = _set4(self._k_reg_input_cb(self._cpu.reg_o & 0xFF) & 0x0F)
    self._cpu.reg_a = _set4(self._cpu.reg_k)

  def _op_tdo(self, _op_code, _last_status):
    # LSB <=> MSB Inverted relative to fuse map (SL = MSB)
    self._cpu.reg_o = self._cpu.reg_a | (0x10 if self._cpu.reg_sl is True else 0)
    if self._o_reg_output_cb:
      self._o_reg_output_cb(self._cpu.reg_o)

  def _op_retn(self, _op_code, _last_status):
    self._cpu.reg_pa = self._cpu.reg_pb
    if self._cpu.reg_cl is True:
      self._cpu.reg_ca = self._cpu.reg_cs
      self._cpu.reg_pc = self._cpu.reg_sr
      self._cpu.reg_cl = False

  def _op_comc(self, _op_code, _last_status):
    self._cpu.reg_cb = _set1(~self._cpu.reg_cb)

  def _op_tcy(self, op_code, _last_status):
    constant = self._op_code_con[op_code]
    assert constant is not None, "op code %02x should have constant set" % op_code
    self._cpu.reg_y = constant

  def _op_tcmiy(self, op_code, _last_status):
    # CKM, YTP, CIN, AUTY
    constant = self._op_code_con[op_code]
    assert constant is not None, "op code %02x should have constant set" % op_code
    self._set_current_ram(constant)
    self._cpu.reg_y = _set4(self._cpu.reg_y + 1)

  def _op_ldx(self, op_code, _last_status):
    constant = self._op_code_con[op_code]
    assert constant is not None, "op code %02x should have constant set" % op_code
    self._cpu.reg_x = constant

  def _op_br(self, op_code, last_status):
    if last_status is True:
      self._cpu.reg_ca = self._cpu.reg_cb
      self._cpu.reg_pc = _getw(op_code)
      if self._cpu.reg_cl is False:
        self._cpu.reg_pa = self._cpu.reg_pb

  def _op_call(self, op_code, last_status):
    if last_status is True:
      if self._cpu.reg_cl is True:
        self._cpu.reg_pb = self._cpu.reg_pa
      else:
        self._cpu.reg_cs = self._cpu.reg_ca
        self._cpu.reg_sr = self._cpu.reg_pc

        # PB <=> PA
        temp = self._cpu.reg_pb
        self._cpu.reg_pb = self._cpu.reg_pa
        self._cpu.reg_pa = temp

        self._cpu.reg_cl = True

      self._cpu.reg_ca = self._cpu.reg_cb
      self._cpu.reg_pc = _getw(op_code)

  def _op_ldp(self, op_code, _last_status):
    constant = self._op_code_con[op_code]
    assert constant is not None, "op code %02x should have constant set" % op_code
    self._cpu.reg_pb = constant

  def _op_comx(self, op_code, last_status):
    pass

  def _set_op_codes_with_contants(self, op_code_list, constants):
    for op_code in op_code_list:
      for index, value in enumerate(constants):
        self._op_code_fn[op_code[0]+index] = op_code[1]
        self._op_code_con[op_code[0]+index] = value

  def _setup_op_codes(self):
    #pylint: disable=too-many-statements

    ## Register to register
    self._op_code_fn[0x20] = self._op_tay
    self._op_code_fn[0x23] = self._op_tya
    self._op_code_fn[0x7f] = self._op_cla

    ## transfer register to memory
    self._op_code_fn[0x27] = self._op_tam
    self._op_code_fn[0x25] = self._op_tamiyc
    self._op_code_fn[0x24] = self._op_tamdyn
    self._op_code_fn[0x26] = self._op_tamza

    ## memory to register
    self._op_code_fn[0x22] = self._op_tmy
    self._op_code_fn[0x21] = self._op_tma
    self._op_code_fn[0x03] = self._op_xma

    ## arithmetic
    self._op_code_fn[0x06] = self._op_amaac
    self._op_code_fn[0x3c] = self._op_saman
    self._op_code_fn[0x3e] = self._op_imac
    self._op_code_fn[0x07] = self._op_dman

    self._op_code_fn[0x70] = self._op_iac
    self._op_code_fn[0x77] = self._op_dan

    self._op_code_fn[0x71] = self._op_a9aac
    self._op_code_fn[0x72] = self._op_a5aac
    self._op_code_fn[0x73] = self._op_a13aac
    self._op_code_fn[0x74] = self._op_a3aac
    self._op_code_fn[0x75] = self._op_a11aac
    self._op_code_fn[0x76] = self._op_a7aac

    self._op_code_fn[0x78] = self._op_a2aac
    self._op_code_fn[0x79] = self._op_a10aac
    self._op_code_fn[0x7a] = self._op_a6aac
    self._op_code_fn[0x7b] = self._op_a14aac
    self._op_code_fn[0x7c] = self._op_a4aac
    self._op_code_fn[0x7d] = self._op_a12aac
    self._op_code_fn[0x7e] = self._op_a8aac

    self._op_code_fn[0x05] = self._op_iyc
    self._op_code_fn[0x04] = self._op_dyn
    self._op_code_fn[0x3d] = self._op_cpaiz

    ## arithmetic compare
    self._op_code_fn[0x01] = self._op_alem

    ## logical compare
    self._op_code_fn[0x00] = self._op_mnea
    self._op_code_fn[0x3f] = self._op_mnez
    self._op_code_fn[0x02] = self._op_ynea

    op_code_list = [
      # 0x10 -> 0x1f = ldp
      (0x10, self._op_ldp),
      # 0x40 -> 0x4f = tcy
      (0x40, self._op_tcy),
      # 0x50 -> 0x5f = ynec
      (0x50, self._op_ynec),
      # 0x60 -> 0x6f = tcmiy
      (0x60, self._op_tcmiy),
    ]
    self._set_op_codes_with_contants(
      op_code_list,
      [0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15])

    ## bits in memory
    self._op_code_fn[0x09] = self._op_comx
    self._op_code_fn[0x0b] = self._op_comc

    op_code_list = [
      # 0x30 -> 0x33 = sbit
      (0x30, self._op_sbit),
      # 0x34 -> 0x37 = rbit
      (0x34, self._op_rbit),
      # 0x38 -> 0x3b = tbit1
      (0x38, self._op_tbit1)
    ]
    self._set_op_codes_with_contants(op_code_list, [0, 2, 1, 3])

    ## input
    self._op_code_fn[0x0e] = self._op_knez
    self._op_code_fn[0x08] = self._op_tka

    ## output
    self._op_code_fn[0x0d] = self._op_setr
    self._op_code_fn[0x0c] = self._op_rstr
    self._op_code_fn[0x0a] = self._op_tdo

    ## ram 'x' addressing
    op_code_list = [
      # 0x28 -> 0x2f = ldx
      (0x28, self._op_ldx),
    ]
    self._set_op_codes_with_contants(op_code_list, [0, 4, 2, 6, 1, 5, 3, 7])

    ## rom addressing
    for i in range(0x00, 0x40):
      # 0x80 -> 0xBF = br
      self._op_code_fn[0x80+i] = self._op_br
      # 0xC0 -> 0xFF = call
      self._op_code_fn[0xC0+i] = self._op_call
    self._op_code_fn[0x0f] = self._op_retn

  def __init__(self, rom, ram_size,
      r_reg_output_cb=None,
      o_reg_output_cb=None,
      k_reg_input_cb=None):
    # pylint: disable=too-many-arguments
    self._ram_size = ram_size
    self._rom = rom
    self._ram = [0x0A]*self._ram_size
    self._r_reg_output_cb = r_reg_output_cb
    self._o_reg_output_cb = o_reg_output_cb
    self._k_reg_input_cb = k_reg_input_cb
    self._cpu = _CPU(11)
    self._setup_op_codes()

  def _exec(self, op_code):
    last_status = self._cpu.reg_s
    self._cpu.reg_s = True
    self._op_code_fn[op_code](op_code, last_status)

  def _current_rom(self):
    return self._rom.data[self._cpu.reg_ca << 10 | self._cpu.reg_pa << 6 | self._cpu.reg_pc]

  def _get_current_ram(self):
    return self._ram[(self._cpu.reg_x << 4) | self._cpu.reg_y]

  def _set_current_ram(self, value):
    self._ram[(self._cpu.reg_x << 4) | self._cpu.reg_y] = value & 0x0F

  # useful for debugging
  # def _dump_cpu(self, op_code):
  #   print("%1x:%02x %02x x:%02x y:%02x a:%02x s:%1x ram:%02x cl:%02x ca:%02x cb:%02x" %
  #     (self._cpu.reg_pa, self._cpu.reg_pc, op_code,
  #     self._cpu.reg_x, self._cpu.reg_y, self._cpu.reg_a,
  #     1 if self._cpu.reg_s else 0, self._get_current_ram(), self._cpu.reg_cl,
  #     self._cpu.reg_ca, self._cpu.reg_cb
  #     ))

  def step(self):
    """ perform one step of the CPU """
    op_code = self._current_rom()
    self._cpu.reg_pc = _set6(self._cpu.reg_pc + 1)
    self._exec(op_code)
