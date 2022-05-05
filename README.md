# FAT32
Emulation of a FAT32 file system written in C

Disclaimer: I hate C. I don't know what I'm doing in C. I don't know good programming practices in C. Please don't make fun of me.

All this stupid program can do is hook into a valid disk file (I used a .img for testing but it can probably work with something like an .iso), read all the files in it, and extract them to the folder the program is run from.

It takes one command like parameter and that is the name and extension of the file you are trying to view.

The commands are:
Dir - Displays all of the files in the root directory (no directory traversing).
Extract <File name & extension> - Extracts the specified file to the folder the program was run from.
Quit - Exits the program cleanly

I don't know of any bugs and I don't care. I just uploaded this because it took a lot of brainpower from my feeble mind and I want it to exist on the internet so I don't forget about it. Maybe someone else can benefit from reading my ramblings in the comments if they have a hard time understanding something about writing your own code for a FAT32 file system as this is about as basic (and well documented) as it gets.
