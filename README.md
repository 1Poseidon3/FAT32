# FAT32
Source code for the emulation of a FAT32 file system written in C

Disclaimer: I don't claim to be good at C. Please don't make fun of me.

This program hooks into a valid disk file (I used a .img for testing but it can probably work with something like an .iso), reads all the files in it, and extracts them to the folder the program is run from.

It takes one command like parameter and that is the name and extension of the file you are trying to view.

The commands are: 
- Dir - Displays all of the files in the root directory (no directory traversing).
- Extract <File name & extension> - Extracts the specified file to the folder the program was run from.
- Quit - Exits the program cleanly.

There are likely some bugs. This was a university project. I decided to upload this because it took a lot of brainpower and I want it to exist on the internet so I don't forget about it. Maybe someone else can benefit from reading my ramblings in the comments if they have a hard time understanding something about writing your own code for a FAT32 file system as this is about as basic (and well documented) as it gets.
