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

To build Bloodview, from the `host/` directory, run:

    make

You can run it with:

    make run

You can build and run a debug build with:

    make VARIANT=debug run

The Makefile allows you to pass arguments to Bloodview, so you could load
the config saved last time Bloodview was run with:

    make run BV_ARGS="-c previous.yaml"

Or use the `runp` target as a shorthand:

    make runp

Some [default configs](config/) are provided for each of the hardware revisions,
for example:

    make run BV_ARGS="-c rev1-default.yaml"
    make run BV_ARGS="-c rev2-default.yaml"

Pass `-d` to automatically select the correct default config for the device:

    make run BV_ARGS="-d"

Or use the `rund` target as a shorthand:

    make rund

User Interface
--------------

Bloodview supports user interaction using both keyboard and mouse input.
The controls depend on the context.

### The main menu

The main menu can be opened at any time, and it is though the main
menu that Bloodview and the device are configured.

| Key                     | Meaning                                 |
| ----------------------- | --------------------------------------- |
| <kbd>Esc</kbd>          | Toggle whether the main menu is shown.  |
| <kbd>Return</kbd>       | Activate current menu entry.            |
| <kbd>Cursor Right</kbd> | Activate current menu entry.            |
| <kbd>Cursor Left</kbd>  | Traverse up to parent menu.             |
| <kbd>Cursor Up</kbd>    | Cycle current menu entry up.            |
| <kbd>Cursor Down</kbd>  | Cycle current menu entry down.          |

If the main menu is open, it will consume keyboard input.

You can also use the mouse to interact with the menu. The <kbd>RIGHT</kbd>
mouse button opens the menu. Clicking outside the menu closes it.

The <kbd>LEFT</kbd> mouse button selects a given menu entry. If you're in a
sub-menu, clicking the <kbd>RIGHT</kbd> mouse button over the menu navigates
back to the parent menu.

### The graph view

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

The mouse can also be used to interact with the graphs.

Clicking the <kbd>LEFT</kbd> mouse button on a graph will enter single
graph mode. Clicking the <kbd>LEFT</kbd> mouse button again returns to
multi-graph view. While in single graph view, moving the mouse up and
down will rotate through the available graphs.

Using the vertical mouse scroll wheel will alter the vertical scaling of
the selected graph (or all graphs, if <kbd>Shift</kbd> is pressed).

Using the horizontal mouse scroll wheel will alter the horizontal scaling of
the selected graph (or all graphs, if <kbd>Shift</kbd> is pressed).

Clicking the <kbd>MIDDLE</kbd> mouse button will flip a graph upside-down.