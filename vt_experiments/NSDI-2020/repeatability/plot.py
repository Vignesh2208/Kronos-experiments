import plotly.graph_objects as go
import numpy as np


x = [0.001, 0.01, 0.1, 1.0, 10.0]
y_kronos = []
y_tk = []
error_y_kronos = []
error_y_tk = []

or_kronos = {}
or_tk = {}
error_or_kronos = {}
error_or_tk = {}

timeslices = ["timeslice_1us","timeslice_10us","timeslice_100us","timeslice_1ms","timeslice_10ms"]
n_processes = ["1_process", "10_process", "20_process", "50_process"]

for ts in timeslices:
	with open('results/kronos/%s-50_process.txt'%ts, 'r') as f:
		 content = f.readlines()
	content = [x.strip() for x in content]
	ts_values = []
	for value in content:
		ts_values.append(float(value[0:-1])/1000.0)
	ts_values = np.array(ts_values)
	y_kronos.append(np.mean(ts_values))
	error_y_kronos.append(np.std(ts_values))

for ts in timeslices:
	with open('results/timekeeper/%s-50_process.txt'%ts, 'r') as f:
		 content = f.readlines()
	content = [x.strip() for x in content]
	ts_values = []
	for value in content:
		ts_values.append(float(value[0:-1])/1000.0)
	ts_values = np.array(ts_values)
	y_tk.append(np.mean(ts_values))
	error_y_tk.append(np.std(ts_values))

print("Mu Kronos: ", y_kronos)
print("Std Kronos: ", error_y_kronos)

print("Mu TimeKeeper: ", y_tk)
print("Std TimeKeeper: ", error_y_tk)


fig = go.Figure()
fig.add_trace(go.Scatter(
    x=x, y=y_kronos,
    error_y=dict(type='data', array=error_y_kronos, thickness=1.5, width=3),
    name='Kronos',
    marker=dict(color='green', size=20, symbol='circle')
))
fig.add_trace(go.Scatter(
    x=x, y=y_tk,
    error_y=dict(type='data', array=error_y_tk, thickness=1.5, width=3),
    name='TimeKeeper',
    marker=dict(color='purple', size=20, symbol='triangle-up-dot')
))

fig.update_layout(
    title=go.layout.Title(
        text="SHA256 Hash computation times",
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
            text="Timeslice (ms)",
            font=dict(
                family="Courier",
                size=25
            )
        ),
    ),
    yaxis=go.layout.YAxis(
        title=go.layout.yaxis.Title(
            text="Elased Virtual Time (ms)",
            font=dict(
                family="Courier",
                size=25
            )
        )
    ),
    legend=go.layout.Legend(
	x = -0.05,
	y = 1.1,
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
fig.update_layout(xaxis_type="log")
fig.update_layout(legend_orientation="h")

fig.update_xaxes(title_font=dict(size=20, family='Courier'), tickfont=dict(family='Courier', size=20), nticks=5)
fig.update_yaxes(title_font=dict(size=20, family='Courier'), tickfont=dict(family='Courier', size=20))
fig.update_xaxes(ticks="outside", tickwidth=5, ticklen=10)
fig.update_yaxes(ticks="outside", tickwidth=5, ticklen=10)

#fig.show()
fig.write_image("repeatability_comparison.jpg", width=900, height=600)


for ts in timeslices:
	for process in n_processes:
		with open('results/kronos/%s-%s-overhead_ratio.txt'%(ts,process), 'r') as f:
			 content = f.readlines()
		content = [x.strip() for x in content]
		or_values = []
		for value in content:
			or_values.append(float(value[0:-1]))
		or_values = np.array(or_values)
		if process not in or_kronos:
			or_kronos[process] = []
			error_or_kronos[process] = []
		or_kronos[process].append(np.mean(or_values))
		error_or_kronos[process].append(np.std(or_values))

		with open('results/timekeeper/%s-%s-overhead_ratio.txt'%(ts,process), 'r') as f:
			 content = f.readlines()
		content = [x.strip() for x in content]
		or_values = []
		for value in content:
			or_values.append(float(value[0:-1]))
		or_values = np.array(or_values)
		if process not in or_tk:
			or_tk[process] = []
			error_or_tk[process] = []
		or_tk[process].append(np.mean(or_values))
		error_or_tk[process].append(np.std(or_values))

fig = go.Figure()
fig.add_trace(go.Scatter(
    x=x, y=or_kronos["1_process"],
    error_y=dict(type='data', array=error_or_kronos["1_process"], thickness=1.5, width=3),
    name='Kronos 1 Container',
    marker=dict(color='blue', size=20, symbol='circle'),
    line=dict(
                color="blue",
                width=4.0,
                dash="dashdot",
            )
))
fig.add_trace(go.Scatter(
    x=x, y=or_kronos["20_process"],
    error_y=dict(type='data', array=error_or_kronos["20_process"], thickness=1.5, width=3),
    name='Kronos 20 Containers',
    marker=dict(color='blue', size=20, symbol="square" ),
    line=dict(
                color="blue",
                width=4.0,
                dash="dash",
            )
))
fig.add_trace(go.Scatter(
    x=x, y=or_kronos["50_process"],
    error_y=dict(type='data', array=error_or_kronos["50_process"], thickness=1.5, width=3),
    name='Kronos 50 Containers',
    marker=dict(color='blue', size=20, symbol="diamond"),
        line=dict(
                color="blue",
                width=4.0,
                dash="longdash",
            )
))

fig.add_trace(go.Scatter(
    x=x, y=or_tk["1_process"],
    error_y=dict(type='data', array=error_or_tk["1_process"], thickness=1.5, width=3),
    name='TimeKeeper 1 Container',
    marker=dict(color='green', size=20, symbol='triangle-up-dot'),
            line=dict(
                color="green",
                width=2.0,
                dash="solid",
            )
))
fig.add_trace(go.Scatter(
    x=x, y=or_tk["20_process"],
    error_y=dict(type='data', array=error_or_tk["20_process"], thickness=1.5, width=3),
    name='TimeKeeper 20 Containers',
    marker=dict(color='green', size=20, symbol='triangle-left-dot'),
            line=dict(
                color="green",
                width=2.0,
                dash="solid",
            )
))
fig.add_trace(go.Scatter(
    x=x, y=or_tk["50_process"],
    error_y=dict(type='data', array=error_or_tk["50_process"], thickness=1.5, width=3),
    name='TimeKeeper 50 Containers',
    marker=dict(color='green', size=20, symbol='triangle-right-dot'),
            line=dict(
                color="green",
                width=2.0,
                dash="solid",
            )
))


fig.update_layout(
    title=go.layout.Title(
        text="Overhead Ratio Comparison",
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
            text="Timeslice (ms)",
            font=dict(
                family="Courier",
                size=25
            )
        ),
    ),
    yaxis=go.layout.YAxis(
        title=go.layout.yaxis.Title(
            text="Overhead Ratio",
            font=dict(
                family="Courier",
                size=25
            )
        )
    ),
    legend=go.layout.Legend(
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
fig.update_layout(xaxis_type="log")

fig.update_xaxes(title_font=dict(size=20, family='Courier'), tickfont=dict(family='Courier', size=20), nticks=5)
fig.update_yaxes(title_font=dict(size=20, family='Courier'), tickfont=dict(family='Courier', size=20))
fig.update_xaxes(ticks="outside", tickwidth=5, ticklen=10)
fig.update_yaxes(ticks="outside", tickwidth=5, ticklen=10)

#fig.show()
fig.write_image("overhead_ratio.jpg", width=900, height=600)
