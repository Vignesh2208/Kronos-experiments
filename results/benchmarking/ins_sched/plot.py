import plotly.graph_objects as go

mu_overshoot_err = [27.555000, 27.379999, 27.195000, 27.690001, 27.920000]
std_overshoot_err = [4.902753, 5.239812, 5.026632, 4.933951, 5.025296]

n_insns = [1000, 10000, 100000, 1000000, 10000000]
fig = go.Figure()
fig.add_trace(go.Scatter(
    x=n_insns, y=mu_overshoot_err,
    error_y=dict(type='data', array=std_overshoot_err, thickness=1.5, width=3),
    marker=dict(color='blue', size=20, symbol="diamond"),
    mode = 'markers',
    line=dict(
                color="blue",
                width=4.0,
                dash="solid",
            )
))

fig.update_layout(
    title=go.layout.Title(
        text="Perf Instruction Overshoot Error",
	font=dict(
                family="Courier",
                size=30
            ),
	xanchor = "auto",
	yanchor = "middle",
	x=0.18
    ),
    xaxis=go.layout.XAxis(
        title=go.layout.xaxis.Title(
            text="Number of progress instructions",
            font=dict(
                family="Courier",
                size=25
            )
        ),
    ),
    yaxis=go.layout.YAxis(
        title=go.layout.yaxis.Title(
            text="Overshoot error",
            font=dict(
                family="Courier",
                size=25
            )
        )
    )
)
fig.update_layout(xaxis_type="log")
fig.update_xaxes(title_font=dict(size=30, family='Courier'), tickfont=dict(family='Courier', size=30), nticks=5)
fig.update_yaxes(title_font=dict(size=30, family='Courier'), tickfont=dict(family='Courier', size=30), range=[0, 50], nticks=5)
fig.update_xaxes(ticks="outside", tickwidth=5, ticklen=10)
fig.update_yaxes(ticks="outside", tickwidth=5, ticklen=10)
fig.write_image("insn_overshoot_error.jpg", width=900, height=600)

