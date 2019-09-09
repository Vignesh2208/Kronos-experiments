import plotly.graph_objects as go
import numpy as np
import sys
import math


theta_values = {
	'kronos-p1': [],
	'best_effort-p1': [],
	'kronos-p2': [],
	'best_effort-p2': [],

}


def read_list_from_file(file_name):
	content = []
	thetas = []
	with open(file_name, 'r') as f:
		content = f.readlines()
	for line in content:
		line = line.rstrip()
		thetas.append(float(line))
	return thetas

tstamps = np.arange(0.0, 5.001, 0.001)
theta_values['kronos-p1'] = read_list_from_file('kronos/pendulum-1.txt')
theta_values['kronos-p2'] = read_list_from_file('kronos/pendulum-2.txt')
theta_values['best_effort-p1'] = read_list_from_file('best_effort/pendulum-1.txt')
theta_values['best_effort-p2'] = read_list_from_file('best_effort/pendulum-2.txt')

fig = go.Figure()
fig.add_trace(go.Scatter(
    x=tstamps, y=theta_values['kronos-p1'],
    name='Pendulum-1 (Kronos)',
    mode='lines',
    line=dict(
                color="blue",
                width=4.0,
                dash="dashdot",
            )
))
fig.add_trace(go.Scatter(
    x=tstamps, y=theta_values['kronos-p2'],
    name='Pendulum-2 (Kronos)',
    mode='lines',
    line=dict(
                color="blue",
                width=4.0,
                dash="dash",
            )
))
fig.add_trace(go.Scatter(
    x=tstamps, y=theta_values['best_effort-p1'],
    name='Pendulum-1 (Best Effort)',
    mode='lines',
    line=dict(
                color="green",
                width=4.0,
                dash="solid",
            )
))
fig.add_trace(go.Scatter(
    x=tstamps, y=theta_values['best_effort-p2'],
    name='Pendulum-1 (Best Effort)',
    mode='lines',
    line=dict(
                color="green",
                width=4.0,
                dash="longdash",
            )
))


fig.update_layout(
    title=go.layout.Title(
        text="Pendulum PID Control with Virtual Time",
	font=dict(
                family="Courier",
                size=30
            ),
	xanchor = "auto",
	yanchor = "middle",
	x=0
    ),
    xaxis=go.layout.XAxis(
        title=go.layout.xaxis.Title(
            text="Time (sec)",
            font=dict(
                family="Courier",
                size=25
            )
        ),
    ),
    yaxis=go.layout.YAxis(
        title=go.layout.yaxis.Title(
            text="Theta (radians)",
            font=dict(
                family="Courier",
                size=25
            )
        )
    ),

    legend=go.layout.Legend(
	x=-.05,
	y=1.1,
        font=dict(
            family="sans-serif",
            size=20,
            color="black"
        ),
        #bgcolor="White",
        #bordercolor="Black",
        #borderwidth=5
    )
    
)
fig.update_layout(legend_orientation="h")

fig.update_xaxes(title_font=dict(size=20, family='Courier'), tickfont=dict(family='Courier', size=20), nticks=5, dtick=1)
fig.update_yaxes(title_font=dict(size=20, family='Courier'), tickfont=dict(family='Courier', size=20), nticks=5, dtick=1)
fig.update_xaxes(ticks="outside", tickwidth=5, ticklen=10)
fig.update_yaxes(ticks="outside", tickwidth=5, ticklen=10)

fig.write_image("pendulum_experiment.jpg", width=900, height=600)

			
