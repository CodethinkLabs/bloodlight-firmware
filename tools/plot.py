#!/usr/bin/env python3

import sys
import select
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from matplotlib import style
import datetime

style.use('ggplot')

fig = plt.figure()
ax = []
for i in range(4):
    ax.append(fig.add_subplot(2,2,i+ 1))


max_data_points = 2000
refreshtime_ms = 100

xs = [[],[],[],[]]
ys = [[],[],[],[]]
buff = ''

num_sources = -1


def get_data_from_line(line):
    s,x,y = line.split(',')
    return  int(s), float(x), float(y)

def animate(i):

    global xs, ys, buff

    max_new_data_to_read = 500
    data_read = 0
    timestart = datetime.datetime.now()
    s_modded = [False,False,False,False]
    while (datetime.datetime.now() - timestart).microseconds < (refreshtime_ms * 1000):
        r, w, e = select.select([sys.stdin],[],[],1)
        
        if sys.stdin in r:
            buff += sys.stdin.read(1)
            if buff.endswith('\n'):
                s, x, y = get_data_from_line(buff)
                xs[s].append(x)
                ys[s].append(y)
                buff = ''
                data_read += 1
                s_modded[s] = True
                if data_read >= max_new_data_to_read:
                    break
        else:
            break
    for i in range(4):
        if(s_modded[i]):
            if len(xs[i]) > max_data_points:
                xs[i] = xs[i][-max_data_points:]
                ys[i] = ys[i][-max_data_points:]

    for i in range(4):
        if(s_modded[i]):
            ax[i].clear()
            ax[i].plot(xs[i], ys[i])

ani = animation.FuncAnimation(fig, animate, interval=refreshtime_ms)
plt.show()