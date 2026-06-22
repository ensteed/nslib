#pragma once

#include "basic_types.h"

namespace nslib
{
enum input_kmcode : u16
{
    KMCODE_UNKNOWN, // 0
    KMCODE_MBUTTON_LEFT,
    KMCODE_MBUTTON_MIDDLE,
    KMCODE_MBUTTON_RIGHT,
    KMCODE_MBUTTON_X1,
    KMCODE_MBUTTON_X2,
    KMCODE_MWHEEL,
    KMCODE_MMOTION,
    KMCODE_KEY_RETURN = '\r',       // '\r'
    KMCODE_KEY_ESCAPE = '\x1B',     // '\x1B'
    KMCODE_KEY_BACKSPACE = '\b',    // '\b'
    KMCODE_KEY_TAB = '\t',          // '\t'
    KMCODE_KEY_SPACE = ' ',         // ' '
    KMCODE_KEY_EXCLAIM,             // '!'
    KMCODE_KEY_DBLAPOSTROPHE,       // '"'
    KMCODE_KEY_HASH,                // '#'
    KMCODE_KEY_DOLLAR,              // '$'
    KMCODE_KEY_PERCENT,             // '%'
    KMCODE_KEY_AMPERSAND,           // '&'
    KMCODE_KEY_APOSTROPHE,          // '\''
    KMCODE_KEY_LEFTPAREN,           // '('
    KMCODE_KEY_RIGHTPAREN,          // ')'
    KMCODE_KEY_ASTERISK,            // '*'
    KMCODE_KEY_PLUS,                // '+'
    KMCODE_KEY_COMMA,               // ','
    KMCODE_KEY_MINUS,               // '-'
    KMCODE_KEY_PERIOD,              // '.'
    KMCODE_KEY_SLASH,               // '/'
    KMCODE_KEY_0,                   // '0'
    KMCODE_KEY_1,                   // '1'
    KMCODE_KEY_2,                   // '2'
    KMCODE_KEY_3,                   // '3'
    KMCODE_KEY_4,                   // '4'
    KMCODE_KEY_5,                   // '5'
    KMCODE_KEY_6,                   // '6'
    KMCODE_KEY_7,                   // '7'
    KMCODE_KEY_8,                   // '8'
    KMCODE_KEY_9,                   // '9'
    KMCODE_KEY_COLON,               // ':'
    KMCODE_KEY_SEMICOLON,           // ';'
    KMCODE_KEY_LESS,                // '<'
    KMCODE_KEY_EQUALS,              // '='
    KMCODE_KEY_GREATER,             // '>'
    KMCODE_KEY_QUESTION,            // '?'
    KMCODE_KEY_AT,                  // '@'
    KMCODE_KEY_LEFTBRACKET = '[',   // '['
    KMCODE_KEY_BACKSLASH,           // '\\'
    KMCODE_KEY_RIGHTBRACKET,        // ']'
    KMCODE_KEY_CARET,               // '^'
    KMCODE_KEY_UNDERSCORE,          // '_'
    KMCODE_KEY_GRAVE,               // '`'
    KMCODE_KEY_A,                   // 'a'
    KMCODE_KEY_B,                   // 'b'
    KMCODE_KEY_C,                   // 'c'
    KMCODE_KEY_D,                   // 'd'
    KMCODE_KEY_E,                   // 'e'
    KMCODE_KEY_F,                   // 'f'
    KMCODE_KEY_G,                   // 'g'
    KMCODE_KEY_H,                   // 'h'
    KMCODE_KEY_I,                   // 'i'
    KMCODE_KEY_J,                   // 'j'
    KMCODE_KEY_K,                   // 'k'
    KMCODE_KEY_L,                   // 'l'
    KMCODE_KEY_M,                   // 'm'
    KMCODE_KEY_N,                   // 'n'
    KMCODE_KEY_O,                   // 'o'
    KMCODE_KEY_P,                   // 'p'
    KMCODE_KEY_Q,                   // 'q'
    KMCODE_KEY_R,                   // 'r'
    KMCODE_KEY_S,                   // 's'
    KMCODE_KEY_T,                   // 't'
    KMCODE_KEY_U,                   // 'u'
    KMCODE_KEY_V,                   // 'v'
    KMCODE_KEY_W,                   // 'w'
    KMCODE_KEY_X,                   // 'x'
    KMCODE_KEY_Y,                   // 'y'
    KMCODE_KEY_Z,                   // 'z'
    KMCODE_KEY_LEFTBRACE,           // '{'
    KMCODE_KEY_PIPE,                // '|'
    KMCODE_KEY_RIGHTBRACE,          // '}'
    KMCODE_KEY_TILDE,               // '~'7
    KMCODE_KEY_DELETE,              // '\x7F'
    KMCODE_KEY_PLUSMINUS = 0x00b1u, // '\xB1'
    KMCODE_KEY_CAPSLOCK,            // Now starting with non printable chars
    KMCODE_KEY_F1,
    KMCODE_KEY_F2,
    KMCODE_KEY_F3,
    KMCODE_KEY_F4,
    KMCODE_KEY_F5,
    KMCODE_KEY_F6,
    KMCODE_KEY_F7,
    KMCODE_KEY_F8,
    KMCODE_KEY_F9,
    KMCODE_KEY_F10,
    KMCODE_KEY_F11,
    KMCODE_KEY_F12,
    KMCODE_KEY_PRINTSCREEN,
    KMCODE_KEY_SCROLLLOCK,
    KMCODE_KEY_PAUSE,
    KMCODE_KEY_INSERT,
    KMCODE_KEY_HOME,
    KMCODE_KEY_PAGEUP,
    KMCODE_KEY_END,
    KMCODE_KEY_PAGEDOWN,
    KMCODE_KEY_RIGHT,
    KMCODE_KEY_LEFT,
    KMCODE_KEY_DOWN,
    KMCODE_KEY_UP,
    KMCODE_KEY_NUMLOCKCLEAR,
    KMCODE_KEY_KP_DIVIDE,
    KMCODE_KEY_KP_MULTIPLY,
    KMCODE_KEY_KP_MINUS,
    KMCODE_KEY_KP_PLUS,
    KMCODE_KEY_KP_ENTER,
    KMCODE_KEY_KP_1,
    KMCODE_KEY_KP_2,
    KMCODE_KEY_KP_3,
    KMCODE_KEY_KP_4,
    KMCODE_KEY_KP_5,
    KMCODE_KEY_KP_6,
    KMCODE_KEY_KP_7,
    KMCODE_KEY_KP_8,
    KMCODE_KEY_KP_9,
    KMCODE_KEY_KP_0,
    KMCODE_KEY_KP_PERIOD,
    KMCODE_KEY_APPLICATION,
    KMCODE_KEY_POWER,
    KMCODE_KEY_KP_EQUALS,
    KMCODE_KEY_F13,
    KMCODE_KEY_F14,
    KMCODE_KEY_F15,
    KMCODE_KEY_F16,
    KMCODE_KEY_F17,
    KMCODE_KEY_F18,
    KMCODE_KEY_F19,
    KMCODE_KEY_F20,
    KMCODE_KEY_F21,
    KMCODE_KEY_F22,
    KMCODE_KEY_F23,
    KMCODE_KEY_F24,
    KMCODE_KEY_EXECUTE,
    KMCODE_KEY_HELP,
    KMCODE_KEY_MENU,
    KMCODE_KEY_SELECT,
    KMCODE_KEY_STOP,
    KMCODE_KEY_AGAIN,
    KMCODE_KEY_UNDO,
    KMCODE_KEY_CUT,
    KMCODE_KEY_COPY,
    KMCODE_KEY_PASTE,
    KMCODE_KEY_FIND,
    KMCODE_KEY_MUTE,
    KMCODE_KEY_VOLUMEUP,
    KMCODE_KEY_VOLUMEDOWN,
    KMCODE_KEY_KP_COMMA,
    KMCODE_KEY_KP_EQUALSAS400,
    KMCODE_KEY_ALTERASE,
    KMCODE_KEY_SYSREQ,
    KMCODE_KEY_CANCEL,
    KMCODE_KEY_CLEAR,
    KMCODE_KEY_PRIOR,
    KMCODE_KEY_RETURN2,
    KMCODE_KEY_SEPARATOR,
    KMCODE_KEY_OUT,
    KMCODE_KEY_OPER,
    KMCODE_KEY_CLEARAGAIN,
    KMCODE_KEY_CRSEL,
    KMCODE_KEY_EXSEL,
    KMCODE_KEY_KP_00,
    KMCODE_KEY_KP_000,
    KMCODE_KEY_THOUSANDSSEPARATOR,
    KMCODE_KEY_DECIMALSEPARATOR,
    KMCODE_KEY_CURRENCYUNIT,
    KMCODE_KEY_CURRENCYSUBUNIT,
    KMCODE_KEY_KP_LEFTPAREN,
    KMCODE_KEY_KP_RIGHTPAREN,
    KMCODE_KEY_KP_LEFTBRACE,
    KMCODE_KEY_KP_RIGHTBRACE,
    KMCODE_KEY_KP_TAB,
    KMCODE_KEY_KP_BACKSPACE,
    KMCODE_KEY_KP_A,
    KMCODE_KEY_KP_B,
    KMCODE_KEY_KP_C,
    KMCODE_KEY_KP_D,
    KMCODE_KEY_KP_E,
    KMCODE_KEY_KP_F,
    KMCODE_KEY_KP_XOR,
    KMCODE_KEY_KP_POWER,
    KMCODE_KEY_KP_PERCENT,
    KMCODE_KEY_KP_LESS,
    KMCODE_KEY_KP_GREATER,
    KMCODE_KEY_KP_AMPERSAND,
    KMCODE_KEY_KP_DBLAMPERSAND,
    KMCODE_KEY_KP_VERTICALBAR,
    KMCODE_KEY_KP_DBLVERTICALBAR,
    KMCODE_KEY_KP_COLON,
    KMCODE_KEY_KP_HASH,
    KMCODE_KEY_KP_SPACE,
    KMCODE_KEY_KP_AT,
    KMCODE_KEY_KP_EXCLAM,
    KMCODE_KEY_KP_MEMSTORE,
    KMCODE_KEY_KP_MEMRECALL,
    KMCODE_KEY_KP_MEMCLEAR,
    KMCODE_KEY_KP_MEMADD,
    KMCODE_KEY_KP_MEMSUBTRACT,
    KMCODE_KEY_KP_MEMMULTIPLY,
    KMCODE_KEY_KP_MEMDIVIDE,
    KMCODE_KEY_KP_PLUSMINUS,
    KMCODE_KEY_KP_CLEAR,
    KMCODE_KEY_KP_CLEARENTRY,
    KMCODE_KEY_KP_BINARY,
    KMCODE_KEY_KP_OCTAL,
    KMCODE_KEY_KP_DECIMAL,
    KMCODE_KEY_KP_HEXADECIMAL,
    KMCODE_KEY_LCTRL,
    KMCODE_KEY_LSHIFT,
    KMCODE_KEY_LALT,
    KMCODE_KEY_LGUI,
    KMCODE_KEY_RCTRL,
    KMCODE_KEY_RSHIFT,
    KMCODE_KEY_RALT,
    KMCODE_KEY_RGUI,
    KMCODE_KEY_MODE,
    KMCODE_KEY_SLEEP,
    KMCODE_KEY_WAKE,
    KMCODE_KEY_CHANNEL_INCREMENT,
    KMCODE_KEY_CHANNEL_DECREMENT,
    KMCODE_KEY_MEDIA_PLAY,
    KMCODE_KEY_MEDIA_PAUSE,
    KMCODE_KEY_MEDIA_RECORD,
    KMCODE_KEY_MEDIA_FAST_FORWARD,
    KMCODE_KEY_MEDIA_REWIND,
    KMCODE_KEY_MEDIA_NEXT_TRACK,
    KMCODE_KEY_MEDIA_PREVIOUS_TRACK,
    KMCODE_KEY_MEDIA_STOP,
    KMCODE_KEY_MEDIA_EJECT,
    KMCODE_KEY_MEDIA_PLAY_PAUSE,
    KMCODE_KEY_MEDIA_SELECT,
    KMCODE_KEY_AC_NEW,
    KMCODE_KEY_AC_OPEN,
    KMCODE_KEY_AC_CLOSE,
    KMCODE_KEY_AC_EXIT,
    KMCODE_KEY_AC_SAVE,
    KMCODE_KEY_AC_PRINT,
    KMCODE_KEY_AC_PROPERTIES,
    KMCODE_KEY_AC_SEARCH,
    KMCODE_KEY_AC_HOME,
    KMCODE_KEY_AC_BACK,
    KMCODE_KEY_AC_FORWARD,
    KMCODE_KEY_AC_STOP,
    KMCODE_KEY_AC_REFRESH,
    KMCODE_KEY_AC_BOOKMARKS,
    KMCODE_KEY_SOFTLEFT,
    KMCODE_KEY_SOFTRIGHT,
    KMCODE_KEY_CALL,
    KMCODE_KEY_ENDCALL,
    KMCODE_KEY_LEFT_TAB,          // Extended key Left Tab
    KMCODE_KEY_LEVEL5_SHIFT,      // Extended key Level 5 Shift
    KMCODE_KEY_MULTI_KEY_COMPOSE, // Extended key Multi-key Compose
    KMCODE_KEY_LMETA,             // Extended key Left Meta
    KMCODE_KEY_RMETA,             // Extended key Right Meta
    KMCODE_KEY_LHYPER,            // Extended key Left Hyper
    KMCODE_KEY_RHYPER             // Extended key Right Hyper
};

#define SDL_KMOD_NONE   0x0000u /**< no modifier is applicable. */
#define SDL_KMOD_LSHIFT 0x0001u /**< the left Shift key is down. */
#define SDL_KMOD_RSHIFT 0x0002u /**< the right Shift key is down. */
#define SDL_KMOD_LEVEL5 0x0004u /**< the Level 5 Shift key is down. */
#define SDL_KMOD_LCTRL  0x0040u /**< the left Ctrl (Control) key is down. */
#define SDL_KMOD_RCTRL  0x0080u /**< the right Ctrl (Control) key is down. */
#define SDL_KMOD_LALT   0x0100u /**< the left Alt key is down. */
#define SDL_KMOD_RALT   0x0200u /**< the right Alt key is down. */
#define SDL_KMOD_LGUI   0x0400u /**< the left GUI key (often the Windows key) is down. */
#define SDL_KMOD_RGUI   0x0800u /**< the right GUI key (often the Windows key) is down. */
#define SDL_KMOD_NUM    0x1000u /**< the Num Lock key (may be located on an extended keypad) is down. */
#define SDL_KMOD_CAPS   0x2000u /**< the Caps Lock key is down. */
#define SDL_KMOD_MODE   0x4000u /**< the !AltGr key is down. */
#define SDL_KMOD_SCROLL 0x8000u /**< the Scroll Lock key is down. */
#define SDL_KMOD_CTRL   (SDL_KMOD_LCTRL | SDL_KMOD_RCTRL)   /**< Any Ctrl key is down. */
#define SDL_KMOD_SHIFT  (SDL_KMOD_LSHIFT | SDL_KMOD_RSHIFT) /**< Any Shift key is down. */
#define SDL_KMOD_ALT    (SDL_KMOD_LALT | SDL_KMOD_RALT)     /**< Any Alt key is down. */
#define SDL_KMOD_GUI    (SDL_KMOD_LGUI | SDL_KMOD_RGUI)     /**< Any GUI key is down. */

// Modifier Masks
enum keymod : u16
{
    KEYMOD_NONE,
    KEYMOD_LSHIFT = 0x0001u,
    KEYMOD_RSHIFT = 0x0002u,
    KEYMOD_LEVEL5 = 0x0004u,
    KEYMOD_LCTRL = 0x0040u,
    KEYMOD_RCTRL = 0x0080u,
    KEYMOD_LALT = 0x0100u,
    KEYMOD_RALT = 0x0200u,
    KEYMOD_LGUI = 0x0400u,
    KEYMOD_RGUI = 0x0800u,
    KEYMOD_NUM = 0x1000u,
    KEYMOD_CAPS = 0x2000u,
    KEYMOD_MODE = 0x4000u,
    KEYMOD_CTRL = KEYMOD_LCTRL | KEYMOD_RCTRL,
    KEYMOD_SHIFT = KEYMOD_LSHIFT | KEYMOD_RSHIFT,
    KEYMOD_ALT = KEYMOD_LALT | KEYMOD_RALT,
    KEYMOD_GUI = KEYMOD_LGUI | KEYMOD_RGUI
};

#define MBUTTON_MASK(X) (1u << ((X) - 1))
enum mbutton_mask : u8
{
    MBUTTON_MASK_NONE,
    MBUTTON_MASK_LEFT = MBUTTON_MASK(KMCODE_MBUTTON_LEFT),
    MBUTTON_MASK_MIDDLE = MBUTTON_MASK(KMCODE_MBUTTON_MIDDLE),
    MBUTTON_MASK_RIGHT = MBUTTON_MASK(KMCODE_MBUTTON_RIGHT),
    MBUTTON_MASK_X1 = MBUTTON_MASK(KMCODE_MBUTTON_X1),
    MBUTTON_MASK_X2 = MBUTTON_MASK(KMCODE_MBUTTON_X2),
    MBUTTON_DOUBLE_CLICK = MBUTTON_MASK(KMCODE_MBUTTON_X2 + 1)
};

enum input_action : u8
{
    INPUT_ACTION_PRESS = 1u << 0,
    INPUT_ACTION_RELEASE = 1u << 1,
    INPUT_ACTION_REPEAT = 1u << 2
};

} // namespace nslib
