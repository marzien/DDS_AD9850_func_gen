"""
DDS function generator carrier board - single source of truth.

Every part, its pin -> net mapping, schematic position and PCB position is
defined here. gen_project.py turns this into the KiCad schematic, symbol and
footprint libraries, and the PCB.

Board coordinates are in mm, origin = board top-left corner, x right, y down.
"""

PROJECT = 'DDS_FuncGen'
BOARD_W = 100.0
BOARD_H = 90.0

# Distance between the two header rows of the AD9850 module (HC-SR08).
# Seller drawing says "20 mm"; measure your module and change if needed.
AD9850_ROW_SPACING = 20.32
BLUEPILL_ROW_SPACING = 15.24

KLIB = 'DDS_FuncGen'          # project symbol / footprint library name

R0805 = 'Resistor_SMD:R_0805_2012Metric_Pad1.20x1.40mm_HandSolder'
C0805 = 'Capacitor_SMD:C_0805_2012Metric_Pad1.18x1.45mm_HandSolder'
CP5 = 'Capacitor_THT:CP_Radial_D5.0mm_P2.00mm'
CP8 = 'Capacitor_THT:CP_Radial_D8.0mm_P3.50mm'
BNC = 'Connector_Coaxial:BNC_TEConnectivity_1478035_Horizontal'

# ---------------------------------------------------------------- Blue Pill
# Header 1 / header 2 pin names (stm32-base.org numbering)
BP_H1 = ['VB', 'C13', 'C14', 'C15', 'A0', 'A1', 'A2', 'A3', 'A4', 'A5',
         'A6', 'A7', 'B0', 'B1', 'B10', 'B11', 'R', '3.3', 'G', 'G']
BP_H2 = ['3.3', 'G', '5V', 'B9', 'B8', 'B7', 'B6', 'B5', 'B4', 'B3',
         'A15', 'A12', 'A11', 'A10', 'A9', 'A8', 'B15', 'B14', 'B13', 'B12']
BP_NAMES = {str(i + 1): n for i, n in enumerate(BP_H1)}
BP_NAMES.update({str(i + 21): n for i, n in enumerate(BP_H2)})

_bp_use = {'A0': 'MCU_WCLK', 'A1': 'MCU_FQUD', 'A2': 'MCU_DATA', 'A3': 'MCU_RESET',
           'A4': 'ENC_DT', 'A5': 'ENC_SW', 'A6': 'ENC_CLK', 'B0': 'SYNC_MCU',
           'B10': 'I2C_SCL', 'B11': 'I2C_SDA', '3.3': '+3V3', 'G': 'GND', '5V': '+5V'}
BP_PINS = {num: _bp_use.get(name) for num, name in BP_NAMES.items()}

# ------------------------------------------------------------ AD9850 module
AD_NAMES = {'1': 'VCC', '2': 'W_CLK', '3': 'FQ_UD', '4': 'DATA', '5': 'RESET',
            '6': 'GND', '7': 'QOUT1', '8': 'QOUT2', '9': 'ZOUT1', '10': 'ZOUT2',
            '11': 'VCC', '12': 'D0', '13': 'D1', '14': 'D2', '15': 'D3',
            '16': 'D4', '17': 'D5', '18': 'D6', '19': 'D7', '20': 'GND'}
# Data row: only D2..D6 tied low (serial mode needs D2=0). VCC, D7 and GND of the
# data row are left open: some modules have D7 and GND swapped on that row.
AD_PINS = {'1': '+5V', '2': 'DDS_WCLK', '3': 'DDS_FQUD', '4': 'DDS_DATA',
           '5': 'DDS_RESET', '6': 'GND', '7': 'SQ_RAW', '8': None, '9': None,
           '10': 'SINE_RAW', '11': None, '12': None, '13': None, '14': 'GND',
           '15': 'GND', '16': 'GND', '17': 'GND', '18': 'GND', '19': None, '20': None}

PS_NAMES = {'1': 'Vin', '2': 'GND', '4': '-Vo', '5': '0V', '6': '+Vo'}

# ------------------------------------------------------------------ parts
# sch: (x, y) on the A3 sheet; for multi-unit parts a dict unit -> (x, y)
# pcb: (x, y, rotation_deg) in board coordinates
PARTS = [
    # ---- power input
    dict(ref='J1', lib='Connector:Barrel_Jack', value='5V DC in', fp='Connector_BarrelJack:BarrelJack_Horizontal',
         pins={'1': 'VIN', '2': 'GND'}, sch=(30.48, 55.88), pcb=(86.0, 82.0, 180)),
    dict(ref='D1', lib='Device:D_Schottky', value='SS34', fp='Diode_SMD:D_SMA_Handsoldering',
         pins={'1': '+5V', '2': 'VIN'}, sch=(60.96, 53.34), pcb=(76.0, 86.0, 0)),
    dict(ref='C1', lib='Device:C_Polarized', value='220uF 10V', fp=CP8,
         pins={'1': '+5V', '2': 'GND'}, sch=(86.36, 60.96), pcb=(64.5, 85.0, 0)),
    dict(ref='FLG1', lib='power:PWR_FLAG', value='PWR_FLAG', fp=None, pins={'1': '+5V'}, sch=(99.06, 45.72)),
    dict(ref='FLG2', lib='power:PWR_FLAG', value='PWR_FLAG', fp=None, pins={'1': 'GND'}, sch=(111.76, 45.72)),
    # ---- isolated +-15 V -> +-12 V for the amplifier
    dict(ref='PS1', lib=KLIB + ':A0515S-2WR3', value='A0515S-2WR3', fp=KLIB + ':Mornsun_SIP6_A_S-2WR3',
         pins={'1': '+5V', '2': 'GND', '4': 'N15_RAW', '5': 'GND', '6': 'P15_RAW'},
         sch=(157.48, 60.96), pcb=(61.5, 74.0, 0)),
    dict(ref='C11', lib='Device:C', value='10uF 16V', fp=C0805,
         pins={'1': '+5V', '2': 'GND'}, sch=(127.0, 60.96), pcb=(62.0, 68.5, 0)),
    dict(ref='C12', lib='Device:C', value='1uF 25V', fp=C0805,
         pins={'1': 'P15_RAW', '2': 'GND'}, sch=(195.58, 50.8), pcb=(75.0, 68.5, 0)),
    dict(ref='C13', lib='Device:C', value='1uF 25V', fp=C0805,
         pins={'1': 'GND', '2': 'N15_RAW'}, sch=(195.58, 76.2), pcb=(69.0, 68.5, 0)),
    dict(ref='L1', lib='Device:L', value='10uH', fp='Inductor_SMD:L_1210_3225Metric_Pad1.42x2.65mm_HandSolder',
         pins={'1': 'P15_RAW', '2': '+15V'}, sch=(215.9, 50.8), pcb=(73.0, 64.5, 0)),
    dict(ref='L2', lib='Device:L', value='10uH', fp='Inductor_SMD:L_1210_3225Metric_Pad1.42x2.65mm_HandSolder',
         pins={'1': 'N15_RAW', '2': '-15V'}, sch=(215.9, 76.2), pcb=(65.0, 64.5, 0)),
    dict(ref='C14', lib='Device:C_Polarized', value='22uF 25V', fp=CP5,
         pins={'1': '+15V', '2': 'GND'}, sch=(236.22, 50.8), pcb=(69.0, 59.5, 0)),
    dict(ref='C15', lib='Device:C_Polarized', value='22uF 25V', fp=CP5,
         pins={'1': 'GND', '2': '-15V'}, sch=(236.22, 76.2), pcb=(61.0, 59.5, 0)),
    dict(ref='FLG3', lib='power:PWR_FLAG', value='PWR_FLAG', fp=None, pins={'1': '+15V'}, sch=(248.92, 40.64)),
    dict(ref='FLG4', lib='power:PWR_FLAG', value='PWR_FLAG', fp=None, pins={'1': '-15V'}, sch=(248.92, 88.9)),
    dict(ref='U3', lib='Regulator_Linear:L78L12_TO92', value='78L12', fp='Package_TO_SOT_THT:TO-92_Inline',
         pins={'1': '+12V', '2': 'GND', '3': '+15V'}, sch=(271.78, 45.72), pcb=(69.0, 50.0, 0)),
    dict(ref='U4', lib='Regulator_Linear:L79L12_TO92', value='79L12', fp='Package_TO_SOT_THT:TO-92_Inline',
         pins={'1': 'GND', '2': '-15V', '3': '-12V'}, sch=(271.78, 86.36), pcb=(61.0, 50.0, 0)),
    dict(ref='C16', lib='Device:C', value='100nF', fp=C0805,
         pins={'1': '+12V', '2': 'GND'}, sch=(294.64, 45.72), pcb=(71.0, 54.5, 0)),
    dict(ref='C17', lib='Device:C', value='100nF', fp=C0805,
         pins={'1': 'GND', '2': '-12V'}, sch=(294.64, 86.36), pcb=(63.0, 54.5, 0)),

    # ---- Blue Pill + level shifter
    dict(ref='U1', lib=KLIB + ':BluePill', value='BluePill STM32F103C8', fp=KLIB + ':BluePill_Socket',
         pins=BP_PINS, sch=(63.5, 152.4), pcb=(7.0, 60.0, 0)),
    dict(ref='U7', lib='74xx:74AHCT125', value='74AHCT125', fp='Package_SO:SOIC-14_3.9x8.7mm_P1.27mm',
         pins={'1': 'GND', '2': 'MCU_WCLK', '3': 'DDS_WCLK', '4': 'GND', '5': 'MCU_FQUD', '6': 'DDS_FQUD',
               '7': 'GND', '8': 'DDS_DATA', '9': 'MCU_DATA', '10': 'GND', '11': 'DDS_RESET',
               '12': 'MCU_RESET', '13': 'GND', '14': '+5V'},
         sch={1: (139.7, 121.92), 2: (139.7, 142.24), 3: (139.7, 162.56), 4: (139.7, 182.88), 5: (114.3, 203.2)},
         pcb=(29.0, 46.0, 90)),
    dict(ref='C3', lib='Device:C', value='100nF', fp=C0805,
         pins={'1': '+5V', '2': 'GND'}, sch=(127.0, 220.98), pcb=(36.5, 46.0, 90)),
    dict(ref='R1', lib='Device:R', value='10k', fp=R0805,
         pins={'1': 'MCU_RESET', '2': 'GND'}, sch=(165.1, 203.2), pcb=(38.0, 52.0, 0)),

    # ---- AD9850 module
    dict(ref='U2', lib=KLIB + ':AD9850_Module', value='AD9850 module (HC-SR08)', fp=KLIB + ':AD9850_Module_Socket',
         pins=AD_PINS, sch=(241.3, 144.78), pcb=(20.0, 10.0, 0)),

    # ---- square output buffer
    dict(ref='U6', lib='74xGxx:74LVC1G17', value='74LVC1G17', fp='Package_TO_SOT_SMD:SOT-23-5_HandSoldering',
         pins={'1': None, '2': 'SQ_RAW', '3': 'GND', '4': 'SQ_BUF', '5': '+5V'},
         sch=(325.12, 124.46), pcb=(46.0, 40.0, 0)),
    dict(ref='C4', lib='Device:C', value='100nF', fp=C0805,
         pins={'1': '+5V', '2': 'GND'}, sch=(347.98, 109.22), pcb=(46.0, 44.5, 0)),
    dict(ref='R2', lib='Device:R', value='68', fp=R0805,
         pins={'1': 'SQ_BUF', '2': 'SQ_OUT'}, sch=(350.52, 129.54), pcb=(51.0, 40.0, 0)),
    dict(ref='J5', lib='Connector:Conn_Coaxial', value='SQUARE', fp=BNC,
         pins={'1': 'SQ_OUT', '2': 'GND'}, sch=(375.92, 124.46), pcb=(85.0, 35.5, -90)),

    # ---- sine: AC coupling, amplitude pot, LM7171 x4 amplifier
    dict(ref='C5', lib='Device:C_Polarized', value='22uF 16V', fp=CP5,
         pins={'1': 'SINE_RAW', '2': 'SINE_AC'}, sch=(281.94, 162.56), pcb=(56.0, 20.0, 90)),
    dict(ref='C6', lib='Device:C', value='100nF', fp=C0805,
         pins={'1': 'SINE_RAW', '2': 'SINE_AC'}, sch=(294.64, 162.56), pcb=(56.0, 25.5, 90)),
    dict(ref='RV1', lib='Device:R_Potentiometer', value='1k lin (amplitude)', fp='Potentiometer_THT:Potentiometer_Alps_RK09K_Single_Vertical',
         pins={'1': 'SINE_AC', '2': 'AMP_WIPER', '3': 'GND'}, sch=(304.8, 175.26), pcb=(61.0, 6.5, 0)),
    dict(ref='R4', lib='Device:R', value='47', fp=R0805,
         pins={'1': 'AMP_WIPER', '2': 'AMP_INP'}, sch=(317.5, 175.26), pcb=(62.0, 30.5, 0)),
    dict(ref='U5', lib='Amplifier_Operational:LM7171xIM', value='LM7171', fp='Package_SO:SOIC-8_3.9x4.9mm_P1.27mm',
         pins={'1': None, '2': 'AMP_FB', '3': 'AMP_INP', '4': '-12V', '5': None, '6': 'AMP_OUT', '7': '+12V', '8': None},
         sch=(360.68, 175.26), pcb=(68.0, 28.0, 0)),
    dict(ref='R5', lib='Device:R', value='1k', fp=R0805,
         pins={'1': 'AMP_OUT', '2': 'AMP_FB'}, sch=(360.68, 154.94), pcb=(68.0, 33.0, 0)),
    dict(ref='R6', lib='Device:R', value='330', fp=R0805,
         pins={'1': 'AMP_FB', '2': 'GND'}, sch=(330.2, 195.58), pcb=(62.0, 26.5, 0)),
    dict(ref='R7', lib='Device:R', value='49.9', fp=R0805,
         pins={'1': 'AMP_OUT', '2': 'SINE_OUT'}, sch=(382.54, 175.26), pcb=(73.5, 29.0, 90)),
    dict(ref='J4', lib='Connector:Conn_Coaxial', value='SINE', fp=BNC,
         pins={'1': 'SINE_OUT', '2': 'GND'}, sch=(400.05, 175.26), pcb=(85.0, 16.0, -90)),
    dict(ref='C7', lib='Device:C', value='100nF', fp=C0805,
         pins={'1': '+12V', '2': 'GND'}, sch=(330.2, 215.9), pcb=(73.0, 24.0, 0)),
    dict(ref='C8', lib='Device:C_Polarized', value='10uF 25V', fp=CP5,
         pins={'1': '+12V', '2': 'GND'}, sch=(342.9, 215.9), pcb=(66.0, 20.0, 0)),
    dict(ref='C9', lib='Device:C', value='100nF', fp=C0805,
         pins={'1': 'GND', '2': '-12V'}, sch=(368.3, 215.9), pcb=(63.0, 34.0, 0)),
    dict(ref='C10', lib='Device:C_Polarized', value='10uF 25V', fp=CP5,
         pins={'1': 'GND', '2': '-12V'}, sch=(381.0, 215.9), pcb=(68.0, 38.5, 0)),

    # ---- sync output
    dict(ref='R3', lib='Device:R', value='100', fp=R0805,
         pins={'1': 'SYNC_MCU', '2': 'SYNC_OUT'}, sch=(165.1, 251.46), pcb=(74.5, 44.5, 0)),
    dict(ref='J6', lib='Connector:Conn_Coaxial', value='SYNC', fp=BNC,
         pins={'1': 'SYNC_OUT', '2': 'GND'}, sch=(190.5, 251.46), pcb=(85.0, 55.0, -90)),

    # ---- LCD and encoder headers
    dict(ref='J2', lib='Connector_Generic:Conn_01x04', value='LCD I2C', fp='Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical',
         pins={'1': 'GND', '2': '+5V', '3': 'I2C_SDA', '4': 'I2C_SCL'}, sch=(30.48, 251.46), pcb=(14.0, 86.0, 90)),
    dict(ref='J3', lib='Connector_Generic:Conn_01x05', value='KY-040 encoder', fp='Connector_PinHeader_2.54mm:PinHeader_1x05_P2.54mm_Vertical',
         pins={'1': 'ENC_CLK', '2': 'ENC_DT', '3': 'ENC_SW', '4': '+3V3', '5': 'GND'}, sch=(30.48, 274.32), pcb=(30.0, 86.0, 90)),
    dict(ref='R8', lib='Device:R', value='10k', fp=R0805,
         pins={'1': '+3V3', '2': 'ENC_SW'}, sch=(68.58, 274.32), pcb=(46.0, 86.0, 0)),

    # ---- mounting holes
    dict(ref='H1', lib='Mechanical:MountingHole', value='M3', fp='MountingHole:MountingHole_3.2mm_M3', pins={}, sch=(381.0, 30.48), pcb=(4.0, 4.0, 0)),
    dict(ref='H2', lib='Mechanical:MountingHole', value='M3', fp='MountingHole:MountingHole_3.2mm_M3', pins={}, sch=(391.16, 30.48), pcb=(96.0, 4.0, 0)),
    dict(ref='H3', lib='Mechanical:MountingHole', value='M3', fp='MountingHole:MountingHole_3.2mm_M3', pins={}, sch=(381.0, 40.64), pcb=(4.0, 86.0, 0)),
    dict(ref='H4', lib='Mechanical:MountingHole', value='M3', fp='MountingHole:MountingHole_3.2mm_M3', pins={}, sch=(391.16, 40.64), pcb=(96.0, 68.5, 0)),
]

POWER_NETS = ['GND', 'VIN', '+5V', '+3V3', 'P15_RAW', 'N15_RAW', '+15V', '-15V', '+12V', '-12V']

SCH_NOTES = [
    ((20.32, 22.86), 'POWER: 5 V DC in -> SS34 reverse protection -> +5V.'),
    ((20.32, 26.67), 'A0515S-2WR3 makes isolated +-15 V, LC filtered, 78L12 / 79L12 -> +-12 V for the LM7171.'),
    ((20.32, 101.6), 'MCU (Blue Pill) -> 74AHCT125 level shifter (3.3 V -> 5 V logic for the AD9850).'),
    ((20.32, 105.41), 'PA3 drives AD9850 RESET; R1 10k pull-down keeps it low while PA3 is an input.'),
    ((210.82, 101.6), 'AD9850 module: ZOUT2 = filtered sine, QOUT1 = comparator square.'),
    ((210.82, 105.41), 'Data row: D2..D6 tied to GND (serial mode). VCC/D7/GND of the data row are'),
    ((210.82, 109.22), 'left open because some modules have D7 and GND swapped on that row.'),
    ((276.86, 241.3), 'Sine: AC coupling (centres on 0 V) -> 1k amplitude pot ->'),
    ((276.86, 245.11), 'LM7171 gain 1 + 1k/330 = 4 -> 49.9 ohm back termination.'),
    ((20.32, 236.22), 'LCD (PCF8574 backpack, 5 V) and KY-040 encoder (3.3 V, SW pull-up R8).'),
    ((140.97, 236.22), 'SYNC = PB0 via 100 ohm.'),
]

# (text, x, y, size, rotation) silkscreen labels on the board
PCB_TEXT = [
    ('AD9850 DDS FUNCTION GENERATOR  v1.0', 33.0, 3.0, 1.2, 0),
    ('SINE', 86.0, 7.0, 1.0, 0),
    ('SQUARE', 86.0, 26.6, 1.0, 0),
    ('SYNC', 86.0, 46.1, 1.0, 0),
    ('5V DC', 92.0, 73.5, 1.0, 0),
    ('AMPL', 56.8, 8.0, 1.0, 90),
    ('LCD I2C', 17.8, 82.6, 1.0, 0),
    ('ENCODER KY-040', 35.1, 82.6, 1.0, 0),
] + [(n, 14.0 + i * 2.54, 88.9, 0.8, 0) for i, n in enumerate(['GND', 'VCC', 'SDA', 'SCL'])] \
  + [(n, 30.0 + i * 2.54, 88.9, 0.8, 0) for i, n in enumerate(['CLK', 'DT', 'SW', '+', 'GND'])]

# Reference designator positions that need moving off other silkscreen (board coords)
REF_POS = {
    'R7': (73.5, 32.6),
    'C14': (74.6, 59.5),
    'C15': (60.6, 63.6),
    'C5': (59.4, 21.5),
    'PS1': (69.1, 72.2),
}
