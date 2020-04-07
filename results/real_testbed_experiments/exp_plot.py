import plotly.graph_objects as go
import numpy as np
import sys
import math

folder_name = 'emu_http{type}_logs'



http_types = ['2', '11']

emulated_elapsed_time_values = {}
emulated_elapsed_time_values['2'] = []
emulated_elapsed_time_values['11'] = []

real_elapsed_time_values = {}
real_elapsed_time_values['2'] = []
real_elapsed_time_values['11'] = []


clients = [2, 3, 4, 6, 7, 8, 10, 11, 12]

real_clients = ["dot08", "dot09", "dot10", "dot12", "dot15", "dot20", "dot50", "dot60", "dot70"]

for http_type in http_types:
	for client in clients:
		content = []
		with open(folder_name.format(type=http_type) + ('/lxc-%d/lxc-%d.log' %(client, client)), 'r') as f:
			content = f.readlines()
		skip_next = False
		for line in content:
			if 'No route' in line:
				skip_next = True
			if 'Time elapsed for request completion:' in line:
				if skip_next:
					skip_next = False
					continue
				else:
					line_mod = line.rstrip()
					elapsed_time = float(line_mod.split(' ')[-1])
					emulated_elapsed_time_values[http_type].append(elapsed_time)



for http_type in http_types:
	for client in real_clients:
		content = []
		with open('raspberry_pi_logs' + ('/%s/http%s_logs/http%s_logs/container_log.txt' %(client, http_type, http_type)), 'r') as f:
			content = f.readlines()
		skip_next = False
		i = 0
		for line in content:
			if i <= 100 or i > 900:
				i += 1
				continue
			line_mod = line.rstrip()
			elapsed_time = float(line_mod)
			real_elapsed_time_values[http_type].append(elapsed_time)
			i += 1



sortedtime_http2_real = np.sort(real_elapsed_time_values['2'])
sortedtime_http11_real = np.sort(real_elapsed_time_values['11'])

sortedtime_http2_emu = np.sort(emulated_elapsed_time_values['2'])
sortedtime_http11_emu = np.sort(emulated_elapsed_time_values['11'])


p_http2_real = 1. * np.arange(len(real_elapsed_time_values['2']))/(len(real_elapsed_time_values['2']) - 1)
p_http11_real = 1. * np.arange(len(real_elapsed_time_values['11']))/(len(real_elapsed_time_values['11']) - 1)
p_http2_emu = 1. * np.arange(len(emulated_elapsed_time_values['2']))/(len(emulated_elapsed_time_values['2']) - 1)
p_http11_emu = 1. * np.arange(len(emulated_elapsed_time_values['11']))/(len(emulated_elapsed_time_values['11']) - 1)
fig = go.Figure()
fig.add_trace(go.Scatter(
    x=sortedtime_http2_real, y=p_http2_real,
    name='HTTP2 Real Testbed',
    mode='lines',
    line=dict(
                color="blue",
                width=4.0,
                dash="dashdot",
            )
))
fig.add_trace(go.Scatter(
    x=sortedtime_http2_emu, y=p_http2_emu,
    name='HTTP2 Kronos emulation',
    mode='lines',
    line=dict(
                color="green",
                width=4.0,
                dash="solid",
            )
))

fig.add_trace(go.Scatter(
    x=sortedtime_http11_real, y=p_http11_real,
    name='HTTP1.1 Real Testbed',
    mode='lines',
    line=dict(
                color="red",
                width=4.0,
                dash="longdash",
            )
))
fig.add_trace(go.Scatter(
    x=sortedtime_http11_emu, y=p_http11_emu,
    name='HTTP1.1 Kronos emulation',
    mode='lines',
    line=dict(
                color="black",
                width=4.0,
                dash="longdashdot",
            )
))

fig.update_layout(
    title=go.layout.Title(
        text="Kronos Emulation vs Real Experiment",
	font=dict(
                family="Courier",
                size=30
            ),
	xanchor = "auto",
	yanchor = "middle",
	x=0.15
    ),
    xaxis=go.layout.XAxis(
        title=go.layout.xaxis.Title(
            text="Request Completion Times (sec)",
            font=dict(
                family="Courier",
                size=25
            )
        ),
    ),
    yaxis=go.layout.YAxis(
        title=go.layout.yaxis.Title(
            text="CDF",
            font=dict(
                family="Courier",
                size=25
            )
        )
    ),

    legend=go.layout.Legend(
	x = 0.6,
	y = 0.1,
        font=dict(
            family="sans-serif",
            size=20,
            color="black"
        ),
        bgcolor="White",
        bordercolor="Black",
        borderwidth=5
    )
    
)
#fig.update_layout(xaxis_type="log")

fig.update_xaxes(title_font=dict(size=30, family='Courier'), tickfont=dict(family='Courier', size=30))
fig.update_yaxes(title_font=dict(size=30, family='Courier'), tickfont=dict(family='Courier', size=30), nticks=4)
fig.update_xaxes(ticks="outside", tickwidth=5, ticklen=10)
fig.update_yaxes(ticks="outside", tickwidth=5, ticklen=10)

fig.write_image("emu_vs_real.jpg", width=900, height=600)
			
