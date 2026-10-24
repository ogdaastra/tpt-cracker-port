Cracker1000's Mod (The Powder Toy v55.1) - offline WebAssembly version
=======================================================================

This is the real game compiled to WebAssembly. It runs entirely in your browser; nothing is installed
and no administrator rights are needed.

Browser: Microsoft Edge or Google Chrome, version 137 or newer (the build needs WebAssembly JSPI).
Firefox and Safari may not run it; the page tells you if your browser is missing what it needs.

How to start
------------
Windows:   double-click "Start Game (Windows).cmd"
           (uses Python or Node.js if installed; otherwise it opens index.html directly)
Linux/mac: run ./start.sh
Any OS:    double-click index.html (opens the game straight from disk, no server needed)

With Python 3:  python serve.py   (or: py -3 serve.py)  then open http://127.0.0.1:8123/
With Node.js:   node serve.mjs                          then open http://127.0.0.1:8123/

Saves and settings
------------------
They are stored in the browser's storage for the address you opened the game from. The local server
address (http://127.0.0.1:8123/) and opening index.html directly are different addresses, so each has
its own saves. Use "Export saves..." / "Import save..." on the page to move saves between them, or
to back them up as ordinary .cps files.

Not available in this version: online save browser, login, multiplayer, the mod's updater.

License and source code
-----------------------
The game is free software under the GNU General Public License v3 (LICENSE.txt). The complete source
code of this build, including the WebAssembly port and build scripts, is in source/ (see SOURCE.html),
together with the FFTW source; licenses of the other bundled libraries are in THIRD_PARTY_NOTICES.txt.
