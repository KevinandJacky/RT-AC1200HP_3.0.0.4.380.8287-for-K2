#ifndef UNLZO_MM_H
#define UNLZO_MM_H

#ifdef STATIC
/* Code active when included from pre-boot environment: */
#define INIT
#else
/* Compile for initramfs/initrd code only */
#define INIT __init
static void(*error)(char *m);
#endif

#endif
