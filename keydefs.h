//USB HID page 7 report byte-zero bits:
#define MKl_ctrl 1
#define MKl_shft 2
#define MKl_alt 4
#define MKl_win 8
#define MKr_ctrl 0x10
#define MKr_shft 0x20
#define MKr_altgr 0x40
#define MKr_win 0x80

//USB HID page 7 scancodes. Based on http://www.freebsddiary.org/APC/usb_hid_usages.php
#define Ka	0x04
#define Kb	0x05
#define Kc	0x06
#define Kd	0x07
#define Ke	0x08
#define Kf	0x09
#define Kg	0x0A
#define Kh	0x0B
#define Ki	0x0C
#define Kj	0x0D
#define Kk	0x0E
#define Kl	0x0F
#define Km	0x10
#define Kn	0x11
#define Ko	0x12
#define Kp	0x13
#define Kq	0x14
#define Kr	0x15
#define Ks	0x16
#define Kt	0x17
#define Ku	0x18
#define Kv	0x19
#define Kw	0x1A
#define Kx	0x1B
#define Ky	0x1C
#define Kz	0x1D
#define K1	0x1E
#define K2	0x1F
#define K3	0x20
#define K4	0x21
#define K5	0x22
#define K6	0x23
#define K7	0x24
#define K8	0x25
#define K9	0x26
#define K0	0x27
#define Ksmrtntr	0x28
#define Fesc	0x29
#define Kdelback	0x2A
#define Ksmrttab	0x2B
#define Kspace	0x2C
#define Kdash	0x2D
#define Kequal	0x2E
#define Kleftbracket	0x2F
#define Krightbracket	0x30
#define Kbackslash	0x31
#define Ksemicolon	0x33
#define Kapostrophe	0x34
#define Kbacktick	0x35 //Not physically present on dlyz; backtick is the shift of underline.
#define Kcomma	0x36
#define Kperiod	0x37
#define Kslash	0x38
#define Fcapslk	0x39
#define F1	0x3A
#define F2	0x3B
#define F3	0x3C
#define F4	0x3D
#define F5	0x3E
#define F6	0x3F
#define F7	0x40
#define F8	0x41
#define F9	0x42
#define F10	0x43
#define F11	0x44
#define F12	0x45
#define Fprintsc	0x46
#define Fscrlk	0x47
#define Fpause	0x48
#define Finsert	0x49
#define Fhome	0x4A
#define Fpgup	0x4B
#define Fdelete	0x4C
#define Fend	0x4D
#define Fpgdn	0x4E
#define Fright	0x4F
#define Fleft	0x50
#define Fdown	0x51
#define Fup	0x52
#define Kasterisk	0x55
#define Kplus	0x57
#define Fctxmenu	0x65
//See below for F13-F24
#define Fhlpprfx	0x75
#define Fsetmark	0x77
#define Frepedit	0x79
#define Fundo	0x7A
#define Fcut	0x7B
#define Fcopy	0x7C
#define Fpaste	0x7D
#define Fisearch	0x7E
#define Fgmute	0x7F
#define Fgvolup	0x80
#define Fgvoldn	0x81
#define Fsysreq	0x9A
#define Kleft_parenthesis 0xB6
#define Kright_parenthesis 0xB7
#define Kcolon 0xCB
#define Kl_ctrl	0xE0
#define Kl_shft	0xE1
#define Kl_alt	0xE2
#define Kl_win	0xE3
#define Kr_ctrl	0xE4 //Not present on dlyz.
#define Kr_shft	0xE5 //Not present on dlyz.
#define Kr_altgr	0xE6
#define Kr_win	0xE7 //Not present on dlyz.

//Nonstandard dlyz keys
#define Kquestionmark	0x88 //International2
#define Kunderline	0x89 //International3
#define Kleft_doublequote	0x8A //International4
#define Kright_doublequote	0x8B //International5

//Qwerty_fakes surrogates, in order to have enough keycodes for all my characters.
#define Ksurr3	0x92 //Keyboard Lang 3
#define Ksurr4	0x93 //Keyboard Lang 4
#define Ksurr5	0x94 //Keyboard Lang 5

//Nonstandard dlyz keys, using standard function key scancodes
#define F13	0x68 //F13
#define F14	0x69 //F14
#define Frevmark	0x6A //F15
#define Fcclcase	0x6B //F16
#define Fgo	0x6C //F17
#define Fuarg	0x6D //F18
#define Fgomark	0x6E //F19
#define Fexchpm	0x6F //F20
#define Ffindchr	0x70 //F21
#define Ffnenter	0x71 //F22
#define Kindent	0x72 //F23
#define Ffw_word	0x73 //F24

//Nonstandard dlyz keys, using reserved scancodes. fill_qwerty_fake_krb() translates all these to standard codes.
#define Fswchbuf	0xE8
#define Fclsebuf	0xE9
#define Fcmitbuf	0xEA
#define Fwebsrch	0xEB
#define Flnkback	0xEC
#define Flinkfwd	0xED
#define Fbmklist	0xEE
#define Fbw_word	0xEF
#define Fend_wrd	0xF0
#define Fexecmd	0xF1
#define Fselsexp	0xF2
#define Fnxtmtch	0xF3
#define Fnextwin	0xF4
#define Findsexp	0xF5
#define Kl_fn	0xF6
#define Kr_fn	0xF7
#define Fwgprefx	0xF8

//#define Kunsupported	0xA5 //Sent in qwerty-fake mode when a dlyz key which conflicts with qwerty is pressed.

//Nonexistent key and function, to maintain array alignment:
#define Kna	0xA6
#define Fna	0xA7

//12 by 6 matrices, row major. "key" matrix for individual keys, and "fn" matrix for fn-chorded keys.
uint8_t logical_key_matrix[]={
Kna, K9, K8, K7, K6, K5, K4, K3, K2, K1, K0, Kna,
Kleft_doublequote, Kdash, Kp, Ku, Kc, Kb, Kk, Kd, Kl, Ky, Kz, Kright_doublequote,
Kleft_parenthesis, Ka, Kn, Ki, Ks, Kw, Kg, Kt, Kh, Ko, Kr, Kright_parenthesis,
Kleftbracket, Kslash, Kperiod, Kcomma, Kf, Kequal, Kq, Km, Kv, Kapostrophe, Kx, Krightbracket,
Kna, Kplus, Kquestionmark, Ksemicolon, Kasterisk, Kl_ctrl, Kl_win, Kj, Kcolon, Kunderline, Kbackslash, Kna,
Kna, Kl_shft, Ke, Kindent, Ksmrttab, Kr_altgr, Kl_alt, Ksmrtntr, Kdelback, Kspace, Kr_fn, Kna
};

uint8_t logical_fn_matrix[]={
Fna, F9, F8, F7, F6, F5, F4, F3, F2, F1, F10, Fna,
F12, Fcclcase, Fexecmd, Fswchbuf, Fcmitbuf, Fclsebuf, Fpgup, Fbw_word, Fup, Fend_wrd, Fgo, F11,
F14, Fuarg, Fcopy, Fpaste, Fisearch, Fwebsrch, Fselsexp, Fleft, Fdown, Fright, Fesc, F13,
Fprintsc, Fwgprefx, Fundo, Frevmark, Fsetmark, Fgomark, Fexchpm, Fhome, Fpgdn, Fend, Ffindchr, Fpause,
Fna, Fscrlk, Fhlpprfx, Frepedit, Fnxtmtch, Kl_ctrl, Kl_win, Flnkback, Fbmklist, Flinkfwd, Fctxmenu, Fna,
Fna, Kl_shft, Fcut, Findsexp, Fnextwin, Kr_altgr, Kl_alt, Ffnenter, Fdelete, Ffw_word, Kr_fn, Fna
};
