SDL ToolKit
===========

This is a simple toolkit for rendering basic UI elements under SDL.

Design
------

Widgets all share a common interface, so code usually doesn't need to care
about the type of any given widget it has a handle to.

Internally all of the widgets have a common base class and a type-specific
v-table of function pointers, implementing the common interface for the
specific type of widget.
