#!/usr/bin/env python3

import sys
import select
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from matplotlib import style
import datetime
import threading
import time
import numpy as np
import math

style.use('ggplot')

max_sources = 8
max_data_points = 2000
refreshtime_ms = 100
killed = False

fig = plt.figure()
axes = []
# the data source channel the axis displays
ax_channel = []
xs = [[] for i in range(max_sources)]
ys = [[] for i in range(max_sources)]
locks = [threading.Lock() for i in range(max_sources)]


def get_data_from_line(line):
    s,x,y = line.split(',')
    return  int(s), float(x), float(y)

def cat_2d_lists(a, b):
    return [a[i] + b[i] if i < len(b) else a[i] for i in range(len(a))]

def read_new_data_from_stdin(xy, ys, max_sources, killed, locks):
    buff = ''
    # If true, don't plot readings of 65535
    ignore_saturates = True
    while not killed:
        max_new_data_to_read = 500
        data_read = 0
        timestart = datetime.datetime.now()
        s_modded = [False for i in range(max_sources)]
        temp_xs = [[] for i in range(max_sources)]        
        temp_ys = [[] for i in range(max_sources)]
        while (datetime.datetime.now() - timestart).microseconds < (refreshtime_ms * 1000):
            r, w, e = select.select([sys.stdin],[],[],1)
            
            if sys.stdin in r:
                buff += sys.stdin.read(1)
                if buff.endswith('\n'):
                    s, x, y = get_data_from_line(buff)
                    if not ignore_saturates or y != 65535:
                        temp_xs[s].append(x)
                        temp_ys[s].append(y)
                        s_modded[s] = True
                    buff = ''
                    data_read += 1
                    if data_read >= max_new_data_to_read:
                        break
            else:
                break
        for i in range(max_sources):
            if(s_modded[i]):
                while not killed:
                    if locks[i].acquire(False):
                        xs[i] = xs[i] + temp_xs[i]
                        ys[i] = ys[i] + temp_ys[i]
                        if len(xs[i]) > max_data_points:
                            xs[i] = xs[i][-max_data_points:]
                            ys[i] = ys[i][-max_data_points:]

                        locks[i].release()
                        break
                    else:
                        time.sleep(0.01)
        time.sleep(0.5)

def calc_grid(n):
    rows = math.ceil(math.sqrt(n + 1/4) - 1/2)
    cols = math.ceil(math.sqrt(n))
    return rows, cols


def animate(a, xs, ys, max_sources, killed, locks, axes, ax_channel):
    for i in range(max_sources):
        # find the axes indexes that belongs to this channel, if there is one
        chan_ax = -1
        for j, chan in enumerate(ax_channel):
            if chan == i:
                chan_ax = j
        # rearrange and add a new graph if we don't have one for this data source
        # and the source has data
        draw = False
        if chan_ax == -1:
            if len(xs[i]):
                temp_ax = []
                temp_ax_channel = []
                for ax in axes:
                    temp_ax.append(ax)
                    fig.delaxes(ax)
                for ax_ch in ax_channel:
                    temp_ax_channel.append(ax_ch)
                num_graphs = len(axes) + 1
                rows, cols = calc_grid(num_graphs)
                axes[:] = []
                ax_channel[:] = []
                for j, ax in enumerate(temp_ax):
                    axes.append(fig.add_subplot(rows, cols, j + 1))
                    ax_channel.append(temp_ax_channel[j])
                axes.append(fig.add_subplot(rows, cols, len(temp_ax_channel) + 1))
                ax_channel.append(i)
                draw = True

        else:
            draw = True
        if draw:
            axes[chan_ax].clear()
            while not killed:
                if locks[i].acquire(False):
                    axes[chan_ax].plot(xs[i], ys[i])
                    locks[i].release()
                    break
                else:
                    time.sleep(0.01)

def handle_close(evt):
    global killed
    killed = True


fig.canvas.mpl_connect('close_event', handle_close)
ani = animation.FuncAnimation(fig, animate, interval=refreshtime_ms, fargs=(xs, ys, max_sources, killed, locks, axes, ax_channel))
data_thread = threading.Thread(target=read_new_data_from_stdin, args=(xs, ys, max_sources, killed, locks))
data_thread.start()
plt.show()
