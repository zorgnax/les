les
===

This is a pager program similar to less and more, with several
improved features.

Screenshot
==========

![Screenshot](https://github.com/zorgnax/les/wiki/screenshot.png)

Options
=======

    -e=encoding   input file encoding
    -f            load forever
    -h            help
    -m            input is a man page
    -p=script     lespipe script
    -t=width      tab width (default 4)
    -w            disable line wrap

Tab Bar
=======

If you open multiple files, the top line will show them all as a
list of tabs. Press t and T to cycle through them, q to close one,
and Q to close all of them.

Status Bar
==========

At the bottom of the screen is a status bar that shows information
like the filename, line numbers, and the file size. The color in the
background shows how far through the file you are.

Unicode
=======

This program handles Unicode in order to get the right number of
characters on a line. This requires the program to know the number
of bytes in each character, and the screen width of the character
(characters in Unicode can take 0, 1, or 2 widths). the input file
is assumed to be UTF-8. If it isn't, then you can give the correct
encoding with the -e (encoding) option.

Searching
=========

When you search, all matches will be highlighted in blue, the one
you are up to in green. If the match is on the page you are currently
viewing the page will not move. If it is less than a page away,
then it will only move as far as it needs to show the next match.
If the match is further away then it will position the screen with
the match in the center. If you move the screen away from the current
match, the next time you press n, the program will search forward
for a match from the beginning of the screen you are currently
looking at.

The number of matches is shown in the status bar.

You can recall your previous search entries by pressing up or down
at the search prompt. Search history is stored in the ~/.les_history
file.

Man Pages
=========

You can set your man page output to be piped through les by adding
the following to your .bashrc:

    alias man="man -P \"les -m\""

The -m option puts the name of the man page into the status line
and recognizes backspace escapes specially.

Git Diff
========

You can set les to be your git diff viewer by adding the following
to your .gitconfig:

    [core]
        pager = les

lespipe Script
==============

Files opened in les are processed through the lespipe script. This
allows the program to print out the contents of tar files or
decompress gziped files on the fly.

The default script is installed in /usr/local/share/les/lespipe,
but it can be overridden by specifying an alternate program name
with the -p option. If the script is specified as "none", then there
will be no processing of inputs.

The lespipe script is given the file name as its first argument.
If the script doesn't want to process the file, it needs to print
nothing to stdout. If the script wants to process the file, it needs
to print one line describing the type of processing, followed by
the processed content. Here is a short working example of a lespipe
script:

    #!/bin/bash
    if [ -d "$1" ]; then
        echo directory
        ls -l "$1"
    elif [[ "$1" = *.gz ]]; then
        echo gzip
        gzcat "$1"
    fi

Mousewheel Scroll
=================

This program allows you to scroll the page using the mousewheel.
It works by putting the terminal into "keyboard transmit mode" which
makes the terminal send up and down arrows when you use the mousewheel.

Recent Files List
=================

When you close a file, that file is remembered in a recent files
list, stored in ~/.les_recents. You can view the list by pressing F2.

Resuming At Your Last Position
==============================

When you reopen a file that you viewed before, you will be positioned
at the line you last left off.

Loading Forever
===============

If you are reading a file that grows such as a log file, you can
instruct the program to load forever with the -f option or the F
key bind. When additional data is read, your screen will move to
the end. When in load forever mode, the status bar will show "..."
in it. This is similar to the "tail -f" command.

Key Binds
=========

    d             go down half a screen
    D,⇟           go down a screen
    F             load forever
    g             go to the top
    G             go to the bottom
    h,←           go left one third a screen
    H,⇱           go left all the way
    j,↓           go down one line
    k,↑           go up one line
    l,→           go right one third a screen
    L,⇲           go right all the way
    m             mark position
    M             go to marked position
    n             go to next match
    N             go to previous match
    q             close file
    Q             close all files
    t             go to next tab
    T             go to previous tab
    u             go up half a screen
    U,⇞           go up a screen
    w             toggle line wrap
    /             search
    c             clear search matches
    F1            view help
    F2            view recently opened files

Improvements Over less
======================

les allows keyboard commands while loading the file.

les will close the program when Ctrl-C is pressed.

les will wrap a line at word boundaries.

In less, you sometimes have to press j multiple times to go down
one wrapped line.

In less, if the line is wrapped and you move right, line wrap is
turned off and you move right, which shuffles the screen and looks
confusing.

In less, left and right movement are bound to the arrow keys when
they should be bound to h and l.

In less, there are a lot of useless command line options and key
binds that no one will use ever. For example (-a, -A, -b, -B, -c,
-C, -d, -D, -e, -E, -f, -g, -G)

In less, if you start searching and want to cancel, pressing esc
doesn't work, it just adds more text to your search prompt.

In less if you do a search it will move the screen so the match is
at the top line, even if the match is already on screen.
