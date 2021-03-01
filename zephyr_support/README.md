# Zephyr Support

## Current status

This is a work in progress section. Currently, in order to be able to use the bloodlight board with zephyr a little workaround is needed.

1. Follow the Zephy [getting started](https://docs.zephyrproject.org/latest/getting_started/index.html) steps.
2. Copy the `bloodlight_rev2` directory to `zephyr/boards/arm` directory.

Now you can run some basic test to make sure the LEDs and USB works:

1. Move to `Zephyr` directory:
2. Run a LED test, this should a blue led blink:

    ```bash
    west build -p -b bloodlight_rev2 samples/basic/blinky
    west flash
    ```

3. Run a USB  test, this should print hello_world via ttyACM* (ttyACM2 in my case):

    ```bash
    west build -p -b bloodlight_rev2 samples/subsys/usb/console/
    west flash
    ```

This should run a basic led blinking test in the board.

## Objective

The idea is to have a `Zephyr` out of tree app. To achieve this we will first need to stablish a connection between our board support files and zephyr and then between our app and zephyr.

The second task won't be hard, but the first one it might be a bit more tricky, since so far I haven't found an example of a running app that uses a board that is not supported by zephyr, and having the board support files stored outside the zephyr repository.
