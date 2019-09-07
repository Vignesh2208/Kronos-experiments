import plotly.graph_objects as go
import numpy as np


x = [0.001, 0.01, 0.1, 1.0, 10.0]
y_kronos = []
y_tk = []
error_y_kronos = []
error_y_tk = []

timeslices = ["timeslice_1us","timeslice_10us","timeslice_100us","timeslice_1ms","timeslice_10ms"]

for ts in timeslices:
	with open('sysbench/kronos/%s.txt'%ts, 'r') as f:
		 content = f.readlines()
	content = [x.strip() for x in content]
	ts_values = []
	for value in content:
		ts_values.append(float(value[0:-1]))
	ts_values = np.array(ts_values)
	y_kronos.append(np.mean(ts_values))
	error_y_kronos.append(np.std(ts_values))

for ts in timeslices:
	with open('sysbench/timekeeper/%s.txt'%ts, 'r') as f:
		 content = f.readlines()
	content = [x.strip() for x in content]
	ts_values = []
	for value in content:
		ts_values.append(float(value[0:-1]))
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
        text="Sysbench Performance Comparison",
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
            text="Elased Virtual Time (sec)",
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
fig.write_image("sysbench_comparison.jpg", width=900, height=600)
