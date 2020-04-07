from __future__ import division
#import matplotlib
#import matplotlib.pyplot as plt
from config import *
import sys
import plotly.graph_objects as go
from plotly.subplots import make_subplots
import numpy as np
import math

#font = {'family' : 'normal',
#        'weight' : 'bold',
#        'size'   : 13}

#matplotlib.rc('font', **font)

gv = {
    'without_kronos': { gen:{"ts":[], "value":[]} for gen in GEN},
    '1_ms': { gen:{"ts":[], "value":[]} for gen in GEN},
    '100_ms': { gen:{"ts":[], "value":[]} for gen in GEN},
    '500_ms': { gen:{"ts":[], "value":[]} for gen in GEN}
}

lq = {
    'without_kronos': {load:{"ts":[], "value":[]} for load in [4]},
    '1_ms': {load:{"ts":[], "value":[]} for load in [4]},
    '100_ms': {load:{"ts":[], "value":[]} for load in [4]},
    '500_ms': {load:{"ts":[], "value":[]} for load in [4]}
}

bv = {

   'without_kronos': {bus:{"ts":[], "value":[]} for bus in PILOT_BUS},
   '1_ms': {bus:{"ts":[], "value":[]} for bus in PILOT_BUS},
   '100_ms': {bus:{"ts":[], "value":[]} for bus in PILOT_BUS},
   '500_ms': {bus:{"ts":[], "value":[]} for bus in PILOT_BUS}
}

proxy_log_file = 'proxy_log_{type}.txt'
types = ['without_kronos', '1_ms', '100_ms', '500_ms']
exp_type_ts = {}
for exp_type in types:
    lines = open(proxy_log_file.format(type=exp_type), "r").readlines()
    for line in lines:
        if not line.startswith("INFO:PLOG:"): continue
        d = line[10:].split(",")
        if len(d) == 1: continue
        # assert(len(d) == 6)
    
        ts = float(d[0])
        objtype = d[3]
        objid = int(d[4])
        fieldtype = d[5]
        value = float(d[6])

        for X, Y, Z in [("gen", "Vg", gv[exp_type]), ("bus", "Vm", bv[exp_type]), ("bus", "Qd", lq[exp_type])]:
            if objtype == X and fieldtype == Y:
                Z[objid]["ts"].append(float(ts))
                Z[objid]["value"].append(float(value))

    exp_type_ts[exp_type] = []
    for gen in gv[exp_type]: exp_type_ts[exp_type] += gv[exp_type][gen]["ts"]
    for bus in bv[exp_type]: exp_type_ts[exp_type] += bv[exp_type][bus]["ts"]
    for load in lq[exp_type]: exp_type_ts[exp_type] += lq[exp_type][load]["ts"]


fig = make_subplots(rows=2, cols=2, x_title='Time (sec)',
                    y_title='Generator Bus Voltage (relative change)',
		    horizontal_spacing = 0.08, vertical_spacing = 0.13,
                    subplot_titles=('(a) Best Effort (no synchronization)','(b) Kronos virtual time synchronization', '(c) 100ms switch link delays','(d) 500ms switch link delays'))

for gen in gv['without_kronos']:
    x = [x - min(exp_type_ts['without_kronos']) for x in gv['without_kronos'][gen]["ts"]]
    y = [gvi / BUS_VM[gen] for gvi in gv['without_kronos'][gen]["value"]]
    fig.add_trace(
        go.Scatter(x=x, y=y, name=gen, mode='lines+markers'),
        row=1, col=1
    )

for gen in gv['1_ms']:

    x = [x - min(exp_type_ts['1_ms']) for x in gv['1_ms'][gen]["ts"]]
    y = [gvi / BUS_VM[gen] for gvi in gv['1_ms'][gen]["value"]]
    fig.add_trace(
        go.Scatter(x=x, y=y, showlegend=False, mode='lines+markers'),
        row=1, col=2
    )


for gen in gv['100_ms']:
    x = [x - min(exp_type_ts['100_ms']) for x in gv['100_ms'][gen]["ts"]]
    y = [gvi / BUS_VM[gen] for gvi in gv['100_ms'][gen]["value"]]
    fig.add_trace(
        go.Scatter(x=x, y=y, showlegend=False, mode='lines+markers'),
        row=2, col=1
    )

for gen in gv['500_ms']:
    x = [x - min(exp_type_ts['500_ms']) for x in gv['500_ms'][gen]["ts"]]
    y = [gvi / BUS_VM[gen] for gvi in gv['500_ms'][gen]["value"]]
    fig.add_trace(
        go.Scatter(x=x, y=y, showlegend=False, mode='lines+markers'),
        row=2, col=2
    )


for i in fig['layout']['annotations']:
    i['font'] = dict(size=20)
    if 'Generator' in i['text']:
        i['x'] = -0.025

fig.update_xaxes(title_font=dict(size=25, family='Courier'), tickfont=dict(family='Courier', size=19), nticks=5)
fig.update_yaxes(title_font=dict(size=25, family='Courier'), tickfont=dict(family='Courier', size=19))
fig.update_xaxes(ticks="outside", tickwidth=5, ticklen=0)
fig.update_yaxes(ticks="outside", tickwidth=5, ticklen=0)
fig.write_image("powersim_experiment.jpg", width=1000, height=600)



