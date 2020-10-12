Bloodview: Live-view and control application
============================================

This is a simple SDL application that controls a Bloodlight device,
and renders samples on the screen in real time.

It allows the user to configure and run acquisitions from a simple UI.
Acquisition data is recorded to file in the current working directory,
with a filename containing the start time of the acquisition.

Building
--------

Bloodview uses [LibCYAML](https://github.com/tlsa/libcyaml), so before
attempting to build Bloodview, fetch the submodule:

    git submodule init
    git submodule update

To build Bloodview, run:

    make

You can run it with:

    make run

You can build and run a debug build with:

    make VARIANT=debug run

The Makefile allows you to pass arguments to Bloodview, so you could load
the config saved last time Bloodview was run with:

    make run BV_ARGS="-c previous.yaml"

Some [default configs](config/) are provided for each of the hardware revisions,
for example:

    make run BV_ARGS="-c rev1-default.yaml"
    make run BV_ARGS="-c rev2-default.yaml"

The main menu
-------------

The main menu can be opened at any time, and it is though the main
menu that Bloodview and the device are configured.

| Key                     | Meaning                                 |
| ----------------------- | --------------------------------------- |
| <kbd>Esc</kbd>          | Toggle whether the main menu is shown. |
| <kbd>Return</kbd>       | Activate current menu entry.            |
| <kbd>Cursor Right</kbd> | Activate current menu entry.            |
| <kbd>Cursor Left</kbd>  | Traverse up to parent menu.             |
| <kbd>Cursor Up</kbd>    | Cycle current menu entry up.            |
| <kbd>Cursor Down</kbd>  | Cycle current menu entry down.          |

If the main menu is open, it will consume keyboard input.

The graph view
--------------

During an acquisition, graphs are drawn in real time, to show the progress
of the acquisition.

| Key                     | Meaning                             |
| ----------------------- | ----------------------------------- |
| <kbd>i</kbd>            | Toggle graph inversion.             |
| <kbd>Return</kbd>       | Toggle single graph mode.           |
| <kbd>Cursor Up</kbd>    | Increase scale in the Y direction.  |
| <kbd>Cursor Down</kbd>  | Decrease scale in the Y direction.  |
| <kbd>Page Up</kbd>      | Cycle current graph selection up.   |
| <kbd>Page Down</kbd>    | Cycle current graph selection down. |
