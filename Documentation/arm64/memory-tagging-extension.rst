===============================================
Memory Tagging Extension (MTE) in AArch64 Linux
===============================================

Authors: Vincenzo Frascino <vincenzo.frascino@arm.com>
         Catalin Marinas <catalin.marinas@arm.com>

Date: 2020-02-25

This document describes the provision of the Memory Tagging Extension
functionality in AArch64 Linux.

Introduction
============

ARMv8.5 based processors introduce the Memory Tagging Extension (MTE)
feature. MTE is built on top of the ARMv8.0 virtual address tagging TBI
(Top Byte Ignore) feature and allows software to access a 4-bit
allocation tag for each 16-byte granule in the physical address space.
Such memory range must be mapped with the Normal-Tagged memory
attribute. A logical tag is derived from bits 59-56 of the virtual
address used for the memory access. A CPU with MTE enabled will compare
the logical tag against the allocation tag and potentially raise an
exception on mismatch, subject to system registers configuration.

Userspace Support
=================

When ``CONFIG_ARM64_MTE`` is selected and Memory Tagging Extension is
supported by the hardware, the kernel advertises the feature to
userspace via ``HWCAP2_MTE``.

PROT_MTE
--------

To access the allocation tags, a user process must enable the Tagged
memory attribute on an address range using a new ``prot`` flag for
``mmap()`` and ``mprotect()``:

``PROT_MTE`` - Pages allow access to the MTE allocation tags.

The allocation tag is set to 0 when such pages are first mapped in the
user address space and preserved on copy-on-write. ``MAP_SHARED`` is
supported and the allocation tags can be shared between processes.

**Note**: ``PROT_MTE`` is only supported on ``MAP_ANONYMOUS`` and
RAM-based file mappings (``tmpfs``, ``memfd``). Passing it to other
types of mapping will result in ``-EINVAL`` returned by these system
calls.

**Note**: The ``PROT_MTE`` flag (and corresponding memory type) cannot
be cleared by ``mprotect()``.

Tag Check Faults
----------------

When ``PROT_MTE`` is enabled on an address range and a mismatch between
the logical and allocation tags occurs on access, there are three
configurable behaviours:

- *Ignore* - This is the default mode. The CPU (and kernel) ignores the
  tag check fault.

- *Synchronous* - The kernel raises a ``SIGSEGV`` synchronously, with
  ``.si_code = SEGV_MTESERR`` and ``.si_addr = <fault-address>``. The
  memory access is not performed.

- *Asynchronous* - The kernel raises a ``SIGSEGV``, in the current
  thread, asynchronously following one or multiple tag check faults,
  with ``.si_code = SEGV_MTEAERR`` and ``.si_addr = 0``.

**Note**: There are no *match-all* logical tags available for user
applications.

The user can select the above modes, per thread, using the
``prctl(PR_SET_TAGGED_ADDR_CTRL, flags, 0, 0, 0)`` system call where
``flags`` contain one of the following values in the ``PR_MTE_TCF_MASK``
bit-field:

- ``PR_MTE_TCF_NONE``  - *Ignore* tag check faults
- ``PR_MTE_TCF_SYNC``  - *Synchronous* tag check fault mode
- ``PR_MTE_TCF_ASYNC`` - *Asynchronous* tag check fault mode

Tag checking can also be disabled for a user thread by setting the
``PSTATE.TCO`` bit with ``MSR TCO, #1``.

**Note**: Signal handlers are always invoked with ``PSTATE.TCO = 0``,
irrespective of the interrupted context.

**Note**: Kernel accesses to user memory (e.g. ``read()`` system call)
follow the same tag checking mode as set by the current thread.

Excluding Tags in the ``IRG``, ``ADDG`` and ``SUBG`` instructions
-----------------------------------------------------------------

The architecture allows excluding certain tags to be randomly generated
via the ``GCR_EL1.Exclude`` register bit-field. By default, Linux
excludes all tags other than 0. A user thread can enable specific tags
in the randomly generated set using the ``prctl(PR_SET_TAGGED_ADDR_CTRL,
flags, 0, 0, 0)`` system call where ``flags`` contains the tags bitmap
in the ``PR_MTE_TAG_MASK`` bit-field.

**Note**: The hardware uses an exclude mask but the ``prctl()``
interface provides an include mask.

Example of correct usage
========================

*MTE Example code*

.. code-block:: c

    /*
     * To be compiled with -march=armv8.5-a+memtag
     */
    #include <errno.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <unistd.h>
    #include <sys/auxv.h>
    #include <sys/mman.h>
    #include <sys/prctl.h>

    /*
     * From arch/arm64/include/uapi/asm/hwcap.h
     */
    #define HWCAP2_MTE              (1 << 18)

    /*
     * From arch/arm64/include/uapi/asm/mman.h
     */
    #define PROT_MTE                 0x20

    /*
     * From include/uapi/linux/prctl.h
     */
    #define PR_SET_TAGGED_ADDR_CTRL 55
    #define PR_GET_TAGGED_ADDR_CTRL 56
    # define PR_TAGGED_ADDR_ENABLE  (1UL << 0)
    # define PR_MTE_TCF_SHIFT       1
    # define PR_MTE_TCF_NONE        (0UL << PR_MTE_TCF_SHIFT)
    # define PR_MTE_TCF_SYNC        (1UL << PR_MTE_TCF_SHIFT)
    # define PR_MTE_TCF_ASYNC       (2UL << PR_MTE_TCF_SHIFT)
    # define PR_MTE_TCF_MASK        (3UL << PR_MTE_TCF_SHIFT)
    # define PR_MTE_TAG_SHIFT       3
    # define PR_MTE_TAG_MASK        (0xffffUL << PR_MTE_TAG_SHIFT)

    /*
     * Insert a random logical tag into the given pointer.
     */
    #define insert_random_tag(ptr) ({                       \
            __u64 __val;                                    \
            asm("irg %0, %1" : "=r" (__val) : "r" (ptr));   \
            __val;                                          \
    })

    /*
     * Set the allocation tag on the destination address.
     */
    #define set_tag(tagged_addr) do {                                      \
            asm volatile("stg %0, [%0]" : : "r" (tagged_addr) : "memory"); \
    } while (0)

    int main()
    {
            unsigned long *a;
            unsigned long page_sz = getpagesize();
            unsigned long hwcap2 = getauxval(AT_HWCAP2);

            /* check if MTE is present */
            if (!(hwcap2 & HWCAP2_MTE))
                    return -1;

            /*
             * Enable the tagged address ABI, synchronous MTE tag check faults and
             * allow all non-zero tags in the randomly generated set.
             */
            if (prctl(PR_SET_TAGGED_ADDR_CTRL,
                      PR_TAGGED_ADDR_ENABLE | PR_MTE_TCF_SYNC | (0xfffe << PR_MTE_TAG_SHIFT),
                      0, 0, 0)) {
                    perror("prctl() failed");
                    return -1;
            }

            a = mmap(0, page_sz, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (a == MAP_FAILED) {
                    perror("mmap() failed");
                    return -1;
            }

            /*
             * Enable MTE on the above anonymous mmap. The flag could be passed
             * directly to mmap() and skip this step.
             */
            if (mprotect(a, page_sz, PROT_READ | PROT_WRITE | PROT_MTE)) {
                    perror("mprotect() failed");
                    return -1;
            }

            /* access with the default tag (0) */
            a[0] = 1;
            a[1] = 2;

            printf("a[0] = %lu a[1] = %lu\n", a[0], a[1]);

            /* set the logical and allocation tags */
            a = (unsigned long *)insert_random_tag(a);
            set_tag(a);

            printf("%p\n", a);

            /* non-zero tag access */
            a[0] = 3;
            printf("a[0] = %lu a[1] = %lu\n", a[0], a[1]);

            /*
             * If MTE is enabled correctly the next instruction will generate an
             * exception.
             */
            printf("Expecting SIGSEGV...\n");
            a[2] = 0xdead;

            /* this should not be printed in the PR_MTE_TCF_SYNC mode */
            printf("...done\n");

            return 0;
    }
