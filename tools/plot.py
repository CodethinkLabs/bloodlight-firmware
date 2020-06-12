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

style.use('ggplot')

max_sources = 4
max_data_points = 2000
refreshtime_ms = 100
killed = False

fig = plt.figure()
ax = [fig.add_subplot(2,2,i+ 1) for i in range(4)]
xs = [[] for i in range(max_sources)]
ys = [[] for i in range(max_sources)]
locks = [threading.Lock() for i in range(max_sources)]

buff = ''

def get_data_from_line(line):
    s,x,y = line.split(',')
    return  int(s), float(x), float(y)

def cat_2d_lists(a, b):
    return [a[i] + b[i] if i < len(b) else a[i] for i in range(len(a))]

def read_new_data_from_stdin():
    global xs, ys, buff, max_sources, killed, locks
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
                    temp_xs[s].append(x)
                    temp_ys[s].append(y)
                    buff = ''
                    data_read += 1
                    s_modded[s] = True
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

def animate(i):

    global xs, ys, locks, killed

    for i in range(max_sources):
        ax[i].clear()
        while not killed:
            if locks[i].acquire(False):
                ax[i].plot(xs[i], ys[i])
                locks[i].release()
                break
            else:
                time.sleep(0.01)

def handle_close(evt):
    global killed
    killed = True


fig.canvas.mpl_connect('close_event', handle_close)
ani = animation.FuncAnimation(fig, animate, interval=refreshtime_ms)
data_thread = threading.Thread(target=read_new_data_from_stdin)
data_thread.start()
plt.show()
