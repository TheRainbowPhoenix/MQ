//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

#include <mq/interfaces/keyboard.h>
#include <stdlib.h>
#include <string.h>

static mqKeyboardKey fxKeys[] = {
    { "F1",    {  20,  30, 55, 24 },  9, 6, MQ_KEY_F1 },
    { "F2",    {  80,  30, 55, 24 },  9, 5, MQ_KEY_F2 },
    { "F3",    { 140,  30, 55, 24 },  9, 4, MQ_KEY_F3 },
    { "F4",    { 200,  30, 55, 24 },  9, 3, MQ_KEY_F4 },
    { "F5",    { 260,  30, 55, 24 },  9, 2, MQ_KEY_F5 },
    { "F6",    { 320,  30, 55, 24 },  9, 1, MQ_KEY_F6 },
    { "SHIFT", {  20,  60, 55, 24 },  8, 6, MQ_KEY_SHIFT },
    { "OTPN",  {  80,  60, 55, 24 },  8, 5, MQ_KEY_OPTN },
    { "VARS",  { 140,  60, 55, 24 },  8, 4, MQ_KEY_VARS },
    { "MENU",  { 200,  60, 55, 24 },  8, 3, MQ_KEY_MENU, },
    { "<",     { 260,  60, 55, 24 },  8, 2, MQ_KEY_LEFT, },
    { "^",     { 320,  60, 55, 24 },  8, 1, MQ_KEY_UP },
    { "ALPHA", {  20,  90, 55, 24 },  7, 6, MQ_KEY_ALPHA },
    { "x2",    {  80,  90, 55, 24 },  7, 5, -1 },
    { "x^y",   { 140,  90, 55, 24 },  7, 4, -1 },
    { "EXIT",  { 200,  90, 55, 24 },  7, 3, MQ_KEY_EXIT },
    { "v",     { 260,  90, 55, 24 },  7, 2, MQ_KEY_DOWN },
    { ">",     { 320,  90, 55, 24 },  7, 1, MQ_KEY_RIGHT },
    { "XOT",   {  20, 120, 55, 24 },  6, 6, -1 },
    { "log",   {  80, 120, 55, 24 },  6, 5, -1 },
    { "ln",    { 140, 120, 55, 24 },  6, 4, -1 },
    { "sin",   { 200, 120, 55, 24 },  6, 3, -1 },
    { "cos",   { 260, 120, 55, 24 },  6, 2, -1 },
    { "tan",   { 320, 120, 55, 24 },  6, 1, -1 },
    { "o/o",   {  20, 150, 55, 24 },  5, 6, -1 },
    { "S<->D", {  80, 150, 55, 24 },  5, 5, -1 },
    { "(",     { 140, 150, 55, 24 },  5, 4, -1 },
    { ")",     { 200, 150, 55, 24 },  5, 3, -1 },
    { ",",     { 260, 150, 55, 24 },  5, 2, -1 },
    { "->",    { 320, 150, 55, 24 },  5, 1, -1 },
    { "7",     {  20, 180, 67, 24 },  4, 6, -1 },
    { "8",     {  92, 180, 67, 24 },  4, 5, -1 },
    { "9",     { 164, 180, 67, 24 },  4, 4, -1 },
    { "DEL",   { 236, 180, 67, 24 },  4, 3, -1 },
    { "AC/ON", { 308, 180, 67, 24 },  0, 0, -1 }, // onKey
    { "4",     {  20, 210, 67, 24 },  3, 6, -1 },
    { "5",     {  92, 210, 67, 24 },  3, 5, -1 },
    { "6",     { 164, 210, 67, 24 },  3, 4, -1 },
    { "×",     { 236, 210, 67, 24 },  3, 3, -1 },
    { "÷",     { 308, 210, 67, 24 },  3, 2, -1 },
    { "1",     {  20, 240, 67, 24 },  2, 6, -1 },
    { "2",     {  92, 240, 67, 24 },  2, 5, -1 },
    { "3",     { 164, 240, 67, 24 },  2, 4, -1 },
    { "+",     { 236, 240, 67, 24 },  2, 3, -1 },
    { "-",     { 308, 240, 67, 24 },  2, 2, -1 },
    { "0",     {  20, 270, 67, 24 },  1, 6, -1 },
    { ".",     {  92, 270, 67, 24 },  1, 5, -1 },
    { "x10^",  { 164, 270, 67, 24 },  1, 4, -1 },
    { "(-)",   { 236, 270, 67, 24 },  1, 3, -1 },
    { "EXE",   { 308, 270, 67, 24 },  1, 2, MQ_KEY_EXE },
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

int mq_keyboard_getPressedKey(mqKeyboard *kbd)
{
    for(uint keyNumber = 0; keyNumber < kbd->keyCount; keyNumber++) {
        if(mq_keyboard_isKeyPressed(kbd, keyNumber))
            return keyNumber;
    }
    return -1;
}

void mq_keyboard_setKeyPressed(mqKeyboard *kbd, uint keyNumber, bool pressed)
{
    if(keyNumber >= kbd->keyCount)
        return;
    kbd->keyStatus[keyNumber] = pressed;
}

void mq_keyboard_setKeycodePressed(
   mqKeyboard *kbd, enum mqKeyboardKeycode keycode, bool pressed)
{
    for(uint i = 0; i < kbd->keyCount; i++) {
        if(kbd->keyInfo[i].keycode == keycode)
            kbd->keyStatus[i] = pressed;
    }
}
