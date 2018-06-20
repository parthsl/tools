/*
  Copyright (c) 2010 Mans Rullgard

  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files (the
  "Software"), to deal in the Software without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to
  the following conditions:

  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>

#define TEMP_SENSOR     0x4a00232c
#define BANDGAP_CLKCTRL 0x4a307888

static const signed char temp_tab[128] = {
    -40.00, -40.00, -40.00, -40.00, -40.00, -40.00, -40.00, -40.00,
    -40.00, -40.00, -40.00, -40.00, -40.00, -39.00, -36.50, -34.50,
    -33.00, -31.00, -29.00, -27.00, -25.00, -23.00, -21.00, -19.25,
    -17.75, -16.00, -14.25, -12.75, -11.00,  -9.00,  -7.25,  -5.75,
     -4.25,  -2.50,  -0.75,   1.00,   2.75,   4.25,   5.75,   7.50,
      9.25,  11.00,  12.75,  14.25,  16.00,  18.00,  20.00,  22.00,
     24.00,  26.00,  27.75,  29.25,  31.00,  32.75,  34.25,  36.00,
     37.75,  39.25,  41.00,  42.75,  44.25,  46.00,  47.75,  49.25,
     51.00,  52.75,  54.25,  56.00,  57.75,  59.25,  61.00,  63.00,
     65.00,  67.00,  69.00,  70.75,  72.50,  74.25,  76.00,  77.75,
     79.25,  81.00,  82.75,  84.25,  86.00,  87.75,  89.25,  91.00,
     92.75,  94.25,  96.00,  97.75,  99.25, 101.00, 102.75, 104.25,
    106.00, 108.00, 110.00, 112.00, 114.00, 116.00, 117.75, 119.25,
    121.00, 122.75, 124.25, 125.00, 125.00, 125.00, 125.00, 125.00,
    125.00, 125.00, 125.00, 125.00, 125.00, 125.00, 125.00, 125.00,
    125.00, 125.00, 125.00, 125.00, 125.00, 125.00, 125.00, 125.00,
};

void *mapreg(int fd, long pagesize, unsigned long reg)
{
    unsigned long pagemask = ~(pagesize - 1);
    unsigned long map_addr = reg & pagemask;
    char *mem;

    mem = mmap(NULL, pagesize, PROT_READ|PROT_WRITE, MAP_SHARED, fd, map_addr);
    if (mem == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    return mem + (reg & ~pagemask);
}

int main(int argc, char **argv)
{
    volatile unsigned *clk, *temp;
    unsigned clkval, tempval;
    long pagesize;
    int fd;

    fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd == -1) {
        perror("/dev/mem");
        return 1;
    }

    pagesize = sysconf(_SC_PAGESIZE);

    clk  = mapreg(fd, pagesize, BANDGAP_CLKCTRL);
    temp = mapreg(fd, pagesize, TEMP_SENSOR);

    clkval = *clk;
    *clk = clkval | 0x100;

    *temp = 0x200;
    while (!(*temp & 0x100)) __asm__ ("" ::: "memory");
    *temp = 0;
    while (*temp & 0x100)    __asm__ ("" ::: "memory");
    tempval = *temp;

    *clk = clkval;

    if (tempval & 0x80)
        printf("unknown value %d\n", tempval & 0xff);
    else
        printf("%d\n", temp_tab[tempval & 0x7f]);

    close(fd);

    return 0;
}
