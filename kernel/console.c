//
// Console input and output, to the uart.
// Reads are line at a time.
// Implements special input characters:
//   newline -- end of line
//   control-h -- backspace
//   control-u -- kill line
//   control-d -- end of file
//   control-p -- print process list
//

#include <stdarg.h>

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"

#define BACKSPACE 0x100

// ctrl-@ is (0x00), ctrl-A is (0x01)
#define C(x)  ((x)-'@')  // Control-x

//
// send one character to the uart.
// called by printf(), and to echo input characters,
// but not from write().
//
void
consputc(int c)
{
    if(c == BACKSPACE){
        // if the user typed backspace, overwrite with a space.
        uartputc_sync('\b'); uartputc_sync(' '); uartputc_sync('\b');
    } else {
        uartputc_sync(c);
    }
}

struct {
    struct spinlock lock;

    // input
#define INPUT_BUF_SIZE 128
    char buf[INPUT_BUF_SIZE];
    uint r;  // Read index
    uint w;  // Write index
    uint e;  // Edit index
} cons;

// Call uartputc for each char.
// May sleep.
// ------------------------------------------
// user write()s to the console go here.
//
int
consolewrite(int user_src, uint64 src, int n)
{
    int i;

    for(i = 0; i < n; i++){
        char c;
        if(either_copyin(&c, user_src, src+i, 1) == -1)
            break;
        uartputc(c);
    }

    return i;
}

// Lock and sleep until r != w.
// copy characters out  of buffer until we encounter \n or EOFchar.
// ------------------------------------------------
// user read()s from the console go here.
// copy (up to) a whole input line to dst.
// user_dist indicates whether dst is a user
// or kernel address.
//
//-------------------------------------------------------------------
// user_dst indicates whether dst is a user or kernel address.
// dst is a address(user or kernel) of a buffer.
// n is the maximum number of bytes to copy.
int
consoleread(int user_dst, uint64 dst, int n)
{
    uint target;
    int c;
    char cbuf;

    target = n;
    acquire(&cons.lock);
    while(n > 0){
        // wait until interrupt handler has put some
        // input into cons.buffer.
        while(cons.r == cons.w){
            if(killed(myproc())){
                release(&cons.lock);
                return -1;
            }
            sleep(&cons.r, &cons.lock);
        }

        c = cons.buf[cons.r++ % INPUT_BUF_SIZE];

        if(c == C('D')){  // end-of-file
            if(n < target){
                // Save ^D for next time, to make sure
                // caller gets a 0-byte result.
                cons.r--;
            }
            break;
        }

        // copy the input byte to the user-space buffer.
        cbuf = c;
        if(either_copyout(user_dst, dst, &cbuf, 1) == -1)
            break;

        dst++;
        --n;

        if(c == '\n'){
            // a whole line has arrived, return to
            // the user-level read().
            break;
        }
    }
    release(&cons.lock);

    return target - n;
}

// The job of consoleintr() is to accumulate input characters
// in cons.buf until a whole line arrives.
// Called to add one char to cons buffer.
// Echo the char to output.
// Add to buffer; e ++
// If \n or EOF... w = e and wakeup sleeping consoleread()(wake up cons.r).
// (consoleread() will sleep( sleep(&cons.r, &cons.lock); ), so need some way to wake it up.)
// ---------------------------------------
// the console input interrupt handler.
// uartintr() calls this for input character.
// do erase/kill processing, append to cons.buf,
// wake up consoleread() if a whole line has arrived.
//
void
consoleintr(int c)
{
    acquire(&cons.lock);

    switch(c){
        case C('P'):  // Print process list.
            procdump();
            break;
        case C('U'):  // Kill line.
            while(cons.e != cons.w &&
                cons.buf[(cons.e-1) % INPUT_BUF_SIZE] != '\n'){
                    cons.e--;
                    consputc(BACKSPACE);
                }
            break;
        case C('H'): // Backspace
        case '\x7f': // Delete key
            if(cons.e != cons.w){
                cons.e--;
                consputc(BACKSPACE);
            }
            break;
        default:
            if(c != 0 && cons.e-cons.r < INPUT_BUF_SIZE){
                c = (c == '\r') ? '\n' : c;

                // echo back to the user.
                consputc(c);

                // store for consumption by consoleread().
                cons.buf[cons.e++ % INPUT_BUF_SIZE] = c;

                if(c == '\n' || c == C('D') || cons.e-cons.r == INPUT_BUF_SIZE){
                    // wake up consoleread() if a whole line (or end-of-file)
                    // has arrived.
                    cons.w = cons.e;
                    wakeup(&cons.r);
                }
            }
            break;
    }

    release(&cons.lock);
}

void
consoleinit(void)
{
    initlock(&cons.lock, "cons");

    // config UART integrated circuit that could be used.
    uartinit();

    // connect read and write system calls
    // to consoleread and consolewrite.
    devsw[CONSOLE].read = consoleread;
    devsw[CONSOLE].write = consolewrite;
}
