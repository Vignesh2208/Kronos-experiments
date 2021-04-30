import plotly.graph_objects as go


x = [1000, 10000, 100000, 1000000, 10000000]

len_1k = [9.3, 19.75, 38.61, 73.69, 187.89]
len_10k = [10.64, 19.68, 38.57, 73.70, 186.81]
len_100k = [29.31, 37.85, 57.75, 93.20, 203.31]
len_1m = [204.76, 213.27, 232.60, 265.58, 380.79]
len_10m = [1953.20, 1969.67, 2002.10,  2074.15, 2149.01]

fig = go.Figure()
fig.add_trace(go.Scatter(
    x=x, y=[9.3, 10.64, 29.31, 204.76, 1953.20],
    name='Delta = 0',
    line=dict(
                color="blue",
                width=2.0,
                dash="solid",
            )
))

fig.add_trace(go.Scatter(
    x=x, y=[19.75, 19.68, 37.85, 213.27, 1969.67],
    name='Delta = 50',
    line=dict(
                color="green",
                width=2.0,
                dash="solid",
            )
))

fig.add_trace(go.Scatter(
    x=x, y=[38.61, 38.57, 57.75, 232.60, 2002.10],
    name='Delta = 100',
    line=dict(
                color="black",
                width=2.0,
                dash="solid",
            )
))


fig.add_trace(go.Scatter(
    x=x, y=[73.69, 73.70, 93.20, 265.58, 2074.15],
    name='Delta = 200',
    line=dict(
                color="red",
                width=2.0,
                dash="solid",
            )
))

fig.add_trace(go.Scatter(
    x=x, y=[187.89, 186.81, 203.31, 380.79, 2149.01],
    name='Delta = 500',
    line=dict(
                color="yellow",
                width=2.0,
                dash="solid",
            )
))






fig.update_layout(
    #title=go.layout.Title(
    #    text="INS-SCHED Microbenchmarking",
	#font=dict(
         #       family="Courier",
          #      size=30
           # ),
	#xanchor = "auto",
	#yanchor = "middle",
	#x=0
    #),
    xaxis=go.layout.XAxis(
        title=go.layout.xaxis.Title(
            text="Instruction Burst length [units]",
            font=dict(
                family="Courier",
                size=25
            )
        ),
    ),
    yaxis=go.layout.YAxis(
        title=go.layout.yaxis.Title(
            text="Elased Time (u-sec)",
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
#fig.update_layout(xaxis_type="log")
#fig.update_layout(yaxis_type="log")
fig.update_layout(legend_orientation="h")

fig.update_xaxes(title_font=dict(size=20, family='Courier'), tickfont=dict(family='Courier', size=20), nticks=5)
fig.update_yaxes(title_font=dict(size=20, family='Courier'), tickfont=dict(family='Courier', size=20))
fig.update_xaxes(ticks="outside", tickwidth=5, ticklen=10)
fig.update_yaxes(ticks="outside", tickwidth=5, ticklen=10)

#fig.show()
fig.write_image("ins_sched_micro_benchmark.jpg", width=900, height=600)


deltas = [0, 50, 100, 200, 500]
fig = go.Figure()
#fig.add_trace(go.Scatter(
#    x=deltas, y=len_1k,
#    name='1K insns',
#    line=dict(
#                color="blue",
#                width=2.0,
#                dash="solid",
#            )
#))

fig.add_trace(go.Scatter(
    x=deltas, y=len_10k,
    name='10K insns',
    line=dict(
                color="green",
                width=2.0,
                dash="dash",
            )
))

fig.add_trace(go.Scatter(
    x=deltas, y=len_100k,
    name='100K insns',
    line=dict(
                color="black",
                width=2.0,
                dash="dashdot",
            )
))


fig.add_trace(go.Scatter(
    x=deltas, y=len_1m,
    name='1M insns',
    line=dict(
                color="red",
                width=2.0,
                dash="longdash",
            )
))

fig.add_trace(go.Scatter(
    x=deltas, y=len_10m,
    name='10M insns',
    line=dict(
                color="orange",
                width=2.0,
                dash="longdashdot",
            )
))






fig.update_layout(
    title=go.layout.Title(
        text="INS-SCHED[H] Microbenchmarking",
	font=dict(
               family="Courier",
               size=30
           ),
	yanchor='top',xanchor='center',x=0.5, y=0.95
	#xanchor = "auto",
	#yanchor = "middle",
	#x=0
    ),
    xaxis=go.layout.XAxis(
        title=go.layout.xaxis.Title(
            text="Single Stepping Window size [units]",
            font=dict(
                family="Courier",
                size=25
            )
        ),
    ),
    yaxis=go.layout.YAxis(
        title=go.layout.yaxis.Title(
            text="Elapsed Time (u-sec)",
            font=dict(
                family="Courier",
                size=25
            )
        )
    ),
    legend=go.layout.Legend(
	#x = -0.05,
        #y = 1.1,
	yanchor='top',xanchor='center',y=1.1,x=0.5,
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
#fig.update_layout(xaxis_type="log")
#fig.update_layout(yaxis_type="log")
fig.update_layout(legend_orientation="h")

fig.update_xaxes(title_font=dict(size=30, family='Courier'), tickfont=dict(family='Courier', size=30), nticks=5)
fig.update_yaxes(title_font=dict(size=30, family='Courier'), tickfont=dict(family='Courier', size=30))
fig.update_xaxes(ticks="outside", tickwidth=5, ticklen=10)
fig.update_yaxes(ticks="outside", tickwidth=5, ticklen=10)


fig.write_image("ins_sched_vs_delta.jpg", width=900, height=600)


