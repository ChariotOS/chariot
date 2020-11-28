#pragma once

// magic file descriptors start with 7F
#define MAGICFD_MASK 0x7F000000

// the executable of this process
#define MAGICFD_EXEC (MAGICFD_MASK + 1)
