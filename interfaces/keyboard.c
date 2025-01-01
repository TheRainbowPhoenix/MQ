//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

#include <mq/interfaces/keyboard.h>
#include <stdlib.h>
#include <string.h>

static mqKeyboardKey fxKeys[] = {
    { "F1",    {  20,  30, 55, 24 },  9, 6 },
    { "F2",    {  80,  30, 55, 24 },  9, 5 },
    { "F3",    { 140,  30, 55, 24 },  9, 4 },
    { "F4",    { 200,  30, 55, 24 },  9, 3 },
    { "F5",    { 260,  30, 55, 24 },  9, 2 },
    { "F6",    { 320,  30, 55, 24 },  9, 1 },
    { "SHIFT", {  20,  60, 55, 24 },  8, 6 },
    { "OTPN",  {  80,  60, 55, 24 },  8, 5 },
    { "VARS",  { 140,  60, 55, 24 },  8, 4 },
    { "MENU",  { 200,  60, 55, 24 },  8, 3 },
    { "◀",     { 260,  60, 55, 24 },  8, 2 },
    { "▲",     { 320,  60, 55, 24 },  8, 1 },
    { "ALPHA", {  20,  90, 55, 24 },  7, 6 },
    { "x2",    {  80,  90, 55, 24 },  7, 5 },
    { "^",     { 140,  90, 55, 24 },  7, 4 },
    { "EXIT",  { 200,  90, 55, 24 },  7, 3 },
    { "▼",     { 260,  90, 55, 24 },  7, 2 },
    { "▶",     { 320,  90, 55, 24 },  7, 1 },
    { "XOT",   {  20, 120, 55, 24 },  6, 6 },
    { "log",   {  80, 120, 55, 24 },  6, 5 },
    { "ln",    { 140, 120, 55, 24 },  6, 4 },
    { "sin",   { 200, 120, 55, 24 },  6, 3 },
    { "cos",   { 260, 120, 55, 24 },  6, 2 },
    { "tan",   { 320, 120, 55, 24 },  6, 1 },
    { "o/o",   {  20, 150, 55, 24 },  5, 6 },
    { "S<->D", {  80, 150, 55, 24 },  5, 5 },
    { "(",     { 140, 150, 55, 24 },  5, 4 },
    { ")",     { 200, 150, 55, 24 },  5, 3 },
    { ",",     { 260, 150, 55, 24 },  5, 2 },
    { "→",     { 320, 150, 55, 24 },  5, 1 },
    { "7",     {  20, 180, 67, 24 },  4, 6 },
    { "8",     {  92, 180, 67, 24 },  4, 5 },
    { "9",     { 164, 180, 67, 24 },  4, 4 },
    { "DEL",   { 236, 180, 67, 24 },  4, 3 },
    { "AC/ON", { 308, 180, 67, 24 },  0, 0 }, // onKey
    { "4",     {  20, 210, 67, 24 },  3, 6 },
    { "5",     {  92, 210, 67, 24 },  3, 5 },
    { "6",     { 164, 210, 67, 24 },  3, 4 },
    { "×",     { 236, 210, 67, 24 },  3, 3 },
    { "÷",     { 308, 210, 67, 24 },  3, 2 },
    { "1",     {  20, 240, 67, 24 },  2, 6 },
    { "2",     {  92, 240, 67, 24 },  2, 5 },
    { "3",     { 164, 240, 67, 24 },  2, 4 },
    { "+",     { 236, 240, 67, 24 },  2, 3 },
    { "-",     { 308, 240, 67, 24 },  2, 2 },
    { "0",     {  20, 270, 67, 24 },  1, 6 },
    { ".",     {  92, 270, 67, 24 },  1, 5 },
    { "x10^",  { 164, 270, 67, 24 },  1, 4 },
    { "(-)",   { 236, 270, 67, 24 },  1, 3 },
    { "EXE",   { 308, 270, 67, 24 },  1, 2 },
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

    kbd->keyStatus = calloc(1, kbd->keyCount);
    kbd->dirty = true;

    if(!kbd->keyStatus)
        mq_keyboard_reset(kbd);
}

bool mq_keyboard_isKeyPressed(mqKeyboard *kbd, uint keyNumber)
{
    return keyNumber < kbd->keyCount && (kbd->keyStatus[keyNumber] != 0);
}

void mq_keyboard_setKeyPressed(mqKeyboard *kbd, uint keyNumber, bool pressed)
{
    if(keyNumber >= kbd->keyCount)
        return;
    kbd->keyStatus[keyNumber] = pressed;
}
