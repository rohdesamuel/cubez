/**
* Author: Samuel Rohde (rohde.samuel@cubez.io)
*
* Copyright 2020 Samuel Rohde
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#ifndef INPUT__H
#define INPUT__H

#include <cubez/cubez.h>

#include <functional>

/**

The following keyboard input code was copied and pasted from the SDL
SDL_scancode.h and SDL_keycode.h header files with 'SDL' replaced with 'QB'.
*/

#define QB_SCANCODE_MASK (1<<30)
#define QB_SCANCODE_TO_KEYCODE(X)  (X | QB_SCANCODE_MASK)

enum qbScanCode {
    QB_SCANCODE_A = 4,
    QB_SCANCODE_B = 5,
    QB_SCANCODE_C = 6,
    QB_SCANCODE_D = 7,
    QB_SCANCODE_E = 8,
    QB_SCANCODE_F = 9,
    QB_SCANCODE_G = 10,
    QB_SCANCODE_H = 11,
    QB_SCANCODE_I = 12,
    QB_SCANCODE_J = 13,
    QB_SCANCODE_K = 14,
    QB_SCANCODE_L = 15,
    QB_SCANCODE_M = 16,
    QB_SCANCODE_N = 17,
    QB_SCANCODE_O = 18,
    QB_SCANCODE_P = 19,
    QB_SCANCODE_Q = 20,
    QB_SCANCODE_R = 21,
    QB_SCANCODE_S = 22,
    QB_SCANCODE_T = 23,
    QB_SCANCODE_U = 24,
    QB_SCANCODE_V = 25,
    QB_SCANCODE_W = 26,
    QB_SCANCODE_X = 27,
    QB_SCANCODE_Y = 28,
    QB_SCANCODE_Z = 29,

    QB_SCANCODE_1 = 30,
    QB_SCANCODE_2 = 31,
    QB_SCANCODE_3 = 32,
    QB_SCANCODE_4 = 33,
    QB_SCANCODE_5 = 34,
    QB_SCANCODE_6 = 35,
    QB_SCANCODE_7 = 36,
    QB_SCANCODE_8 = 37,
    QB_SCANCODE_9 = 38,
    QB_SCANCODE_0 = 39,

    QB_SCANCODE_RETURN = 40,
    QB_SCANCODE_ESCAPE = 41,
    QB_SCANCODE_BACKSPACE = 42,
    QB_SCANCODE_TAB = 43,
    QB_SCANCODE_SPACE = 44,

    QB_SCANCODE_MINUS = 45,
    QB_SCANCODE_EQUALS = 46,
    QB_SCANCODE_LEFTBRACKET = 47,
    QB_SCANCODE_RIGHTBRACKET = 48,
    QB_SCANCODE_BACKSLASH = 49, /**< Located at the lower left of the return
                                  *   key on ISO keyboards and at the right end
                                  *   of the QWERTY row on ANSI keyboards.
                                  *   Produces REVERSE SOLIDUS (backslash) and
                                  *   VERTICAL LINE in a US layout, REVERSE
                                  *   SOLIDUS and VERTICAL LINE in a UK Mac
                                  *   layout, NUMBER SIGN and TILDE in a UK
                                  *   Windows layout, DOLLAR SIGN and POUND SIGN
                                  *   in a Swiss German layout, NUMBER SIGN and
                                  *   APOSTROPHE in a German layout, GRAVE
                                  *   ACCENT and POUND SIGN in a French Mac
                                  *   layout, and ASTERISK and MICRO SIGN in a
                                  *   French Windows layout.
                                  */
    QB_SCANCODE_NONUSHASH = 50, /**< ISO USB keyboards actually use this code
                                  *   instead of 49 for the same key, but all
                                  *   OSes I've seen treat the two codes
                                  *   identically. So, as an implementor, unless
                                  *   your keyboard generates both of those
                                  *   codes and your OS treats them differently,
                                  *   you should generate QB_SCANCODE_BACKSLASH
                                  *   instead of this code. As a user, you
                                  *   should not rely on this code because SDL
                                  *   will never generate it with most (all?)
                                  *   keyboards.
                                  */
    QB_SCANCODE_SEMICOLON = 51,
    QB_SCANCODE_APOSTROPHE = 52,
    QB_SCANCODE_GRAVE = 53, /**< Located in the top left corner (on both ANSI
                              *   and ISO keyboards). Produces GRAVE ACCENT and
                              *   TILDE in a US Windows layout and in US and UK
                              *   Mac layouts on ANSI keyboards, GRAVE ACCENT
                              *   and NOT SIGN in a UK Windows layout, SECTION
                              *   SIGN and PLUS-MINUS SIGN in US and UK Mac
                              *   layouts on ISO keyboards, SECTION SIGN and
                              *   DEGREE SIGN in a Swiss German layout (Mac:
                              *   only on ISO keyboards), CIRCUMFLEX ACCENT and
                              *   DEGREE SIGN in a German layout (Mac: only on
                              *   ISO keyboards), SUPERSCRIPT TWO and TILDE in a
                              *   French Windows layout, COMMERCIAL AT and
                              *   NUMBER SIGN in a French Mac layout on ISO
                              *   keyboards, and LESS-THAN SIGN and GREATER-THAN
                              *   SIGN in a Swiss German, German, or French Mac
                              *   layout on ANSI keyboards.
                              */
    QB_SCANCODE_COMMA = 54,
    QB_SCANCODE_PERIOD = 55,
    QB_SCANCODE_SLASH = 56,

    QB_SCANCODE_CAPSLOCK = 57,

    QB_SCANCODE_F1 = 58,
    QB_SCANCODE_F2 = 59,
    QB_SCANCODE_F3 = 60,
    QB_SCANCODE_F4 = 61,
    QB_SCANCODE_F5 = 62,
    QB_SCANCODE_F6 = 63,
    QB_SCANCODE_F7 = 64,
    QB_SCANCODE_F8 = 65,
    QB_SCANCODE_F9 = 66,
    QB_SCANCODE_F10 = 67,
    QB_SCANCODE_F11 = 68,
    QB_SCANCODE_F12 = 69,

    QB_SCANCODE_PRINTSCREEN = 70,
    QB_SCANCODE_SCROLLLOCK = 71,
    QB_SCANCODE_PAUSE = 72,
    QB_SCANCODE_INSERT = 73, /**< insert on PC, help on some Mac keyboards (but
                                   does send code 73, not 117) */
    QB_SCANCODE_HOME = 74,
    QB_SCANCODE_PAGEUP = 75,
    QB_SCANCODE_DELETE = 76,
    QB_SCANCODE_END = 77,
    QB_SCANCODE_PAGEDOWN = 78,
    QB_SCANCODE_RIGHT = 79,
    QB_SCANCODE_LEFT = 80,
    QB_SCANCODE_DOWN = 81,
    QB_SCANCODE_UP = 82,

    QB_SCANCODE_NUMLOCKCLEAR = 83, /**< num lock on PC, clear on Mac keyboards
                                     */
    QB_SCANCODE_KP_DIVIDE = 84,
    QB_SCANCODE_KP_MULTIPLY = 85,
    QB_SCANCODE_KP_MINUS = 86,
    QB_SCANCODE_KP_PLUS = 87,
    QB_SCANCODE_KP_ENTER = 88,
    QB_SCANCODE_KP_1 = 89,
    QB_SCANCODE_KP_2 = 90,
    QB_SCANCODE_KP_3 = 91,
    QB_SCANCODE_KP_4 = 92,
    QB_SCANCODE_KP_5 = 93,
    QB_SCANCODE_KP_6 = 94,
    QB_SCANCODE_KP_7 = 95,
    QB_SCANCODE_KP_8 = 96,
    QB_SCANCODE_KP_9 = 97,
    QB_SCANCODE_KP_0 = 98,
    QB_SCANCODE_KP_PERIOD = 99,

    QB_SCANCODE_NONUSBACKSLASH = 100, /**< This is the additional key that ISO
                                        *   keyboards have over ANSI ones,
                                        *   located between left shift and Y.
                                        *   Produces GRAVE ACCENT and TILDE in a
                                        *   US or UK Mac layout, REVERSE SOLIDUS
                                        *   (backslash) and VERTICAL LINE in a
                                        *   US or UK Windows layout, and
                                        *   LESS-THAN SIGN and GREATER-THAN SIGN
                                        *   in a Swiss German, German, or French
                                        *   layout. */
    QB_SCANCODE_APPLICATION = 101, /**< windows contextual menu, compose */
    QB_SCANCODE_POWER = 102, /**< The USB document says this is a status flag,
                               *   not a physical key - but some Mac keyboards
                               *   do have a power key. */
    QB_SCANCODE_KP_EQUALS = 103,
    QB_SCANCODE_F13 = 104,
    QB_SCANCODE_F14 = 105,
    QB_SCANCODE_F15 = 106,
    QB_SCANCODE_F16 = 107,
    QB_SCANCODE_F17 = 108,
    QB_SCANCODE_F18 = 109,
    QB_SCANCODE_F19 = 110,
    QB_SCANCODE_F20 = 111,
    QB_SCANCODE_F21 = 112,
    QB_SCANCODE_F22 = 113,
    QB_SCANCODE_F23 = 114,
    QB_SCANCODE_F24 = 115,
    QB_SCANCODE_EXECUTE = 116,
    QB_SCANCODE_HELP = 117,
    QB_SCANCODE_MENU = 118,
    QB_SCANCODE_SELECT = 119,
    QB_SCANCODE_STOP = 120,
    QB_SCANCODE_AGAIN = 121,   /**< redo */
    QB_SCANCODE_UNDO = 122,
    QB_SCANCODE_CUT = 123,
    QB_SCANCODE_COPY = 124,
    QB_SCANCODE_PASTE = 125,
    QB_SCANCODE_FIND = 126,
    QB_SCANCODE_MUTE = 127,
    QB_SCANCODE_VOLUMEUP = 128,
    QB_SCANCODE_VOLUMEDOWN = 129,
/* not sure whether there's a reason to enable these */
/*     QB_SCANCODE_LOCKINGCAPSLOCK = 130,  */
/*     QB_SCANCODE_LOCKINGNUMLOCK = 131, */
/*     QB_SCANCODE_LOCKINGSCROLLLOCK = 132, */
    QB_SCANCODE_KP_COMMA = 133,
    QB_SCANCODE_KP_EQUALSAS400 = 134,

    QB_SCANCODE_INTERNATIONAL1 = 135, /**< used on Asian keyboards, see
                                            footnotes in USB doc */
    QB_SCANCODE_INTERNATIONAL2 = 136,
    QB_SCANCODE_INTERNATIONAL3 = 137, /**< Yen */
    QB_SCANCODE_INTERNATIONAL4 = 138,
    QB_SCANCODE_INTERNATIONAL5 = 139,
    QB_SCANCODE_INTERNATIONAL6 = 140,
    QB_SCANCODE_INTERNATIONAL7 = 141,
    QB_SCANCODE_INTERNATIONAL8 = 142,
    QB_SCANCODE_INTERNATIONAL9 = 143,
    QB_SCANCODE_LANG1 = 144, /**< Hangul/English toggle */
    QB_SCANCODE_LANG2 = 145, /**< Hanja conversion */
    QB_SCANCODE_LANG3 = 146, /**< Katakana */
    QB_SCANCODE_LANG4 = 147, /**< Hiragana */
    QB_SCANCODE_LANG5 = 148, /**< Zenkaku/Hankaku */
    QB_SCANCODE_LANG6 = 149, /**< reserved */
    QB_SCANCODE_LANG7 = 150, /**< reserved */
    QB_SCANCODE_LANG8 = 151, /**< reserved */
    QB_SCANCODE_LANG9 = 152, /**< reserved */

    QB_SCANCODE_ALTERASE = 153, /**< Erase-Eaze */
    QB_SCANCODE_SYSREQ = 154,
    QB_SCANCODE_CANCEL = 155,
    QB_SCANCODE_CLEAR = 156,
    QB_SCANCODE_PRIOR = 157,
    QB_SCANCODE_RETURN2 = 158,
    QB_SCANCODE_SEPARATOR = 159,
    QB_SCANCODE_OUT = 160,
    QB_SCANCODE_OPER = 161,
    QB_SCANCODE_CLEARAGAIN = 162,
    QB_SCANCODE_CRSEL = 163,
    QB_SCANCODE_EXSEL = 164,

    QB_SCANCODE_KP_00 = 176,
    QB_SCANCODE_KP_000 = 177,
    QB_SCANCODE_THOUSANDSSEPARATOR = 178,
    QB_SCANCODE_DECIMALSEPARATOR = 179,
    QB_SCANCODE_CURRENCYUNIT = 180,
    QB_SCANCODE_CURRENCYSUBUNIT = 181,
    QB_SCANCODE_KP_LEFTPAREN = 182,
    QB_SCANCODE_KP_RIGHTPAREN = 183,
    QB_SCANCODE_KP_LEFTBRACE = 184,
    QB_SCANCODE_KP_RIGHTBRACE = 185,
    QB_SCANCODE_KP_TAB = 186,
    QB_SCANCODE_KP_BACKSPACE = 187,
    QB_SCANCODE_KP_A = 188,
    QB_SCANCODE_KP_B = 189,
    QB_SCANCODE_KP_C = 190,
    QB_SCANCODE_KP_D = 191,
    QB_SCANCODE_KP_E = 192,
    QB_SCANCODE_KP_F = 193,
    QB_SCANCODE_KP_XOR = 194,
    QB_SCANCODE_KP_POWER = 195,
    QB_SCANCODE_KP_PERCENT = 196,
    QB_SCANCODE_KP_LESS = 197,
    QB_SCANCODE_KP_GREATER = 198,
    QB_SCANCODE_KP_AMPERSAND = 199,
    QB_SCANCODE_KP_DBLAMPERSAND = 200,
    QB_SCANCODE_KP_VERTICALBAR = 201,
    QB_SCANCODE_KP_DBLVERTICALBAR = 202,
    QB_SCANCODE_KP_COLON = 203,
    QB_SCANCODE_KP_HASH = 204,
    QB_SCANCODE_KP_SPACE = 205,
    QB_SCANCODE_KP_AT = 206,
    QB_SCANCODE_KP_EXCLAM = 207,
    QB_SCANCODE_KP_MEMSTORE = 208,
    QB_SCANCODE_KP_MEMRECALL = 209,
    QB_SCANCODE_KP_MEMCLEAR = 210,
    QB_SCANCODE_KP_MEMADD = 211,
    QB_SCANCODE_KP_MEMSUBTRACT = 212,
    QB_SCANCODE_KP_MEMMULTIPLY = 213,
    QB_SCANCODE_KP_MEMDIVIDE = 214,
    QB_SCANCODE_KP_PLUSMINUS = 215,
    QB_SCANCODE_KP_CLEAR = 216,
    QB_SCANCODE_KP_CLEARENTRY = 217,
    QB_SCANCODE_KP_BINARY = 218,
    QB_SCANCODE_KP_OCTAL = 219,
    QB_SCANCODE_KP_DECIMAL = 220,
    QB_SCANCODE_KP_HEXADECIMAL = 221,

    QB_SCANCODE_LCTRL = 224,
    QB_SCANCODE_LSHIFT = 225,
    QB_SCANCODE_LALT = 226, /**< alt, option */
    QB_SCANCODE_LGUI = 227, /**< windows, command (apple), meta */
    QB_SCANCODE_RCTRL = 228,
    QB_SCANCODE_RSHIFT = 229,
    QB_SCANCODE_RALT = 230, /**< alt gr, option */
    QB_SCANCODE_RGUI = 231, /**< windows, command (apple), meta */

    QB_SCANCODE_MODE = 257,    /**< I'm not sure if this is really not covered
                                 *   by any of the above, but since there's a
                                 *   special KMOD_MODE for it I'm adding it here
                                 */

    /* @} *//* Usage page 0x07 */

    /**
     *  \name Usage page 0x0C
     *
     *  These values are mapped from usage page 0x0C (USB consumer page).
     */
    /* @{ */

    QB_SCANCODE_AUDIONEXT = 258,
    QB_SCANCODE_AUDIOPREV = 259,
    QB_SCANCODE_AUDIOSTOP = 260,
    QB_SCANCODE_AUDIOPLAY = 261,
    QB_SCANCODE_AUDIOMUTE = 262,
    QB_SCANCODE_MEDIASELECT = 263,
    QB_SCANCODE_WWW = 264,
    QB_SCANCODE_MAIL = 265,
    QB_SCANCODE_CALCULATOR = 266,
    QB_SCANCODE_COMPUTER = 267,
    QB_SCANCODE_AC_SEARCH = 268,
    QB_SCANCODE_AC_HOME = 269,
    QB_SCANCODE_AC_BACK = 270,
    QB_SCANCODE_AC_FORWARD = 271,
    QB_SCANCODE_AC_STOP = 272,
    QB_SCANCODE_AC_REFRESH = 273,
    QB_SCANCODE_AC_BOOKMARKS = 274,

    /* @} *//* Usage page 0x0C */

    /**
     *  \name Walther keys
     *
     *  These are values that Christian Walther added (for mac keyboard?).
     */
    /* @{ */

    QB_SCANCODE_BRIGHTNESSDOWN = 275,
    QB_SCANCODE_BRIGHTNESSUP = 276,
    QB_SCANCODE_DISPLAYSWITCH = 277, /**< display mirroring/dual display
                                           switch, video mode switch */
    QB_SCANCODE_KBDILLUMTOGGLE = 278,
    QB_SCANCODE_KBDILLUMDOWN = 279,
    QB_SCANCODE_KBDILLUMUP = 280,
    QB_SCANCODE_EJECT = 281,
    QB_SCANCODE_SLEEP = 282,

    QB_SCANCODE_APP1 = 283,
    QB_SCANCODE_APP2 = 284,

    /* @} *//* Walther keys */

    /**
     *  \name Usage page 0x0C (additional media keys)
     *
     *  These values are mapped from usage page 0x0C (USB consumer page).
     */
    /* @{ */

    QB_SCANCODE_AUDIOREWIND = 285,
    QB_SCANCODE_AUDIOFASTFORWARD = 286,

    /* @} *//* Usage page 0x0C (additional media keys) */

    /* Add any other keys here. */

    QB_NUM_SCANCODES = 512 /**< not a key, just marks the number of scancodes
                                 for array bounds */
};


enum qbKey {
  QB_KEY_UNKNOWN,
  QB_KEY_RETURN = '\r',
  QB_KEY_ESCAPE = '\033',
  QB_KEY_BACKSPACE = '\b',
  QB_KEY_TAB = '\t',
  QB_KEY_SPACE = ' ',
  QB_KEY_EXCLAIM = '!',
  QB_KEY_QUOTEDBL = '"',
  QB_KEY_HASH = '#',
  QB_KEY_PERCENT = '%',
  QB_KEY_DOLLAR = '$',
  QB_KEY_AMPERSAND = '&',
  QB_KEY_QUOTE = '\'',
  QB_KEY_LPAREN = '(',
  QB_KEY_RPAREN = ')',
  QB_KEY_ASTERISK = '*',
  QB_KEY_PLUS = '+',
  QB_KEY_COMMA = ',',
  QB_KEY_MINUS = '-',
  QB_KEY_PERIOD = '.',
  QB_KEY_SLASH = '/',
  QB_KEY_0 = '0',
  QB_KEY_1 = '1',
  QB_KEY_2 = '2',
  QB_KEY_3 = '3',
  QB_KEY_4 = '4',
  QB_KEY_5 = '5',
  QB_KEY_6 = '6',
  QB_KEY_7 = '7',
  QB_KEY_8 = '8',
  QB_KEY_9 = '9',
  QB_KEY_COLON = ':',
  QB_KEY_SEMICOLON = ';',
  QB_KEY_LESS = '<',
  QB_KEY_EQUALS = '=',
  QB_KEY_GREATER = '>',
  QB_KEY_QUESTION = '?',
  QB_KEY_AT = '@',
  QB_KEY_LBRACKET = '[',
  QB_KEY_BACKSLASH = '\\',
  QB_KEY_RBRACKET = ']',
  QB_KEY_CARET = '^',
  QB_KEY_UNDERSCORE = '_',
  QB_KEY_BACKQUOTE = '`',
  QB_KEY_A = 'a',
  QB_KEY_B = 'b',
  QB_KEY_C = 'c',
  QB_KEY_D = 'd',
  QB_KEY_E = 'e',
  QB_KEY_F = 'f',
  QB_KEY_G = 'g',
  QB_KEY_H = 'h',
  QB_KEY_I = 'i',
  QB_KEY_J = 'j',
  QB_KEY_K = 'k',
  QB_KEY_L = 'l',
  QB_KEY_M = 'm',
  QB_KEY_N = 'n',
  QB_KEY_O = 'o',
  QB_KEY_P = 'p',
  QB_KEY_Q = 'q',
  QB_KEY_R = 'r',
  QB_KEY_S = 's',
  QB_KEY_T = 't',
  QB_KEY_U = 'u',
  QB_KEY_V = 'v',
  QB_KEY_W = 'w',
  QB_KEY_X = 'x',
  QB_KEY_Y = 'y',
  QB_KEY_Z = 'z',

  QB_KEY_CAPSLOCK = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_CAPSLOCK),

  QB_KEY_F1 = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_F1),
  QB_KEY_F2 = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_F2),
  QB_KEY_F3 = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_F3),
  QB_KEY_F4 = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_F4),
  QB_KEY_F5 = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_F5),
  QB_KEY_F6 = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_F6),
  QB_KEY_F7 = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_F7),
  QB_KEY_F8 = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_F8),
  QB_KEY_F9 = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_F9),
  QB_KEY_F10 = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_F10),
  QB_KEY_F11 = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_F11),
  QB_KEY_F12 = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_F12),

  QB_KEY_PRINTSCREEN = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_PRINTSCREEN),
  QB_KEY_SCROLLLOCK = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_SCROLLLOCK),
  QB_KEY_PAUSE = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_PAUSE),
  QB_KEY_INSERT = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_INSERT),
  QB_KEY_HOME = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_HOME),
  QB_KEY_PAGEUP = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_PAGEUP),
  QB_KEY_DELETE = '\177',
  QB_KEY_END = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_END),
  QB_KEY_PAGEDOWN = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_PAGEDOWN),
  QB_KEY_RIGHT = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_RIGHT),
  QB_KEY_LEFT = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_LEFT),
  QB_KEY_DOWN = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_DOWN),
  QB_KEY_UP = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_UP),

  QB_KEY_NUMLOCKCLEAR = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_NUMLOCKCLEAR),
  QB_KEY_KP_DIVIDE = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_DIVIDE),
  QB_KEY_KP_MULTIPLY = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_MULTIPLY),
  QB_KEY_KP_MINUS = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_MINUS),
  QB_KEY_KP_PLUS = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_PLUS),
  QB_KEY_KP_ENTER = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_ENTER),
  QB_KEY_KP_1 = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_1),
  QB_KEY_KP_2 = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_2),
  QB_KEY_KP_3 = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_3),
  QB_KEY_KP_4 = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_4),
  QB_KEY_KP_5 = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_5),
  QB_KEY_KP_6 = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_6),
  QB_KEY_KP_7 = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_7),
  QB_KEY_KP_8 = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_8),
  QB_KEY_KP_9 = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_9),
  QB_KEY_KP_0 = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_0),
  QB_KEY_KP_PERIOD = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_PERIOD),

  QB_KEY_APPLICATION = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_APPLICATION),
  QB_KEY_POWER = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_POWER),
  QB_KEY_KP_EQUALS = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_EQUALS),
  QB_KEY_F13 = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_F13),
  QB_KEY_F14 = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_F14),
  QB_KEY_F15 = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_F15),
  QB_KEY_F16 = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_F16),
  QB_KEY_F17 = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_F17),
  QB_KEY_F18 = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_F18),
  QB_KEY_F19 = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_F19),
  QB_KEY_F20 = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_F20),
  QB_KEY_F21 = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_F21),
  QB_KEY_F22 = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_F22),
  QB_KEY_F23 = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_F23),
  QB_KEY_F24 = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_F24),
  QB_KEY_EXECUTE = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_EXECUTE),
  QB_KEY_HELP = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_HELP),
  QB_KEY_MENU = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_MENU),
  QB_KEY_SELECT = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_SELECT),
  QB_KEY_STOP = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_STOP),
  QB_KEY_AGAIN = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_AGAIN),
  QB_KEY_UNDO = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_UNDO),
  QB_KEY_CUT = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_CUT),
  QB_KEY_COPY = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_COPY),
  QB_KEY_PASTE = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_PASTE),
  QB_KEY_FIND = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_FIND),
  QB_KEY_MUTE = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_MUTE),
  QB_KEY_VOLUMEUP = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_VOLUMEUP),
  QB_KEY_VOLUMEDOWN = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_VOLUMEDOWN),
  QB_KEY_KP_COMMA = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_COMMA),
  QB_KEY_KP_EQUALSAS400 =
  QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_EQUALSAS400),

  QB_KEY_ALTERASE = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_ALTERASE),
  QB_KEY_SYSREQ = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_SYSREQ),
  QB_KEY_CANCEL = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_CANCEL),
  QB_KEY_CLEAR = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_CLEAR),
  QB_KEY_PRIOR = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_PRIOR),
  QB_KEY_RETURN2 = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_RETURN2),
  QB_KEY_SEPARATOR = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_SEPARATOR),
  QB_KEY_OUT = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_OUT),
  QB_KEY_OPER = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_OPER),
  QB_KEY_CLEARAGAIN = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_CLEARAGAIN),
  QB_KEY_CRSEL = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_CRSEL),
  QB_KEY_EXSEL = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_EXSEL),

  QB_KEY_KP_00 = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_00),
  QB_KEY_KP_000 = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_000),
  QB_KEY_THOUSANDSSEPARATOR =
  QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_THOUSANDSSEPARATOR),
  QB_KEY_DECIMALSEPARATOR =
  QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_DECIMALSEPARATOR),
  QB_KEY_CURRENCYUNIT = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_CURRENCYUNIT),
  QB_KEY_CURRENCYSUBUNIT =
  QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_CURRENCYSUBUNIT),
  QB_KEY_KP_LEFTPAREN = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_LEFTPAREN),
  QB_KEY_KP_RIGHTPAREN = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_RIGHTPAREN),
  QB_KEY_KP_LEFTBRACE = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_LEFTBRACE),
  QB_KEY_KP_RIGHTBRACE = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_RIGHTBRACE),
  QB_KEY_KP_TAB = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_TAB),
  QB_KEY_KP_BACKSPACE = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_BACKSPACE),
  QB_KEY_KP_A = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_A),
  QB_KEY_KP_B = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_B),
  QB_KEY_KP_C = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_C),
  QB_KEY_KP_D = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_D),
  QB_KEY_KP_E = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_E),
  QB_KEY_KP_F = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_F),
  QB_KEY_KP_XOR = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_XOR),
  QB_KEY_KP_POWER = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_POWER),
  QB_KEY_KP_PERCENT = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_PERCENT),
  QB_KEY_KP_LESS = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_LESS),
  QB_KEY_KP_GREATER = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_GREATER),
  QB_KEY_KP_AMPERSAND = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_AMPERSAND),
  QB_KEY_KP_DBLAMPERSAND =
  QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_DBLAMPERSAND),
  QB_KEY_KP_VERTICALBAR =
  QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_VERTICALBAR),
  QB_KEY_KP_DBLVERTICALBAR =
  QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_DBLVERTICALBAR),
  QB_KEY_KP_COLON = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_COLON),
  QB_KEY_KP_HASH = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_HASH),
  QB_KEY_KP_SPACE = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_SPACE),
  QB_KEY_KP_AT = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_AT),
  QB_KEY_KP_EXCLAM = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_EXCLAM),
  QB_KEY_KP_MEMSTORE = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_MEMSTORE),
  QB_KEY_KP_MEMRECALL = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_MEMRECALL),
  QB_KEY_KP_MEMCLEAR = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_MEMCLEAR),
  QB_KEY_KP_MEMADD = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_MEMADD),
  QB_KEY_KP_MEMSUBTRACT =
  QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_MEMSUBTRACT),
  QB_KEY_KP_MEMMULTIPLY =
  QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_MEMMULTIPLY),
  QB_KEY_KP_MEMDIVIDE = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_MEMDIVIDE),
  QB_KEY_KP_PLUSMINUS = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_PLUSMINUS),
  QB_KEY_KP_CLEAR = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_CLEAR),
  QB_KEY_KP_CLEARENTRY = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_CLEARENTRY),
  QB_KEY_KP_BINARY = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_BINARY),
  QB_KEY_KP_OCTAL = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_OCTAL),
  QB_KEY_KP_DECIMAL = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_DECIMAL),
  QB_KEY_KP_HEXADECIMAL =
  QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KP_HEXADECIMAL),

  QB_KEY_LCTRL = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_LCTRL),
  QB_KEY_LSHIFT = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_LSHIFT),
  QB_KEY_LALT = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_LALT),
  QB_KEY_LGUI = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_LGUI),
  QB_KEY_RCTRL = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_RCTRL),
  QB_KEY_RSHIFT = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_RSHIFT),
  QB_KEY_RALT = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_RALT),
  QB_KEY_RGUI = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_RGUI),

  QB_KEY_MODE = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_MODE),

  QB_KEY_AUDIONEXT = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_AUDIONEXT),
  QB_KEY_AUDIOPREV = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_AUDIOPREV),
  QB_KEY_AUDIOSTOP = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_AUDIOSTOP),
  QB_KEY_AUDIOPLAY = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_AUDIOPLAY),
  QB_KEY_AUDIOMUTE = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_AUDIOMUTE),
  QB_KEY_MEDIASELECT = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_MEDIASELECT),
  QB_KEY_WWW = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_WWW),
  QB_KEY_MAIL = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_MAIL),
  QB_KEY_CALCULATOR = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_CALCULATOR),
  QB_KEY_COMPUTER = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_COMPUTER),
  QB_KEY_AC_SEARCH = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_AC_SEARCH),
  QB_KEY_AC_HOME = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_AC_HOME),
  QB_KEY_AC_BACK = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_AC_BACK),
  QB_KEY_AC_FORWARD = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_AC_FORWARD),
  QB_KEY_AC_STOP = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_AC_STOP),
  QB_KEY_AC_REFRESH = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_AC_REFRESH),
  QB_KEY_AC_BOOKMARKS = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_AC_BOOKMARKS),

  QB_KEY_BRIGHTNESSDOWN =
  QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_BRIGHTNESSDOWN),
  QB_KEY_BRIGHTNESSUP = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_BRIGHTNESSUP),
  QB_KEY_DISPLAYSWITCH = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_DISPLAYSWITCH),
  QB_KEY_KBDILLUMTOGGLE =
  QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KBDILLUMTOGGLE),
  QB_KEY_KBDILLUMDOWN = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KBDILLUMDOWN),
  QB_KEY_KBDILLUMUP = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_KBDILLUMUP),
  QB_KEY_EJECT = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_EJECT),
  QB_KEY_SLEEP = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_SLEEP),
  QB_KEY_APP1 = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_APP1),
  QB_KEY_APP2 = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_APP2),

  QB_KEY_AUDIOREWIND = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_AUDIOREWIND),
  QB_KEY_AUDIOFASTFORWARD = QB_SCANCODE_TO_KEYCODE(QB_SCANCODE_AUDIOFASTFORWARD)
};

enum qbButton {
  QB_BUTTON_LEFT,
  QB_BUTTON_MIDDLE,
  QB_BUTTON_RIGHT,
  QB_BUTTON_X1,
  QB_BUTTON_X2
};

#define QB_KEY_RELEASED 0
#define QB_KEY_PRESSED 1

enum qbInputEventType {
  QB_INPUT_EVENT_KEY,
  QB_INPUT_EVENT_MOUSE,
};

typedef struct {
  qbBool was_pressed;
  qbBool is_pressed;
  qbKey key;
  qbScanCode scane_code;
} qbKeyEvent_, *qbKeyEvent;

enum qbMouseState {
  QB_MOUSE_UP,
  QB_MOUSE_DOWN
};

enum qbMouseEventType {
  QB_MOUSE_EVENT_MOTION,
  QB_MOUSE_EVENT_BUTTON,
  QB_MOUSE_EVENT_SCROLL,
};

typedef struct {
  qbMouseState state;
  qbButton button;
} qbMouseButtonEvent_, *qbMouseButtonEvent;

typedef struct {
  int x;
  int y;
  int xrel;
  int yrel;
} qbMouseMotionEvent_, *qbMouseMotionEvent;

typedef struct {
  int x;
  int y;
  int xrel;
  int yrel;
} qbMouseScrollEvent_, *qbMouseScrollEvent;

typedef struct {
  union {
    qbMouseButtonEvent_ button;
    qbMouseMotionEvent_ motion;
    qbMouseScrollEvent_ scroll;
  };
  qbMouseEventType type;
} qbMouseEvent_, *qbMouseEvent;

typedef struct {
  union {
    qbKeyEvent_ key_event;
    qbMouseEvent_ mouse_event;
  };
  qbInputEventType type;
} qbInputEvent_, *qbInputEvent;

QB_API void qb_send_key_event(qbKeyEvent event);
QB_API void qb_send_mouse_click_event(qbMouseButtonEvent event);
QB_API void qb_send_mouse_move_event(qbMouseMotionEvent event);
QB_API void qb_send_mouse_scroll_event(qbMouseScrollEvent event);

QB_API void qb_handle_input(void(*on_shutdown)(qbVar arg), void(*on_resize)(qbVar arg, uint32_t width, uint32_t height), qbVar shutdown_arg, qbVar resize_arg);

QB_API qbResult qb_on_key_event(qbSystem system);
QB_API qbResult qb_on_mouse_event(qbSystem system);

QB_API qbBool qb_scancode_ispressed(qbScanCode scan_code);
QB_API qbBool qb_key_ispressed(qbKey key);
QB_API qbBool qb_mouse_ispressed(qbButton mouse_button);
QB_API qbBool qb_mouse_waspressed(qbButton mouse_button);

QB_API void qb_mouse_getposition(int* x, int* y);
QB_API void qb_mouse_getrelposition(int* relx, int* rely);
QB_API void qb_mouse_getwheel(int* scroll_x, int* scroll_y);

QB_API int qb_mouse_setrelative(int enabled);
QB_API int qb_mouse_getrelative();

typedef enum qbInputFocus {
  QB_FOCUS_APP,
  QB_FOCUS_GUI,
} qbInputFocus;

QB_API qbInputFocus qb_user_focus();

#endif  // INPUT__H