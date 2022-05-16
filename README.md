# Copyright Micu Florian-Luis 331CA 2022 - Assignment 3

# Organization

This program mimics the functionality of an executable loader for Windows 
(Portable Executable) and Linux (Executable and Linkable Format). The part 
that had to be implemented was the signal interception, specifically the 
SEGMENTATION FAULT signal, and the mapping of memory to read from a file.

This assignment is useful to better understand how a calloc/malloc/realloc
call actually works and how the operating system manages the virtual and
physical memory.

# Implementation

## Function so_init_loader
For this function, I assigned a custom handler that would simply respond to
a SEGMENTATION FAULT SIGNAL. For Linux, I used the "sigaction" structure,
where I set the bits of the mask to zero except for the SIGSEGV bits. Also,
SA_SIGINFO was used to accept the handler passed to the sa_sigaction. For
Windows, I used "AddVectoredExceptionHandler".

## Custom Handler
First I check if the signal received is a SIGSEGV for safety, if the signal
differs, I let the old handler act on the signal. Then, I iterate through all
of the segments available and I check if the faulty memory location is between
the current segment's virtual address and its memory length. Once the condition
is satisfied, I compute the page index by subtracting from the faulty memory 
the virtual address of the segment and then I divide the result by a page size.
To continue, I check if the current page in the segment has been already 
allocated, if this check is true, I let the old handler act on the signal, 
otherwise I get the page address and I use mmap on Linux to allocate memory and
read directly from the file (I set a global file descriptor for this to work).
The permissions are set directly from the executable and the MAP_FIXED flag has
been used so that the memory allocated is forced to choose the base address. 
The MAP_PRIVATE flag has been used since it is faster than its counterpart and
this program does not require multi-threading capabilities. The offset has been
computed as being the segment's offset plus the current page. I use the "data"
void pointer to store an array of pages that have been hit so far by the 
function, thus I set the according index in the array to true. At last, I check
if the file size is smaller than the memory size since it could result in 
writing in a zero location. I also check if the upper bound of the page falls
outside of the file size, since if this condition is true it means that 
anything above the file size must be set to zero so that the ".bss" section 
works correctly. If no segment is found for the memory fault, I let the old
handler process the signal.

### Note
1. Although, it is much faster to read a file with mmap and there is a 
counterpart for this behavior in Windows, I choose to allocate memory without a 
handler and then I copy the content of the file in the page. I did this because 
I has some problems with CreateFileMapping + MapViewOfFile. In addition, I had 
to set the permissions manually as those have been set in the executable by
xor-ing multiple permissions which is not valid in Windows.
2. To check if the array of flags has been initialized for the page hits (void 
*data), I set in the so_execute the data field to NULL for safety.
3. In Windows, special returns were needed to continue the execution of a 
program after the handler finished or to let the old handler process the 
signal.

# Bibliography
I have used various lab exercises from the SO team to finish this homework as 
well as some Linux and Windows manuals:
https://ocw.cs.pub.ro/courses/so/laboratoare/laborator-06
https://ocw.cs.pub.ro/courses/so/laboratoare/laborator-04
https://man7.org/linux/man-pages/man2/mmap.2.html
