//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

#include <mq/interfaces/keyboard.h>
#include <stdlib.h>
#include <string.h>

static mqKeyboardKey fxKeys[] = {
    { "F1",    {  20,  30, 55, 24 },  0, 0 },
    { "F2",    {  80,  30, 55, 24 },  0, 1 },
    { "F3",    { 140,  30, 55, 24 },  0, 2 },
    { "F4",    { 200,  30, 55, 24 },  0, 3 },
    { "F5",    { 260,  30, 55, 24 },  0, 4 },
    { "F6",    { 320,  30, 55, 24 },  0, 5 },
    { "SHIFT", {  20,  60, 55, 24 },  1, 0 },
    { "OTPN",  {  80,  60, 55, 24 },  1, 1 },
    { "VARS",  { 140,  60, 55, 24 },  1, 2 },
    { "MENU",  { 200,  60, 55, 24 },  1, 3 },
    { "◀",     { 260,  60, 55, 24 },  1, 4 },
    { "▲",     { 320,  60, 55, 24 },  1, 5 },
    { "ALPHA", {  20,  90, 55, 24 },  2, 0 },
    { "x2",    {  80,  90, 55, 24 },  2, 1 },
    { "^",     { 140,  90, 55, 24 },  2, 2 },
    { "EXIT",  { 200,  90, 55, 24 },  2, 3 },
    { "▼",     { 260,  90, 55, 24 },  2, 4 },
    { "▶",     { 320,  90, 55, 24 },  2, 5 },
    { "XOT",   {  20, 120, 55, 24 },  3, 0 },
    { "log",   {  80, 120, 55, 24 },  3, 1 },
    { "ln",    { 140, 120, 55, 24 },  3, 2 },
    { "sin",   { 200, 120, 55, 24 },  3, 3 },
    { "cos",   { 260, 120, 55, 24 },  3, 4 },
    { "tan",   { 320, 120, 55, 24 },  3, 5 },
    { "o/o",   {  20, 150, 55, 24 },  4, 0 },
    { "S<->D", {  80, 150, 55, 24 },  4, 1 },
    { "(",     { 140, 150, 55, 24 },  4, 2 },
    { ")",     { 200, 150, 55, 24 },  4, 3 },
    { ",",     { 260, 150, 55, 24 },  4, 4 },
    { "→",     { 320, 150, 55, 24 },  4, 5 },
    { "7",     {  20, 180, 67, 24 },  5, 0 },
    { "8",     {  92, 180, 67, 24 },  5, 1 },
    { "9",     { 164, 180, 67, 24 },  5, 2 },
    { "DEL",   { 236, 180, 67, 24 },  5, 3 },
    { "AC/ON", { 308, 180, 67, 24 },  5, 4 }, // onKey
    { "4",     {  20, 210, 67, 24 },  6, 0 },
    { "5",     {  92, 210, 67, 24 },  6, 1 },
    { "6",     { 164, 210, 67, 24 },  6, 2 },
    { "×",     { 236, 210, 67, 24 },  6, 3 },
    { "÷",     { 308, 210, 67, 24 },  6, 4 },
    { "1",     {  20, 240, 67, 24 },  7, 0 },
    { "2",     {  92, 240, 67, 24 },  7, 1 },
    { "3",     { 164, 240, 67, 24 },  7, 2 },
    { "+",     { 236, 240, 67, 24 },  7, 3 },
    { "-",     { 308, 240, 67, 24 },  7, 4 },
    { "0",     {  20, 270, 67, 24 },  8, 0 },
    { ".",     {  92, 270, 67, 24 },  8, 1 },
    { "x10^",  { 164, 270, 67, 24 },  8, 2 },
    { "(-)",   { 236, 270, 67, 24 },  8, 3 },
    { "EXE",   { 308, 270, 67, 24 },  8, 4 },
};

//============================================================================//

mqKeyboard *mq_keyboard_create(void)
{
    mqKeyboard *kbd = calloc(1, sizeof *kbd);
    /* All fields have default value 0 */
    return kbd;
}

void mq_keyboard_reset(mqKeyboard *kbd)
{
    free(kbd->keyStatus);
    memset(kbd, 0x00, sizeof *kbd);
    kbd->dirty = true;
}

void mq_keyboard_destroy(mqKeyboard *kbd)
{
    mq_keyboard_reset(kbd);
    free(kbd);
}

void mq_keyboard_initialize(mqKeyboard *kbd, enum mqKeyboardStandardLayout l)
{
    mq_keyboard_reset(kbd);

    if(l == MQ_KEYBOARD_STANDARD_LAYOUT_FX) {
        kbd->keyCount = sizeof fxKeys / sizeof *fxKeys;
        kbd->keyInfo = fxKeys;
        kbd->onKey = 34;
    }
    else
        return;

    kbd->keyStatus = malloc(kbd->keyCount);
    kbd->dirty = true;

    if(!kbd->keyStatus)
        mq_keyboard_reset(kbd);
}
